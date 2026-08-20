// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/emulation/routine_replay_corpus.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace devils_crush::emulation {
namespace {

constexpr std::array<std::byte, 8> kMagic{
    std::byte{'D'}, std::byte{'C'}, std::byte{'R'}, std::byte{'R'},
    std::byte{'E'}, std::byte{'P'}, std::byte{'L'}, std::byte{'Y'},
};
constexpr std::size_t kRomHashLength = 64U;
constexpr std::size_t kMaximumCoreRevisionLength = 128U;
constexpr std::size_t kMaximumCoreStateSize = 64U * 1024U * 1024U;
constexpr std::size_t kMaximumWorkRamSize = 64U * 1024U;
constexpr std::size_t kMaximumWrites = 1'000'000U;
constexpr std::size_t kMaximumPayloadSize = 96U * 1024U * 1024U;

[[nodiscard]] constexpr bool is_lower_hex(const char value) noexcept
{
    return (value >= '0' && value <= '9')
        || (value >= 'a' && value <= 'f');
}

[[nodiscard]] bool valid_entry(
    const RoutineReplayCorpusEntry& entry) noexcept
{
    return entry.format_version == kRoutineReplayCorpusFormatVersion
        && entry.rom_sha256.size() == kRomHashLength
        && std::ranges::all_of(entry.rom_sha256, is_lower_hex)
        && !entry.core_revision.empty()
        && entry.core_revision.size() <= kMaximumCoreRevisionLength
        && entry.ordinal != 0U
        && entry.checkpoint_entry.frame_number != 0U
        && !entry.checkpoint.core_state.empty()
        && entry.checkpoint.core_state.size() <= kMaximumCoreStateSize
        && !entry.pre_call_work_ram.empty()
        && entry.pre_call_work_ram.size() <= kMaximumWorkRamSize
        && entry.post_call_work_ram.size()
            == entry.pre_call_work_ram.size()
        && entry.expected_call.entry == entry.checkpoint_entry
        && entry.expected_call.return_absolute_master_cycle
            >= entry.expected_call.entry.absolute_master_cycle
        && entry.expected_call.work_ram_writes.size() <= kMaximumWrites;
}

void append_u8(std::vector<std::byte>& output, const std::uint8_t value)
{
    output.push_back(static_cast<std::byte>(value));
}

void append_u16(std::vector<std::byte>& output, const std::uint16_t value)
{
    for (unsigned shift = 0U; shift < 16U; shift += 8U) {
        append_u8(output, static_cast<std::uint8_t>(value >> shift));
    }
}

void append_u32(std::vector<std::byte>& output, const std::uint32_t value)
{
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        append_u8(output, static_cast<std::uint8_t>(value >> shift));
    }
}

void append_u64(std::vector<std::byte>& output, const std::uint64_t value)
{
    for (unsigned shift = 0U; shift < 64U; shift += 8U) {
        append_u8(output, static_cast<std::uint8_t>(value >> shift));
    }
}

void append_string(
    std::vector<std::byte>& output,
    const std::string_view value)
{
    append_u32(output, static_cast<std::uint32_t>(value.size()));
    for (const char character : value) {
        append_u8(output, static_cast<std::uint8_t>(character));
    }
}

void append_bytes(
    std::vector<std::byte>& output,
    const std::span<const std::byte> bytes)
{
    append_u64(output, static_cast<std::uint64_t>(bytes.size()));
    output.insert(output.end(), bytes.begin(), bytes.end());
}

void append_key(std::vector<std::byte>& output, const RoutineKey key)
{
    append_u8(output, key.physical_bank);
    append_u16(output, key.logical_pc);
}

void append_registers(
    std::vector<std::byte>& output,
    const CpuRegisters& registers)
{
    append_u16(output, registers.program_counter);
    append_u8(output, registers.accumulator);
    append_u8(output, registers.x);
    append_u8(output, registers.y);
    append_u8(output, registers.stack_pointer);
    append_u8(output, registers.status);
    for (const auto mpr : registers.mpr) {
        append_u8(output, mpr);
    }
}

void append_entry(
    std::vector<std::byte>& output,
    const RoutineEntry& entry)
{
    append_key(output, entry.routine);
    append_registers(output, entry.registers);
    append_u16(output, entry.input.player_one);
    append_u16(output, entry.input.player_two);
    append_u64(output, entry.frame_number);
    append_u32(output, entry.master_cycle);
    append_u64(output, entry.absolute_master_cycle);
    append_u32(output, entry.master_cycles_until_user_event);
    append_u32(output, entry.master_cycles_until_timer_interrupt);
}

void append_write(
    std::vector<std::byte>& output,
    const ObservedMemoryWrite& write)
{
    append_key(output, write.instruction);
    append_u16(output, write.logical_address);
    append_u16(output, write.work_ram_offset);
    append_u8(output, write.value);
    append_u64(output, write.frame_number);
    append_u32(output, write.master_cycle);
    append_u64(output, write.absolute_master_cycle);
}

[[nodiscard]] std::uint32_t crc32(
    const std::span<const std::byte> bytes) noexcept
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint32_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                0U - static_cast<std::uint32_t>(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

class Reader {
public:
    explicit Reader(const std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]] bool read_u8(std::uint8_t& value) noexcept
    {
        if (remaining() < 1U) {
            return false;
        }
        value = static_cast<std::uint8_t>(bytes_[offset_++]);
        return true;
    }

    [[nodiscard]] bool read_u16(std::uint16_t& value) noexcept
    {
        value = 0U;
        for (unsigned shift = 0U; shift < 16U; shift += 8U) {
            std::uint8_t byte = 0U;
            if (!read_u8(byte)) {
                return false;
            }
            value |= static_cast<std::uint16_t>(byte) << shift;
        }
        return true;
    }

    [[nodiscard]] bool read_u32(std::uint32_t& value) noexcept
    {
        value = 0U;
        for (unsigned shift = 0U; shift < 32U; shift += 8U) {
            std::uint8_t byte = 0U;
            if (!read_u8(byte)) {
                return false;
            }
            value |= static_cast<std::uint32_t>(byte) << shift;
        }
        return true;
    }

    [[nodiscard]] bool read_u64(std::uint64_t& value) noexcept
    {
        value = 0U;
        for (unsigned shift = 0U; shift < 64U; shift += 8U) {
            std::uint8_t byte = 0U;
            if (!read_u8(byte)) {
                return false;
            }
            value |= static_cast<std::uint64_t>(byte) << shift;
        }
        return true;
    }

    [[nodiscard]] bool read_string(
        std::string& value,
        const std::size_t maximum_length)
    {
        std::uint32_t length = 0U;
        if (!read_u32(length) || length > maximum_length
            || remaining() < length) {
            return false;
        }
        value.clear();
        value.reserve(length);
        for (std::uint32_t index = 0U; index < length; ++index) {
            value.push_back(static_cast<char>(bytes_[offset_++]));
        }
        return true;
    }

    [[nodiscard]] bool read_bytes(
        std::vector<std::byte>& value,
        const std::size_t maximum_length)
    {
        std::uint64_t length = 0U;
        if (!read_u64(length) || length > maximum_length
            || length > remaining()) {
            return false;
        }
        const auto count = static_cast<std::size_t>(length);
        value.assign(
            bytes_.begin() + static_cast<std::ptrdiff_t>(offset_),
            bytes_.begin() + static_cast<std::ptrdiff_t>(offset_ + count));
        offset_ += count;
        return true;
    }

    [[nodiscard]] bool read_key(RoutineKey& key) noexcept
    {
        return read_u8(key.physical_bank) && read_u16(key.logical_pc);
    }

    [[nodiscard]] bool read_registers(CpuRegisters& registers) noexcept
    {
        if (!read_u16(registers.program_counter)
            || !read_u8(registers.accumulator)
            || !read_u8(registers.x)
            || !read_u8(registers.y)
            || !read_u8(registers.stack_pointer)
            || !read_u8(registers.status)) {
            return false;
        }
        for (auto& mpr : registers.mpr) {
            if (!read_u8(mpr)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool read_entry(RoutineEntry& entry) noexcept
    {
        return read_key(entry.routine)
            && read_registers(entry.registers)
            && read_u16(entry.input.player_one)
            && read_u16(entry.input.player_two)
            && read_u64(entry.frame_number)
            && read_u32(entry.master_cycle)
            && read_u64(entry.absolute_master_cycle)
            && read_u32(entry.master_cycles_until_user_event)
            && read_u32(entry.master_cycles_until_timer_interrupt);
    }

    [[nodiscard]] bool read_write(ObservedMemoryWrite& write) noexcept
    {
        return read_key(write.instruction)
            && read_u16(write.logical_address)
            && read_u16(write.work_ram_offset)
            && read_u8(write.value)
            && read_u64(write.frame_number)
            && read_u32(write.master_cycle)
            && read_u64(write.absolute_master_cycle);
    }

    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return bytes_.size() - offset_;
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t offset_ = 0U;
};

[[nodiscard]] RoutineReplayCorpusDecodeResult failure(std::string message)
{
    return {
        .decoded = false,
        .entry = {},
        .message = std::move(message),
    };
}

} // namespace

std::vector<std::byte> encode_routine_replay_corpus(
    const RoutineReplayCorpusEntry& entry)
{
    if (!valid_entry(entry)) {
        throw std::invalid_argument(
            "The routine-replay corpus entry is incomplete or inconsistent.");
    }

    std::vector<std::byte> payload;
    append_string(payload, entry.rom_sha256);
    append_string(payload, entry.core_revision);
    append_u64(payload, entry.ordinal);
    append_entry(payload, entry.checkpoint_entry);
    append_u64(payload, entry.checkpoint.frame_number);
    append_bytes(payload, entry.checkpoint.core_state);
    append_bytes(payload, entry.pre_call_work_ram);
    append_key(payload, entry.expected_call.return_instruction);
    append_registers(payload, entry.expected_call.output_registers);
    append_u16(payload, entry.expected_call.caller_pc);
    append_u16(payload, entry.expected_call.return_pc);
    append_u32(payload, entry.expected_call.return_master_cycle);
    append_u64(payload, entry.expected_call.return_absolute_master_cycle);
    append_u64(payload, entry.expected_call.body_master_cycles);
    append_u32(
        payload,
        static_cast<std::uint32_t>(
            entry.expected_call.work_ram_writes.size()));
    for (const auto& write : entry.expected_call.work_ram_writes) {
        append_write(payload, write);
    }
    append_bytes(payload, entry.post_call_work_ram);

    if (payload.size() > kMaximumPayloadSize) {
        throw std::invalid_argument(
            "The routine-replay corpus payload is too large.");
    }

    std::vector<std::byte> output;
    output.reserve(kMagic.size() + 16U + payload.size());
    output.insert(output.end(), kMagic.begin(), kMagic.end());
    append_u32(output, kRoutineReplayCorpusFormatVersion);
    append_u64(output, static_cast<std::uint64_t>(payload.size()));
    append_u32(output, crc32(payload));
    output.insert(output.end(), payload.begin(), payload.end());
    return output;
}

RoutineReplayCorpusDecodeResult decode_routine_replay_corpus(
    const std::span<const std::byte> bytes)
{
    constexpr std::size_t header_size = kMagic.size() + 4U + 8U + 4U;
    if (bytes.size() < header_size) {
        return failure("The routine-replay corpus file is truncated.");
    }
    if (!std::ranges::equal(kMagic, bytes.first(kMagic.size()))) {
        return failure("The routine-replay corpus signature is invalid.");
    }

    Reader header(bytes.subspan(kMagic.size()));
    std::uint32_t version = 0U;
    std::uint64_t payload_size = 0U;
    std::uint32_t payload_crc = 0U;
    if (!header.read_u32(version) || !header.read_u64(payload_size)
        || !header.read_u32(payload_crc)) {
        return failure("The routine-replay corpus header is truncated.");
    }
    if (version != kRoutineReplayCorpusFormatVersion) {
        return failure("The routine-replay corpus version is unsupported.");
    }
    if (payload_size == 0U || payload_size > kMaximumPayloadSize
        || payload_size != bytes.size() - header_size) {
        return failure("The routine-replay corpus payload size is invalid.");
    }

    const auto payload = bytes.subspan(header_size);
    if (crc32(payload) != payload_crc) {
        return failure(
            "The routine-replay corpus payload checksum does not match.");
    }

    Reader reader(payload);
    RoutineReplayCorpusEntry entry;
    entry.format_version = version;
    if (!reader.read_string(entry.rom_sha256, kRomHashLength)
        || !reader.read_string(
            entry.core_revision, kMaximumCoreRevisionLength)
        || !reader.read_u64(entry.ordinal)
        || !reader.read_entry(entry.checkpoint_entry)
        || !reader.read_u64(entry.checkpoint.frame_number)
        || !reader.read_bytes(
            entry.checkpoint.core_state, kMaximumCoreStateSize)
        || !reader.read_bytes(
            entry.pre_call_work_ram, kMaximumWorkRamSize)
        || !reader.read_key(entry.expected_call.return_instruction)
        || !reader.read_registers(entry.expected_call.output_registers)
        || !reader.read_u16(entry.expected_call.caller_pc)
        || !reader.read_u16(entry.expected_call.return_pc)
        || !reader.read_u32(entry.expected_call.return_master_cycle)
        || !reader.read_u64(
            entry.expected_call.return_absolute_master_cycle)
        || !reader.read_u64(entry.expected_call.body_master_cycles)) {
        return failure("The routine-replay corpus metadata is truncated.");
    }

    entry.expected_call.entry = entry.checkpoint_entry;
    std::uint32_t write_count = 0U;
    if (!reader.read_u32(write_count) || write_count > kMaximumWrites) {
        return failure("The routine-replay corpus write count is invalid.");
    }
    entry.expected_call.work_ram_writes.resize(write_count);
    for (auto& write : entry.expected_call.work_ram_writes) {
        if (!reader.read_write(write)) {
            return failure(
                "The routine-replay corpus write list is truncated.");
        }
    }
    if (!reader.read_bytes(
            entry.post_call_work_ram, kMaximumWorkRamSize)) {
        return failure(
            "The routine-replay corpus post-call RAM is truncated.");
    }
    if (reader.remaining() != 0U) {
        return failure("The routine-replay corpus has trailing bytes.");
    }
    if (!valid_entry(entry)) {
        return failure(
            "The routine-replay corpus entry is inconsistent.");
    }

    return {
        .decoded = true,
        .entry = std::move(entry),
        .message = "Routine-replay corpus entry decoded.",
    };
}

} // namespace devils_crush::emulation

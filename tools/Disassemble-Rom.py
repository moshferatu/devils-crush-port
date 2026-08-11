# SPDX-License-Identifier: GPL-3.0-or-later

"""Disassembles a HuC6280 span of the ROM, addressed the way this project
writes addresses.

Everything else in the port names code as `bank:pc` -- `00:E323` for the
frame-sync spin loop, `45:4142` for the PSG programmer -- because that is
what `--instruction-profile`, `--routine-call-trace` and the registry all
emit. Nothing could turn one of those back into instructions, so reading a
candidate meant guessing at a hex dump. This does the mapping and the
decode.

The mapping is the cartridge's, not a convention: a 384 KB HuCard such as
Devil's Crush maps banks `$00`-`$3F` to the first 256 KB with the top half
mirrored, and banks `$40`-`$7F` to the last 128 KB mirrored four times, so
`45:4142` is ROM offset `$4A142` rather than anything arithmetic on `$45`.
Sizes with a different rule are refused rather than mapped approximately.

Usage:
    python tools/Disassemble-Rom.py "Devil's Crush.pce" 00:E321-E32B
    python tools/Disassemble-Rom.py "Devil's Crush.pce" 45:4142 --count 40

Output is one instruction per line: address, raw bytes, mnemonic, operand.
Branch targets are resolved to absolute addresses. A byte that is not an
opcode prints as `.db`; the disassembler does not track data, so a span
that starts mid-instruction will print nonsense until it resynchronises.
"""

import argparse
import sys

BANK_SIZE = 8192

# Operand shapes, and the total instruction length in bytes for each.
SIZES = {
    "imp": 1, "acc": 1, "imm": 2, "zp": 2, "zpx": 2, "zpy": 2, "izp": 2,
    "izx": 2, "izy": 2, "abs": 3, "abx": 3, "aby": 3, "ind": 3, "iax": 3,
    "rel": 2, "zprel": 3, "imm_zp": 3, "imm_zpx": 3, "imm_abs": 4,
    "imm_abx": 4, "blk": 7,
}

OPCODES = {}


def _op(code, name, mode):
    OPCODES[code] = (name, mode)


# The eight-way families first, so the table below stays one line per opcode.
for _bit in range(8):
    _op(0x0F + _bit * 0x10, "BBR%d" % _bit, "zprel")
    _op(0x8F + _bit * 0x10, "BBS%d" % _bit, "zprel")
    _op(0x07 + _bit * 0x10, "RMB%d" % _bit, "zp")
    _op(0x87 + _bit * 0x10, "SMB%d" % _bit, "zp")

for _code, _name, _mode in [
    (0x00, "BRK", "imp"), (0x01, "ORA", "izx"), (0x02, "SXY", "imp"),
    (0x03, "ST0", "imm"), (0x04, "TSB", "zp"), (0x05, "ORA", "zp"),
    (0x06, "ASL", "zp"), (0x08, "PHP", "imp"), (0x09, "ORA", "imm"),
    (0x0A, "ASL", "acc"), (0x0C, "TSB", "abs"), (0x0D, "ORA", "abs"),
    (0x0E, "ASL", "abs"),
    (0x10, "BPL", "rel"), (0x11, "ORA", "izy"), (0x12, "ORA", "izp"),
    (0x13, "ST1", "imm"), (0x14, "TRB", "zp"), (0x15, "ORA", "zpx"),
    (0x16, "ASL", "zpx"), (0x18, "CLC", "imp"), (0x19, "ORA", "aby"),
    (0x1A, "INC", "acc"), (0x1C, "TRB", "abs"), (0x1D, "ORA", "abx"),
    (0x1E, "ASL", "abx"),
    (0x20, "JSR", "abs"), (0x21, "AND", "izx"), (0x22, "SAX", "imp"),
    (0x23, "ST2", "imm"), (0x24, "BIT", "zp"), (0x25, "AND", "zp"),
    (0x26, "ROL", "zp"), (0x28, "PLP", "imp"), (0x29, "AND", "imm"),
    (0x2A, "ROL", "acc"), (0x2C, "BIT", "abs"), (0x2D, "AND", "abs"),
    (0x2E, "ROL", "abs"),
    (0x30, "BMI", "rel"), (0x31, "AND", "izy"), (0x32, "AND", "izp"),
    (0x34, "BIT", "zpx"), (0x35, "AND", "zpx"), (0x36, "ROL", "zpx"),
    (0x38, "SEC", "imp"), (0x39, "AND", "aby"), (0x3A, "DEC", "acc"),
    (0x3C, "BIT", "abs"), (0x3D, "AND", "abx"), (0x3E, "ROL", "abx"),
    (0x40, "RTI", "imp"), (0x41, "EOR", "izx"), (0x42, "SAY", "imp"),
    (0x43, "TMA", "imm"), (0x44, "BSR", "rel"), (0x45, "EOR", "zp"),
    (0x46, "LSR", "zp"), (0x48, "PHA", "imp"), (0x49, "EOR", "imm"),
    (0x4A, "LSR", "acc"), (0x4C, "JMP", "abs"), (0x4D, "EOR", "abs"),
    (0x4E, "LSR", "abs"),
    (0x50, "BVC", "rel"), (0x51, "EOR", "izy"), (0x52, "EOR", "izp"),
    (0x53, "TAM", "imm"), (0x54, "CSL", "imp"), (0x55, "EOR", "zpx"),
    (0x56, "LSR", "zpx"), (0x58, "CLI", "imp"), (0x59, "EOR", "aby"),
    (0x5A, "PHY", "imp"), (0x5D, "EOR", "abx"), (0x5E, "LSR", "abx"),
    (0x60, "RTS", "imp"), (0x61, "ADC", "izx"), (0x62, "CLA", "imp"),
    (0x64, "STZ", "zp"), (0x65, "ADC", "zp"), (0x66, "ROR", "zp"),
    (0x68, "PLA", "imp"), (0x69, "ADC", "imm"), (0x6A, "ROR", "acc"),
    (0x6C, "JMP", "ind"), (0x6D, "ADC", "abs"), (0x6E, "ROR", "abs"),
    (0x70, "BVS", "rel"), (0x71, "ADC", "izy"), (0x72, "ADC", "izp"),
    (0x73, "TII", "blk"), (0x74, "STZ", "zpx"), (0x75, "ADC", "zpx"),
    (0x76, "ROR", "zpx"), (0x78, "SEI", "imp"), (0x79, "ADC", "aby"),
    (0x7A, "PLY", "imp"), (0x7C, "JMP", "iax"), (0x7D, "ADC", "abx"),
    (0x7E, "ROR", "abx"),
    (0x80, "BRA", "rel"), (0x81, "STA", "izx"), (0x82, "CLX", "imp"),
    (0x83, "TST", "imm_zp"), (0x84, "STY", "zp"), (0x85, "STA", "zp"),
    (0x86, "STX", "zp"), (0x88, "DEY", "imp"), (0x89, "BIT", "imm"),
    (0x8A, "TXA", "imp"), (0x8C, "STY", "abs"), (0x8D, "STA", "abs"),
    (0x8E, "STX", "abs"),
    (0x90, "BCC", "rel"), (0x91, "STA", "izy"), (0x92, "STA", "izp"),
    (0x93, "TST", "imm_abs"), (0x94, "STY", "zpx"), (0x95, "STA", "zpx"),
    (0x96, "STX", "zpy"), (0x98, "TYA", "imp"), (0x99, "STA", "aby"),
    (0x9A, "TXS", "imp"), (0x9C, "STZ", "abs"), (0x9D, "STA", "abx"),
    (0x9E, "STZ", "abx"),
    (0xA0, "LDY", "imm"), (0xA1, "LDA", "izx"), (0xA2, "LDX", "imm"),
    (0xA3, "TST", "imm_zpx"), (0xA4, "LDY", "zp"), (0xA5, "LDA", "zp"),
    (0xA6, "LDX", "zp"), (0xA8, "TAY", "imp"), (0xA9, "LDA", "imm"),
    (0xAA, "TAX", "imp"), (0xAC, "LDY", "abs"), (0xAD, "LDA", "abs"),
    (0xAE, "LDX", "abs"),
    (0xB0, "BCS", "rel"), (0xB1, "LDA", "izy"), (0xB2, "LDA", "izp"),
    (0xB3, "TST", "imm_abx"), (0xB4, "LDY", "zpx"), (0xB5, "LDA", "zpx"),
    (0xB6, "LDX", "zpy"), (0xB8, "CLV", "imp"), (0xB9, "LDA", "aby"),
    (0xBA, "TSX", "imp"), (0xBC, "LDY", "abx"), (0xBD, "LDA", "abx"),
    (0xBE, "LDX", "aby"),
    (0xC0, "CPY", "imm"), (0xC1, "CMP", "izx"), (0xC2, "CLY", "imp"),
    (0xC3, "TDD", "blk"), (0xC4, "CPY", "zp"), (0xC5, "CMP", "zp"),
    (0xC6, "DEC", "zp"), (0xC8, "INY", "imp"), (0xC9, "CMP", "imm"),
    (0xCA, "DEX", "imp"), (0xCC, "CPY", "abs"), (0xCD, "CMP", "abs"),
    (0xCE, "DEC", "abs"),
    (0xD0, "BNE", "rel"), (0xD1, "CMP", "izy"), (0xD2, "CMP", "izp"),
    (0xD3, "TIN", "blk"), (0xD4, "CSH", "imp"), (0xD5, "CMP", "zpx"),
    (0xD6, "DEC", "zpx"), (0xD8, "CLD", "imp"), (0xD9, "CMP", "aby"),
    (0xDA, "PHX", "imp"), (0xDD, "CMP", "abx"), (0xDE, "DEC", "abx"),
    (0xE0, "CPX", "imm"), (0xE1, "SBC", "izx"), (0xE3, "TIA", "blk"),
    (0xE4, "CPX", "zp"), (0xE5, "SBC", "zp"), (0xE6, "INC", "zp"),
    (0xE8, "INX", "imp"), (0xE9, "SBC", "imm"), (0xEA, "NOP", "imp"),
    (0xEC, "CPX", "abs"), (0xED, "SBC", "abs"), (0xEE, "INC", "abs"),
    (0xF0, "BEQ", "rel"), (0xF1, "SBC", "izy"), (0xF2, "SBC", "izp"),
    (0xF3, "TAI", "blk"), (0xF4, "SET", "imp"), (0xF5, "SBC", "zpx"),
    (0xF6, "INC", "zpx"), (0xF8, "SED", "imp"), (0xF9, "SBC", "aby"),
    (0xFA, "PLX", "imp"), (0xFD, "SBC", "abx"), (0xFE, "INC", "abx"),
]:
    _op(_code, _name, _mode)


def rom_offset(size, bank, address):
    """Maps a `bank:pc` pair to a ROM offset, or raises for a cartridge
    whose mapping this does not know."""
    within = address & (BANK_SIZE - 1)

    if size == 0x60000:  # 384 KB, the Devil's Crush shape
        if bank < 0x40:
            return (bank & 0x1F) * BANK_SIZE + within
        if bank < 0x80:
            return ((bank & 0x0F) + 32) * BANK_SIZE + within
        raise ValueError("bank $%02X is not ROM on a 384 KB HuCard" % bank)

    banks = size // BANK_SIZE
    if banks and size % BANK_SIZE == 0:
        # The plain case: banks repeat to fill the 128 available.
        return (bank % banks) * BANK_SIZE + within

    raise ValueError(
        "no known bank mapping for a %d-byte ROM; only sizes that are a "
        "whole number of 8 KB banks are handled, and 384 KB specially"
        % size)


def format_operand(mode, address, raw):
    if mode == "imp":
        return ""
    if mode == "acc":
        return "A"
    if mode == "imm":
        return "#$%02X" % raw[1]
    if mode == "zp":
        return "<$%02X" % raw[1]
    if mode == "zpx":
        return "<$%02X,X" % raw[1]
    if mode == "zpy":
        return "<$%02X,Y" % raw[1]
    if mode == "izp":
        return "[$%02X]" % raw[1]
    if mode == "izx":
        return "[$%02X,X]" % raw[1]
    if mode == "izy":
        return "[$%02X],Y" % raw[1]
    if mode == "abs":
        return "$%04X" % (raw[1] | raw[2] << 8)
    if mode == "abx":
        return "$%04X,X" % (raw[1] | raw[2] << 8)
    if mode == "aby":
        return "$%04X,Y" % (raw[1] | raw[2] << 8)
    if mode == "ind":
        return "[$%04X]" % (raw[1] | raw[2] << 8)
    if mode == "iax":
        return "[$%04X,X]" % (raw[1] | raw[2] << 8)
    if mode == "rel":
        return "$%04X" % ((address + 2 + signed(raw[1])) & 0xFFFF)
    if mode == "zprel":
        return "<$%02X, $%04X" % (
            raw[1], (address + 3 + signed(raw[2])) & 0xFFFF)
    if mode == "imm_zp":
        return "#$%02X, <$%02X" % (raw[1], raw[2])
    if mode == "imm_zpx":
        return "#$%02X, <$%02X,X" % (raw[1], raw[2])
    if mode == "imm_abs":
        return "#$%02X, $%04X" % (raw[1], raw[2] | raw[3] << 8)
    if mode == "imm_abx":
        return "#$%02X, $%04X,X" % (raw[1], raw[2] | raw[3] << 8)
    if mode == "blk":
        return "$%04X, $%04X, $%04X" % (
            raw[1] | raw[2] << 8, raw[3] | raw[4] << 8, raw[5] | raw[6] << 8)
    raise AssertionError("unhandled operand shape %r" % mode)


def signed(byte):
    return byte - 256 if byte > 127 else byte


def parse_span(text):
    """`00:E321-E32B`, or `45:4142` for a start with no end."""
    if ":" not in text:
        raise ValueError(
            "expected bank:address, as in 00:E321-E32B, not %r" % text)

    bank_text, rest = text.split(":", 1)
    bank = int(bank_text, 16)

    if "-" in rest:
        start_text, end_text = rest.split("-", 1)
        return bank, int(start_text, 16), int(end_text, 16)

    return bank, int(rest, 16), None


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Disassembles a HuC6280 span addressed as bank:pc.")
    parser.add_argument("rom", help="path to the .pce image")
    parser.add_argument(
        "span",
        help="bank:start-end, or bank:start with --count")
    parser.add_argument(
        "--count", type=int, default=None,
        help="instructions to decode when the span has no end (default 24)")
    arguments = parser.parse_args(argv)

    with open(arguments.rom, "rb") as handle:
        rom = handle.read()

    try:
        bank, start, end = parse_span(arguments.span)
    except ValueError as error:
        parser.error(str(error))

    if end is not None and arguments.count is not None:
        parser.error("--count and an explicit end are mutually exclusive")

    limit = arguments.count if arguments.count is not None else 24
    try:
        base = rom_offset(len(rom), bank, start)
    except ValueError as error:
        parser.error(str(error))

    address = start
    decoded = 0
    while True:
        if end is not None:
            if address > end:
                break
        elif decoded >= limit:
            break

        offset = base + (address - start)
        if offset >= len(rom):
            break

        name, mode = OPCODES.get(rom[offset], (None, None))
        if name is None:
            print("%02X:%04X  %-20s .db   $%02X"
                  % (bank, address, "%02X" % rom[offset], rom[offset]))
            address += 1
            decoded += 1
            continue

        length = SIZES[mode]
        raw = rom[offset:offset + length]
        if len(raw) < length:
            break

        print("%02X:%04X  %-20s %-5s %s" % (
            bank, address,
            " ".join("%02X" % byte for byte in raw),
            name, format_operand(mode, address, raw)))
        address += length
        decoded += 1

    return 0


if __name__ == "__main__":
    sys.exit(main())

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/video/pce_vdc.hpp"

#include <cstdint>

namespace devils_crush::video {

// What a colour encoder needs from a video display controller, and nothing
// else.
//
// This is the first place in the port where two devices have to talk to each
// other rather than each talking to the emulator. The encoder owns the
// schedule -- it divides the master clock, counts the two blanking periods,
// and decides when the controller runs -- so it has to hold a controller,
// and there are two of those: the native model and the emulator's own chip.
// An interface is what lets the encoder be installed on its own, with the
// controller either native or handed back, instead of only in a pair.
//
// The eight methods here are exactly the ones `VCE` calls, in the emulator's
// own signatures, with two departures. `next_event` is gone: it is an
// in/out parameter of `VDC::Read` and `VDC::Write` that neither body reads
// or assigns, so passing it would suggest a contract that does not exist.
// And the debugging peeks are absent, because they are the frame snapshot's
// path into the controller and never the encoder's.
class VdcDevice {
public:
    virtual ~VdcDevice() = default;

    // Returns the first clock count at which the device has something to do.
    // Every other call returns the same, and the encoder is required to keep
    // the most recent one: it bounds the chunk the schedule may run.
    virtual std::int32_t reset() noexcept = 0;

    // `clocks` dot clocks, one output word per clock into `pixels` unless
    // `skip` is set. `pixels` must hold at least `clocks` words; the
    // emulator gives it 2048.
    virtual std::int32_t run(
        std::int32_t clocks,
        std::uint16_t* pixels,
        bool skip) noexcept
        = 0;
    virtual std::int32_t hsync(bool in_hblank) noexcept = 0;
    virtual std::int32_t vsync(bool in_vblank) noexcept = 0;

    virtual void write(std::uint32_t address, std::uint8_t value) noexcept
        = 0;
    virtual std::uint8_t read(std::uint32_t address, bool peek) noexcept = 0;

    // The interrupt line, re-derived rather than remembered. `VCE` throws
    // away the direction its hook is passed and asks every chip instead,
    // which is what lets one chip be native while the other is not.
    [[nodiscard]] virtual bool peek_irq() const noexcept = 0;
    // `id` is one of `PceVdc::Register`, which numbers as the emulator's
    // `VDC::GSREG_*` does. The encoder reads HSR and HDR for its own
    // scanline bookkeeping and nothing else.
    [[nodiscard]] virtual std::uint32_t get_register(
        unsigned id) const noexcept
        = 0;

    virtual void set_layer_enable_mask(std::uint64_t mask) noexcept = 0;
    virtual void set_unlimited_sprites(bool unlimited) noexcept = 0;
};

// The native controller behind that interface.
//
// A wrapper rather than a base class of `PceVdc`. The controller is already
// installed and proved against the emulator's own chip, and giving it a
// vtable and two widened signatures to satisfy an interface would be a
// change to a device that is currently correct, made for the benefit of a
// device that is not yet written. The wrapper costs one indirection on a
// call that already crosses a scheduling boundary.
class PceVdcDevice final : public VdcDevice {
public:
    explicit PceVdcDevice(PceVdc& vdc) noexcept
        : vdc_(&vdc)
    {
    }

    [[nodiscard]] PceVdc& device() const noexcept { return *vdc_; }

    std::int32_t reset() noexcept override { return vdc_->reset(); }

    std::int32_t run(
        const std::int32_t clocks,
        std::uint16_t* const pixels,
        const bool skip) noexcept override
    {
        return vdc_->run(clocks, pixels, skip);
    }

    std::int32_t hsync(const bool in_hblank) noexcept override
    {
        return vdc_->hsync(in_hblank);
    }

    std::int32_t vsync(const bool in_vblank) noexcept override
    {
        return vdc_->vsync(in_vblank);
    }

    void write(
        const std::uint32_t address,
        const std::uint8_t value) noexcept override
    {
        vdc_->write(address, value);
    }

    std::uint8_t read(
        const std::uint32_t address,
        const bool peek) noexcept override
    {
        return vdc_->read(address, peek);
    }

    [[nodiscard]] bool peek_irq() const noexcept override
    {
        return vdc_->peek_irq();
    }

    [[nodiscard]] std::uint32_t get_register(
        const unsigned id) const noexcept override
    {
        return vdc_->get_register(static_cast<PceVdc::Register>(id));
    }

    void set_layer_enable_mask(const std::uint64_t mask) noexcept override
    {
        vdc_->set_layer_enable_mask(mask);
    }

    void set_unlimited_sprites(const bool unlimited) noexcept override
    {
        vdc_->set_unlimited_sprites(unlimited);
    }

private:
    PceVdc* vdc_;
};

}  // namespace devils_crush::video

-- SPDX-License-Identifier: GPL-3.0-or-later

-- Audio-only Mesen 2.1.1 oracle for the host's 600-frame canonical input
-- sequence. A runner patched only at the managed command-line boundary starts
-- and stops MesenCore.dll's existing WaveRecorder around this script.

local frame_count = 0

local backup_ram_size = emu.getMemorySize(emu.memType.pceSaveRam)
if backup_ram_size ~= 2048 then
  error("Mesen backup RAM must be exactly 2048 bytes")
end
local mesen_backup_header = {
  0x48, 0x55, 0x42, 0x4D, 0x00, 0xA0, 0x10, 0x80
}
for address = 0, #mesen_backup_header - 1 do
  local expected = mesen_backup_header[address + 1]
  local actual = emu.read(address, emu.memType.pceSaveRam)
  local normalized_repeat =
    address == 5 and actual == 0x88
  if actual ~= expected and not normalized_repeat then
    error("Mesen backup RAM has an unexpected initial header")
  end
end
emu.write(5, 0x88, emu.memType.pceSaveRam)

local function on_input_polled()
  local frame = emu.getState().frameCount + 1
  emu.setInput({
    i = frame == 420 or frame == 421 or frame == 540 or frame == 541,
    ii = frame == 480 or frame == 481,
    run = frame == 180 or frame == 181
      or frame == 300 or frame == 301,
    select = false,
    up = false,
    down = false,
    left = frame == 540 or frame == 541,
    right = false
  }, 0)
end

local function on_end_frame()
  frame_count = frame_count + 1
  if frame_count == 600 then
    emu.stop(0)
  end
end

emu.addEventCallback(on_input_polled, emu.eventType.inputPolled)
emu.addEventCallback(on_end_frame, emu.eventType.endFrame)

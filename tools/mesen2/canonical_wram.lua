-- SPDX-License-Identifier: GPL-3.0-or-later

-- Mesen 2.1.1 headless oracle for the host's 600-frame canonical input
-- sequence. The launcher sets both output paths to ignored files.

local output_path = os.getenv("DC_MESEN_WRAM_OUTPUT")
if output_path == nil or output_path == "" then
  error("DC_MESEN_WRAM_OUTPUT is required")
end
local framebuffer_path = os.getenv("DC_MESEN_FRAMEBUFFER_OUTPUT")
if framebuffer_path == nil or framebuffer_path == "" then
  error("DC_MESEN_FRAMEBUFFER_OUTPUT is required")
end
local trace_path = os.getenv("DC_MESEN_TRACE_OUTPUT")
if trace_path == nil or trace_path == "" then
  error("DC_MESEN_TRACE_OUTPUT is required")
end
local cpu_path = os.getenv("DC_MESEN_CPU_OUTPUT")
if cpu_path == nil or cpu_path == "" then
  error("DC_MESEN_CPU_OUTPUT is required")
end

local output = assert(io.open(output_path, "wb"))
local framebuffer_output = assert(io.open(framebuffer_path, "wb"))
local trace_output = assert(io.open(trace_path, "w"))
local cpu_output = assert(io.open(cpu_path, "w"))
local frame_count = 0
local framebuffer_width = 0
local framebuffer_height = 0
local wrote_state_keys = false

-- Mesen initializes the 2 KiB CD-style backup RAM header with an end pointer
-- of $A000, while the pinned Beetle HuCard baseline uses $8800. Devil's Crush
-- copies the resulting $1FF0/$07F0 free-space difference into work RAM.
-- Start both oracles from Beetle's exact eight-byte header so the comparison
-- measures emulation behavior rather than frontend initialization policy.
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

local differing_offsets = {
  0x00B4,
  0x0D5C,
  0x01F6,
  0x01F2,
  0x01EF,
  0x002F,
  0x01F1,
  0x01ED,
  0x01F3,
  0x0030,
  0x01F0,
  0x01F4,
  0x01EE,
  0x01F5,
  0x0037,
  0x0038,
  0x01F7
}

local function state_value(state, key)
  local value = state[key]
  if value == nil then
    return -1
  end
  return value
end

local function write_state_keys(state)
  if wrote_state_keys then
    return
  end

  local keys = {}
  for key, _ in pairs(state) do
    keys[#keys + 1] = key
  end
  table.sort(keys)
  for _, key in ipairs(keys) do
    trace_output:write(
      string.format("#state,%s,%s\n", key, tostring(state[key])))
  end
  wrote_state_keys = true
end

local function on_differing_write(address, value)
  local state = emu.getState()
  write_state_keys(state)
  local pc = state_value(state, "cpu.pc")
  local mprs = {}
  for index = 0, 7 do
    mprs[index + 1] =
      state_value(state, string.format("memoryManager.mpr[%d]", index))
  end
  local pc_mpr = mprs[((pc >> 13) & 0x07) + 1]
  local physical_pc = pc_mpr * 0x2000 + (pc & 0x1FFF)
  trace_output:write(string.format(
    "%d,%d,0x%04X,0x%04X,0x%02X,0x%04X,0x%02X,0x%06X,"
      .. "0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,"
      .. "0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,"
      .. "0x%02X,0x%02X\n",
    state_value(state, "frameCount") + 1,
    state_value(state, "masterClock"),
    address,
    address & 0x1FFF,
    value,
    pc,
    pc_mpr,
    physical_pc,
    state_value(state, "cpu.a"),
    state_value(state, "cpu.x"),
    state_value(state, "cpu.y"),
    state_value(state, "cpu.sp"),
    state_value(state, "cpu.ps"),
    mprs[1],
    mprs[2],
    mprs[3],
    mprs[4],
    mprs[5],
    mprs[6],
    mprs[7],
    mprs[8]))
end

trace_output:write(
  "frame,masterClock,logicalAddress,workRamOffset,value,pc,pcMpr,"
    .. "physicalPc,a,x,y,sp,ps,"
    .. "mpr0,mpr1,mpr2,mpr3,mpr4,mpr5,mpr6,mpr7\n")
cpu_output:write(
  "frame,masterClock,pc,pcMpr,physicalPc,a,x,y,sp,ps\n")
for _, offset in ipairs(differing_offsets) do
  emu.addMemoryCallback(
    on_differing_write,
    emu.callbackType.write,
    offset,
    offset,
    emu.cpuType.pce,
    emu.memType.pceWorkRam)
end

local function on_input_polled()
  -- During an input-poll callback frameCount names the last completed frame.
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
  local size = emu.getMemorySize(emu.memType.pceWorkRam)
  if size ~= 8192 then
    output:close()
    framebuffer_output:close()
    trace_output:close()
    cpu_output:close()
    emu.stop(2)
    return
  end

  local bytes = {}
  for address = 0, size - 1 do
    bytes[#bytes + 1] =
      string.char(emu.read(address, emu.memType.pceWorkRam))
  end
  output:write(table.concat(bytes))

  local screen_size = emu.getScreenSize()
  if frame_count == 1 then
    framebuffer_width = screen_size.width
    framebuffer_height = screen_size.height
    framebuffer_output:write("DCF1")
    framebuffer_output:write(string.pack(
      "<I4I4I4",
      framebuffer_width,
      framebuffer_height,
      600))
  elseif screen_size.width ~= framebuffer_width
      or screen_size.height ~= framebuffer_height then
    output:close()
    framebuffer_output:close()
    trace_output:close()
    cpu_output:close()
    emu.stop(3)
    return
  end

  local screen = emu.getScreenBuffer()
  local rgb = {}
  for index = 1, #screen do
    local pixel = screen[index]
    rgb[index] = string.char(
      (pixel >> 16) & 0xFF,
      (pixel >> 8) & 0xFF,
      pixel & 0xFF)
  end
  framebuffer_output:write(table.concat(rgb))

  local state = emu.getState()
  local pc = state_value(state, "cpu.pc")
  local pc_mpr = state_value(
    state,
    string.format("memoryManager.mpr[%d]", (pc >> 13) & 0x07))
  local physical_pc = pc_mpr * 0x2000 + (pc & 0x1FFF)
  cpu_output:write(string.format(
    "%d,%d,0x%04X,0x%02X,0x%06X,0x%02X,0x%02X,0x%02X,"
      .. "0x%02X,0x%02X\n",
    frame_count,
    state_value(state, "masterClock"),
    pc,
    pc_mpr,
    physical_pc,
    state_value(state, "cpu.a"),
    state_value(state, "cpu.x"),
    state_value(state, "cpu.y"),
    state_value(state, "cpu.sp"),
    state_value(state, "cpu.ps")))

  if frame_count == 600 then
    output:close()
    framebuffer_output:close()
    trace_output:close()
    cpu_output:close()
    emu.stop(0)
  end
end

emu.addEventCallback(on_input_polled, emu.eventType.inputPolled)
emu.addEventCallback(on_end_frame, emu.eventType.endFrame)

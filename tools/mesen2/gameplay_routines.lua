-- SPDX-License-Identifier: GPL-3.0-or-later

-- Mesen 2.1.1 oracle for the host's deterministic 1,800-frame gameplay
-- route. The launcher supplies ignored output paths.

local trace_path = os.getenv("DC_MESEN_GAMEPLAY_TRACE_OUTPUT")
if trace_path == nil or trace_path == "" then
  error("DC_MESEN_GAMEPLAY_TRACE_OUTPUT is required")
end
local work_ram_path = os.getenv("DC_MESEN_GAMEPLAY_WRAM_OUTPUT")
if work_ram_path == nil or work_ram_path == "" then
  error("DC_MESEN_GAMEPLAY_WRAM_OUTPUT is required")
end
local call_path = os.getenv("DC_MESEN_GAMEPLAY_CALL_OUTPUT")
if call_path == nil or call_path == "" then
  error("DC_MESEN_GAMEPLAY_CALL_OUTPUT is required")
end
local write_path = os.getenv("DC_MESEN_GAMEPLAY_WRITE_OUTPUT")
if write_path == nil or write_path == "" then
  error("DC_MESEN_GAMEPLAY_WRITE_OUTPUT is required")
end

local trace_output = assert(io.open(trace_path, "w"))
local work_ram_output = assert(io.open(work_ram_path, "wb"))
local call_output = assert(io.open(call_path, "w"))
local write_output = assert(io.open(write_path, "w"))
local frame_count = 0
local sequence = 0
local call_sequence = 0
local active_calls = {}
local profile = os.getenv("DC_MESEN_GAMEPLAY_PROFILE") or "gameplay"

-- Match Beetle's HuCard backup-RAM initialization policy.
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
  local normalized_repeat = address == 5 and actual == 0x88
  if actual ~= expected and not normalized_repeat then
    error("Mesen backup RAM has an unexpected initial header")
  end
end
emu.write(5, 0x88, emu.memType.pceSaveRam)

local gameplay_routines = {
  {
    offset = 0x000195,
    bank = 0x00,
    pc = 0xE195,
    returns = { [0xE1F7] = true }
  },
  {
    offset = 0x0023CB,
    bank = 0x01,
    pc = 0xC3CB,
    returns = {
      [0xC3DC] = true,
      [0xC3FA] = true,
      [0xC43E] = true,
      [0xC456] = true,
      [0xC49D] = true
    }
  },
  {
    offset = 0x00249E,
    bank = 0x01,
    pc = 0xC49E,
    returns = { [0xC4C8] = true, [0xC52B] = true }
  },
  {
    offset = 0x006966,
    bank = 0x03,
    pc = 0x6966,
    returns = {
      [0x6988] = true,
      [0x6994] = true,
      [0x69A1] = true,
      [0x6A37] = true
    }
  }
}

local physics_routines = {
  {
    offset = 0x0011B4,
    bank = 0x00,
    pc = 0xF1B4,
    returns = { [0xF240] = true }
  },
  {
    offset = 0x001241,
    bank = 0x00,
    pc = 0xF241,
    returns = { [0xF25F] = true }
  },
  {
    offset = 0x0031A2,
    bank = 0x01,
    pc = 0xD1A2,
    returns = { [0xD227] = true }
  },
  {
    offset = 0x003D09,
    bank = 0x01,
    pc = 0xDD09,
    returns = { [0xDD6D] = true }
  }
}

local routines = gameplay_routines
if profile == "physics" then
  routines = physics_routines
elseif profile ~= "gameplay" then
  error("Unknown gameplay routine profile: " .. profile)
end

local function state_value(state, key)
  local value = state[key]
  if value == nil then
    return -1
  end
  return value
end

trace_output:write(
  "sequence,frame,master_cycle,physical_bank,logical_pc,"
    .. "caller_bank,caller_pc,"
    .. "a,x,y,sp,status,mpr0,mpr1,mpr2,mpr3,mpr4,mpr5,mpr6,mpr7\n")
call_output:write(
  "sequence,frame,entry_absolute_master_cycle,physical_bank,logical_pc,"
    .. "caller_pc,return_pc,return_physical_bank,return_instruction_pc,"
    .. "return_absolute_master_cycle,body_master_cycles,"
    .. "input_a,input_x,input_y,input_sp,input_status,"
    .. "input_mpr0,input_mpr1,input_mpr2,input_mpr3,"
    .. "input_mpr4,input_mpr5,input_mpr6,input_mpr7,"
    .. "output_a,output_x,output_y,output_sp,output_status,"
    .. "output_mpr0,output_mpr1,output_mpr2,output_mpr3,"
    .. "output_mpr4,output_mpr5,output_mpr6,output_mpr7,write_count\n")
write_output:write(
  "call_sequence,write_sequence,absolute_master_cycle,"
    .. "instruction_physical_bank,instruction_logical_pc,"
    .. "work_ram_offset,value\n")

local function on_routine_entry(address, routine)
  local state = emu.getState()
  local pc = state_value(state, "cpu.pc")
  local pc_mpr = state_value(
    state,
    string.format("memoryManager.mpr[%d]", (pc >> 13) & 0x07))
  local physical_pc = pc_mpr * 0x2000 + (pc & 0x1FFF)
  if pc ~= address or physical_pc ~= routine.offset then
    error("Mesen execution callback reported an inconsistent address")
  end

  sequence = sequence + 1
  local mprs = {}
  for index = 0, 7 do
    mprs[index + 1] =
      state_value(state, string.format("memoryManager.mpr[%d]", index))
  end
  local stack_pointer = state_value(state, "cpu.sp")
  local stacked_return = emu.read(
      0x100 | ((stack_pointer + 1) & 0xFF),
      emu.memType.pceWorkRam)
    | (emu.read(
        0x100 | ((stack_pointer + 2) & 0xFF),
        emu.memType.pceWorkRam) << 8)
  local caller_pc = (stacked_return - 2) & 0xFFFF
  local return_pc = (stacked_return + 1) & 0xFFFF
  local caller_bank = mprs[((caller_pc >> 13) & 0x07) + 1]
  trace_output:write(string.format(
    "%d,%d,%d,%02X,%04X,%02X,%04X,%02X,%02X,%02X,%02X,%02X,"
      .. "%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
    sequence,
    state_value(state, "frameCount") + 1,
    state_value(state, "masterClock"),
    pc_mpr,
    pc,
    caller_bank,
    caller_pc,
    state_value(state, "cpu.a"),
    state_value(state, "cpu.x"),
    state_value(state, "cpu.y"),
    stack_pointer,
    state_value(state, "cpu.ps"),
    mprs[1],
    mprs[2],
    mprs[3],
    mprs[4],
    mprs[5],
    mprs[6],
    mprs[7],
    mprs[8]))

  call_sequence = call_sequence + 1
  active_calls[#active_calls + 1] = {
    sequence = call_sequence,
    routine = routine,
    frame = state_value(state, "frameCount") + 1,
    entry_cycle = state_value(state, "masterClock"),
    caller_pc = caller_pc,
    return_pc = return_pc,
    stack_pointer = stack_pointer,
    input_a = state_value(state, "cpu.a"),
    input_x = state_value(state, "cpu.x"),
    input_y = state_value(state, "cpu.y"),
    input_status = state_value(state, "cpu.ps"),
    input_mprs = mprs,
    write_count = 0
  }
end

for _, routine in ipairs(routines) do
  local expected_routine = routine
  emu.addMemoryCallback(
    function(address)
      if address ~= expected_routine.pc then
        error("Mesen execution callback entered the wrong routine")
      end
      on_routine_entry(address, expected_routine)
    end,
    emu.callbackType.exec,
    routine.pc,
    routine.pc,
    emu.cpuType.pce,
    emu.memType.pceMemory)
end

local return_callbacks = {}
for _, routine in ipairs(routines) do
  for return_pc, _ in pairs(routine.returns) do
    if return_callbacks[return_pc] == nil then
      return_callbacks[return_pc] = {}
    end
    return_callbacks[return_pc][#return_callbacks[return_pc] + 1] = routine
  end
end

local function on_routine_return(address, candidates)
  local state = emu.getState()
  local stack_pointer = state_value(state, "cpu.sp")
  local pc_mpr = state_value(
    state,
    string.format("memoryManager.mpr[%d]", (address >> 13) & 0x07))
  local cycle = state_value(state, "masterClock")
  local output_mprs = {}
  for index = 0, 7 do
    output_mprs[index + 1] =
      state_value(state, string.format("memoryManager.mpr[%d]", index))
  end

  for index = #active_calls, 1, -1 do
    local active = active_calls[index]
    local candidate = false
    for _, routine in ipairs(candidates) do
      if active.routine == routine then
        candidate = true
        break
      end
    end
    if candidate
      and active.stack_pointer == stack_pointer
      and active.routine.returns[address] then
      call_output:write(string.format(
        "%d,%d,%d,%02X,%04X,%04X,%04X,%02X,%04X,%d,%d,"
          .. "%02X,%02X,%02X,%02X,%02X,"
          .. "%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,"
          .. "%02X,%02X,%02X,%02X,%02X,"
          .. "%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%d\n",
        active.sequence,
        active.frame,
        active.entry_cycle,
        active.routine.bank,
        active.routine.pc,
        active.caller_pc,
        active.return_pc,
        pc_mpr,
        address,
        cycle,
        cycle - active.entry_cycle,
        active.input_a,
        active.input_x,
        active.input_y,
        active.stack_pointer,
        active.input_status,
        active.input_mprs[1],
        active.input_mprs[2],
        active.input_mprs[3],
        active.input_mprs[4],
        active.input_mprs[5],
        active.input_mprs[6],
        active.input_mprs[7],
        active.input_mprs[8],
        state_value(state, "cpu.a"),
        state_value(state, "cpu.x"),
        state_value(state, "cpu.y"),
        stack_pointer,
        state_value(state, "cpu.ps"),
        output_mprs[1],
        output_mprs[2],
        output_mprs[3],
        output_mprs[4],
        output_mprs[5],
        output_mprs[6],
        output_mprs[7],
        output_mprs[8],
        active.write_count))
      table.remove(active_calls, index)
    end
  end
end

for return_pc, candidates in pairs(return_callbacks) do
  local expected_pc = return_pc
  local expected_candidates = candidates
  emu.addMemoryCallback(
    function(address)
      if address ~= expected_pc then
        error("Mesen return callback entered the wrong address")
      end
      on_routine_return(address, expected_candidates)
    end,
    emu.callbackType.exec,
    return_pc,
    return_pc,
    emu.cpuType.pce,
    emu.memType.pceMemory)
end

local function on_work_ram_write(address, value)
  if #active_calls == 0 then
    return
  end
  local state = emu.getState()
  local pc = state_value(state, "cpu.pc")
  local pc_mpr = state_value(
    state,
    string.format("memoryManager.mpr[%d]", (pc >> 13) & 0x07))
  local cycle = state_value(state, "masterClock")
  for _, active in ipairs(active_calls) do
    active.write_count = active.write_count + 1
    write_output:write(string.format(
      "%d,%d,%d,%02X,%04X,%04X,%02X\n",
      active.sequence,
      active.write_count,
      cycle,
      pc_mpr,
      pc,
      address & 0x1FFF,
      value))
  end
end

emu.addMemoryCallback(
  on_work_ram_write,
  emu.callbackType.write,
  0,
  0x1FFF,
  emu.cpuType.pce,
  emu.memType.pceWorkRam)

local function is_pair(frame, first_frame)
  return frame == first_frame or frame == first_frame + 1
end

local function on_input_polled()
  local frame = emu.getState().frameCount + 1
  local one = is_pair(frame, 420)
    or is_pair(frame, 700)
    or (frame >= 850 and frame <= 880)
    or is_pair(frame, 1000)
    or is_pair(frame, 1100)
    or is_pair(frame, 1200)
    or is_pair(frame, 1300)
    or is_pair(frame, 1400)
    or is_pair(frame, 1500)
    or is_pair(frame, 1600)
    or is_pair(frame, 1700)
  local left = is_pair(frame, 1000)
    or is_pair(frame, 1100)
    or is_pair(frame, 1200)
    or is_pair(frame, 1300)
    or is_pair(frame, 1400)
    or is_pair(frame, 1500)
    or is_pair(frame, 1600)
    or is_pair(frame, 1700)
  local two = is_pair(frame, 1050)
    or is_pair(frame, 1250)
    or is_pair(frame, 1450)
    or is_pair(frame, 1650)
  emu.setInput({
    i = one,
    ii = two,
    run = is_pair(frame, 180) or is_pair(frame, 300),
    select = false,
    up = false,
    down = false,
    left = left,
    right = false
  }, 0)
end

local function close_outputs()
  trace_output:close()
  work_ram_output:close()
  call_output:close()
  write_output:close()
end

local function on_end_frame()
  frame_count = frame_count + 1
  local size = emu.getMemorySize(emu.memType.pceWorkRam)
  if size ~= 8192 then
    close_outputs()
    emu.stop(2)
    return
  end

  local bytes = {}
  for address = 0, size - 1 do
    bytes[address + 1] =
      string.char(emu.read(address, emu.memType.pceWorkRam))
  end
  work_ram_output:write(table.concat(bytes))

  if frame_count == 1800 then
    close_outputs()
    emu.stop(0)
  end
end

emu.addEventCallback(on_input_polled, emu.eventType.inputPolled)
emu.addEventCallback(on_end_frame, emu.eventType.endFrame)

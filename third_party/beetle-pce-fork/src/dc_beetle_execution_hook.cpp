// SPDX-License-Identifier: GPL-3.0-or-later

#include "dc_beetle_execution_hook.h"

dc_beetle_instruction_hook dc_beetle_active_instruction_hook = nullptr;
void* dc_beetle_active_instruction_hook_user_data = nullptr;

void dc_beetle_set_instruction_hook(
    const dc_beetle_instruction_hook hook,
    void* const user_data)
{
    dc_beetle_active_instruction_hook_user_data = user_data;
    dc_beetle_active_instruction_hook = hook;
}

void dc_beetle_clear_instruction_hook(void)
{
    dc_beetle_active_instruction_hook = nullptr;
    dc_beetle_active_instruction_hook_user_data = nullptr;
}

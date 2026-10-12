// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

int run_unsigned_square_root_semantic_fixture();

int run_unsigned_word_multiply_partial_semantic_fixture();

int run_signed_word_multiply_semantic_fixture();

int run_signed_word_normalizer_semantic_fixture();

int run_side_gate_deadline_check_semantic_fixture();

} // namespace devils_crush::app::host

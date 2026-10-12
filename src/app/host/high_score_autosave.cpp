// SPDX-License-Identifier: GPL-3.0-or-later
#include "high_score_autosave.hpp"

namespace devils_crush::app::host {

namespace {

// `commit_budget_milliseconds` is zero for the caller in the frame loop. A
// commit refused because something else has the file open is worth waiting out
// where the alternative is losing the write, but this caller does not face that
// alternative: a failed store stays pending, the poll below comes back twice a
// second, and the flush at shutdown gets the last word. Waiting there would buy
// nothing and be seen as a dropped frame.
void store_high_scores(
    HighScoreAutosave& autosave,
    const int commit_budget_milliseconds)
{
    const auto result = devils_crush::backup::save_atomic(
        autosave.path,
        devils_crush::core::encode_high_score_table(autosave.last),
        commit_budget_milliseconds);
    if (result.saved) {
        autosave.pending = false;
        autosave.settled_samples = 0;
        autosave.reported_failure = false;
        return;
    }
    // The write stays pending so the flush at shutdown tries once more, and
    // the complaint is made once rather than twice a second.
    if (!autosave.reported_failure) {
        std::cerr << "Could not save the high-score table: " << result.message
                  << '\n';
        autosave.reported_failure = true;
    }
}

// `01:C107` is the boot-time `TII $FAF4,$2200,$002A` that lays the ROM's
// default table over the block, and the work-RAM write census over the census
// route finds no other instruction writing the ranked columns. Hooking
// `01:C10E`, the instruction after it, therefore lands at the one moment when
// overwriting the whole table is safe: the defaults are in place, and nothing
// has read them yet.
void restore_high_scores(
    devils_crush::emulation::PceRuntime& runtime,
    HighScoreAutosave& autosave)
{
    autosave.table_seen = true;
    auto work_ram = runtime.export_work_ram();
    if (autosave.stored) {
        if (!devils_crush::core::write_high_score_table(
                work_ram, *autosave.stored)) {
            return;
        }
        if (!runtime.import_work_ram(work_ram)) {
            return;
        }
    }
    // Capture either the restored file or the cartridge defaults immediately,
    // so the startup DMD never has to wait for the first autosave poll.
    const auto current =
        devils_crush::core::read_high_score_table(work_ram);
    if (!current) {
        return;
    }
    // The poll compares against this, so a restore must not read back as a
    // change and rewrite the file it just came from.
    autosave.last = *current;
    autosave.pending = false;
    autosave.settled_samples = 0;
}

} // namespace

[[nodiscard]] HighScoreAutosave open_high_score_autosave(
    const std::filesystem::path& path)
{
    HighScoreAutosave autosave;
    autosave.path = path;
    const auto result = devils_crush::backup::load_exact(
        path, devils_crush::core::kHighScoreFileSize);
    if (result.status == devils_crush::backup::LoadStatus::not_found) {
        return autosave;
    }
    if (result.loaded()) {
        autosave.stored =
            devils_crush::core::decode_high_score_table(result.bytes);
        if (autosave.stored) {
            autosave.last = *autosave.stored;
        }
    }
    if (!autosave.stored) {
        // Loud, because the alternative is a player silently starting the
        // session on the ROM defaults and only noticing days later.
        std::cerr << "Ignoring an unreadable high-score table at "
                  << path.string() << "; this run starts from the "
                  << "cartridge defaults.\n";
    }
    return autosave;
}

[[nodiscard]] bool install_high_score_restore(
    devils_crush::emulation::PceRuntime& runtime,
    HighScoreAutosave& autosave)
{
    return runtime.install_execution_hook({
        .routine =
            RoutineKey{.physical_bank = 0x01U, .logical_pc = 0xC10EU},
        .mode = HookMode::observe,
        .callback =
            [&runtime, &autosave](const RoutineEntry&) {
                restore_high_scores(runtime, autosave);
            },
    });
}

// Sampling twice a second is far finer than the table can change -- it only
// moves on the sort at a game over and on the letters of a name being entered
// -- and settling for a second before writing keeps a name being scrolled from
// costing one file write per letter.
void poll_high_scores(
    devils_crush::emulation::PceRuntime& runtime,
    HighScoreAutosave& autosave,
    const int frame_number)
{
    constexpr int kSampleInterval = 30;
    constexpr int kSettledSamples = 2;

    if (!autosave.table_seen || frame_number % kSampleInterval != 0) {
        return;
    }
    const auto table =
        devils_crush::core::read_high_score_table(runtime.export_work_ram());
    if (!table) {
        return;
    }
    if (*table != autosave.last) {
        autosave.last = *table;
        autosave.pending = true;
        autosave.settled_samples = 0;
        return;
    }
    if (!autosave.pending || ++autosave.settled_samples < kSettledSamples) {
        return;
    }
    store_high_scores(autosave, 0);
}

void flush_high_scores(HighScoreAutosave& autosave)
{
    if (!autosave.table_seen || !autosave.pending) {
        return;
    }
    store_high_scores(
        autosave,
        devils_crush::core::kDefaultCommitBudgetMilliseconds);
}

} // namespace devils_crush::app::host

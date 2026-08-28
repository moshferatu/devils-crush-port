// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/tower_insect_scheduler.hpp"

namespace devils_crush::native_runtime {

TowerInsectRuleScheduler::TowerInsectRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

std::optional<std::size_t> TowerInsectRuleScheduler::index_for(
    const std::uint8_t slot) noexcept
{
    for (std::size_t index = 0U; index < kTowerInsectObservedSlots.size(); ++index) {
        if (kTowerInsectObservedSlots[index] == slot) {
            return index;
        }
    }
    return std::nullopt;
}

bool TowerInsectRuleScheduler::seed(
    const TowerInsectGenerationState generation) noexcept
{
    if (!generation.source.generation_active
        || generation.source.live_children > kTowerInsectObservedSlots.size()
        || generation.source.group_clear_pending
        || generation.source.group_clear_awarded
        || generation.source.child_spawn_pending) {
        return false;
    }

    std::uint8_t captured_children = 0U;
    for (const auto& insect : generation.insects) {
        if (!insect) {
            continue;
        }
        if (insect->hits_remaining == 0U || insect->hits_remaining > 2U) {
            return false;
        }
        ++captured_children;
    }
    if (captured_children != generation.source.live_children) {
        return false;
    }
    if (generation.source.spawns_remaining > kTowerInsectObservedSlots.size()
        || (generation.source.spawns_remaining != 0U
            && (generation.source.ticks_until_next_spawn == 0U
                || generation.source.ticks_until_next_spawn > 48U
                || static_cast<std::size_t>(generation.source.spawns_remaining)
                    + generation.source.live_children
                    > kTowerInsectObservedSlots.size()))) {
        return false;
    }

    source_ = generation.source;
    insects_ = generation.insects;
    contact_queued_.fill(false);
    return true;
}

std::optional<TowerInsectSourceStep> TowerInsectRuleScheduler::begin_generation(
    const bool tower_insect_cycle_requested) noexcept
{
    const auto step = begin_tower_insect_generation(
        source_, tower_insect_cycle_requested);
    if (step.disposition == TowerInsectSourceDisposition::generation_started) {
        source_ = step.state;
        insects_.fill(std::nullopt);
        contact_queued_.fill(false);
        return step;
    }
    return step.disposition == TowerInsectSourceDisposition::waiting_for_next_cycle
        ? std::optional<TowerInsectSourceStep>(step)
        : std::nullopt;
}

std::optional<TowerInsectSpawnStep> TowerInsectRuleScheduler::spawn_child(
    const std::uint8_t slot) noexcept
{
    const auto index = index_for(slot);
    if (!index || insects_[*index]) {
        return std::nullopt;
    }
    const auto step = spawn_tower_insect_child(source_);
    if (step.disposition != TowerInsectSpawnDisposition::child_spawned) {
        return std::nullopt;
    }
    source_ = step.state;
    insects_[*index] = TowerInsectState{
        .appearance = source_.child_appearance,
        .collision_effect_association = source_.child_collision_effect_association,
    };
    return step;
}

bool TowerInsectRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    const auto index = index_for(contact.target.actor_slot);
    if (!index || contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response || !insects_[*index]
        || contact_queued_[*index]) {
        return false;
    }
    contact_queued_[*index] = true;
    return true;
}

std::optional<TowerInsectRulesUpdate> TowerInsectRuleScheduler::advance_actor(
    const std::uint8_t slot) noexcept
{
    const auto index = index_for(slot);
    if (!index || !contact_queued_[*index] || !insects_[*index]) {
        return std::nullopt;
    }
    contact_queued_[*index] = false;
    const auto update = resolve_tower_insect_contact(
        *insects_[*index], source_, score_);
    source_ = update.insect.source;
    score_ = update.completion_score_award.state;
    if (update.insect.releases_child) {
        insects_[*index].reset();
    } else {
        insects_[*index] = update.insect.state;
    }
    return update;
}

std::optional<TowerInsectSourceStep> TowerInsectRuleScheduler::advance_source(
    const std::uint8_t slot) noexcept
{
    if (slot != kTowerInsectSourceSlot) {
        return std::nullopt;
    }
    const auto step = advance_tower_insect_source(source_);
    source_ = step.state;
    if (step.disposition == TowerInsectSourceDisposition::waiting) {
        return std::nullopt;
    }
    if (step.score_award_units != 0U) {
        score_ = apply_score_award(score_, step.score_award_units).state;
    }
    return step;
}

std::optional<TowerInsectSourceStep> TowerInsectRuleScheduler::restart_generation(
    const bool tower_insect_cycle_requested) noexcept
{
    if (!source_.group_clear_awarded) {
        return std::nullopt;
    }
    const auto step = restart_tower_insect_generation(
        source_, tower_insect_cycle_requested);
    if (step.disposition == TowerInsectSourceDisposition::generation_restarted) {
        source_ = step.state;
        insects_.fill(std::nullopt);
        contact_queued_.fill(false);
    }
    return step;
}

const TowerInsectSourceState& TowerInsectRuleScheduler::source() const noexcept
{
    return source_;
}

const TowerInsectState* TowerInsectRuleScheduler::insect(
    const std::uint8_t slot) const noexcept
{
    const auto index = index_for(slot);
    return index && insects_[*index] ? &*insects_[*index] : nullptr;
}

} // namespace devils_crush::native_runtime

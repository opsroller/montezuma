// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "replay.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "montezuma/bonuses.hpp"
#include "montezuma/rng.hpp"
#include "montezuma/totems.hpp"

namespace montezuma::godot_binding::bench {

namespace {

// Separate from the board's own generator, so planning choices never shift
// the board's draws.
constexpr std::uint64_t kPlannerSalt = 0x6d6f6e74657a756dull;  // "montezum"

std::size_t cleared_count(const MoveResult& r) {
    std::size_t n = 0;
    for (const CascadeStep& s : r.cascades) n += s.cleared.size();
    return n;
}

// The legal swap that resolves the most cascade steps, then clears the most
// pieces; the first in row-major order on a tie. False when none is legal
// (the board shuffles itself at the end of a move, so this is not expected).
bool deepest_swap(const Board& board, Cell& a, Cell& b) {
    int best_steps = -1;
    std::size_t best_cleared = 0;
    for (int y = 0; y < board.height(); ++y) {
        for (int x = 0; x < board.width(); ++x) {
            for (const Cell n : {Cell{x + 1, y}, Cell{x, y + 1}}) {
                if (!board.in_bounds(n.x, n.y)) continue;
                Board trial = board;
                const auto r = trial.try_swap({x, y}, n);
                if (!r) continue;
                const int steps = static_cast<int>(r->cascades.size());
                const std::size_t cleared = cleared_count(*r);
                if (steps > best_steps || (steps == best_steps && cleared > best_cleared)) {
                    best_steps = steps;
                    best_cleared = cleared;
                    a = {x, y};
                    b = n;
                }
            }
        }
    }
    return best_steps >= 0;
}

struct Planner {
    Board board;
    Rng rng;
    TotemSystem totems;
    Replay out;

    void record(Action action, const std::optional<MoveResult>& r) {
        action.steps = r ? static_cast<int>(r->cascades.size()) : 0;
        action.board_hash = hash_cells(board.cells());
        if (action.steps >= 4) ++out.deep_cascades;
        if (action.steps > out.max_steps) out.max_steps = action.steps;
        out.actions.push_back(std::move(action));
    }

    void swap() {
        Action act;
        act.kind = Action::Kind::Swap;
        act.label = "swap";
        if (!deepest_swap(board, act.a, act.b)) return;
        const auto r = board.try_swap(act.a, act.b);
        record(std::move(act), r);
    }

    void clear(std::string label, std::vector<Cell> cells) {
        Action act;
        act.kind = Action::Kind::Clear;
        act.label = std::move(label);
        act.cells = std::move(cells);
        const auto r = board.clear_cells(act.cells);
        if (!r) return;
        record(std::move(act), r);
    }

    void power_up(PowerUpId id) {
        // Inside the outer ring, so every ray and blast reaches several cells.
        const Cell at{1 + rng.below(board.width() - 2), 1 + rng.below(board.height() - 2)};
        const PowerUpEffect e = power_up_effect(id, kMaxPowerUpTier, board, at);
        clear("powerup:" + std::string(power_up_name(id)), e.cells_to_clear);
        ++out.power_ups;
    }

    void totem(TotemId id) {
        ++out.totems;
        const TotemEffect e = totems.make_effect(id, board, rng);
        const std::string label = "totem:" + std::string(totem_name(id));
        // Only fire clears cells the Board models; paint's repaint, gems,
        // steel, time and points belong to the game layer, so the replay
        // marks the beat for their effect scenes and leaves the board alone.
        if (!e.cells_to_clear.empty()) {
            clear(label, prepare_board_effect(e, board));
            return;
        }
        Action act;
        act.kind = Action::Kind::Marker;
        act.label = label;
        record(std::move(act), std::nullopt);
    }
};

std::vector<TotemUnlock> every_totem_at_top_tier() {
    std::vector<TotemUnlock> out;
    for (const TotemDef& def : kTotemDefs) out.push_back({def.id, def.tier_count});
    return out;
}

}  // namespace

std::uint64_t hash_cells(const std::vector<int>& cells) {
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (int v : cells) {
        h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(v));
        h *= 0x100000001b3ull;
    }
    return h;
}

Replay plan_replay(const ReplayConfig& config) {
    auto board = Board::try_create(config.width, config.height, config.kinds, config.seed);
    if (!board) throw std::invalid_argument("benchmark replay: invalid board size, kinds or seed");
    Planner p{std::move(*board), Rng(config.seed ^ kPlannerSalt), TotemSystem(every_totem_at_top_tier()), {}};
    for (int round = 0; round < config.rounds; ++round) {
        for (int i = 0; i < 3; ++i) p.swap();
        p.power_up(PowerUpId::Tnt);
        for (int i = 0; i < 3; ++i) p.swap();
        p.power_up(PowerUpId::Lightning);
        for (int i = 0; i < 2; ++i) p.swap();
        for (const TotemDef& def : kTotemDefs) p.totem(def.id);
    }
    return std::move(p.out);
}

}  // namespace montezuma::godot_binding::bench

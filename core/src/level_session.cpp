// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/level_session.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <string>

namespace montezuma {

namespace {

// Stream keys for derive_seed.
constexpr std::uint64_t kBoardStream = 1;
constexpr std::uint64_t kLevelStream = 2;

// rule: SHUF-05 the levels with a free idle hint.
constexpr std::array<std::string_view, 2> kFreeHintLevels{"e1-l01", "e1-l02"};

bool row_major_less(Cell a, Cell b) { return a.y != b.y ? a.y < b.y : a.x < b.x; }

const LevelData& validated(const LevelData& level, Difficulty difficulty, const Upgrades& up) {
    if (difficulty != Difficulty::Normal && difficulty != Difficulty::Hard)
        throw std::invalid_argument("difficulty must be normal or hard");
    if (!level.blocked_cells.empty()) throw std::invalid_argument("blocked cells are not supported yet");
    if (!level.gem_goal && level.quotas.empty()) throw std::invalid_argument("level has no goal");
    // rule: OBS-05, BOARD-04 chained cells must be on the board.
    for (Cell c : level.chained_cells)
        if (c.x < 0 || c.y < 0 || c.x >= level.board.width || c.y >= level.board.height)
            throw std::invalid_argument("chained cell (" + std::to_string(c.x) + ", " + std::to_string(c.y) +
                                        ") is off the board");
    // rule: GOAL-03 throws for a timer tier out of range.
    (void)level.time_limit_for(difficulty, up.timer_tier);
    if (up.gems_tier < 0 || up.gems_tier > kMaxGemsTier)
        throw std::invalid_argument("gems tier must be 0.." + std::to_string(kMaxGemsTier));
    if (up.frenzy_tier < 0 || up.frenzy_tier > kMaxFrenzyTier)
        throw std::invalid_argument("frenzy tier must be 0.." + std::to_string(kMaxFrenzyTier));
    for (std::size_t i = 0; i < kTotemCount; ++i) {
        const int tier = up.totem_tiers[i];
        if (tier < 0 || tier > kTotemDefs[i].tier_count)
            throw std::invalid_argument("totem " + std::string(totem_name(static_cast<TotemId>(i))) + " has no tier " +
                                        std::to_string(tier));
    }
    for (std::size_t i = 0; i < kPowerUpCount; ++i) {
        const int tier = up.power_up_tiers[i];
        if (tier < 0 || tier > kMaxPowerUpTier)
            throw std::invalid_argument("power-up " + std::string(power_up_name(static_cast<PowerUpId>(i))) +
                                        " has no tier " + std::to_string(tier));
    }
    return level;
}

}  // namespace

namespace {

std::uint64_t splitmix(std::uint64_t z) noexcept {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

}  // namespace

std::uint64_t derive_seed(std::uint64_t seed, std::uint64_t stream) noexcept {
    // Mix the seed and the stream key apart before combining them, so no
    // linear relation between two seeds maps one's stream onto another's.
    return splitmix(splitmix(seed) ^ splitmix(~stream));
}

std::int64_t to_micros(double seconds) {
    // Clamped to about 31,700 years either way so the rounding cannot overflow.
    constexpr double kLimit = 1e15;
    return std::llround(std::clamp(seconds * 1e6, -kLimit, kLimit));
}

// Per-move bookkeeping shared by the board hooks.
struct LevelSession::MoveContext {
    MoveReport report;
    std::optional<Swap> pending_swap;    // marks to swap in step 0
    std::vector<StepReport>* steps = nullptr;
    int step_base = 0;                   // global index of the resolution's step 0
    int base_points = 0;
    int multiplier = 1;
    bool frenzy_fill = false;            // the bar filled this step
    std::vector<Cell> held;              // this step's unchained pieces (OBS-05)
};

LevelSession::LevelSession(const LevelData& level, Difficulty difficulty, const Upgrades& up, std::uint64_t seed)
    : level_(validated(level, difficulty, up)),
      difficulty_(difficulty),
      board_(level.board.width, level.board.height, level.board.kinds, derive_seed(seed, kBoardStream)),
      rng_(derive_seed(seed, kLevelStream)),
      power_ups_(board_) {
    const std::size_t n = board_.cells().size();
    marks_.steel.assign(n, false);
    marks_.gem.assign(n, false);
    chains_.assign(n, false);

    // rule: PROG-03, progression Unlocks: listed ids only, tier 1 at least.
    for (const std::string& name : level_.powerups_unlocked)
        if (auto id = power_up_from_name(name))
            power_up_tiers_[power_up_index(*id)] = std::max(kMinPowerUpTier, up.power_up_tiers[power_up_index(*id)]);
    for (const std::string& name : level_.totems_unlocked) {
        auto id = totem_from_name(name);
        if (!id) throw std::invalid_argument("unknown totem id: " + name);
        totems_.unlock(*id, std::max(1, up.totem_tiers[static_cast<std::size_t>(*id)]));
    }
    // rule: TOT-06
    totems_.start_level();

    gems_max_ = kBaseGemsOnBoard + up.gems_tier;
    if (level_.gem_goal) gem_goal_ = difficulty == Difficulty::Hard ? level_.gem_goal->hard : level_.gem_goal->normal;
    time_left_us_ = to_micros(level_.time_limit_for(difficulty, up.timer_tier));
    frenzy_tier_ = up.frenzy_tier;
    free_hints_ = std::find(kFreeHintLevels.begin(), kFreeHintLevels.end(), level_.id) != kFreeHintLevels.end();

    // rule: OBS-05, BOARD-04 chains from the level data.
    for (Cell c : level_.chained_cells) chains_[idx(c)] = true;
    // rule: BOARD-03 chains may leave no usable move: reshuffle the movable
    // pieces, with no time penalty.
    if (legal_moves().empty()) {
        if (do_shuffle().stuck) throw std::runtime_error("no playable board for this level");
    }
    // rule: GEM-02
    place_initial_gems();
}

// ---- Queries ---------------------------------------------------------------

int LevelSession::gems_on_board() const {
    return static_cast<int>(std::count(marks_.gem.begin(), marks_.gem.end(), true));
}

int LevelSession::steel_on_board() const {
    return static_cast<int>(std::count(marks_.steel.begin(), marks_.steel.end(), true));
}

int LevelSession::kind_for_run(const std::vector<int>& kinds, Cell c) const {
    // rule: OBS-01, RES-01 steel ends a run.
    if (steel(c)) return Board::kEmpty;
    return kinds[idx(c)];
}

bool LevelSession::in_run_after_swap(const std::vector<int>& kinds, Cell c) const {
    const int k = kind_for_run(kinds, c);
    if (k == Board::kEmpty) return false;
    const auto same = [&](int x, int y) { return board_.in_bounds(x, y) && kind_for_run(kinds, {x, y}) == k; };
    int len = 1;
    for (int x = c.x - 1; same(x, c.y); --x) ++len;
    for (int x = c.x + 1; same(x, c.y); ++x) ++len;
    if (len >= 3) return true;
    len = 1;
    for (int y = c.y - 1; same(c.x, y); --y) ++len;
    for (int y = c.y + 1; same(c.x, y); ++y) ++len;
    return len >= 3;
}

bool LevelSession::is_legal(Cell a, Cell b) const {
    if (!board_.in_bounds(a.x, a.y) || !board_.in_bounds(b.x, b.y)) return false;
    if (std::abs(a.x - b.x) + std::abs(a.y - b.y) != 1) return false;
    // rule: MOVE-03
    if (!movable(a) || !movable(b)) return false;
    std::vector<int> kinds = board_.cells();
    std::swap(kinds[idx(a)], kinds[idx(b)]);
    // rule: MOVE-02 on a still board a new run passes through a or b.
    return in_run_after_swap(kinds, a) || in_run_after_swap(kinds, b);
}

std::vector<Swap> LevelSession::legal_moves() const {
    std::vector<Swap> out;
    std::vector<int> kinds = board_.cells();
    const auto test = [&](Cell a, Cell b) {
        if (!movable(a) || !movable(b)) return;
        std::swap(kinds[idx(a)], kinds[idx(b)]);
        if (in_run_after_swap(kinds, a) || in_run_after_swap(kinds, b)) out.push_back({a, b});
        std::swap(kinds[idx(a)], kinds[idx(b)]);
    };
    for (int y = 0; y < board_.height(); ++y)
        for (int x = 0; x < board_.width(); ++x) {
            if (x + 1 < board_.width()) test({x, y}, {x + 1, y});
            if (y + 1 < board_.height()) test({x, y}, {x, y + 1});
        }
    return out;
}

std::vector<Run> LevelSession::runs() const {
    StepHooks hooks;
    hooks.matchable = [this](const Board&, Cell c) { return !steel(c); };
    return board_.find_runs(&hooks);
}

std::vector<Run> LevelSession::runs_for_swap(Cell a, Cell b) const {
    if (!is_legal(a, b)) return {};
    std::vector<int> kinds = board_.cells();
    std::swap(kinds[idx(a)], kinds[idx(b)]);
    std::vector<Run> horizontal;
    std::vector<Run> vertical;
    for (Cell c : {a, b}) {
        const int k = kind_for_run(kinds, c);
        if (k == Board::kEmpty) continue;
        const auto same = [&](int x, int y) { return board_.in_bounds(x, y) && kind_for_run(kinds, {x, y}) == k; };
        int x0 = c.x;
        int x1 = c.x;
        while (same(x0 - 1, c.y)) --x0;
        while (same(x1 + 1, c.y)) ++x1;
        if (x1 - x0 + 1 >= 3) {
            Run run{{}, k};
            for (int x = x0; x <= x1; ++x) run.cells.push_back({x, c.y});
            if (std::find(horizontal.begin(), horizontal.end(), run) == horizontal.end()) horizontal.push_back(run);
        }
        int y0 = c.y;
        int y1 = c.y;
        while (same(c.x, y0 - 1)) --y0;
        while (same(c.x, y1 + 1)) ++y1;
        if (y1 - y0 + 1 >= 3) {
            Run run{{}, k};
            for (int y = y0; y <= y1; ++y) run.cells.push_back({c.x, y});
            if (std::find(vertical.begin(), vertical.end(), run) == vertical.end()) vertical.push_back(run);
        }
    }
    // CascadeStep::runs order: horizontal by row, then vertical by column.
    std::sort(horizontal.begin(), horizontal.end(),
              [](const Run& l, const Run& r) { return row_major_less(l.cells[0], r.cells[0]); });
    std::sort(vertical.begin(), vertical.end(), [](const Run& l, const Run& r) {
        return l.cells[0].x != r.cells[0].x ? l.cells[0].x < r.cells[0].x : l.cells[0].y < r.cells[0].y;
    });
    horizontal.insert(horizontal.end(), vertical.begin(), vertical.end());
    return horizontal;
}

std::vector<Cell> LevelSession::plain_cells() const {
    // rule: GEM-01 not steel, no power-up, no gem. Chained pieces count.
    std::vector<Cell> plain;
    for (int y = 0; y < board_.height(); ++y)
        for (int x = 0; x < board_.width(); ++x) {
            const Cell c{x, y};
            if (board_.at(x, y) != Board::kEmpty && !steel(c) && !gem(c) && !power_ups_.has(c)) plain.push_back(c);
        }
    return plain;
}

void LevelSession::place_initial_gems() {
    std::vector<Cell> plain = plain_cells();
    for (int i = 0; i < gems_max_ && !plain.empty(); ++i) {
        const auto pick = static_cast<std::size_t>(rng_.below(static_cast<int>(plain.size())));
        marks_.gem[idx(plain[pick])] = true;
        plain.erase(plain.begin() + static_cast<std::ptrdiff_t>(pick));
    }
}

bool LevelSession::goal_met() const {
    // rule: GOAL-01, GOAL-06 every goal the level has.
    if (gem_goal_ && gems_collected_ < *gem_goal_) return false;
    for (const auto& [kind, need] : level_.quotas) {
        auto it = cleared_by_kind_.find(kind);
        if ((it == cleared_by_kind_.end() ? 0 : it->second) < need) return false;
    }
    return true;
}

// ---- Resolution ------------------------------------------------------------

StepHooks LevelSession::make_hooks(MoveContext& ctx) {
    StepHooks hooks;
    hooks.matchable = [this](const Board&, Cell c) { return !steel(c); };
    hooks.movable = [this](const Board&, Cell c) { return movable(c); };
    // RES-08: the session reshuffles after the goal check.
    hooks.settle_shuffle = false;
    hooks.max_steps = Board::kMaxCascadeSteps;
    hooks.extend_clear = [this, &ctx](const Board& b, const std::vector<Cell>& cleared, int step) {
        if (step == 0 && ctx.pending_swap) {
            const Cell a = (*ctx.pending_swap)[0];
            const Cell c = (*ctx.pending_swap)[1];
            // Swapping a gem piece moves the gem with it (GEM-05, PWR-03).
            const bool ga = marks_.gem[idx(a)];
            marks_.gem[idx(a)] = marks_.gem[idx(c)];
            marks_.gem[idx(c)] = ga;
            power_ups_.apply_swap(a, c);
            ctx.pending_swap.reset();
        }
        const int global = ctx.step_base + step;
        StepClear sc = power_ups_.expand_clear(b, cleared, global, marks_, chains_);
        ctx.held = sc.held;
        on_step_clear(ctx, sc.cells, sc, global);
        return sc.power_up_cells;
    };
    // rule: OBS-05 pieces whose chain broke stay where they are.
    hooks.keep = [&ctx](const Board&, int) { return ctx.held; };
    hooks.after_refill = [this, &ctx](const Board&, const CascadeStep& step, int index) {
        on_step_refill(ctx, step, index);
    };
    return hooks;
}

void LevelSession::collect_gem_points(MoveContext& ctx, int gems, int& points) {
    gems_collected_ += gems;
    ctx.report.gems_collected += gems;
    points += kPointsPerGem * gems;  // rule: SCORE-02
    // rule: FRZ-04
    if (frenzy_on_) {
        star_sand_ += gems;
        ctx.report.star_sand_gained += gems;
    }
}

void LevelSession::on_step_clear(MoveContext& ctx, const std::vector<Cell>& cells, const StepClear& sc, int step) {
    StepReport sr;
    sr.step = step;
    sr.cleared = cells;
    sr.power_ups_triggered = sc.triggered;
    sr.power_up_effects = sc.effects;
    // rule: FRZ-02 a step that fills the bar is not doubled itself.
    sr.frenzy = frenzy_on_;
    int points = 0;
    int gems = 0;
    for (Cell c : cells) {
        const int kind = board_.at(c.x, c.y);
        ++sr.cleared_by_kind[kind];
        ++cleared_by_kind_[kind];  // rule: GOAL-06 every cleared piece
        // rule: RES-03 gem, then points, then the Frenzy bar.
        if (gem(c)) {
            sr.gems_collected.push_back(c);
            ++gems;
        }
        points += kPointsPerPiece * (step + 1);  // rule: SCORE-01, OBS-03
        if (steel(c)) sr.steel_removed.push_back(c);
    }
    // rule: OBS-05 the chain goes; the piece stays, unscored, and is plain now.
    sr.chains_broken = sc.held;
    for (Cell c : sc.held) chains_[idx(c)] = false;
    collect_gem_points(ctx, gems, points);
    if (frenzy_on_) points *= kFrenzyPointsFactor;  // rule: SCORE-03
    sr.points = points;
    ctx.base_points += points;
    // rule: FRZ-01, FRZ-02, FRZ-03 the bar fills only when unlocked and off,
    // clamped at kFrenzyBarMax.
    if (frenzy_tier_ >= 1 && !frenzy_on_) {
        frenzy_bar_ = std::min(kFrenzyBarMax, frenzy_bar_ + static_cast<int>(cells.size()));
        if (frenzy_bar_ >= kFrenzyBarMax) ctx.frenzy_fill = true;
    }
    sr.frenzy_bar_after = frenzy_bar_;
    // rule: PWR-09, PWR-10, PWR-11, GOAL-05
    for (const PowerUpEffect& e : sc.effects) {
        time_left_us_ += to_micros(e.time_delta_seconds);
        ctx.report.time_delta_seconds += e.time_delta_seconds;
        ctx.multiplier = std::max(ctx.multiplier, e.score_multiplier);
        if (e.hint_seconds > 0.0) ctx.report.hint_seconds = e.hint_seconds;
    }
    ctx.steps->push_back(std::move(sr));
}

void LevelSession::on_step_refill(MoveContext& ctx, const CascadeStep& step, int /*index*/) {
    StepReport& sr = ctx.steps->back();
    sr.runs = step.runs;
    sr.falls = step.falls;
    sr.spawns = step.spawns;
    // Marks follow their pieces: cleared ones go, falling ones move.
    for (Cell c : step.cleared) {
        marks_.gem[idx(c)] = false;
        marks_.steel[idx(c)] = false;
        chains_[idx(c)] = false;
    }
    struct Moving {
        Cell to;
        bool gem, steel, chain;
    };
    std::vector<Moving> moving;
    for (const Fall& f : step.falls) {
        const std::size_t i = idx(f.from);
        moving.push_back({f.to, marks_.gem[i], marks_.steel[i], chains_[i]});
        marks_.gem[i] = marks_.steel[i] = chains_[i] = false;
    }
    for (const Moving& m : moving) {
        marks_.gem[idx(m.to)] = m.gem;
        marks_.steel[idx(m.to)] = m.steel;
        chains_[idx(m.to)] = m.chain;
    }
    power_ups_.apply_step(step);

    // rule: OBS-02, TOT-14 steel, one draw per spawn, while below the cap.
    if (level_.steel_rate > 0.0 && !steel_stopped_) {
        const auto threshold = static_cast<int>(std::llround(level_.steel_rate * kSteelRollScale));
        int count = steel_on_board();
        for (const Spawn& sp : step.spawns) {
            if (count >= kMaxSteelOnBoard) break;
            if (rng_.below(kSteelRollScale) < threshold) {
                marks_.steel[idx(sp.to)] = true;
                sr.steel_spawned.push_back(sp.to);
                ++count;
            }
        }
    }
    // rule: GEM-04 gems on this refill's new plain pieces, one at a time.
    int shortfall = gems_max_ - gems_on_board();
    if (shortfall > 0) {
        std::vector<Cell> fresh;
        for (const Spawn& sp : step.spawns)
            if (!steel(sp.to) && !gem(sp.to) && !power_ups_.has(sp.to)) fresh.push_back(sp.to);
        while (shortfall > 0 && !fresh.empty()) {
            const auto pick = static_cast<std::size_t>(rng_.below(static_cast<int>(fresh.size())));
            marks_.gem[idx(fresh[pick])] = true;
            sr.gems_placed.push_back(fresh[pick]);
            fresh.erase(fresh.begin() + static_cast<std::ptrdiff_t>(pick));
            --shortfall;
        }
    }
    // rule: RES-05, PWR-01, PWR-02, OBS-05 a run whose pieces all held
    // cleared nothing and earns nothing.
    std::vector<Run> earning;
    for (const Run& run : step.runs)
        if (std::any_of(run.cells.begin(), run.cells.end(),
                        [&](Cell c) { return std::find(step.cleared.begin(), step.cleared.end(), c) != step.cleared.end(); }))
            earning.push_back(run);
    sr.power_ups_placed = power_ups_.place_for_step(earning, board_, rng_, power_up_tiers_, marks_);
    // rule: FRZ-02 Frenzy starts at the end of the step that filled the bar.
    if (ctx.frenzy_fill) {
        ctx.frenzy_fill = false;
        frenzy_on_ = true;
        frenzy_left_us_ = to_micros(kFrenzySeconds[static_cast<std::size_t>(frenzy_tier_)]);
        sr.frenzy_started = true;
        ctx.report.frenzy_started = true;
    }
}

std::optional<MoveResult> LevelSession::clear_effect(MoveContext& ctx, const std::vector<Cell>& cells,
                                                     std::vector<StepReport>& steps) {
    if (cells.empty()) return std::nullopt;
    ctx.steps = &steps;
    StepHooks hooks = make_hooks(ctx);
    std::optional<MoveResult> r = board_.clear_cells(cells, &hooks);
    if (r) {
        // AMBIGUOUS: a totem's steps continue the move's numbering.
        ctx.step_base += static_cast<int>(r->cascades.size());
        if (r->stuck) ctx.report.stuck = true;
    }
    return r;
}

void LevelSession::start_frenzy(MoveContext& ctx, bool& started, bool& restarted) {
    // rule: FRZ-03, TOT-12 start, or restart the duration during one.
    if (frenzy_tier_ < 1) return;
    frenzy_left_us_ = to_micros(kFrenzySeconds[static_cast<std::size_t>(frenzy_tier_)]);
    if (frenzy_on_) {
        restarted = true;
        ctx.report.frenzy_restarted = true;
    } else {
        frenzy_on_ = true;
        started = true;
        ctx.report.frenzy_started = true;
    }
}

void LevelSession::fire_totem(MoveContext& ctx, TotemId id, TotemFiring& firing) {
    // rule: TOT-05, TOT-17 chosen on the board as the previous totem left it.
    firing.effect = totems_.make_effect(id, board_, rng_, marks_.steel);
    const TotemEffect& e = firing.effect;
    switch (id) {
    case TotemId::Fire:
        // rule: TOT-10, OBS-03 steel included.
        firing.result = clear_effect(ctx, e.cells_to_clear, firing.steps);
        break;
    case TotemId::Time:
        // rule: TOT-11, GOAL-05
        time_left_us_ += to_micros(e.time_delta_seconds);
        ctx.report.time_delta_seconds += e.time_delta_seconds;
        break;
    case TotemId::Gold: {
        // AMBIGUOUS: doubled when Frenzy was on before gold fired.
        int pts = e.points * (frenzy_on_ ? kFrenzyPointsFactor : 1);
        firing.points = pts;
        ctx.base_points += pts;
        if (e.start_frenzy) start_frenzy(ctx, firing.frenzy_started, firing.frenzy_restarted);
        break;
    }
    case TotemId::Harvest: {
        // rule: TOT-13 collect every gem (pieces stay), place new gems
        // ignoring gems_max, then remove every chain.
        int gems = 0;
        for (int y = 0; y < board_.height(); ++y)
            for (int x = 0; x < board_.width(); ++x)
                if (gem({x, y})) {
                    firing.gems_harvested.push_back({x, y});
                    marks_.gem[idx({x, y})] = false;
                    ++gems;
                }
        int pts = 0;
        collect_gem_points(ctx, gems, pts);
        if (frenzy_on_) pts *= kFrenzyPointsFactor;
        firing.points = pts;
        ctx.base_points += pts;
        std::vector<Cell> plain = plain_cells();
        for (int i = 0; i < e.gems_to_place && !plain.empty(); ++i) {
            const auto pick = static_cast<std::size_t>(rng_.below(static_cast<int>(plain.size())));
            marks_.gem[idx(plain[pick])] = true;
            firing.gems_placed.push_back(plain[pick]);
            plain.erase(plain.begin() + static_cast<std::ptrdiff_t>(pick));
        }
        if (e.remove_chains)
            for (int y = 0; y < board_.height(); ++y)
                for (int x = 0; x < board_.width(); ++x)
                    if (chained({x, y})) {
                        chains_[idx({x, y})] = false;
                        firing.chains_removed.push_back({x, y});
                    }
        break;
    }
    case TotemId::Ward: {
        // rule: TOT-14, OBS-03 destroy every steel piece (a clear), tier 3
        // also the pieces next to them; tier 2+ stops new steel.
        std::set<std::pair<int, int>> cells;
        for (int y = 0; y < board_.height(); ++y)
            for (int x = 0; x < board_.width(); ++x)
                if (steel({x, y})) {
                    cells.insert({y, x});
                    if (!e.clear_around_steel) continue;
                    for (Cell n : {Cell{x - 1, y}, Cell{x + 1, y}, Cell{x, y - 1}, Cell{x, y + 1}})
                        if (board_.in_bounds(n.x, n.y)) cells.insert({n.y, n.x});
                }
        if (e.stop_steel) steel_stopped_ = true;
        std::vector<Cell> list;
        for (const auto& [y, x] : cells) list.push_back({x, y});
        firing.result = clear_effect(ctx, list, firing.steps);
        break;
    }
    case TotemId::Gift:
        // rule: TOT-15
        firing.power_ups_placed = power_ups_.place_random(e.powerups_to_place, board_, rng_, power_up_tiers_, marks_,
                                                          e.powerup_board_cap);
        break;
    case TotemId::Paint: {
        // rule: TOT-16 repaint (gems, power-ups and chains stay), then the
        // runs it made resolve as steps.
        for (Cell c : e.cells_to_repaint) board_.set(c.x, c.y, e.repaint_kind);
        MoveContext probe;
        StepHooks hooks = make_hooks(probe);
        std::vector<Cell> cells;
        for (const Run& run : board_.find_runs(&hooks)) cells.insert(cells.end(), run.cells.begin(), run.cells.end());
        std::sort(cells.begin(), cells.end(), row_major_less);
        cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
        firing.result = clear_effect(ctx, cells, firing.steps);
        break;
    }
    }
}

std::optional<MoveReport> LevelSession::swap(Cell a, Cell b) {
    if (outcome_ != Outcome::Playing || !is_legal(a, b)) return std::nullopt;
    MoveContext ctx;
    ctx.report.swap = {a, b};
    ctx.pending_swap = Swap{a, b};
    ctx.steps = &ctx.report.steps;
    StepHooks hooks = make_hooks(ctx);
    std::optional<MoveResult> r = board_.try_swap(a, b, &hooks);
    if (!r) return std::nullopt;
    MoveReport& rep = ctx.report;
    rep.result = *r;
    rep.stuck = r->stuck;
    ctx.step_base = static_cast<int>(r->cascades.size());
    // rule: MOVE-01, SHUF-04 a move; it clears a bought hint.
    ++moves_;
    hint_.reset();
    idle_us_ = 0;
    free_hint_shown_ = false;

    // rule: TOT-02..04, RES-07 totems after the last cascade, kind order.
    const std::vector<TotemId> fired = totems_.record_move(*r);
    if (!rep.stuck)
        for (TotemId id : fired) {
            rep.totems_fired.emplace_back();
            fire_totem(ctx, id, rep.totems_fired.back());
            if (rep.stuck) break;
        }
    for (std::size_t i = 0; i < kTotemCount; ++i) {
        const auto id = static_cast<TotemId>(i);
        if (!totems_.unlocked(id)) continue;
        const bool did_fire = std::find(fired.begin(), fired.end(), id) != fired.end();
        rep.totem_charges.push_back({id, did_fire ? 1.0 : totems_.charge(id)});
    }

    // rule: SCORE-04, PWR-10 the highest multiplier scales the whole move.
    rep.base_points = ctx.base_points;
    rep.multiplier = ctx.multiplier;
    rep.points = ctx.base_points * ctx.multiplier;
    score_ += rep.points;

    // rule: RES-08, GOAL-02 goal first, then the automatic reshuffle.
    if (goal_met()) {
        outcome_ = Outcome::Won;
    } else if (rep.stuck) {
        outcome_ = Outcome::Lost;
    } else if (legal_moves().empty()) {
        // rule: SHUF-02
        ShuffleReport sh = do_shuffle();
        sh.time_delta_seconds = -kAutoShufflePenaltySeconds;
        rep.time_delta_seconds -= kAutoShufflePenaltySeconds;
        // AMBIGUOUS: the penalty clamps at 0 and loses at once.
        time_left_us_ = std::max<std::int64_t>(0, time_left_us_ - to_micros(kAutoShufflePenaltySeconds));
        if (sh.stuck) rep.stuck = true;
        rep.auto_shuffle = sh;
        if (rep.stuck || time_left_us_ <= 0) outcome_ = Outcome::Lost;
    }
    rep.outcome = outcome_;
    return std::move(ctx.report);
}

ShuffleReport LevelSession::do_shuffle() {
    ShuffleReport sh;
    MoveContext probe;
    StepHooks hooks = make_hooks(probe);
    std::vector<std::pair<Cell, Cell>> map;
    bool redealt = false;
    // rule: SHUF-03, OBS-06 movable pieces only.
    if (!board_.shuffle(&hooks, map, redealt)) {
        sh.stuck = true;
        return sh;
    }
    sh.shuffled = true;
    sh.redealt = redealt;
    sh.shuffle_map = map;
    MoveResult as_result;
    as_result.shuffled = true;
    as_result.redealt = redealt;
    as_result.shuffle_map = map;
    if (!redealt) {
        // rule: GEM-05, SHUF-03 gems and power-ups stay on their pieces.
        const std::vector<bool> before = marks_.gem;
        for (const auto& [from, to] : map) marks_.gem[idx(to)] = before[idx(from)];
        power_ups_.apply_shuffle(as_result, board_, rng_, marks_);
        return sh;
    }
    // AMBIGUOUS: after a redeal the movable pieces' gems go back on random
    // plain pieces, after the power-ups (PowerUpLayer::apply_shuffle).
    int gems = 0;
    for (int y = 0; y < board_.height(); ++y)
        for (int x = 0; x < board_.width(); ++x)
            if (gem({x, y}) && movable({x, y})) {
                marks_.gem[idx({x, y})] = false;
                ++gems;
            }
    power_ups_.apply_shuffle(as_result, board_, rng_, marks_);
    std::vector<Cell> plain = plain_cells();
    for (int i = 0; i < gems && !plain.empty(); ++i) {
        const auto pick = static_cast<std::size_t>(rng_.below(static_cast<int>(plain.size())));
        marks_.gem[idx(plain[pick])] = true;
        plain.erase(plain.begin() + static_cast<std::ptrdiff_t>(pick));
    }
    return sh;
}

// ---- Other commands --------------------------------------------------------

std::optional<TickReport> LevelSession::tick(double dt) {
    if (!(dt >= 0.0)) throw std::invalid_argument("tick: dt must not be negative");
    if (outcome_ != Outcome::Playing) return std::nullopt;
    TickReport t;
    // Game time counts in whole microseconds, so the same time gives the
    // same game however it is split into ticks.
    const std::int64_t us = to_micros(dt);
    if (us > 0) {
        // rule: GOAL-04, FRZ-05
        time_left_us_ -= us;
        if (frenzy_on_) {
            frenzy_left_us_ -= us;
            if (frenzy_left_us_ <= 0) {
                // rule: FRZ-03
                frenzy_on_ = false;
                frenzy_left_us_ = 0;
                frenzy_bar_ = 0;
                t.frenzy_ended = true;
            }
        }
        idle_us_ += us;
        // rule: SHUF-05
        if (free_hints_ && !free_hint_shown_ && idle_us_ >= to_micros(kFreeHintIdleSeconds)) {
            const std::vector<Swap> moves = legal_moves();
            if (!moves.empty()) t.free_hint = moves.front();
            free_hint_shown_ = true;
        }
        // rule: GOAL-01
        if (time_left_us_ <= 0) {
            time_left_us_ = 0;
            outcome_ = Outcome::Lost;
            t.outcome_changed = true;
        }
    }
    t.time_left = time_left();
    t.frenzy_left = frenzy_left();
    t.outcome = outcome_;
    return t;
}

std::optional<ShuffleReport> LevelSession::shuffle() {
    // rule: SHUF-01 free, twice per level; not a move (TOT-07).
    if (outcome_ != Outcome::Playing || manual_shuffles_left_ <= 0) return std::nullopt;
    ShuffleReport sh = do_shuffle();
    if (!sh.stuck) {
        --manual_shuffles_left_;
        hint_.reset();
        // rule: SHUF-05 a shuffle is player activity: the idle time restarts
        // and the free hint may show again.
        idle_us_ = 0;
        free_hint_shown_ = false;
    }
    return sh;
}

std::optional<Swap> LevelSession::buy_hint() {
    // rule: SHUF-04 a hint stays shown until the next move: asking again
    // returns it at no further cost.
    if (outcome_ != Outcome::Playing) return std::nullopt;
    if (hint_) return hint_;
    if (score_ < kHintCost) return std::nullopt;
    const std::vector<Swap> moves = legal_moves();
    if (moves.empty()) return std::nullopt;
    score_ -= kHintCost;
    hint_ = moves.front();
    return hint_;
}

// ---- Scenario setup --------------------------------------------------------

void LevelSession::set_piece(Cell c, int kind) { board_.set(c.x, c.y, kind); }
void LevelSession::set_steel(Cell c, bool on) { marks_.steel[checked_idx(c, "set_steel")] = on; }
void LevelSession::set_gem(Cell c, bool on) { marks_.gem[checked_idx(c, "set_gem")] = on; }
void LevelSession::set_chain(Cell c, bool on) { chains_[checked_idx(c, "set_chain")] = on; }

std::size_t LevelSession::checked_idx(Cell c, const char* what) const {
    if (!board_.in_bounds(c.x, c.y)) throw std::out_of_range(std::string(what) + ": cell off the board");
    return idx(c);
}
void LevelSession::set_power_up(Cell c, std::optional<PowerUp> p) {
    if (p) power_ups_.set(c, *p);
    else power_ups_.erase(c);
}
void LevelSession::set_frenzy(bool on, double left) {
    frenzy_on_ = on;
    frenzy_left_us_ = on ? to_micros(left) : 0;
}

}  // namespace montezuma

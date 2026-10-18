// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/bonuses.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace montezuma {

namespace {

// Builders keep the table readable: each sets only the part that applies.
constexpr PowerUpTierParams area(int chebyshev, int manhattan) {
    PowerUpTierParams p;
    p.area_chebyshev = chebyshev;
    p.area_manhattan = manhattan;
    return p;
}
constexpr PowerUpTierParams rays(unsigned mask) {
    PowerUpTierParams p;
    p.rays = mask;
    return p;
}
constexpr PowerUpTierParams kind_clear(bool steel, bool neighbours) {
    PowerUpTierParams p;
    p.kind_clear = true;
    p.kind_steel = steel;
    p.kind_neighbours = neighbours;
    return p;
}
constexpr PowerUpTierParams time_bonus(double seconds) {
    PowerUpTierParams p;
    p.time_seconds = seconds;
    return p;
}
constexpr PowerUpTierParams multiply(int factor) {
    PowerUpTierParams p;
    p.score_multiplier = factor;
    return p;
}
constexpr PowerUpTierParams hint(double seconds) {
    PowerUpTierParams p;
    p.hint_seconds = seconds;
    return p;
}

// The defs table, one row per id in PowerUpId order. Numbers come from
// docs/reference/rules-power-ups.md; change them only with a spec change.
constexpr std::array<PowerUpDef, kPowerUpCount> kPowerUpDefs{{
    // rule: PWR-06, OBS-03 tier 1 the 3x3 square (9 cells), tier 2 the
    // diamond of radius 2 (13), tier 3 the 5x5 square without its corners
    // (21). Clears steel.
    {PowerUpId::Tnt, "tnt", {{area(1, 2), area(2, 2), area(2, 3)}}},
    // rule: PWR-07, OBS-03 tier 1 every non-steel piece of its piece's kind,
    // tier 2 also that kind's steel, tier 3 also every non-steel piece
    // orthogonally next to a cleared one.
    {PowerUpId::ColorClear, "color_clear", {{kind_clear(false, false), kind_clear(true, false), kind_clear(true, true)}}},
    // rule: PWR-08, OBS-03 tier 1 every cell above it in its column, tier 2
    // above plus the whole row, tier 3 the whole row and column. Includes its
    // own cell. Clears steel.
    {PowerUpId::Lightning, "lightning", {{rays(kRayUp), rays(kRayUp | kRayLeft | kRayRight), rays(kRayAll)}}},
    // rule: PWR-09
    {PowerUpId::Clock, "clock", {{time_bonus(15.0), time_bonus(20.0), time_bonus(25.0)}}},
    // rule: PWR-10
    {PowerUpId::Multiplier, "multiplier", {{multiply(5), multiply(7), multiply(10)}}},
    // rule: PWR-11
    {PowerUpId::Hint, "hint", {{hint(20.0), hint(25.0), hint(30.0)}}},
}};

// nullptr when `defs` is well formed, otherwise what is wrong.
constexpr const char* defs_error(std::span<const PowerUpDef> defs) {
    if (defs.size() != kPowerUpCount) return "power-up defs: wrong number of rows";
    for (std::size_t i = 0; i < defs.size(); ++i) {
        const PowerUpDef& d = defs[i];
        if (power_up_index(d.id) != i) return "power-up defs: ids out of order";
        if (d.name.empty()) return "power-up defs: empty name";
        for (std::size_t j = 0; j < i; ++j)
            if (defs[j].name == d.name) return "power-up defs: duplicate name";
        for (const PowerUpTierParams& p : d.tiers) {
            if (p.area_chebyshev < -1 || (p.area_chebyshev >= 0 && p.area_manhattan < 0))
                return "power-up defs: bad area";
            if ((p.rays & ~static_cast<unsigned>(kRayAll)) != 0) return "power-up defs: bad ray mask";
            if ((p.kind_steel || p.kind_neighbours) && !p.kind_clear)
                return "power-up defs: steel or neighbours without kind_clear";
            if (p.score_multiplier < 1) return "power-up defs: multiplier below 1";
            if (p.time_seconds < 0.0 || p.hint_seconds < 0.0) return "power-up defs: negative time";
        }
    }
    return nullptr;
}
static_assert(defs_error(kPowerUpDefs) == nullptr, "power-up defs table is malformed");

constexpr int clamp_tier(int tier) { return std::clamp(tier, kMinPowerUpTier, kMaxPowerUpTier); }

bool mark_at(const std::vector<bool>& marks, const Board& board, Cell c) noexcept {
    if (!board.in_bounds(c.x, c.y)) return false;
    const std::size_t i = static_cast<std::size_t>(c.y) * static_cast<std::size_t>(board.width()) +
                          static_cast<std::size_t>(c.x);
    return i < marks.size() && marks[i];
}

}  // namespace

std::string_view power_up_name(PowerUpId id) noexcept { return kPowerUpDefs[power_up_index(id)].name; }

std::optional<PowerUpId> power_up_from_name(std::string_view name) noexcept {
    for (const PowerUpDef& d : kPowerUpDefs)
        if (d.name == name) return d.id;
    return std::nullopt;
}

const PowerUpTierParams& PowerUpDef::at_tier(int tier) const noexcept {
    return tiers[static_cast<std::size_t>(clamp_tier(tier) - kMinPowerUpTier)];
}

std::span<const PowerUpDef, kPowerUpCount> power_up_defs() noexcept { return kPowerUpDefs; }

const PowerUpDef& power_up_def(PowerUpId id) noexcept { return kPowerUpDefs[power_up_index(id)]; }

void validate_power_up_defs(std::span<const PowerUpDef> defs) {
    if (const char* error = defs_error(defs)) throw std::invalid_argument(error);
}

void validate_power_up_tiers(const PowerUpTiers& tiers) {
    for (std::size_t i = 0; i < kPowerUpCount; ++i)
        if (tiers[i] < 0 || tiers[i] > kMaxPowerUpTier)
            throw std::invalid_argument("power-up tiers: tier " + std::to_string(tiers[i]) + " out of range for '" +
                                        std::string(kPowerUpDefs[i].name) + "'");
}

bool PieceMarks::is_steel(const Board& board, Cell c) const noexcept { return mark_at(steel, board, c); }
bool PieceMarks::has_gem(const Board& board, Cell c) const noexcept { return mark_at(gem, board, c); }

PowerUpEffect power_up_effect(PowerUpId id, int tier, const Board& board, Cell at, const PieceMarks& marks) noexcept {
    const PowerUpTierParams& p = power_up_def(id).at_tier(tier);

    PowerUpEffect effect;
    effect.id = id;
    effect.tier = clamp_tier(tier);
    effect.time_delta_seconds = p.time_seconds;
    effect.score_multiplier = p.score_multiplier;
    effect.hint_seconds = p.hint_seconds;
    if (!board.in_bounds(at.x, at.y)) return effect;

    const int w = board.width();
    const int h = board.height();
    const auto idx = [w](int x, int y) { return static_cast<std::size_t>(y * w + x); };
    std::vector<char> mark(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
    const auto put = [&](int x, int y) {
        if (board.in_bounds(x, y)) mark[idx(x, y)] = 1;
    };

    // rule: PWR-04 the power-up's own piece is always part of its clear.
    put(at.x, at.y);
    if (p.area_chebyshev >= 0) {
        for (int dy = -p.area_chebyshev; dy <= p.area_chebyshev; ++dy)
            for (int dx = -p.area_chebyshev; dx <= p.area_chebyshev; ++dx)
                if (std::abs(dx) + std::abs(dy) <= p.area_manhattan) put(at.x + dx, at.y + dy);
    }
    if (p.rays != kRayNone) {
        if (p.rays & kRayUp)
            for (int y = at.y - 1; y >= 0; --y) put(at.x, y);
        if (p.rays & kRayDown)
            for (int y = at.y + 1; y < h; ++y) put(at.x, y);
        if (p.rays & kRayLeft)
            for (int x = at.x - 1; x >= 0; --x) put(x, at.y);
        if (p.rays & kRayRight)
            for (int x = at.x + 1; x < w; ++x) put(x, at.y);
    }
    if (p.kind_clear) {
        const int kind = board.at(at.x, at.y);
        if (kind != Board::kEmpty) {
            std::vector<char> hit(mark.size(), 0);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    if (board.at(x, y) == kind && (p.kind_steel || !marks.is_steel(board, {x, y})))
                        hit[idx(x, y)] = 1;
            hit[idx(at.x, at.y)] = 1;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (!hit[idx(x, y)]) continue;
                    put(x, y);
                    if (!p.kind_neighbours) continue;
                    // rule: PWR-07, OBS-03 the 4 orthogonal neighbours; steel of
                    // another kind stays (color_clear removes only its kind's).
                    for (const Cell n : {Cell{x - 1, y}, Cell{x + 1, y}, Cell{x, y - 1}, Cell{x, y + 1}})
                        if (board.in_bounds(n.x, n.y) && !marks.is_steel(board, n)) put(n.x, n.y);
                }
            }
        }
    }

    // Scanning in row-major order sorts and de-duplicates; skip empty cells.
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (mark[idx(x, y)] && board.at(x, y) != Board::kEmpty) effect.cells_to_clear.push_back({x, y});
    return effect;
}

PowerUpEffect power_up_effect(const TriggeredPowerUp& triggered, const Board& board, const PieceMarks& marks) noexcept {
    return power_up_effect(triggered.power_up.id, triggered.power_up.tier, board, triggered.cell, marks);
}

int move_score_multiplier(std::span<const PowerUpEffect> effects) noexcept {
    int best = 1;
    for (const PowerUpEffect& e : effects) best = std::max(best, e.score_multiplier);
    return best;
}

int power_ups_earned_by(std::span<const Run> runs) noexcept {
    int earned = 0;
    for (const Run& run : runs) {
        const std::size_t len = run.cells.size();
        if (len == 4) earned += kPowerUpsPerRunOf4;
        else if (len >= 5) earned += kPowerUpsPerRunOf5;
    }
    return earned;
}

PowerUpLayer::PowerUpLayer(int w, int h) : width(w), height(h) {
    if (w < 1 || h < 1) throw std::invalid_argument("power-up layer: width and height must be at least 1");
    cells.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), std::nullopt);
}

std::optional<PowerUp> PowerUpLayer::at(Cell c) const noexcept {
    if (!in_bounds(c) || index(c) >= cells.size()) return std::nullopt;
    return cells[index(c)];
}

void PowerUpLayer::set(Cell c, PowerUp p) noexcept {
    if (in_bounds(c) && index(c) < cells.size()) cells[index(c)] = p;
}

void PowerUpLayer::erase(Cell c) noexcept {
    if (in_bounds(c) && index(c) < cells.size()) cells[index(c)].reset();
}

void PowerUpLayer::clear() noexcept {
    std::fill(cells.begin(), cells.end(), std::nullopt);
    pending.clear();
}

int PowerUpLayer::count() const noexcept {
    return static_cast<int>(std::count_if(cells.begin(), cells.end(), [](const auto& c) { return c.has_value(); }));
}

std::vector<PowerUpPlacement> PowerUpLayer::entries() const {
    std::vector<PowerUpPlacement> out;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (const auto p = at({x, y})) out.push_back({{x, y}, *p});
    return out;
}

StepClear PowerUpLayer::expand_clear(const Board& board, std::span<const Cell> run_cells, int step,
                                     const PieceMarks& marks, const std::vector<bool>& holds) const {
    StepClear out;
    const int w = board.width();
    const int h = board.height();
    std::vector<char> in_clear(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
    std::vector<char> from_run(in_clear.size(), 0);
    std::vector<char> fired(in_clear.size(), 0);
    const auto idx = [w](Cell c) { return static_cast<std::size_t>(c.y * w + c.x); };
    const auto usable = [&](Cell c) { return board.in_bounds(c.x, c.y) && board.at(c.x, c.y) != Board::kEmpty; };
    // rule: OBS-05 a clear that reaches a chained piece only breaks its chain.
    std::vector<char> held(in_clear.size(), 0);
    const auto holds_at = [&](Cell c) { return idx(c) < holds.size() && holds[idx(c)]; };

    for (const Cell c : run_cells)
        if (usable(c)) {
            if (holds_at(c)) held[idx(c)] = 1;
            else in_clear[idx(c)] = from_run[idx(c)] = 1;
        }

    // Power-ups on cleared pieces go off: first those on run cells in
    // row-major order, then those each effect reaches, in the order reached.
    std::vector<Cell> queue;
    const auto enqueue_new = [&](const std::vector<Cell>& candidates) {
        for (const Cell c : candidates)
            if (in_clear[idx(c)] && !fired[idx(c)] && has(c)) {
                fired[idx(c)] = 1;
                queue.push_back(c);
            }
    };
    std::vector<Cell> start;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (in_clear[idx({x, y})]) start.push_back({x, y});
    enqueue_new(start);

    for (std::size_t q = 0; q < queue.size(); ++q) {
        const Cell c = queue[q];
        const PowerUp p = *at(c);
        PowerUpEffect e = power_up_effect(p.id, p.tier, board, c, marks);
        std::vector<Cell> added;
        for (const Cell t : e.cells_to_clear)
            if (holds_at(t)) {
                held[idx(t)] = 1;
            } else if (!in_clear[idx(t)]) {
                in_clear[idx(t)] = 1;
                added.push_back(t);
            }
        out.triggered.push_back({c, p, step});
        out.effects.push_back(std::move(e));
        enqueue_new(added);  // row-major, as cells_to_clear is
    }

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (held[idx({x, y})]) out.held.push_back({x, y});
            if (!in_clear[idx({x, y})]) continue;
            out.cells.push_back({x, y});
            if (!from_run[idx({x, y})]) out.power_up_cells.push_back({x, y});
        }
    return out;
}

void PowerUpLayer::apply_swap(Cell a, Cell b) noexcept {
    if (in_bounds(a) && in_bounds(b) && cells.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
        std::swap(cells[index(a)], cells[index(b)]);
}

void PowerUpLayer::apply_step(const CascadeStep& step) noexcept {
    for (const Cell c : step.cleared) erase(c);
    // Falls happen at once: lift every moving power-up, then drop them.
    std::vector<std::pair<Cell, PowerUp>> moving;
    for (const Fall& f : step.falls) {
        if (const auto p = at(f.from)) {
            moving.emplace_back(f.to, *p);
            erase(f.from);
        }
    }
    for (const auto& [to, p] : moving) set(to, p);
    // Spawns are new plain pieces: nothing to do.
}

void PowerUpLayer::apply_shuffle(const MoveResult& result, const Board& board, Rng& rng, const PieceMarks& marks) {
    if (!result.shuffled) return;
    if (!result.redealt) {
        const std::vector<std::optional<PowerUp>> before = cells;
        for (const auto& [from, to] : result.shuffle_map) {
            if (!in_bounds(from) || !in_bounds(to)) continue;
            cells[index(to)] = before[index(from)];
        }
        return;
    }
    // A fresh deal: put the same power-ups back on random plain pieces.
    const std::vector<PowerUpPlacement> kept = entries();
    std::fill(cells.begin(), cells.end(), std::nullopt);
    std::vector<Cell> plain = plain_cells(board, marks);
    for (const PowerUpPlacement& k : kept) {
        if (plain.empty()) {
            pending.emplace_back(k.power_up.id);
            continue;
        }
        const std::size_t pick = static_cast<std::size_t>(rng.below(static_cast<int>(plain.size())));
        set(plain[pick], k.power_up);
        plain.erase(plain.begin() + static_cast<std::ptrdiff_t>(pick));
    }
}

void PowerUpLayer::remap(const MoveResult& result, const Board& board, Rng& rng, const PieceMarks& marks) {
    if (result.swap) apply_swap((*result.swap)[0], (*result.swap)[1]);
    for (const CascadeStep& step : result.cascades) apply_step(step);
    apply_shuffle(result, board, rng, marks);
}

std::vector<TriggeredPowerUp> PowerUpLayer::triggered_by(const MoveResult& result) const {
    std::vector<TriggeredPowerUp> fired;
    PowerUpLayer scratch = *this;
    if (result.swap) scratch.apply_swap((*result.swap)[0], (*result.swap)[1]);
    for (std::size_t s = 0; s < result.cascades.size(); ++s) {
        for (const Cell c : result.cascades[s].cleared)
            if (const auto p = scratch.at(c)) fired.push_back({c, *p, static_cast<int>(s)});
        scratch.apply_step(result.cascades[s]);
    }
    return fired;
}

void PowerUpLayer::check_size(const Board& board) const {
    if (board.width() != width || board.height() != height ||
        cells.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
        throw std::invalid_argument("power-up layer: size differs from the board");
}

std::vector<Cell> PowerUpLayer::plain_cells(const Board& board, const PieceMarks& marks) const {
    // rule: PWR-02, GEM-01, OBS-04 a plain piece: occupied, not steel, no gem,
    // no power-up.
    std::vector<Cell> plain;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const Cell c{x, y};
            if (board.in_bounds(x, y) && board.at(x, y) != Board::kEmpty && !has(c) && !marks.is_steel(board, c) &&
                !marks.has_gem(board, c))
                plain.push_back(c);
        }
    return plain;
}

std::vector<PowerUpPlacement> PowerUpLayer::place(std::vector<std::optional<PowerUpId>> wanted, const Board& board,
                                                  Rng& rng, const PowerUpTiers& tiers, const PieceMarks& marks,
                                                  int max_on_board,
                                                  std::vector<std::optional<PowerUpId>>* left_over) {
    std::vector<PowerUpPlacement> placed;
    std::vector<PowerUpId> pool;
    for (const PowerUpDef& d : kPowerUpDefs)
        if (tiers[power_up_index(d.id)] >= kMinPowerUpTier) pool.push_back(d.id);
    if (pool.empty()) return placed;

    std::vector<Cell> plain = plain_cells(board, marks);
    int on_board = count();
    for (std::size_t i = 0; i < wanted.size(); ++i) {
        // rule: PWR-02 at the cap, further power-ups are not placed.
        if (on_board >= max_on_board) break;
        if (plain.empty()) {
            // Rules spec edge case: no plain piece, so they wait for the next
            // refill.
            if (left_over) left_over->insert(left_over->end(), wanted.begin() + static_cast<std::ptrdiff_t>(i), wanted.end());
            break;
        }
        // rule: PWR-02 drawn uniformly from the unlocked power-ups, then put
        // on a random plain piece. Draw order: id first, then cell.
        PowerUpId id;
        if (wanted[i] && tiers[power_up_index(*wanted[i])] >= kMinPowerUpTier) id = *wanted[i];
        else id = pool[static_cast<std::size_t>(rng.below(static_cast<int>(pool.size())))];
        const std::size_t pick = static_cast<std::size_t>(rng.below(static_cast<int>(plain.size())));
        const Cell c = plain[pick];
        plain.erase(plain.begin() + static_cast<std::ptrdiff_t>(pick));
        const PowerUp p{id, tiers[power_up_index(id)]};
        set(c, p);
        placed.push_back({c, p});
        ++on_board;
    }
    return placed;
}

std::vector<PowerUpPlacement> PowerUpLayer::place_for_step(std::span<const Run> runs, const Board& board, Rng& rng,
                                                           const PowerUpTiers& tiers, const PieceMarks& marks,
                                                           int max_on_board) {
    validate_power_up_tiers(tiers);
    check_size(board);
    std::vector<std::optional<PowerUpId>> wanted = std::move(pending);
    pending.clear();
    wanted.resize(wanted.size() + static_cast<std::size_t>(power_ups_earned_by(runs)));
    if (wanted.empty()) return {};
    return place(std::move(wanted), board, rng, tiers, marks, max_on_board, &pending);
}

std::vector<PowerUpPlacement> PowerUpLayer::place_random(int n, const Board& board, Rng& rng,
                                                         const PowerUpTiers& tiers, const PieceMarks& marks,
                                                         int max_on_board) {
    validate_power_up_tiers(tiers);
    check_size(board);
    if (n <= 0) return {};
    return place(std::vector<std::optional<PowerUpId>>(static_cast<std::size_t>(n)), board, rng, tiers, marks,
                 max_on_board, nullptr);
}

namespace {

// Runs one move through the board with power-up hooks. For a swap, the
// layer's swap is applied in step 0's extend_clear, which the board calls
// only for a legal swap.
template <class Resolve>
std::optional<MoveResult> resolve_with_power_ups(Board& board, PowerUpLayer& layer, Rng& rng,
                                                 const PowerUpTiers& tiers, PowerUpMoveReport& report,
                                                 const PieceMarks& marks, std::optional<std::array<Cell, 2>> swap,
                                                 Resolve&& resolve) {
    validate_power_up_tiers(tiers);
    if (board.width() != layer.width || board.height() != layer.height ||
        layer.cells.size() != static_cast<std::size_t>(layer.width) * static_cast<std::size_t>(layer.height))
        throw std::invalid_argument("power-up layer: size differs from the board");

    PowerUpMoveReport rep;
    StepHooks hooks;
    hooks.extend_clear = [&](const Board& b, const std::vector<Cell>& cleared, int step) {
        if (step == 0 && swap) layer.apply_swap((*swap)[0], (*swap)[1]);
        StepClear sc = layer.expand_clear(b, cleared, step, marks);
        std::vector<Cell> extra = sc.power_up_cells;
        rep.steps.push_back({std::move(sc), {}});
        return extra;
    };
    hooks.after_refill = [&](const Board& b, const CascadeStep& step, int index) {
        layer.apply_step(step);
        if (static_cast<std::size_t>(index) < rep.steps.size())
            rep.steps[static_cast<std::size_t>(index)].placed = layer.place_for_step(step.runs, b, rng, tiers, marks);
    };

    std::optional<MoveResult> result = resolve(&hooks);
    if (!result) {
        report = PowerUpMoveReport{};
        return result;
    }
    layer.apply_shuffle(*result, board, rng, marks);

    for (const PowerUpStep& s : rep.steps) {
        rep.score_multiplier = std::max(rep.score_multiplier, move_score_multiplier(s.clear.effects));
        for (const PowerUpEffect& e : s.clear.effects) {
            rep.time_delta_seconds += e.time_delta_seconds;
            if (e.hint_seconds > 0.0) rep.hint_seconds = e.hint_seconds;
        }
    }
    report = std::move(rep);
    return result;
}

}  // namespace

std::optional<MoveResult> swap_with_power_ups(Board& board, PowerUpLayer& layer, Cell a, Cell b, Rng& rng,
                                              const PowerUpTiers& tiers, PowerUpMoveReport& report,
                                              const PieceMarks& marks) {
    return resolve_with_power_ups(board, layer, rng, tiers, report, marks, std::array<Cell, 2>{a, b},
                                  [&](const StepHooks* hooks) { return board.try_swap(a, b, hooks); });
}

std::optional<MoveResult> clear_cells_with_power_ups(Board& board, PowerUpLayer& layer,
                                                     const std::vector<Cell>& cells, Rng& rng,
                                                     const PowerUpTiers& tiers, PowerUpMoveReport& report,
                                                     const PieceMarks& marks) {
    return resolve_with_power_ups(board, layer, rng, tiers, report, marks, std::nullopt,
                                  [&](const StepHooks* hooks) { return board.clear_cells(cells, hooks); });
}

}  // namespace montezuma

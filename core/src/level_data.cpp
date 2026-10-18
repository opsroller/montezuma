// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "montezuma/level_data.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <exception>
#include <set>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace montezuma {

namespace {

using nlohmann::json;

// Upper bound on board width and height. Not a rule: it keeps a typo such as
// 800 from allocating a huge board or overflowing cell indices.
constexpr int kMaxBoardSize = 64;

// ---------------------------------------------------------------------------
// Contract id lists: the only copies of the totem and power-up ids in the
// level parser. Temporary: once PR #6 (totems) and PR #8 (power-ups) land,
// replace these with totem_from_name() and the power-up equivalent so the
// ids have a single source in core.
// rule: <tbd>
constexpr std::array<std::string_view, 7> kTotemIdsUntilPr6{
    "fire", "time", "gold", "gift", "harvest", "paint", "ward"};
// rule: <tbd>
constexpr std::array<std::string_view, 6> kPowerupIdsUntilPr8{
    "tnt", "color_clear", "lightning", "clock", "multiplier", "hint"};
// ---------------------------------------------------------------------------

constexpr std::array<std::string_view, 15> kTopLevelFields{
    "id",         "episode",         "index",      "board",         "gem_goal",
    "quotas",     "time_limit",      "totems_unlocked", "powerups_unlocked",
    "steel_rate", "blocked_cells",   "chained_cells",   "rules",    "theme",
    "seed"};
constexpr std::array<std::string_view, 3> kBoardFields{"width", "height", "kinds"};
constexpr std::array<std::string_view, 2> kDifficultyFields{"normal", "hard"};

template <std::size_t N>
bool contains(const std::array<std::string_view, N>& list, std::string_view value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

template <std::size_t N>
std::string join(const std::array<std::string_view, N>& list) {
    std::string out;
    for (std::string_view item : list) {
        if (!out.empty()) out += ", ";
        out += item;
    }
    return out;
}

std::string child(const std::string& path, std::string_view key) {
    return path.empty() ? std::string(key) : path + "." + std::string(key);
}

std::string element(const std::string& path, std::size_t i) { return path + "[" + std::to_string(i) + "]"; }

// What a value is, for "expected X, got Y" messages. Numbers show their value.
std::string describe(const json& j) {
    if (j.is_number() || j.is_boolean() || j.is_null()) return j.dump();
    return j.type_name();
}

std::string expected_id(int episode, int index) {
    std::string nn = std::to_string(index);
    if (nn.size() < 2) nn.insert(0, 2 - nn.size(), '0');
    return "e" + std::to_string(episode) + "-l" + nn;
}

// A rule-set or theme id: non-empty, only a-z, 0-9 and _.
bool valid_symbol(std::string_view s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

bool all_digits(std::string_view s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// "e<N>-l<NN>": N without leading zeros, NN at least two digits.
bool valid_id_format(std::string_view id) {
    const auto dash = id.find("-l");
    if (id.size() < 2 || id[0] != 'e' || dash == std::string_view::npos) return false;
    const std::string_view episode = id.substr(1, dash - 1);
    const std::string_view index = id.substr(dash + 2);
    return all_digits(episode) && episode[0] != '0' && all_digits(index) && index.size() >= 2;
}

class Validator {
public:
    std::vector<LevelDataError> errors;

    void error(std::string path, std::string message) { errors.push_back({std::move(path), std::move(message)}); }

    LevelData validate(const json& root) {
        LevelData level;
        if (!root.is_object()) {
            error("", "expected an object at the top level, got " + describe(root));
            return level;
        }
        for (const auto& item : root.items())
            if (!contains(kTopLevelFields, item.key())) error(item.key(), "unknown field");

        const std::optional<int> episode = required_int(root, "", "episode", 1, INT_MAX);
        const std::optional<int> index = required_int(root, "", "index", 1, INT_MAX);
        if (episode) level.episode = *episode;
        if (index) level.index = *index;
        read_id(root, episode, index, level);
        const bool board_ok = read_board(root, level.board);
        const bool has_gem_goal = read_gem_goal(root, level);
        const bool has_quotas = read_quotas(root, board_ok, level);
        // rule: <tbd>
        if (!has_gem_goal && !has_quotas) error("", "needs a goal: gem_goal, quotas or both");
        read_time_limit(root, level);
        read_id_list(root, "totems_unlocked", true, "totem", kTotemIdsUntilPr6, level.totems_unlocked);
        read_id_list(root, "powerups_unlocked", false, "power-up", kPowerupIdsUntilPr8, level.powerups_unlocked);
        read_steel_rate(root, level);
        // With an invalid board only the sign can be checked (size 0 means unknown).
        const int width = board_ok ? level.board.width : 0;
        const int height = board_ok ? level.board.height : 0;
        const CellSet blocked = read_cells(root, "blocked_cells", width, height, level.blocked_cells);
        const CellSet chained = read_cells(root, "chained_cells", width, height, level.chained_cells);
        // rule: <tbd>
        for (const auto& [cell, i] : chained) {
            const auto it = blocked.find(cell);
            if (it != blocked.end())
                error(element("chained_cells", i),
                      "cell is also blocked (" + element("blocked_cells", it->second) + ")");
        }
        read_symbol(root, "rules", level.rules);
        read_symbol(root, "theme", level.theme);
        read_seed(root, level);
        return level;
    }

private:
    using CellSet = std::map<std::pair<int, int>, std::size_t>;  // valid cell -> first index

    // The field's value, or null after reporting it missing when required.
    const json* field(const json& object, const std::string& path, std::string_view key, bool required) {
        const auto it = object.find(key);
        if (it == object.end()) {
            if (required) error(child(path, key), "required field missing");
            return nullptr;
        }
        return &*it;
    }

    std::optional<std::int64_t> integer(const json& j, const std::string& path) {
        if (j.is_number_unsigned()) {
            const auto u = j.get<std::uint64_t>();
            if (u > static_cast<std::uint64_t>(INT64_MAX)) {
                error(path, "integer out of range, got " + describe(j));
                return std::nullopt;
            }
            return static_cast<std::int64_t>(u);
        }
        if (j.is_number_integer()) return j.get<std::int64_t>();
        error(path, "expected an integer, got " + describe(j));
        return std::nullopt;
    }

    std::optional<int> int_in_range(const json& j, const std::string& path, int lo, int hi) {
        const std::optional<std::int64_t> v = integer(j, path);
        if (!v) return std::nullopt;
        if (*v < lo || *v > hi) {
            if (hi == INT_MAX && *v > hi)
                error(path, "too large, got " + std::to_string(*v));
            else if (hi == INT_MAX)
                error(path, "must be at least " + std::to_string(lo) + ", got " + std::to_string(*v));
            else
                error(path, "must be between " + std::to_string(lo) + " and " + std::to_string(hi) + ", got " +
                                std::to_string(*v));
            return std::nullopt;
        }
        return static_cast<int>(*v);
    }

    std::optional<int> required_int(const json& object, const std::string& path, std::string_view key, int lo, int hi) {
        const json* j = field(object, path, key, true);
        return j ? int_in_range(*j, child(path, key), lo, hi) : std::nullopt;
    }

    template <std::size_t N>
    void reject_unknown(const json& object, const std::string& path, const std::array<std::string_view, N>& known) {
        for (const auto& item : object.items())
            if (!contains(known, item.key())) error(child(path, item.key()), "unknown field");
    }

    void read_id(const json& root, std::optional<int> episode, std::optional<int> index, LevelData& level) {
        const json* j = field(root, "", "id", true);
        if (!j) return;
        if (!j->is_string()) {
            error("id", "expected a string, got " + describe(*j));
            return;
        }
        level.id = j->get<std::string>();
        if (!valid_id_format(level.id)) {
            error("id", "expected the form e<N>-l<NN> (e.g. e1-l03), got \"" + level.id + "\"");
            return;
        }
        if (episode && index && level.id != expected_id(*episode, *index))
            error("id", "\"" + level.id + "\" does not match episode " + std::to_string(*episode) + " and index " +
                            std::to_string(*index) + " (expected \"" + expected_id(*episode, *index) + "\")");
    }

    // True when the board (given or defaulted) is valid.
    bool read_board(const json& root, BoardSpec& board) {
        const json* j = field(root, "", "board", false);
        if (!j) return true;
        if (!j->is_object()) {
            error("board", "expected an object, got " + describe(*j));
            return false;
        }
        reject_unknown(*j, "board", kBoardFields);
        bool ok = true;
        auto read = [&](std::string_view key, int lo, int hi, int& out) {
            const json* v = field(*j, "board", key, false);
            if (!v) return;
            const std::optional<int> n = int_in_range(*v, child("board", key), lo, hi);
            if (n)
                out = *n;
            else
                ok = false;
        };
        read("width", Board::kMinSize, kMaxBoardSize, board.width);
        read("height", Board::kMinSize, kMaxBoardSize, board.height);
        read("kinds", BoardSpec::kMinKinds, BoardSpec::kMaxKinds, board.kinds);
        return ok;
    }

    // True when gem_goal is present (valid or not), so a bad gem_goal does not
    // also report a missing goal.
    // rule: <tbd>
    bool read_gem_goal(const json& root, LevelData& level) {
        const json* j = field(root, "", "gem_goal", false);
        if (!j) return false;
        if (!j->is_object()) {
            error("gem_goal", "expected an object with normal and hard, got " + describe(*j));
            return true;
        }
        reject_unknown(*j, "gem_goal", kDifficultyFields);
        const std::optional<int> normal = required_int(*j, "gem_goal", "normal", 1, INT_MAX);
        const std::optional<int> hard = required_int(*j, "gem_goal", "hard", 1, INT_MAX);
        if (normal && hard) level.gem_goal = GemGoal{*normal, *hard};
        return true;
    }

    // Optional rule-variant goal. True when quotas is present (valid or not).
    // rule: <tbd>
    bool read_quotas(const json& root, bool board_ok, LevelData& level) {
        const json* j = field(root, "", "quotas", false);
        if (!j) return false;
        if (!j->is_object()) {
            error("quotas", "expected an object of kind name to count, got " + describe(*j));
            return true;
        }
        if (j->empty()) error("quotas", "must list at least one kind");
        for (const auto& item : j->items()) {
            const std::string path = child("quotas", item.key());
            const std::optional<int> kind = kind_index(item.key());
            if (!kind) {
                error(path, "unknown kind (expected one of " + join(kKindNames) + ")");
                continue;
            }
            const std::optional<int> count = int_in_range(item.value(), path, 1, INT_MAX);
            if (!count) continue;
            if (board_ok && *kind >= level.board.kinds) {
                error(path, "kind " + std::to_string(*kind) + " is not on a board with " +
                                std::to_string(level.board.kinds) + " kinds");
                continue;
            }
            level.quotas[*kind] = *count;
        }
        return true;
    }

    // Optional; when absent the timer tier decides the time.
    // rule: <tbd>
    void read_time_limit(const json& root, LevelData& level) {
        const json* j = field(root, "", "time_limit", false);
        if (!j) return;
        if (!j->is_object()) {
            error("time_limit", "expected an object with normal and hard, got " + describe(*j));
            return;
        }
        reject_unknown(*j, "time_limit", kDifficultyFields);
        bool ok = true;
        auto read = [&](std::string_view key) -> double {
            const json* v = field(*j, "time_limit", key, true);
            if (!v) {
                ok = false;
                return 0.0;
            }
            const std::string path = child("time_limit", key);
            if (!v->is_number()) {
                error(path, "expected a number of seconds, got " + describe(*v));
                ok = false;
                return 0.0;
            }
            const double seconds = v->get<double>();
            if (!std::isfinite(seconds) || seconds <= 0.0) {
                error(path, "must be greater than 0 seconds, got " + describe(*v));
                ok = false;
                return 0.0;
            }
            return seconds;
        };
        const double normal = read("normal");
        const double hard = read("hard");
        if (ok) level.time_limit = TimeLimit{normal, hard};
    }

    // An array of ids from `known`, each at most once, kept in file order.
    template <std::size_t N>
    void read_id_list(const json& root, const char* key, bool required, const std::string& noun,
                      const std::array<std::string_view, N>& known, std::vector<std::string>& out) {
        const json* j = field(root, "", key, required);
        if (!j) return;
        if (!j->is_array()) {
            error(key, "expected an array of " + noun + " ids, got " + describe(*j));
            return;
        }
        for (std::size_t i = 0; i < j->size(); ++i) {
            const json& v = (*j)[i];
            const std::string path = element(key, i);
            if (!v.is_string()) {
                error(path, "expected a " + noun + " id string, got " + describe(v));
                continue;
            }
            const auto id = v.get<std::string>();
            if (!contains(known, id)) {
                error(path, "unknown " + noun + " \"" + id + "\" (expected one of " + join(known) + ")");
                continue;
            }
            if (std::find(out.begin(), out.end(), id) != out.end()) {
                error(path, "duplicate " + noun + " \"" + id + "\"");
                continue;
            }
            out.push_back(id);
        }
    }

    // Chance per spawned piece to be steel.
    // rule: <tbd>
    void read_steel_rate(const json& root, LevelData& level) {
        const json* j = field(root, "", "steel_rate", false);
        if (!j) return;
        if (!j->is_number()) {
            error("steel_rate", "expected a number from 0 to 1, got " + describe(*j));
            return;
        }
        const double rate = j->get<double>();
        if (!std::isfinite(rate) || rate < 0.0 || rate > 1.0) {
            error("steel_rate", "must be between 0 and 1, got " + describe(*j));
            return;
        }
        level.steel_rate = rate;
    }

    // Optional rule-set or theme id; keeps the default when absent.
    void read_symbol(const json& root, const char* key, std::string& out) {
        const json* j = field(root, "", key, false);
        if (!j) return;
        if (!j->is_string()) {
            error(key, "expected a string, got " + describe(*j));
            return;
        }
        auto value = j->get<std::string>();
        if (!valid_symbol(value)) {
            error(key, "expected a non-empty id of a-z, 0-9 and _, got \"" + value + "\"");
            return;
        }
        out = std::move(value);
    }

    // An optional array of [x, y] cells, each on the board and at most once.
    // Returns the valid cells with their first index. width and height are 0
    // when the board is invalid: then only the sign can be checked.
    CellSet read_cells(const json& root, const char* key, int width, int height, std::vector<Cell>& out) {
        CellSet seen;
        const json* j = field(root, "", key, false);
        if (!j) return seen;
        if (!j->is_array()) {
            error(key, "expected an array of [x, y] pairs, got " + describe(*j));
            return seen;
        }
        for (std::size_t i = 0; i < j->size(); ++i) {
            const json& v = (*j)[i];
            const std::string path = element(key, i);
            if (!v.is_array() || v.size() != 2) {
                error(path, "expected [x, y], got " + (v.is_array() ? "an array of " + std::to_string(v.size()) : describe(v)));
                continue;
            }
            const std::optional<int> x = coordinate(v[0], element(path, 0), "x", width);
            const std::optional<int> y = coordinate(v[1], element(path, 1), "y", height);
            if (!x || !y) continue;
            const auto [it, inserted] = seen.emplace(std::make_pair(*x, *y), i);
            if (!inserted) {
                error(path, "duplicate of " + element(key, it->second));
                continue;
            }
            out.push_back({*x, *y});
        }
        return seen;
    }

    std::optional<int> coordinate(const json& j, const std::string& path, std::string_view axis,
                                  int size) {
        const std::optional<std::int64_t> v = integer(j, path);
        if (!v) return std::nullopt;
        if (size <= 0) {
            if (*v >= 0 && *v <= INT_MAX) return static_cast<int>(*v);
            error(path, std::string(axis) + " " + std::to_string(*v) + " is outside the board");
            return std::nullopt;
        }
        if (*v < 0 || *v >= size) {
            error(path, std::string(axis) + " " + std::to_string(*v) + " is outside the board (0 to " +
                            std::to_string(size - 1) + ")");
            return std::nullopt;
        }
        return static_cast<int>(*v);
    }

    void read_seed(const json& root, LevelData& level) {
        const json* j = field(root, "", "seed", true);
        if (!j) return;
        if (j->is_number_unsigned()) {
            level.seed = j->get<std::uint64_t>();
        } else if (j->is_number_integer()) {
            error("seed", "must not be negative, got " + describe(*j));
        } else {
            error("seed", "expected an integer, got " + describe(*j));
        }
    }
};

// nlohmann::json keeps the last of duplicate keys silently; report them.
class DuplicateKeyCheck {
public:
    explicit DuplicateKeyCheck(std::vector<LevelDataError>& errors) : errors_(errors) {}

    bool operator()(int /*depth*/, json::parse_event_t event, json& parsed) {
        switch (event) {
            case json::parse_event_t::object_start: keys_.emplace_back(); break;
            case json::parse_event_t::object_end:
                if (!keys_.empty()) keys_.pop_back();
                break;
            case json::parse_event_t::key:
                if (!keys_.empty() && !keys_.back().insert(parsed.get<std::string>()).second)
                    errors_.push_back({"", "duplicate key \"" + parsed.get<std::string>() + "\""});
                break;
            default: break;
        }
        return true;
    }

private:
    std::vector<LevelDataError>& errors_;
    std::vector<std::set<std::string>> keys_;
};

// nlohmann messages start with an id such as "[json.exception.parse_error.101] "
// or "[json.exception.out_of_range.406] "; drop it.
std::string strip_exception_id(const char* what) {
    std::string message = what;
    if (!message.empty() && message[0] == '[') {
        const auto end = message.find("] ");
        if (end != std::string::npos) message.erase(0, end + 2);
    }
    return message;
}

}  // namespace

std::optional<int> kind_index(std::string_view name) {
    const auto it = std::find(kKindNames.begin(), kKindNames.end(), name);
    if (it == kKindNames.end()) return std::nullopt;
    return static_cast<int>(it - kKindNames.begin());
}

std::optional<Difficulty> difficulty_from_name(std::string_view name) {
    if (name == "normal") return Difficulty::Normal;
    if (name == "hard") return Difficulty::Hard;
    return std::nullopt;
}

std::string LevelDataError::to_string() const { return path.empty() ? message : path + ": " + message; }

std::string LevelParseResult::error_text() const {
    std::string out;
    for (const LevelDataError& e : errors) {
        if (!out.empty()) out += '\n';
        out += e.to_string();
    }
    return out;
}

LevelParseResult LevelData::parse(std::string_view text) noexcept {
    LevelParseResult result;
    try {
        std::vector<LevelDataError> duplicates;
        const json root = json::parse(text.begin(), text.end(), DuplicateKeyCheck(duplicates));
        Validator validator;
        validator.errors = std::move(duplicates);
        LevelData level = validator.validate(root);
        result.errors = std::move(validator.errors);
        if (result.errors.empty()) result.level = std::move(level);
    } catch (const json::parse_error& e) {
        result.level.reset();
        result.errors = {{"", "invalid JSON: " + strip_exception_id(e.what())}};
    } catch (const json::exception& e) {
        result.level.reset();
        result.errors = {{"", "could not read level data: " + strip_exception_id(e.what())}};
    } catch (const std::exception& e) {
        result.level.reset();
        result.errors = {{"", std::string("could not read level data: ") + e.what()}};
    } catch (...) {
        result.level.reset();
        result.errors = {{"", "could not read level data"}};
    }
    return result;
}

LevelData LevelData::load(std::string_view json) {
    LevelParseResult result = parse(json);
    if (!result.ok()) throw std::invalid_argument("Invalid level data:\n" + result.error_text());
    return std::move(*result.level);
}

LevelGoal LevelData::goal() const { return LevelGoal{gem_goal.has_value(), !quotas.empty()}; }

double LevelData::time_limit_for(Difficulty difficulty, int timer_tier) const {
    // rule: <tbd>
    if (timer_tier < kMinTimerTier || timer_tier > kMaxTimerTier)
        throw std::invalid_argument("timer_tier must be between " + std::to_string(kMinTimerTier) + " and " +
                                    std::to_string(kMaxTimerTier) + ", got " + std::to_string(timer_tier));
    if (time_limit) return difficulty == Difficulty::Hard ? time_limit->hard : time_limit->normal;
    return kBaseTimerSeconds + kTimerTierStepSeconds * timer_tier;
}

LevelState LevelData::make_state(Difficulty difficulty, int timer_tier) const {
    const double seconds = time_limit_for(difficulty, timer_tier);
    // LevelState tracks quotas only; gems arrive in a later core change.
    if (quotas.empty()) throw std::invalid_argument("gem goals need the gem rules (coming)");
    return LevelState(quotas, seconds);
}

}  // namespace montezuma

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

constexpr std::array<std::string_view, 9> kTopLevelFields{
    "id", "episode", "index", "board", "quotas", "time_limit", "totems_unlocked", "blocked_cells", "seed"};
constexpr std::array<std::string_view, 3> kBoardFields{"width", "height", "kinds"};
constexpr std::array<std::string_view, 2> kTimeLimitFields{"relaxed", "normal"};

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
        read_quotas(root, board_ok, level);
        read_time_limit(root, level.time_limit);
        read_totems(root, level.totems_unlocked);
        read_blocked_cells(root, board_ok, level);
        read_seed(root, level);
        return level;
    }

private:
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

    void read_quotas(const json& root, bool board_ok, LevelData& level) {
        const json* j = field(root, "", "quotas", true);
        if (!j) return;
        if (!j->is_object()) {
            error("quotas", "expected an object of kind name to count, got " + describe(*j));
            return;
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
    }

    void read_time_limit(const json& root, TimeLimit& limit) {
        const json* j = field(root, "", "time_limit", true);
        if (!j) return;
        if (!j->is_object()) {
            error("time_limit", "expected an object with relaxed and normal, got " + describe(*j));
            return;
        }
        reject_unknown(*j, "time_limit", kTimeLimitFields);
        auto read = [&](std::string_view key, double& out) {
            const json* v = field(*j, "time_limit", key, true);
            if (!v) return;
            const std::string path = child("time_limit", key);
            if (!v->is_number()) {
                error(path, "expected a number of seconds, got " + describe(*v));
                return;
            }
            const double seconds = v->get<double>();
            if (!std::isfinite(seconds) || seconds <= 0.0) {
                error(path, "must be greater than 0 seconds, got " + describe(*v));
                return;
            }
            out = seconds;
        };
        read("relaxed", limit.relaxed);
        read("normal", limit.normal);
    }

    void read_totems(const json& root, std::vector<std::string>& totems) {
        const json* j = field(root, "", "totems_unlocked", true);
        if (!j) return;
        if (!j->is_array()) {
            error("totems_unlocked", "expected an array of totem ids, got " + describe(*j));
            return;
        }
        for (std::size_t i = 0; i < j->size(); ++i) {
            const json& v = (*j)[i];
            const std::string path = element("totems_unlocked", i);
            if (!v.is_string()) {
                error(path, "expected a totem id string, got " + describe(v));
                continue;
            }
            const auto id = v.get<std::string>();
            if (!contains(kTotemIds, id)) {
                error(path, "unknown totem \"" + id + "\" (expected one of " + join(kTotemIds) + ")");
                continue;
            }
            if (std::find(totems.begin(), totems.end(), id) != totems.end()) {
                error(path, "duplicate totem \"" + id + "\"");
                continue;
            }
            totems.push_back(id);
        }
    }

    void read_blocked_cells(const json& root, bool board_ok, LevelData& level) {
        const json* j = field(root, "", "blocked_cells", false);
        if (!j) return;
        if (!j->is_array()) {
            error("blocked_cells", "expected an array of [x, y] pairs, got " + describe(*j));
            return;
        }
        std::map<std::pair<int, int>, std::size_t> seen;  // cell -> first index
        for (std::size_t i = 0; i < j->size(); ++i) {
            const json& v = (*j)[i];
            const std::string path = element("blocked_cells", i);
            if (!v.is_array() || v.size() != 2) {
                error(path, "expected [x, y], got " + (v.is_array() ? "an array of " + std::to_string(v.size()) : describe(v)));
                continue;
            }
            // With an invalid board only the sign can be checked.
            const std::optional<int> width = board_ok ? std::optional<int>(level.board.width) : std::nullopt;
            const std::optional<int> height = board_ok ? std::optional<int>(level.board.height) : std::nullopt;
            const std::optional<int> x = coordinate(v[0], element(path, 0), "x", width);
            const std::optional<int> y = coordinate(v[1], element(path, 1), "y", height);
            if (!x || !y) continue;
            const auto [it, inserted] = seen.emplace(std::make_pair(*x, *y), i);
            if (!inserted) {
                error(path, "duplicate of " + element("blocked_cells", it->second));
                continue;
            }
            level.blocked_cells.push_back({*x, *y});
        }
    }

    std::optional<int> coordinate(const json& j, const std::string& path, std::string_view axis,
                                  std::optional<int> size) {
        const std::optional<std::int64_t> v = integer(j, path);
        if (!v) return std::nullopt;
        if (!size) {
            if (*v >= 0 && *v <= INT_MAX) return static_cast<int>(*v);
            error(path, std::string(axis) + " " + std::to_string(*v) + " is outside the board");
            return std::nullopt;
        }
        if (*v < 0 || *v >= *size) {
            error(path, std::string(axis) + " " + std::to_string(*v) + " is outside the board (0 to " +
                            std::to_string(*size - 1) + ")");
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

// nlohmann messages start with "[json.exception.parse_error.101] "; drop it.
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
    if (name == "relaxed") return Difficulty::Relaxed;
    if (name == "normal") return Difficulty::Normal;
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

double LevelData::time_limit_for(Difficulty difficulty) const {
    return difficulty == Difficulty::Relaxed ? time_limit.relaxed : time_limit.normal;
}

LevelState LevelData::make_state(Difficulty difficulty) const { return LevelState(quotas, time_limit_for(difficulty)); }

}  // namespace montezuma

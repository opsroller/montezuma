// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// LevelData parser tests. No framework dependency; run via ctest.
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "montezuma/level_data.hpp"

using namespace montezuma;

namespace {

int checks = 0;
int failures = 0;

void check(bool cond, const std::string& label) {
    ++checks;
    if (!cond) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", label.c_str());
    }
}

// A complete, valid level as top-level field -> raw JSON value.
using Fields = std::vector<std::pair<std::string, std::string>>;

const Fields kValid = {
    {"id", R"("e2-l07")"},
    {"episode", "2"},
    {"index", "7"},
    {"board", R"({"width": 9, "height": 7, "kinds": 6})"},
    {"gem_goal", R"({"normal": 40, "hard": 60})"},
    {"quotas", R"({"red": 10, "blue": 8, "purple": 3})"},
    {"base_clock", R"({"normal": 240, "hard": 150.5})"},
    {"totems_unlocked", R"(["fire", "time"])"},
    {"powerups_unlocked", R"(["tnt", "hint"])"},
    {"steel_rate", "0.25"},
    {"blocked_cells", "[[0, 0], [8, 6]]"},
    {"chained_cells", "[[1, 1], [2, 3]]"},
    {"rules", R"("classic")"},
    {"theme", R"("jungle_2")"},
    {"seed", "12345"},
};

// kValid with fields replaced; an empty value removes the field. A field not
// in kValid is appended.
std::string level(const Fields& changes = {}) {
    Fields fields = kValid;
    for (const auto& [key, value] : changes) {
        bool found = false;
        for (auto& f : fields)
            if (f.first == key) {
                f.second = value;
                found = true;
            }
        if (!found) fields.emplace_back(key, value);
    }
    std::string out = "{";
    for (const auto& [key, value] : fields) {
        if (value.empty()) continue;
        if (out.size() > 1) out += ", ";
        out += "\"" + key + "\": " + value;
    }
    return out + "}";
}

bool has_error(const LevelParseResult& r, const std::string& path, const std::string& fragment) {
    for (const auto& e : r.errors)
        if (e.path == path && e.message.find(fragment) != std::string::npos) return true;
    return false;
}

void expect_error(const std::string& json, const std::string& path, const std::string& fragment) {
    const LevelParseResult r = LevelData::parse(json);
    const bool ok = !r.ok() && has_error(r, path, fragment);
    check(ok, "error '" + path + ": ..." + fragment + "...' for " + json);
    if (!ok && !r.errors.empty()) std::fprintf(stderr, "  got:\n%s\n", r.error_text().c_str());
}

void test_valid_full_level_parses() {
    const LevelParseResult r = LevelData::parse(level());
    check(r.ok() && r.errors.empty(), "full level parses");
    if (!r.ok()) {
        std::fprintf(stderr, "%s\n", r.error_text().c_str());
        return;
    }
    const LevelData& l = *r.level;
    check(l.id == "e2-l07", "id read");
    check(l.episode == 2 && l.index == 7, "episode and index read");
    check(l.board.width == 9 && l.board.height == 7 && l.board.kinds == 6, "board read");
    check(l.gem_goal && l.gem_goal->normal == 40 && l.gem_goal->hard == 60, "gem goal read");
    check(l.quotas == std::map<int, int>{{0, 10}, {4, 8}, {5, 3}}, "quotas mapped to kind indices");
    check(l.base_clock && l.base_clock->normal == 240.0 && l.base_clock->hard == 150.5,
          "base clocks read, int and float");
    check(l.totems_unlocked == std::vector<std::string>{"fire", "time"}, "totems read in order");
    check(l.powerups_unlocked == std::vector<std::string>{"tnt", "hint"}, "power-ups read in order");
    check(l.steel_rate == 0.25, "steel rate read");
    check(l.blocked_cells.size() == 2 && l.blocked_cells[0] == Cell{0, 0} && l.blocked_cells[1] == Cell{8, 6},
          "blocked cells read");
    check(l.chained_cells.size() == 2 && l.chained_cells[0] == Cell{1, 1} && l.chained_cells[1] == Cell{2, 3},
          "chained cells read");
    check(l.rules == "classic" && l.theme == "jungle_2", "rules and theme read");
    check(l.seed == 12345u, "seed read");
    check(l.goal().gems && l.goal().quotas, "both goal kinds reported");
}

void test_defaults_apply() {
    const LevelParseResult r = LevelData::parse(level({{"board", ""}, {"blocked_cells", ""}}));
    check(r.ok(), "level without board or blocked_cells parses");
    if (r.ok()) {
        check(r.level->board.width == 8 && r.level->board.height == 8 && r.level->board.kinds == 7,
              "board defaults to 8x8 with 7 kinds");
        check(r.level->blocked_cells.empty(), "blocked_cells defaults to empty");
    }
    const LevelParseResult partial = LevelData::parse(level({{"board", R"({"kinds": 5})"}, {"quotas", R"({"red": 1})"}, {"blocked_cells", "[[7, 7]]"}}));
    check(partial.ok() && partial.level->board.width == 8 && partial.level->board.height == 8 &&
              partial.level->board.kinds == 5,
          "missing board fields default individually");
    const LevelParseResult empty = LevelData::parse(level({{"board", "{}"}, {"blocked_cells", "[]"}}));
    check(empty.ok() && empty.level->board.kinds == 7, "empty board object and empty blocked_cells accepted");
    const LevelParseResult no_totems = LevelData::parse(level({{"totems_unlocked", "[]"}}));
    check(no_totems.ok() && no_totems.level->totems_unlocked.empty(), "empty totems_unlocked accepted");

    const LevelParseResult minimal = LevelData::parse(level({{"quotas", ""},
                                                              {"base_clock", ""},
                                                              {"powerups_unlocked", ""},
                                                              {"steel_rate", ""},
                                                              {"chained_cells", ""},
                                                              {"rules", ""},
                                                              {"theme", ""}}));
    check(minimal.ok(), "level without the optional designer fields parses");
    if (minimal.ok()) {
        const LevelData& l = *minimal.level;
        check(l.quotas.empty() && !l.base_clock, "quotas and base_clock default to absent");
        check(l.powerups_unlocked.empty(), "powerups_unlocked defaults to empty");
        check(l.steel_rate == 0.0, "steel_rate defaults to 0");
        check(l.chained_cells.empty(), "chained_cells defaults to empty");
        check(l.rules == "classic" && l.theme == "temple", "rules and theme default to classic and temple");
    }
    const LevelParseResult empties = LevelData::parse(level({{"powerups_unlocked", "[]"}, {"chained_cells", "[]"}}));
    check(empties.ok() && empties.level->powerups_unlocked.empty() && empties.level->chained_cells.empty(),
          "empty powerups_unlocked and chained_cells accepted");
}

void test_goal_kinds() {
    const LevelParseResult gems = LevelData::parse(level({{"quotas", ""}}));
    check(gems.ok() && gems.level->goal().gems && !gems.level->goal().quotas, "gem goal alone accepted");
    const LevelParseResult quotas = LevelData::parse(level({{"gem_goal", ""}}));
    check(quotas.ok() && !quotas.level->goal().gems && quotas.level->goal().quotas && !quotas.level->gem_goal,
          "quotas alone accepted");
    expect_error(level({{"gem_goal", ""}, {"quotas", ""}}), "", "needs a goal: gem_goal, quotas or both");
    // A present but broken goal reports its own error, not a missing goal.
    const LevelParseResult bad = LevelData::parse(level({{"gem_goal", "5"}, {"quotas", ""}}));
    check(!bad.ok() && bad.errors.size() == 1 && has_error(bad, "gem_goal", "expected an object"),
          "broken gem_goal is not also a missing goal");
    const LevelParseResult bad_quotas = LevelData::parse(level({{"gem_goal", ""}, {"quotas", "{}"}}));
    check(!bad_quotas.ok() && bad_quotas.errors.size() == 1 && has_error(bad_quotas, "quotas", "at least one kind"),
          "empty quotas is not also a missing goal");
}

void test_gem_goal_errors() {
    expect_error(level({{"gem_goal", "40"}}), "gem_goal", "expected an object with normal and hard, got 40");
    expect_error(level({{"gem_goal", R"({"normal": 40})"}}), "gem_goal.hard", "required field missing");
    expect_error(level({{"gem_goal", R"({"hard": 40})"}}), "gem_goal.normal", "required field missing");
    expect_error(level({{"gem_goal", R"({"normal": 0, "hard": 5})"}}), "gem_goal.normal", "must be at least 1, got 0");
    expect_error(level({{"gem_goal", R"({"normal": 5, "hard": -1})"}}), "gem_goal.hard", "must be at least 1");
    expect_error(level({{"gem_goal", R"({"normal": 5, "hard": 2.5})"}}), "gem_goal.hard", "expected an integer");
    expect_error(level({{"gem_goal", R"({"normal": 5, "hard": 6, "relaxed": 3})"}}), "gem_goal.relaxed",
                 "unknown field");
    check(LevelData::parse(level({{"gem_goal", R"({"normal": 1, "hard": 1})"}})).ok(), "gem goal of 1 accepted");
}

void test_powerup_errors() {
    expect_error(level({{"powerups_unlocked", R"("tnt")"}}), "powerups_unlocked", "expected an array of power-up ids");
    expect_error(level({{"powerups_unlocked", R"(["tnt", "laser"])"}}), "powerups_unlocked[1]",
                 "unknown power-up \"laser\" (expected one of tnt, color_clear, lightning, clock, multiplier, hint)");
    expect_error(level({{"powerups_unlocked", "[1]"}}), "powerups_unlocked[0]", "expected a power-up id string");
    expect_error(level({{"powerups_unlocked", R"(["clock", "hint", "clock"])"}}), "powerups_unlocked[2]",
                 "duplicate power-up \"clock\"");
    check(LevelData::parse(level({{"powerups_unlocked",
                                   R"(["tnt", "color_clear", "lightning", "clock", "multiplier", "hint"])"}}))
              .ok(),
          "every power-up id accepted");
}

void test_steel_rate() {
    for (const char* ok : {"0", "1", "0.0", "1.0", "0.5"})
        check(LevelData::parse(level({{"steel_rate", ok}})).ok(), std::string("steel_rate ") + ok + " accepted");
    expect_error(level({{"steel_rate", "-0.01"}}), "steel_rate", "must be between 0 and 1, got -0.01");
    expect_error(level({{"steel_rate", "1.01"}}), "steel_rate", "must be between 0 and 1, got 1.01");
    expect_error(level({{"steel_rate", "2"}}), "steel_rate", "must be between 0 and 1, got 2");
    expect_error(level({{"steel_rate", R"("0.1")"}}), "steel_rate", "expected a number from 0 to 1, got string");
    expect_error(level({{"steel_rate", "null"}}), "steel_rate", "expected a number");
}

void test_chained_cells() {
    expect_error(level({{"chained_cells", R"({"x": 1})"}}), "chained_cells", "expected an array of [x, y] pairs");
    expect_error(level({{"chained_cells", "[[1]]"}}), "chained_cells[0]", "expected [x, y], got an array of 1");
    expect_error(level({{"chained_cells", "[[9, 0]]"}}), "chained_cells[0][0]", "x 9 is outside the board (0 to 8)");
    expect_error(level({{"chained_cells", "[[0, 7]]"}}), "chained_cells[0][1]", "y 7 is outside the board (0 to 6)");
    expect_error(level({{"chained_cells", "[[1, 2], [1, 2]]"}}), "chained_cells[1]", "duplicate of chained_cells[0]");
    // kValid blocks [0, 0] and [8, 6].
    expect_error(level({{"chained_cells", "[[3, 3], [8, 6]]"}}), "chained_cells[1]",
                 "cell is also blocked (blocked_cells[1])");
    const LevelParseResult both = LevelData::parse(level({{"chained_cells", "[[0, 0], [8, 6]]"}}));
    check(!both.ok() && both.errors.size() == 2, "each chained-and-blocked cell reported");
}

void test_rules_and_theme() {
    for (const char* key : {"rules", "theme"}) {
        for (const char* ok : {R"("classic")", R"("a")", R"("rule_set_2")", R"("_")", R"("0")"})
            check(LevelData::parse(level({{key, ok}})).ok(), std::string(key) + " " + ok + " accepted");
        for (const char* bad : {R"("")", R"("Classic")", R"("two words")", R"("dash-ed")", R"("dot.ted")",
                                R"("café")"})
            expect_error(level({{key, bad}}), key, "expected a non-empty id of a-z, 0-9 and _");
        expect_error(level({{key, "3"}}), key, "expected a string, got 3");
        expect_error(level({{key, "null"}}), key, "expected a string, got null");
    }
}

void test_boundary_values_accepted() {
    check(LevelData::parse(level({{"board", R"({"width": 3, "height": 3, "kinds": 3})"},
                                  {"quotas", R"({"yellow": 1})"},
                                  {"blocked_cells", "[[2, 2]]"},
                                  {"chained_cells", "[[0, 1]]"}}))
              .ok(),
          "smallest board (3x3, 3 kinds) accepted");
    check(LevelData::parse(level({{"board", R"({"kinds": 7})"}, {"quotas", R"({"white": 1})"}, {"blocked_cells", ""}})).ok(),
          "white (kind 6) accepted with 7 kinds");
    check(LevelData::parse(level({{"id", R"("e12-l103")"}, {"episode", "12"}, {"index", "103"}})).ok(),
          "multi-digit episode and three-digit index accepted");
    check(LevelData::parse(level({{"seed", "0"}})).ok(), "seed 0 accepted");
    check(LevelData::parse(level({{"seed", "18446744073709551615"}})).ok(), "largest uint64 seed accepted");
    check(LevelData::parse(level({{"base_clock", R"({"normal": 0.5, "hard": 0.25})"}})).ok(),
          "fractional base clocks accepted");
    check(LevelData::parse("\n  " + level() + "\n").ok(), "surrounding whitespace accepted");
}

void test_document_errors() {
    expect_error("", "", "invalid JSON");
    expect_error("{\"id\": ", "", "invalid JSON");
    expect_error(level() + " trailing", "", "invalid JSON");
    expect_error("// comment\n" + level(), "", "invalid JSON");
    // nlohmann reports number overflow as out_of_range, not parse_error; its id is stripped too.
    {
        const LevelParseResult r = LevelData::parse(level({{"steel_rate", "1e400"}}));
        check(!r.ok() && r.errors.size() == 1 && r.errors[0].message.rfind("could not read level data: ", 0) == 0 &&
                  r.errors[0].message.find("[json.exception") == std::string::npos,
              "json::exception message has its id stripped: " + r.error_text());
    }
    expect_error("[]", "", "expected an object at the top level, got array");
    expect_error("42", "", "expected an object at the top level");
    expect_error(level({{"extra", "1"}}), "extra", "unknown field");
    expect_error("{\"seed\": 3, " + level().substr(1), "", "duplicate key \"seed\"");
    expect_error(level({{"board", R"({"width": 8, "width": 9})"}}), "", "duplicate key \"width\"");
}

void test_required_fields() {
    for (const char* key : {"id", "episode", "index", "totems_unlocked", "seed"})
        expect_error(level({{key, ""}}), key, "required field missing");
    expect_error(level({{"base_clock", R"({"normal": 60})"}}), "base_clock.hard", "required field missing");
    expect_error(level({{"base_clock", R"({"hard": 60})"}}), "base_clock.normal", "required field missing");
}

void test_id_errors() {
    expect_error(level({{"id", "3"}}), "id", "expected a string");
    for (const char* bad : {R"("")", R"("e2-7")", R"("e2-l7")", R"("E2-l07")", R"("e02-l07")", R"("e-l07")",
                            R"("e2-l0x")", R"("e2_l07")", R"("level7")"})
        expect_error(level({{"id", bad}}), "id", "expected the form e<N>-l<NN>");
    expect_error(level({{"id", R"("e2-l08")"}}), "id", "does not match episode 2 and index 7 (expected \"e2-l07\")");
    expect_error(level({{"id", R"("e3-l07")"}}), "id", "does not match");
}

void test_episode_and_index_errors() {
    expect_error(level({{"episode", "0"}}), "episode", "must be at least 1, got 0");
    expect_error(level({{"index", "-1"}}), "index", "must be at least 1, got -1");
    expect_error(level({{"episode", "1.5"}}), "episode", "expected an integer, got 1.5");
    expect_error(level({{"index", R"("7")"}}), "index", "expected an integer, got string");
    expect_error(level({{"episode", "true"}}), "episode", "expected an integer, got true");
    expect_error(level({{"episode", "null"}}), "episode", "expected an integer, got null");
    expect_error(level({{"episode", "3000000000"}}), "episode", "too large, got 3000000000");
}

void test_board_errors() {
    expect_error(level({{"board", "[8, 8, 7]"}}), "board", "expected an object, got array");
    expect_error(level({{"board", R"({"width": 2})"}}), "board.width", "must be between 3 and 64, got 2");
    expect_error(level({{"board", R"({"height": 65})"}}), "board.height", "must be between 3 and 64, got 65");
    expect_error(level({{"board", R"({"kinds": 2})"}}), "board.kinds", "must be between 3 and 7, got 2");
    expect_error(level({{"board", R"({"kinds": 8})"}}), "board.kinds", "must be between 3 and 7, got 8");
    expect_error(level({{"board", R"({"width": 8.0})"}}), "board.width", "expected an integer");
    expect_error(level({{"board", R"({"depth": 3})"}}), "board.depth", "unknown field");
}

void test_quota_errors() {
    expect_error(level({{"quotas", "[10, 8]"}}), "quotas", "expected an object");
    expect_error(level({{"quotas", "{}"}}), "quotas", "must list at least one kind");
    expect_error(level({{"quotas", R"({"pink": 3})"}}), "quotas.pink", "unknown kind");
    expect_error(level({{"quotas", R"({"Red": 3})"}}), "quotas.Red", "unknown kind");
    expect_error(level({{"quotas", R"({"red": 0})"}}), "quotas.red", "must be at least 1, got 0");
    expect_error(level({{"quotas", R"({"red": -4})"}}), "quotas.red", "must be at least 1");
    expect_error(level({{"quotas", R"({"red": 2.5})"}}), "quotas.red", "expected an integer");
    expect_error(level({{"quotas", R"({"red": "10"})"}}), "quotas.red", "expected an integer");
    // kValid's board has 6 kinds (indices 0..5), so white (6) is off the board.
    expect_error(level({{"quotas", R"({"white": 5})"}}), "quotas.white", "kind 6 is not on a board with 6 kinds");
    expect_error(level({{"board", R"({"kinds": 3})"}, {"quotas", R"({"green": 5})"}}), "quotas.green",
                 "kind 3 is not on a board with 3 kinds");
}

void test_base_clock_errors() {
    expect_error(level({{"base_clock", "120"}}), "base_clock", "expected an object with normal and hard");
    expect_error(level({{"base_clock", R"({"normal": 0, "hard": 60})"}}), "base_clock.normal",
                 "must be greater than 0 seconds, got 0");
    expect_error(level({{"base_clock", R"({"normal": 60, "hard": -1.5})"}}), "base_clock.hard",
                 "must be greater than 0 seconds");
    expect_error(level({{"base_clock", R"({"normal": "60", "hard": 60})"}}), "base_clock.normal",
                 "expected a number of seconds, got string");
    expect_error(level({{"base_clock", R"({"relaxed": 60, "normal": 60, "hard": 30})"}}), "base_clock.relaxed",
                 "unknown field");
    // The old override replaced the timer tier; it is gone, with a pointer to its successor.
    expect_error(level({{"base_clock", ""}, {"time_limit", R"({"normal": 240, "hard": 150})"}}), "time_limit",
                 "replaced by base_clock, which the timer tier adds to (GOAL-03)");
}

void test_totem_errors() {
    expect_error(level({{"totems_unlocked", R"("time")"}}), "totems_unlocked", "expected an array");
    expect_error(level({{"totems_unlocked", R"(["time", "laser"])"}}), "totems_unlocked[1]",
                 "unknown totem \"laser\" (expected one of fire, time, gold, gift, harvest, paint, ward)");
    expect_error(level({{"totems_unlocked", "[3]"}}), "totems_unlocked[0]", "expected a totem id string");
    expect_error(level({{"totems_unlocked", R"(["time", "fire", "time"])"}}), "totems_unlocked[2]",
                 "duplicate totem \"time\"");
    // Ids from the earlier draft are gone.
    expect_error(level({{"totems_unlocked", R"(["row_clear"])"}}), "totems_unlocked[0]", "unknown totem");
    check(LevelData::parse(level({{"totems_unlocked", R"(["fire", "time", "gold", "gift", "harvest", "paint", "ward"])"}}))
              .ok(),
          "every totem id accepted");
}

void test_blocked_cell_errors() {
    expect_error(level({{"blocked_cells", R"({"x": 1})"}}), "blocked_cells", "expected an array");
    expect_error(level({{"blocked_cells", "[[1]]"}}), "blocked_cells[0]", "expected [x, y], got an array of 1");
    expect_error(level({{"blocked_cells", "[[1, 2, 3]]"}}), "blocked_cells[0]", "expected [x, y]");
    expect_error(level({{"blocked_cells", R"([[0, 0], {"x": 1, "y": 2}])"}}), "blocked_cells[1]", "expected [x, y]");
    expect_error(level({{"blocked_cells", "[[1.5, 2]]"}}), "blocked_cells[0][0]", "expected an integer");
    // kValid's board is 9 wide and 7 high.
    expect_error(level({{"blocked_cells", "[[9, 0]]"}}), "blocked_cells[0][0]", "x 9 is outside the board (0 to 8)");
    expect_error(level({{"blocked_cells", "[[0, 7]]"}}), "blocked_cells[0][1]", "y 7 is outside the board (0 to 6)");
    expect_error(level({{"blocked_cells", "[[-1, 0]]"}}), "blocked_cells[0][0]", "outside the board");
    expect_error(level({{"blocked_cells", "[[1, 2], [3, 4], [1, 2]]"}}), "blocked_cells[2]",
                 "duplicate of blocked_cells[0]");
    expect_error(level({{"board", ""}, {"blocked_cells", "[[8, 0]]"}}), "blocked_cells[0][0]",
                 "outside the board (0 to 7)");
}

void test_seed_errors() {
    expect_error(level({{"seed", "-1"}}), "seed", "must not be negative");
    expect_error(level({{"seed", "1.5"}}), "seed", "expected an integer");
    expect_error(level({{"seed", R"("abc")"}}), "seed", "expected an integer, got string");
}

void test_all_errors_are_reported_together() {
    const LevelParseResult r = LevelData::parse(
        level({{"episode", "0"}, {"quotas", R"({"pink": 1})"}, {"seed", ""}, {"extra", "true"}}));
    check(!r.ok() && r.errors.size() == 4, "four independent problems give four errors");
    check(has_error(r, "episode", "at least 1") && has_error(r, "quotas.pink", "unknown kind") &&
              has_error(r, "seed", "required") && has_error(r, "extra", "unknown field"),
          "each problem named by its path");
    check(r.error_text().find("quotas.pink: unknown kind") != std::string::npos, "error_text joins path: message");
    check(LevelDataError{"", "invalid JSON"}.to_string() == "invalid JSON", "error without path prints message only");
}

void test_invalid_board_skips_dependent_checks() {
    // With kinds out of range, quota and blocked-cell bounds are not judged against it.
    const LevelParseResult r = LevelData::parse(level({{"board", R"({"kinds": 9, "width": 1})"}}));
    check(!r.ok() && r.errors.size() == 2, "only the two board errors reported");
    expect_error(level({{"board", R"({"width": 1})"}, {"blocked_cells", "[[-2, 0]]"}}), "blocked_cells[0][0]",
                 "x -2 is outside the board");
}

template <typename F>
std::string invalid_argument_message(F&& f) {
    try {
        f();
    } catch (const std::invalid_argument& e) {
        return e.what();
    }
    return "<no throw>";
}

void test_timer_tiers() {
    const LevelData untimed = LevelData::load(level({{"base_clock", ""}}));
    check(untimed.time_limit_for(Difficulty::Normal) == 270.0 && untimed.time_limit_for(Difficulty::Hard) == 270.0,
          "tier 0 is the default: 270 s");
    const double expected[] = {270.0, 300.0, 330.0, 360.0, 390.0};
    for (int tier = 0; tier <= 4; ++tier)
        check(untimed.time_limit_for(Difficulty::Normal, tier) == expected[tier] &&
                  untimed.make_state(Difficulty::Hard, tier).time_left() == expected[tier],
              "tier " + std::to_string(tier) + " time");

    check(untimed.base_clock_for(Difficulty::Normal) == 270.0 && untimed.base_clock_for(Difficulty::Hard) == 270.0,
          "base clock defaults to 270 s");

    // GOAL-03: the tier's bonus goes on top of the level's base clock, so a
    // bought timer upgrade keeps its value on every level.
    const LevelData timed = LevelData::load(level());
    check(timed.base_clock_for(Difficulty::Normal) == 240.0 && timed.base_clock_for(Difficulty::Hard) == 150.5,
          "base_clock_for reads the level's clock");
    check(timed.time_limit_for(Difficulty::Normal, 4) == 360.0 && timed.time_limit_for(Difficulty::Hard, 3) == 240.5,
          "the timer tier adds to base_clock");
    const LevelData long_clock = LevelData::load(level({{"base_clock", R"({"normal": 200, "hard": 300})"}}));
    check(long_clock.time_limit_for(Difficulty::Normal, 2) == 260.0 && long_clock.time_limit_for(Difficulty::Hard, 4) == 420.0,
          "tier 2 normal and tier 4 hard on a 200/300 clock");
    for (int tier = 0; tier <= 4; ++tier)
        check(long_clock.time_limit_for(Difficulty::Hard, tier) - long_clock.time_limit_for(Difficulty::Hard, 0) ==
                  30.0 * tier,
              "tier " + std::to_string(tier) + " adds " + std::to_string(30 * tier) + " s");

    for (int bad : {-1, 5, 100}) {
        const std::string expect = "timer_tier must be between 0 and 4, got " + std::to_string(bad);
        check(invalid_argument_message([&] { (void)untimed.time_limit_for(Difficulty::Normal, bad); }) == expect,
              "out-of-range tier " + std::to_string(bad) + " throws");
        check(invalid_argument_message([&] { (void)timed.time_limit_for(Difficulty::Normal, bad); }) == expect,
              "out-of-range tier " + std::to_string(bad) + " throws even with base_clock");
        check(invalid_argument_message([&] { (void)timed.make_state(Difficulty::Normal, bad); }) == expect,
              "make_state rejects tier " + std::to_string(bad));
    }
}

void test_load_and_level_state() {
    const LevelData l = LevelData::load(level());
    check(l.time_limit_for(Difficulty::Normal) == 240.0, "normal base clock selected");
    check(l.time_limit_for(Difficulty::Hard) == 150.5, "hard base clock selected");
    const LevelState normal = l.make_state(Difficulty::Normal);
    const LevelState hard = l.make_state(Difficulty::Hard);
    check(normal.time_left() == 240.0 && hard.time_left() == 150.5, "level state uses the difficulty's timer");
    check(hard.quotas() == l.quotas && hard.outcome() == LevelState::Outcome::Playing,
          "level state starts with the level's quotas");

    const LevelData quotas_only = LevelData::load(level({{"gem_goal", ""}}));
    check(quotas_only.make_state(Difficulty::Normal).quotas() == quotas_only.quotas, "quota-only level makes a state");
    const LevelData gems_only = LevelData::load(level({{"quotas", ""}}));
    check(invalid_argument_message([&] { (void)gems_only.make_state(Difficulty::Normal); }) ==
              "gem goals need the gem rules (coming)",
          "gem-only level cannot make a state yet");

    bool threw = false;
    try {
        LevelData::load(level({{"quotas", R"({"pink": 1})"}}));
    } catch (const std::invalid_argument& e) {
        threw = std::string(e.what()).find("quotas.pink: unknown kind") != std::string::npos;
    }
    check(threw, "load throws invalid_argument naming the error");
}

void test_name_helpers() {
    check(kind_index("red") == 0 && kind_index("white") == 6 && !kind_index("pink"), "kind names map to indices");
    for (int i = 0; i < static_cast<int>(kKindNames.size()); ++i)
        check(kind_index(kKindNames[static_cast<std::size_t>(i)]) == i, "kind name round trip " + std::to_string(i));
    check(difficulty_from_name("normal") == Difficulty::Normal && difficulty_from_name("hard") == Difficulty::Hard &&
              !difficulty_from_name("relaxed") && !difficulty_from_name("adventure") && !difficulty_from_name("Hard"),
          "difficulty names map");
}

void test_parse_is_deterministic() {
    const LevelParseResult a = LevelData::parse(level({{"quotas", R"({"pink": 1, "teal": 2})"}, {"extra", "1"}}));
    const LevelParseResult b = LevelData::parse(level({{"quotas", R"({"pink": 1, "teal": 2})"}, {"extra", "1"}}));
    check(a.error_text() == b.error_text() && !a.errors.empty(), "same input gives the same errors in the same order");
}

}  // namespace

int main() {
    test_valid_full_level_parses();
    test_defaults_apply();
    test_goal_kinds();
    test_gem_goal_errors();
    test_powerup_errors();
    test_steel_rate();
    test_chained_cells();
    test_rules_and_theme();
    test_boundary_values_accepted();
    test_document_errors();
    test_required_fields();
    test_id_errors();
    test_episode_and_index_errors();
    test_board_errors();
    test_quota_errors();
    test_base_clock_errors();
    test_totem_errors();
    test_blocked_cell_errors();
    test_seed_errors();
    test_all_errors_are_reported_together();
    test_invalid_board_skips_dependent_checks();
    test_timer_tiers();
    test_load_and_level_state();
    test_name_helpers();
    test_parse_is_deterministic();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

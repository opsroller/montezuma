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
    {"quotas", R"({"red": 10, "blue": 8, "purple": 3})"},
    {"time_limit", R"({"relaxed": 240, "normal": 150.5})"},
    {"totems_unlocked", R"(["row_clear", "time"])"},
    {"blocked_cells", "[[0, 0], [8, 6]]"},
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
    check(l.quotas == std::map<int, int>{{0, 10}, {4, 8}, {5, 3}}, "quotas mapped to kind indices");
    check(l.time_limit.relaxed == 240.0 && l.time_limit.normal == 150.5, "time limits read, int and float");
    check(l.totems_unlocked == std::vector<std::string>{"row_clear", "time"}, "totems read in order");
    check(l.blocked_cells.size() == 2 && l.blocked_cells[0] == Cell{0, 0} && l.blocked_cells[1] == Cell{8, 6},
          "blocked cells read");
    check(l.seed == 12345u, "seed read");
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
}

void test_boundary_values_accepted() {
    check(LevelData::parse(level({{"board", R"({"width": 3, "height": 3, "kinds": 3})"},
                                  {"quotas", R"({"yellow": 1})"},
                                  {"blocked_cells", "[[2, 2]]"}}))
              .ok(),
          "smallest board (3x3, 3 kinds) accepted");
    check(LevelData::parse(level({{"board", R"({"kinds": 7})"}, {"quotas", R"({"white": 1})"}, {"blocked_cells", ""}})).ok(),
          "white (kind 6) accepted with 7 kinds");
    check(LevelData::parse(level({{"id", R"("e12-l103")"}, {"episode", "12"}, {"index", "103"}})).ok(),
          "multi-digit episode and three-digit index accepted");
    check(LevelData::parse(level({{"seed", "0"}})).ok(), "seed 0 accepted");
    check(LevelData::parse(level({{"seed", "18446744073709551615"}})).ok(), "largest uint64 seed accepted");
    check(LevelData::parse(level({{"time_limit", R"({"relaxed": 0.5, "normal": 0.25})"}})).ok(),
          "fractional time limits accepted");
    check(LevelData::parse("\n  " + level() + "\n").ok(), "surrounding whitespace accepted");
}

void test_document_errors() {
    expect_error("", "", "invalid JSON");
    expect_error("{\"id\": ", "", "invalid JSON");
    expect_error(level() + " trailing", "", "invalid JSON");
    expect_error("// comment\n" + level(), "", "invalid JSON");
    expect_error("[]", "", "expected an object at the top level, got array");
    expect_error("42", "", "expected an object at the top level");
    expect_error(level({{"extra", "1"}}), "extra", "unknown field");
    expect_error("{\"seed\": 3, " + level().substr(1), "", "duplicate key \"seed\"");
    expect_error(level({{"board", R"({"width": 8, "width": 9})"}}), "", "duplicate key \"width\"");
}

void test_required_fields() {
    for (const char* key : {"id", "episode", "index", "quotas", "time_limit", "totems_unlocked", "seed"})
        expect_error(level({{key, ""}}), key, "required field missing");
    expect_error(level({{"time_limit", R"({"relaxed": 60})"}}), "time_limit.normal", "required field missing");
    expect_error(level({{"time_limit", R"({"normal": 60})"}}), "time_limit.relaxed", "required field missing");
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

void test_time_limit_errors() {
    expect_error(level({{"time_limit", "120"}}), "time_limit", "expected an object");
    expect_error(level({{"time_limit", R"({"relaxed": 0, "normal": 60})"}}), "time_limit.relaxed",
                 "must be greater than 0 seconds, got 0");
    expect_error(level({{"time_limit", R"({"relaxed": 60, "normal": -1.5})"}}), "time_limit.normal",
                 "must be greater than 0 seconds");
    expect_error(level({{"time_limit", R"({"relaxed": "60", "normal": 60})"}}), "time_limit.relaxed",
                 "expected a number of seconds, got string");
    expect_error(level({{"time_limit", R"({"relaxed": 60, "normal": 60, "hard": 30})"}}), "time_limit.hard",
                 "unknown field");
}

void test_totem_errors() {
    expect_error(level({{"totems_unlocked", R"("time")"}}), "totems_unlocked", "expected an array");
    expect_error(level({{"totems_unlocked", R"(["time", "laser"])"}}), "totems_unlocked[1]", "unknown totem \"laser\"");
    expect_error(level({{"totems_unlocked", "[3]"}}), "totems_unlocked[0]", "expected a totem id string");
    expect_error(level({{"totems_unlocked", R"(["time", "row_clear", "time"])"}}), "totems_unlocked[2]",
                 "duplicate totem \"time\"");
    expect_error(level({{"totems_unlocked", R"(["hammer"])"}}), "totems_unlocked[0]", "unknown totem");
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

void test_load_and_level_state() {
    const LevelData l = LevelData::load(level());
    check(l.time_limit_for(Difficulty::Relaxed) == 240.0, "relaxed time limit selected");
    check(l.time_limit_for(Difficulty::Normal) == 150.5, "normal time limit selected");
    const LevelState relaxed = l.make_state(Difficulty::Relaxed);
    const LevelState normal = l.make_state(Difficulty::Normal);
    check(relaxed.time_left() == 240.0 && normal.time_left() == 150.5, "level state uses the difficulty's timer");
    check(normal.quotas() == l.quotas && normal.outcome() == LevelState::Outcome::Playing,
          "level state starts with the level's quotas");

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
    check(difficulty_from_name("relaxed") == Difficulty::Relaxed && difficulty_from_name("normal") == Difficulty::Normal &&
              !difficulty_from_name("hard"),
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
    test_boundary_values_accepted();
    test_document_errors();
    test_required_fields();
    test_id_errors();
    test_episode_and_index_errors();
    test_board_errors();
    test_quota_errors();
    test_time_limit_errors();
    test_totem_errors();
    test_blocked_cell_errors();
    test_seed_errors();
    test_all_errors_are_reported_together();
    test_invalid_board_skips_dependent_checks();
    test_load_and_level_state();
    test_name_helpers();
    test_parse_is_deterministic();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

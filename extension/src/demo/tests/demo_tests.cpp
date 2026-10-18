// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Web demo hand-off script tests. No Godot, no framework; run via ctest.
#include <cstdio>
#include <string>

#include "handoff_script.hpp"

using namespace montezuma::demo;

namespace {

int checks = 0;
int failures = 0;

void check_eq(const std::string& got, const std::string& want, const char* label) {
    ++checks;
    if (got != want) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n  got:  %s\n  want: %s\n", label, got.c_str(), want.c_str());
    }
}

void test_level_completed_matches_the_contract() {
    check_eq(level_completed_script("e1-l01", 1234),
             R"(window.dispatchEvent(new CustomEvent("montezuma:level-completed",{detail:{"v":1,"level":"e1-l01","score":1234}}));)",
             "level-completed script");
    check_eq(level_completed_script("e1-l01", 0),
             R"(window.dispatchEvent(new CustomEvent("montezuma:level-completed",{detail:{"v":1,"level":"e1-l01","score":0}}));)",
             "zero score");
}

void test_ready_matches_the_contract() {
    check_eq(ready_script("0.0.0-dev.8.gb98fcf7"),
             R"(window.dispatchEvent(new CustomEvent("montezuma:ready",{detail:{"v":1,"version":"0.0.0-dev.8.gb98fcf7"}}));)",
             "ready script");
    check_eq(ready_script(""), R"(window.dispatchEvent(new CustomEvent("montezuma:ready",{detail:{"v":1,"version":""}}));)",
             "empty version");
}

void test_strings_cannot_break_out() {
    check_eq(json_string(R"(a"b\c)"), R"("a\"b\\c")", "quote and backslash");
    check_eq(json_string("x\n\t\x01"), "\"x\\u000a\\u0009\\u0001\"", "control characters");
    check_eq(json_string("</script><!--&"), "\"\\u003c/script\\u003e\\u003c!--\\u0026\"", "markup characters");
    check_eq(json_string("a\xe2\x80\xa8" "b\xe2\x80\xa9"), "\"a\\u2028b\\u2029\"", "line separators");
    check_eq(json_string("\xc3\xa9\xe2\x80\x94"), "\"\xc3\xa9\xe2\x80\x94\"", "other UTF-8 kept as is");
    check_eq(json_string("\xe2\x80"), "\"\xe2\x80\"", "truncated sequence at the end");
}

}  // namespace

int main() {
    test_level_completed_matches_the_contract();
    test_ready_matches_the_contract();
    test_strings_cannot_break_out();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

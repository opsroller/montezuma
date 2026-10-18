// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Every committed level file (game/data/levels/**/*.json) parses, is named
// for its id, and deals a playable LevelSession on both difficulties, so a
// level the game cannot load never reaches main. No framework dependency;
// run via ctest.
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "montezuma/level_data.hpp"
#include "montezuma/level_session.hpp"

using namespace montezuma;
namespace fs = std::filesystem;

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

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// "e1/l01.json" -> "e1-l01", the id the file must carry (contracts: LevelData).
std::string id_for(const fs::path& relative) {
    return relative.parent_path().generic_string() + "-" + relative.stem().string();
}

void check_level(const fs::path& root, const fs::path& path) {
    const fs::path relative = fs::relative(path, root);
    const std::string name = relative.generic_string();
    const LevelParseResult parsed = LevelData::parse(read_file(path));
    check(parsed.ok(), name + " parses" + (parsed.ok() ? "" : ":\n" + parsed.error_text()));
    if (!parsed.ok()) return;
    const LevelData& level = *parsed.level;
    check(level.id == id_for(relative), name + " id \"" + level.id + "\" matches its path");
    check("e" + std::to_string(level.episode) + "-l" + (level.index < 10 ? "0" : "") + std::to_string(level.index) ==
              level.id,
          name + " episode and index match its id");
    for (Difficulty d : {Difficulty::Normal, Difficulty::Hard}) {
        const char* label = d == Difficulty::Normal ? "normal" : "hard";
        try {
            LevelSession session(level, d, Upgrades{}, level.seed);
            check(!session.legal_moves().empty(), name + " " + label + " deals a board with a legal move");
        } catch (const std::exception& e) {
            check(false, name + " " + label + " starts a LevelSession: " + e.what());
        }
    }
}

}  // namespace

int main() {
    const fs::path root = fs::path(MONTEZUMA_SOURCE_DIR) / "game" / "data" / "levels";
    std::vector<fs::path> files;
    if (fs::is_directory(root))
        for (const auto& entry : fs::recursive_directory_iterator(root))
            if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
    check(!files.empty(), "found level files under " + root.generic_string());
    for (const fs::path& path : files) check_level(root, path);
    std::printf("%zu level files, %d checks, %d failures\n", files.size(), checks, failures);
    return failures == 0 ? 0 : 1;
}

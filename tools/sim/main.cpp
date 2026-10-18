// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// montezuma_sim: plays level files with bots and checks balance targets.
// Usage in docs/how-to/run-the-balance-simulator.md.

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "sim.hpp"

using namespace montezuma;
using namespace montezuma::sim;
namespace fs = std::filesystem;

namespace {

constexpr const char* kUsage =
    "usage: montezuma_sim [options] <level.json | directory>...\n"
    "  --seeds N                 runs per level, bot and difficulty (default 200)\n"
    "  --bots LIST               comma list of random, greedy, planner (default all)\n"
    "  --difficulty normal|hard|both   (default both)\n"
    "  --upgrades auto|none      (default auto)\n"
    "  --format table|csv|json   (default table)\n"
    "  --check                   exit 1 when a target, curve or clock rule fails\n";

int usage(const std::string& error) {
    if (!error.empty()) std::cerr << "montezuma_sim: " << error << "\n";
    std::cerr << kUsage;
    return kExitUsage;
}

bool read_file(const fs::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    int seeds = kDefaultSeeds;
    std::vector<Bot> bots{Bot::Random, Bot::Greedy, Bot::Planner};
    std::vector<Difficulty> difficulties{Difficulty::Normal, Difficulty::Hard};
    bool auto_up = true;
    Format format = Format::Table;
    bool check = false;
    std::vector<std::string> inputs;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&](std::string& out) {
            if (i + 1 >= argc) return false;
            out = argv[++i];
            return true;
        };
        std::string v;
        if (arg == "--seeds") {
            if (!value(v)) return usage("--seeds needs a number");
            try {
                std::size_t used = 0;
                const long n = std::stol(v, &used);
                if (used != v.size() || n < 1 || n > 1000000) return usage("--seeds must be 1 or more");
                seeds = static_cast<int>(n);
            } catch (const std::exception&) {
                return usage("--seeds must be a number");
            }
        } else if (arg == "--bots") {
            if (!value(v)) return usage("--bots needs a list");
            bots.clear();
            std::stringstream ss(v);
            std::string name;
            while (std::getline(ss, name, ',')) {
                const auto b = bot_from_name(name);
                if (!b) return usage("unknown bot: " + name);
                if (std::find(bots.begin(), bots.end(), *b) == bots.end()) bots.push_back(*b);
            }
            if (bots.empty()) return usage("--bots needs at least one bot");
            std::sort(bots.begin(), bots.end());
        } else if (arg == "--difficulty") {
            if (!value(v)) return usage("--difficulty needs a value");
            if (v == "normal") difficulties = {Difficulty::Normal};
            else if (v == "hard") difficulties = {Difficulty::Hard};
            else if (v == "both") difficulties = {Difficulty::Normal, Difficulty::Hard};
            else return usage("--difficulty must be normal, hard or both");
        } else if (arg == "--upgrades") {
            if (!value(v)) return usage("--upgrades needs a value");
            if (v == "auto") auto_up = true;
            else if (v == "none") auto_up = false;
            else return usage("--upgrades must be auto or none");
        } else if (arg == "--format") {
            if (!value(v)) return usage("--format needs a value");
            if (v == "table") format = Format::Table;
            else if (v == "csv") format = Format::Csv;
            else if (v == "json") format = Format::Json;
            else return usage("--format must be table, csv or json");
        } else if (arg == "--check") {
            check = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << kUsage;
            return kExitOk;
        } else if (!arg.empty() && arg[0] == '-') {
            return usage("unknown option: " + arg);
        } else {
            inputs.push_back(arg);
        }
    }
    if (inputs.empty()) return usage("no level file or directory given");

    // Collect files: directories are scanned recursively for *.json.
    std::vector<fs::path> files;
    for (const std::string& in : inputs) {
        std::error_code ec;
        if (fs::is_directory(in, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(in, ec))
                if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
        } else if (fs::is_regular_file(in, ec)) {
            files.push_back(in);
        } else {
            std::cerr << "montezuma_sim: " << in << ": no such file or directory\n";
            return kExitUsage;
        }
    }
    std::sort(files.begin(), files.end());

    std::vector<LevelData> levels;
    bool bad = false;
    for (const fs::path& f : files) {
        std::string text;
        if (!read_file(f, text)) {
            std::cerr << f.string() << ": cannot read\n";
            bad = true;
            continue;
        }
        LevelParseResult r = LevelData::parse(text);
        if (!r.ok()) {
            for (const LevelDataError& e : r.errors) std::cerr << f.string() << ": " << e.to_string() << "\n";
            bad = true;
            continue;
        }
        // AMBIGUOUS: blocked cells are not supported by the session yet.
        if (!r.level->blocked_cells.empty()) {
            std::cerr << f.string() << ": blocked cells are not supported yet\n";
            bad = true;
            continue;
        }
        levels.push_back(std::move(*r.level));
    }
    if (bad) return kExitUsage;
    if (levels.empty()) return usage("no level files found");
    std::stable_sort(levels.begin(), levels.end(), [](const LevelData& a, const LevelData& b) {
        return a.episode != b.episode ? a.episode < b.episode : a.index < b.index;
    });

    std::vector<Row> rows;
    try {
        for (const LevelData& level : levels) {
            const Upgrades up = auto_up ? auto_upgrades(level) : no_upgrades(level);
            for (Bot bot : bots)
                for (Difficulty d : difficulties) {
                    Row row;
                    row.level = level.id;
                    row.episode = level.episode;
                    row.index = level.index;
                    row.bot = bot;
                    row.difficulty = d;
                    row.seeds = seeds;
                    for (int i = 0; i < seeds; ++i) {
                        const RunResult r = play(level, d, up, bot, run_seed(level.seed, i));
                        row.total_moves += r.moves;
                        if (r.won) {
                            ++row.wins;
                            row.win_times.push_back(r.time_left);
                        }
                        if (r.stuck) ++row.stuck;
                    }
                    rows.push_back(std::move(row));
                }
        }
    } catch (const std::exception& e) {
        std::cerr << "montezuma_sim: " << e.what() << "\n";
        return kExitUsage;
    }

    std::vector<CurveCheck> curves = curve_checks(levels, rows);
    for (CurveCheck& c : clock_checks(levels, rows)) curves.push_back(std::move(c));
    std::cout << format_report(rows, curves, format);

    bool failed = false;
    for (const Row& r : rows) failed = failed || evaluate(r) == Verdict::Fail;
    for (const CurveCheck& c : curves) failed = failed || c.verdict == Verdict::Fail;
    return check && failed ? kExitFail : kExitOk;
}

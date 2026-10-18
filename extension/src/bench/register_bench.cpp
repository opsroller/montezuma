// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "register_bench.hpp"

#include <godot_cpp/core/class_db.hpp>

#include "benchmark_runner.hpp"
#include "readability_capture.hpp"

namespace montezuma::godot_binding {

void register_bench() {
    GDREGISTER_CLASS(BenchmarkRunner);
    GDREGISTER_CLASS(ReadabilityCapture);
}

}  // namespace montezuma::godot_binding

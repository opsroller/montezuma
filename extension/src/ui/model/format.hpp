// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <string>

namespace montezuma::ui {

// "m:ss" for a countdown, rounding up so 0:00 shows only when time is out.
// Negative and non-finite values show as 0:00.
std::string format_clock(double seconds);

// LevelData id for an episode and level, both 1-based: e1-l03.
std::string level_id(int episode, int index);

}  // namespace montezuma::ui

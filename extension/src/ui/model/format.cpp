// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "format.hpp"

#include <cmath>
#include <cstdio>

namespace montezuma::ui {

std::string format_clock(double seconds) {
    long long whole = 0;
    if (std::isfinite(seconds) && seconds > 0.0) whole = static_cast<long long>(std::ceil(seconds));
    char buf[32];
    std::snprintf(buf, sizeof buf, "%lld:%02lld", whole / 60, whole % 60);
    return buf;
}

std::string level_id(int episode, int index) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "e%d-l%02d", episode, index);
    return buf;
}

}  // namespace montezuma::ui

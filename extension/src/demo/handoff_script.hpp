// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

// The page scripts for the web demo hand-off (contracts: Web demo hand-off).
// Godot-free so the exact text the page receives is unit-tested.
namespace montezuma::demo {

inline constexpr int kHandoffVersion = 1;
inline constexpr std::string_view kLevelCompletedEvent = "montezuma:level-completed";
inline constexpr std::string_view kReadyEvent = "montezuma:ready";

// A JSON string literal for s, also safe inside a JavaScript expression:
// quotes, backslashes, control characters, '<', '>', '&' and the line
// separators U+2028 and U+2029 are escaped.
inline std::string json_string(std::string_view s) {
    std::string out = "\"";
    char buf[8];
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20 || c == '<' || c == '>' || c == '&' || c == 0x7f) {
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
        } else if (c == 0xe2 && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x80 &&
                   (static_cast<unsigned char>(s[i + 2]) == 0xa8 || static_cast<unsigned char>(s[i + 2]) == 0xa9)) {
            out += static_cast<unsigned char>(s[i + 2]) == 0xa8 ? "\\u2028" : "\\u2029";
            i += 2;
        } else {
            out += static_cast<char>(c);
        }
    }
    out += '"';
    return out;
}

// window.dispatchEvent(new CustomEvent(<event>, {detail: <detail>}));
inline std::string dispatch_script(std::string_view event, const std::string& detail_json) {
    return "window.dispatchEvent(new CustomEvent(" + json_string(event) + ",{detail:" + detail_json + "}));";
}

// {v:1, level, score}: once, when the intro level is won.
inline std::string level_completed_script(std::string_view level, std::int64_t score) {
    return dispatch_script(kLevelCompletedEvent, "{\"v\":" + std::to_string(kHandoffVersion) + ",\"level\":" +
                                                     json_string(level) + ",\"score\":" + std::to_string(score) + "}");
}

// {v:1, version}: first frame shown.
inline std::string ready_script(std::string_view version) {
    return dispatch_script(kReadyEvent,
                           "{\"v\":" + std::to_string(kHandoffVersion) + ",\"version\":" + json_string(version) + "}");
}

}  // namespace montezuma::demo

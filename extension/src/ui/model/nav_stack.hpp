// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace montezuma::ui {

// Screen ids from bottom to top. The navigation owner (ScreenStack) keeps
// one of these beside its screen instances, so the rules for push, back
// and replace are testable without Godot.
class NavStack {
public:
    // Throws std::invalid_argument for an empty id.
    void push(std::string id);
    // Replaces the top screen, or pushes when empty.
    void replace(std::string id);
    // Clears the stack and pushes id.
    void reset(std::string id);
    // Pops the top screen unless it is the root; returns whether it popped.
    bool pop();

    bool empty() const { return ids_.empty(); }
    std::size_t depth() const { return ids_.size(); }
    // Empty string when the stack is empty.
    const std::string& top() const;
    bool contains(const std::string& id) const;
    const std::vector<std::string>& ids() const { return ids_; }

private:
    std::vector<std::string> ids_;
};

}  // namespace montezuma::ui

// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "nav_stack.hpp"

#include <algorithm>
#include <stdexcept>

namespace montezuma::ui {

namespace {
const std::string kNone;

void require_id(const std::string& id) {
    if (id.empty()) throw std::invalid_argument("screen id must not be empty");
}
}  // namespace

void NavStack::push(std::string id) {
    require_id(id);
    ids_.push_back(std::move(id));
}

void NavStack::replace(std::string id) {
    require_id(id);
    if (ids_.empty())
        ids_.push_back(std::move(id));
    else
        ids_.back() = std::move(id);
}

void NavStack::reset(std::string id) {
    require_id(id);
    ids_.clear();
    ids_.push_back(std::move(id));
}

bool NavStack::pop() {
    if (ids_.size() <= 1) return false;
    ids_.pop_back();
    return true;
}

const std::string& NavStack::top() const { return ids_.empty() ? kNone : ids_.back(); }

bool NavStack::contains(const std::string& id) const {
    return std::find(ids_.begin(), ids_.end(), id) != ids_.end();
}

}  // namespace montezuma::ui

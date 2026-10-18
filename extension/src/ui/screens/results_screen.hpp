// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include "../screen.hpp"

namespace montezuma::godot_binding {

// End of a level. Params: outcome (LevelState::Outcome, 1 won, 2 lost),
// score, level_id. Scene nodes: %Title, %Score, %Next, %Retry (NavButtons
// whose targets are set here).
class ResultsScreen : public Screen {
    GDCLASS(ResultsScreen, Screen)

public:
    static constexpr int kOutcomeWon = 1;

    void on_open(const godot::Dictionary& params) override;
    void _notification(int what);

protected:
    static void _bind_methods() {}

private:
    void refresh();
};

}  // namespace montezuma::godot_binding

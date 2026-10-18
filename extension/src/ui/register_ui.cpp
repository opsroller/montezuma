// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "register_ui.hpp"

#include <godot_cpp/core/class_db.hpp>

#include "nav_button.hpp"
#include "screen.hpp"
#include "screen_stack.hpp"
#include "screens/credits_screen.hpp"
#include "screens/episode_map_screen.hpp"
#include "screens/intro_screen.hpp"
#include "screens/results_screen.hpp"
#include "screens/settings_screen.hpp"
#include "screens/trophies_screen.hpp"
#include "ui_root.hpp"
#include "ui_theme.hpp"

namespace montezuma::godot_binding {

void register_ui() {
    GDREGISTER_CLASS(UiTheme);
    GDREGISTER_CLASS(Screen);
    GDREGISTER_CLASS(ScreenStack);
    GDREGISTER_CLASS(NavButton);
    GDREGISTER_CLASS(UiRoot);
    GDREGISTER_CLASS(SettingsScreen);
    GDREGISTER_CLASS(CreditsScreen);
    GDREGISTER_CLASS(EpisodeMapScreen);
    GDREGISTER_CLASS(ResultsScreen);
    GDREGISTER_CLASS(TrophiesScreen);
    GDREGISTER_CLASS(IntroScreen);
}

}  // namespace montezuma::godot_binding

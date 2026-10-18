// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include "../screen.hpp"

namespace montezuma::godot_binding {

class UiRoot;

// Binds the settings scene's controls (unique names %Music, %Sfx,
// %TextSize, %ReducedMotion, %Fullscreen, %Language, %Difficulty) to
// UiRoot settings, and %Quality to the QualityTiers choice. Changes apply
// live; UiRoot settings are saved when the screen closes, the quality tier
// at once.
class SettingsScreen : public Screen {
    GDCLASS(SettingsScreen, Screen)

public:
    void on_open(const godot::Dictionary& params) override;
    void on_close() override;

protected:
    static void _bind_methods() {}

private:
    UiRoot* root() const;
    void bind_slider(const char* node, const char* key);
    void bind_toggle(const char* node, const char* key);
    void bind_option(const char* node, const char* key, const godot::PackedStringArray& values,
                     const godot::PackedStringArray& labels);
    void bind_quality();
    void on_quality(int64_t index);
    void on_value(double value, const godot::String& key);
    void on_toggled(bool on, const godot::String& key);
    void on_option(int64_t index, const godot::String& node, const godot::String& key);
};

}  // namespace montezuma::godot_binding

// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "quality.hpp"

namespace godot {
class Environment;
class CameraAttributes;
class Light3D;
}  // namespace godot

namespace montezuma::godot_binding {

// Applies the quality tier (desktop-high, desktop-standard, mobile, web)
// to the engine: picks it at start from the rendering method and GPU,
// switches it at runtime when the player changes the "quality_tier"
// setting, and turns on exactly the tier's features. Runs as the
// QualityTiers autoload; C++ callers reach it through get_singleton().
//
// What it switches (docs/reference/quality-tiers.md):
// - root viewport scaling, AA, anisotropy and LOD threshold;
// - shadow atlas sizes, soft shadow and subsurface scattering quality;
// - Environment and CameraAttributes effects: a theme authors its full
//   desktop-high look and a tier switches off what it lacks, so an
//   effect is on only when the theme enabled it and the tier allows it;
// - nodes in group "quality:<feature>" show only where the tier has the
//   feature, nodes in "quality_fallback:<feature>" only where it lacks it;
// - Light3Ds in group "quality_shadow_light": the nearest N to the camera
//   keep their shadows, N per tier.
class QualityTiers : public godot::Node {
    GDCLASS(QualityTiers, godot::Node)

public:
    static QualityTiers* get_singleton() { return singleton_; }

    QualityTiers();
    ~QualityTiers() override;

    void _enter_tree() override;
    void _exit_tree() override;
    void _ready() override;

    // The setting the settings screen shows: "auto" or a tier id from
    // get_available_tiers(). Saved to user://quality.cfg.
    godot::String get_choice() const { return choice_; }
    void set_choice(const godot::String& choice);

    // The tier in effect and the one auto would pick.
    godot::String get_tier() const;
    godot::String get_detected_tier() const;
    // Tiers the running rendering method can show; the method itself is
    // fixed per process, so on desktop these are the two desktop tiers.
    godot::PackedStringArray get_available_tiers() const;

    bool has_feature(const godot::String& feature) const;
    godot::PackedStringArray get_active_features() const;

    // The active tier's variant of an asset ("dir/hi/file" on desktop,
    // "dir/lo/file" on web), or the path itself when no variant exists.
    godot::String resolve_variant(const godot::String& path) const;

    // Re-applies the tier to a subtree, for content added outside the tree
    // and then moved in; nodes entering the tree are handled already.
    void apply_to(godot::Node* root);

    const quality::Profile& get_profile() const { return quality::profile(tier_); }

protected:
    static void _bind_methods();

private:
    void switch_to(const godot::String& choice, bool save);
    void load_choice();
    void save_choice() const;
    void apply_all();
    void apply_engine_settings();
    void apply_viewport();
    void apply_node(godot::Node* node);
    void apply_environment(const godot::Ref<godot::Environment>& env);
    void apply_camera_attributes(const godot::Ref<godot::CameraAttributes>& attributes);
    void request_shadow_budget();
    void apply_shadow_budget();
    void on_node_added(godot::Node* node);
    void on_viewport_resized();
    void run_self_test();

    static QualityTiers* singleton_;

    quality::Adapter adapter_;
    quality::Tier tier_ = quality::Tier::DesktopStandard;
    godot::String choice_ = "auto";
    godot::String settings_path_ = "user://quality.cfg";
    bool shadow_budget_pending_ = false;
};

}  // namespace montezuma::godot_binding

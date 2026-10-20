// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/list.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "tuner_model.hpp"

namespace godot {
class Button;
class FoldableContainer;
class Label;
class Node;
class VBoxContainer;
}  // namespace godot

namespace montezuma::godot_binding {

// Settings that live outside any one object (RenderingServer globals, the
// window, the quality tier), shown as properties so the tuning screen edits
// them like any engine object's.
class TunerSettings : public godot::RefCounted {
    GDCLASS(TunerSettings, godot::RefCounted)

public:
    using Getter = std::function<godot::Variant()>;
    using Setter = std::function<void(const godot::Variant&)>;
    using LabelFn = std::function<godot::String()>;

    void add_group(const godot::String& title);
    // `label`, when given, is shown instead of the capitalized name.
    void add(const godot::PropertyInfo& info, Getter get, Setter set, LabelFn label = {});
    godot::String label_of(const godot::StringName& name) const;

protected:
    static void _bind_methods() {}
    bool _set(const godot::StringName& name, const godot::Variant& value);
    bool _get(const godot::StringName& name, godot::Variant& value) const;
    void _get_property_list(godot::List<godot::PropertyInfo>* list) const;

private:
    struct Row {
        godot::PropertyInfo info;
        LabelFn label;
        Getter get;
        Setter set;
    };
    const Row* find(const godot::StringName& name) const;
    std::vector<Row> rows_;
};

// The tuning screen's frame-time graph: whole frame and GPU time of the
// last frames, with lines at 120 and 60 fps.
class FrameGraph : public godot::Control {
    GDCLASS(FrameGraph, godot::Control)

public:
    void set_window(const tuner::FrameWindow* window) { window_ = window; }
    void _draw() override;

protected:
    static void _bind_methods() {}

private:
    const tuner::FrameWindow* window_ = nullptr;
    godot::PackedVector2Array frame_points_;
    godot::PackedVector2Array gpu_points_;
};

// The render tuning screen (docs/reference/render-tuner.md): a panel over
// the running board demo with a live performance readout and a control
// for every rendering and lighting setting it finds, built from the
// engine's own property lists: the quality tier and its features, the
// window, the root viewport, RenderingServer's global quality settings,
// every Environment, CameraAttributes and compositor effect, every light,
// GI node, fog volume, particle system and decal, and the parameters of
// every ShaderMaterial in the scene. Changes apply at once and last for
// the run; nothing is saved. F1 hides and shows the panel, F12 saves a
// screenshot.
class RenderTuner : public godot::Control {
    GDCLASS(RenderTuner, godot::Control)

public:
    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;
    void _input(const godot::Ref<godot::InputEvent>& event) override;

    // Saves the window as a PNG (F12 saves to user://tuner/).
    void save_screenshot(const godot::String& path);

    // Scans the scene again and rebuilds the sections (new nodes, a tier switch).
    void rebuild();
    // Puts every setting shown so far back to the value it had when first
    // shown, and the tier and feature overrides back to the start.
    void reset_all();

protected:
    static void _bind_methods();

private:
    struct Binding {
        godot::ObjectID object;
        godot::StringName property;
        godot::Variant::Type type = godot::Variant::NIL;
        bool integer_range = false;
        // The editing controls: a CheckBox, an OptionButton (with item ids as
        // values), a ColorPickerButton, or a slider with a SpinBox beside it.
        godot::Control* control = nullptr;
        godot::Control* spin = nullptr;
        // The row's name; refreshed for TunerSettings rows, whose labels carry live state.
        godot::Label* text = nullptr;
    };
    using Builder = std::function<void(godot::VBoxContainer*)>;
    using Filter = std::function<bool(const godot::String&)>;

    void build_layout();
    void apply_ui_scale();
    godot::FoldableContainer* add_section(godot::VBoxContainer* parent, const godot::String& title, Builder build,
                                          bool folded = true);
    void on_folding_changed(bool folded, int64_t section);
    // One row per editable bool, int, float or Color property of `object`;
    // `filter` keeps a property by name (all when empty).
    void add_properties(godot::VBoxContainer* box, godot::Object* object, const Filter& filter);
    void add_property_row(godot::VBoxContainer* box, godot::Object* object, const godot::Dictionary& property,
                          const godot::String& label);
    void add_node_sections(godot::VBoxContainer* box, const std::vector<godot::ObjectID>& nodes,
                           const godot::String& stop_class);
    void remember_original(godot::Object* object, const godot::StringName& property);
    void apply_binding(int64_t index, const godot::Variant& value);
    void on_toggled(bool on, int64_t index);
    void on_item_selected(int64_t item, int64_t index);
    void on_color_changed(const godot::Color& color, int64_t index);
    void on_slider_changed(double value, int64_t index);
    void on_spin_changed(double value, int64_t index);
    void sync_controls();
    void on_tier_changed(const godot::String& tier);
    void seed_tier_settings();
    void scan();
    void make_settings();
    void update_readout();
    // `-- --tuner-selftest`: opens every section, drives the controls and
    // checks the settings follow, then quits with the result.
    void run_self_test();
    int64_t find_binding(godot::Object* object, const godot::StringName& property) const;

    std::vector<Binding> bindings_;
    std::vector<Builder> builders_;
    std::vector<godot::FoldableContainer*> sections_;
    struct Original {
        godot::ObjectID object;
        godot::StringName property;
        godot::Variant value;
    };
    // Each setting shown so far with the value it had when first shown;
    // `seen_` holds "<instance id>:<property>" for each.
    std::vector<Original> originals_;
    godot::HashMap<godot::String, bool> seen_;
    // Titles of the sections open now, so a rebuild keeps them open.
    godot::HashMap<godot::String, bool> open_titles_;
    godot::String original_choice_;

    godot::Ref<TunerSettings> quality_;
    godot::Ref<TunerSettings> display_;
    godot::Ref<TunerSettings> renderer_;
    // Values for RenderingServer setters that take several at once and have no getters.
    godot::HashMap<godot::String, godot::Variant> renderer_state_;

    // What the last scan found, by kind.
    std::vector<godot::ObjectID> environments_, camera_attributes_, compositor_effects_, lights_, gi_nodes_,
        fog_volumes_, particles_, decals_, shader_materials_;
    // Instance id of an embedded resource -> the node it was found on.
    godot::HashMap<uint64_t, godot::String> titles_;

    godot::Control* panel_ = nullptr;
    godot::VBoxContainer* section_box_ = nullptr;
    godot::Label* readout_ = nullptr;
    godot::Label* passes_ = nullptr;
    FrameGraph* graph_ = nullptr;

    tuner::FrameWindow window_;
    uint64_t last_frame_usec_ = 0;
    double readout_timer_ = 0.0;
    double sync_timer_ = 0.0;
    float ui_scale_ = 1.0f;
    // `-- --tuner-shot=<png>`: save a screenshot this long after start, then quit.
    godot::String shot_path_;
    double shot_timer_ = 0.0;
    int shot_number_ = 0;
};

}  // namespace montezuma::godot_binding

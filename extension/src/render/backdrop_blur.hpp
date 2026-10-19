// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/camera_attributes_practical.hpp>
#include <godot_cpp/classes/compositor.hpp>
#include <godot_cpp/classes/compositor_effect.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/render_data.hpp>
#include <godot_cpp/variant/rid.hpp>

namespace godot {
class Viewport;
}  // namespace godot

namespace montezuma::godot_binding {

// Far depth of field for the backdrop, in place of Godot's: blurs what lies
// beyond the board, never the board itself. It reads the far distance,
// transition and amount from CameraAttributesPractical (where the theme
// authors them and BoardCamera3D refits them) with Godot's DoF there left
// off. Runs after transparents and before TAA or temporal upscaling, at
// internal resolution, in two compute passes that skip every pixel short of
// the far distance: a half-resolution blur and a full-resolution blend
// (docs/explanation/about-post-processing.md).
class BackdropBlur : public godot::CompositorEffect {
    GDCLASS(BackdropBlur, godot::CompositorEffect)

public:
    BackdropBlur();

    void _render_callback(int32_t p_effect_callback_type, godot::RenderData* p_render_data) override;

    // Compiles the shaders on the render thread now rather than in the
    // first frame that draws the blur.
    void warm();

    void set_camera_attributes(const godot::Ref<godot::CameraAttributesPractical>& attributes);
    godot::Ref<godot::CameraAttributesPractical> get_camera_attributes() const { return attributes_; }

    // The BackdropBlur among `compositor`'s effects, or null.
    static godot::Ref<BackdropBlur> in(const godot::Ref<godot::Compositor>& compositor);
    // The BackdropBlur on the first WorldEnvironment under `root`, or null.
    static godot::Ref<BackdropBlur> under(godot::Node* root);
    // The BackdropBlur `viewport` renders with: its camera's, else its world's.
    static godot::Ref<BackdropBlur> drawing(godot::Viewport* viewport);

protected:
    static void _bind_methods();
    void _notification(int p_what);

private:
    bool ensure_pipelines();
    void compile_on_render_thread();
    void free_pipelines();

    godot::Ref<godot::CameraAttributesPractical> attributes_;
    godot::RID blur_shader_;
    godot::RID blur_pipeline_;
    godot::RID blend_shader_;
    godot::RID blend_pipeline_;
    godot::RID linear_sampler_;
    godot::RID nearest_sampler_;
    bool failed_ = false;
};

}  // namespace montezuma::godot_binding

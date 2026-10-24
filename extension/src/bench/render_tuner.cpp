// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "render_tuner.hpp"

#include "render/flicker_light_3d.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/camera_attributes.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/color_picker_button.hpp>
#include <godot_cpp/classes/compositor.hpp>
#include <godot_cpp/classes/compositor_effect.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/decal.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/fog_volume.hpp>
#include <godot_cpp/classes/foldable_container.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/lightmap_gi.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/reflection_probe.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/popup_panel.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/voxel_gi.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>


using namespace godot;

namespace montezuma::godot_binding {

namespace {

// Panel width and text size in UI units (scaled by the UI scale setting).
constexpr float kPanelWidth = 480.0f;
constexpr int kFontSize = 13;
constexpr double kReadoutSeconds = 0.25;
constexpr double kSyncSeconds = 0.5;
// Readout averages cover this much recent time.
constexpr double kAverageMs = 1000.0;
// `--tuner-shot` waits this long so the scan, the demo and the readout have run.
constexpr double kShotSeconds = 6.0;

// The project's own shaders (game/assets/materials/shaders). Surface shaders
// fall back to a stock material when switched off; the rest hide their nodes.
constexpr const char* kSurfaceShaders[] = {"SH_Gem", "SH_TriplanarPBR", "SH_Water", "SH_WaterSimple"};
constexpr const char* kEffectShaders[] = {"SH_Fire",     "SH_FireSmoke",     "SH_Ember", "SH_EmberTrail", "SH_Spark",
                                          "SH_HeatHaze", "SH_AtmosphereFog", "SH_Dust",  "SH_CellHighlight"};
constexpr const char* kStockMeta = "tuner_stock_override";

String shader_name(const Ref<Material>& material) {
    const Ref<ShaderMaterial> sm = material;
    if (sm.is_null() || sm->get_shader().is_null()) return String();
    return sm->get_shader()->get_path().get_file().get_basename();
}

// The materials a node draws with, from overrides, its mesh or its particles.
std::vector<Ref<Material>> materials_of(Node* node) {
    std::vector<Ref<Material>> out;
    if (auto* fog = Object::cast_to<FogVolume>(node)) out.push_back(fog->get_material());
    auto* geometry = Object::cast_to<GeometryInstance3D>(node);
    if (!geometry) return out;
    out.push_back(geometry->get_material_override());
    out.push_back(geometry->get_material_overlay());
    std::vector<Ref<Mesh>> meshes;
    if (auto* mi = Object::cast_to<MeshInstance3D>(node)) {
        for (int32_t i = 0; i < mi->get_surface_override_material_count(); ++i)
            out.push_back(mi->get_surface_override_material(i));
        meshes.push_back(mi->get_mesh());
    } else if (auto* gpu = Object::cast_to<GPUParticles3D>(node)) {
        for (int32_t i = 0; i < gpu->get_draw_passes(); ++i) meshes.push_back(gpu->get_draw_pass_mesh(i));
    } else if (auto* cpu = Object::cast_to<CPUParticles3D>(node)) {
        meshes.push_back(cpu->get_mesh());
    }
    for (const Ref<Mesh>& mesh : meshes) {
        if (mesh.is_null()) continue;
        for (int32_t i = 0; i < mesh->get_surface_count(); ++i) out.push_back(mesh->surface_get_material(i));
    }
    return out;
}

bool draws_with(Node* node, const String& shader) {
    for (const Ref<Material>& m : materials_of(node))
        if (shader_name(m) == shader) return true;
    return false;
}

String ms(double v) { return String::num(v, 2) + " ms"; }

String mib(double bytes) { return String::num(bytes / (1024.0 * 1024.0), 0) + " MB"; }

const char* msaa_name(Viewport::MSAA msaa) {
    switch (msaa) {
        case Viewport::MSAA_2X: return "2x";
        case Viewport::MSAA_4X: return "4x";
        case Viewport::MSAA_8X: return "8x";
        default: return "off";
    }
}

const char* aa_name(Viewport::ScreenSpaceAA aa) {
    switch (aa) {
        case Viewport::SCREEN_SPACE_AA_FXAA: return "FXAA";
        case Viewport::SCREEN_SPACE_AA_SMAA: return "SMAA";
        default: return "off";
    }
}

const char* scaling_name(Viewport::Scaling3DMode mode) {
    switch (mode) {
        case Viewport::SCALING_3D_MODE_BILINEAR: return "bilinear";
        case Viewport::SCALING_3D_MODE_FSR: return "FSR 1";
        case Viewport::SCALING_3D_MODE_FSR2: return "FSR 2";
        case Viewport::SCALING_3D_MODE_METALFX_SPATIAL: return "MetalFX spatial";
        case Viewport::SCALING_3D_MODE_METALFX_TEMPORAL: return "MetalFX temporal";
        default: return "other";
    }
}

const char* vsync_name(DisplayServer::VSyncMode mode) {
    switch (mode) {
        case DisplayServer::VSYNC_DISABLED: return "off";
        case DisplayServer::VSYNC_ENABLED: return "on";
        case DisplayServer::VSYNC_ADAPTIVE: return "adaptive";
        case DisplayServer::VSYNC_MAILBOX: return "mailbox";
    }
    return "?";
}

Variant setting(const char* key, const Variant& fallback) {
    ProjectSettings* ps = ProjectSettings::get_singleton();
    return ps->has_setting(key) ? ps->get_setting_with_override(key) : fallback;
}

// Root-viewport properties the tuner shows (Viewport's rendering settings).
bool viewport_property(const String& name) {
    static const char* const kNames[] = {
        "scaling_3d_mode", "scaling_3d_scale", "fsr_sharpness", "texture_mipmap_bias",
        "anisotropic_filtering_level", "msaa_3d", "screen_space_aa", "use_taa", "use_debanding",
        "use_occlusion_culling", "mesh_lod_threshold", "positional_shadow_atlas_size",
        "positional_shadow_atlas_16_bits", "positional_shadow_atlas_quad_0", "positional_shadow_atlas_quad_1",
        "positional_shadow_atlas_quad_2", "positional_shadow_atlas_quad_3", "debug_draw", "use_hdr_2d", "vrs_mode",
    };
    for (const char* n : kNames)
        if (name == n) return true;
    return false;
}

Ref<StyleBoxFlat> panel_style() {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(Color(0.06f, 0.06f, 0.08f, 0.88f));
    style->set_content_margin_all(8.0f);
    return style;
}

}  // namespace

// ---- TunerSettings ----------------------------------------------------------

void TunerSettings::add_group(const String& title) {
    rows_.push_back({PropertyInfo(Variant::NIL, title, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), {}, {}, {}});
}

void TunerSettings::add(const PropertyInfo& info, Getter get, Setter set, LabelFn label) {
    rows_.push_back({info, std::move(label), std::move(get), std::move(set)});
}

const TunerSettings::Row* TunerSettings::find(const StringName& name) const {
    for (const Row& r : rows_)
        if (r.info.type != Variant::NIL && r.info.name == name) return &r;
    return nullptr;
}

String TunerSettings::label_of(const StringName& name) const {
    const Row* r = find(name);
    if (r && r->label) return r->label();
    return String(name).capitalize();
}

bool TunerSettings::_set(const StringName& name, const Variant& value) {
    const Row* r = find(name);
    if (!r || !r->set) return false;
    r->set(value);
    return true;
}

bool TunerSettings::_get(const StringName& name, Variant& value) const {
    const Row* r = find(name);
    if (!r || !r->get) return false;
    value = r->get();
    return true;
}

void TunerSettings::_get_property_list(List<PropertyInfo>* list) const {
    for (const Row& r : rows_) list->push_back(r.info);
}

// ---- FrameGraph -------------------------------------------------------------

void FrameGraph::_draw() {
    const Vector2 size = get_size();
    draw_rect(Rect2(Vector2(), size), Color(0, 0, 0, 0.35f));
    if (!window_ || window_->size() < 2) return;
    const std::size_t n = window_->size();
    double top = 20.0;
    for (std::size_t i = 0; i < n; ++i) top = std::max(top, window_->at(i).frame_ms * 1.1);
    top = std::min(top, 100.0);
    const auto y_of = [&](double v) { return size.y - static_cast<float>(std::min(v, top) / top) * size.y; };
    const Ref<Font> font = get_theme_default_font();
    // Labels made once, so a redraw allocates nothing.
    static const String k120 = "120 fps", k60 = "60 fps", kFrame = "frame", kGpu = "GPU";
    for (const auto& [line_ms, label] : {std::pair{1000.0 / 120.0, &k120}, std::pair{1000.0 / 60.0, &k60}}) {
        const float y = y_of(line_ms);
        draw_dashed_line(Vector2(0, y), Vector2(size.x, y), Color(1, 1, 1, 0.3f), 1.0f, 4.0f);
        if (font.is_valid()) draw_string(font, Vector2(4, y - 2), *label, HORIZONTAL_ALIGNMENT_LEFT, -1, 10);
    }
    if (top != top_ms_) {
        top_ms_ = top;
        top_label_ = String::num(top, 0) + " ms";
    }
    std::size_t gpu_count = 0;
    for (std::size_t i = 0; i < n; ++i) gpu_count += window_->at(i).gpu_ms > 0.0 ? 1 : 0;
    // Sized only when the counts change: once the window is full they stay put.
    if (frame_points_.size() != static_cast<int64_t>(n)) frame_points_.resize(static_cast<int64_t>(n));
    if (gpu_points_.size() != static_cast<int64_t>(gpu_count)) gpu_points_.resize(static_cast<int64_t>(gpu_count));
    const float dx = size.x / static_cast<float>(tuner::FrameWindow::kCapacity - 1);
    const float x0 = size.x - dx * static_cast<float>(n - 1);
    int64_t gpu_n = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const tuner::FrameSample& s = window_->at(i);
        const float x = x0 + dx * static_cast<float>(i);
        frame_points_.set(static_cast<int64_t>(i), Vector2(x, y_of(s.frame_ms)));
        if (s.gpu_ms > 0.0) gpu_points_.set(gpu_n++, Vector2(x, y_of(s.gpu_ms)));
    }
    draw_polyline(frame_points_, Color(0.92f, 0.92f, 0.95f), 1.5f);
    if (gpu_n > 1) draw_polyline(gpu_points_, Color(1.0f, 0.62f, 0.2f), 1.5f);
    if (font.is_valid()) {
        draw_string(font, Vector2(size.x - 150, 12), kFrame, HORIZONTAL_ALIGNMENT_LEFT, -1, 10,
                    Color(0.92f, 0.92f, 0.95f));
        draw_string(font, Vector2(size.x - 100, 12), kGpu, HORIZONTAL_ALIGNMENT_LEFT, -1, 10,
                    Color(1.0f, 0.62f, 0.2f));
        draw_string(font, Vector2(size.x - 60, 12), top_label_, HORIZONTAL_ALIGNMENT_LEFT, -1, 10);
    }
}

// ---- RenderTuner ------------------------------------------------------------

void RenderTuner::_bind_methods() {
    ClassDB::bind_method(D_METHOD("rebuild"), &RenderTuner::rebuild);
    ClassDB::bind_method(D_METHOD("reset_all"), &RenderTuner::reset_all);
    ClassDB::bind_method(D_METHOD("save_screenshot", "path"), &RenderTuner::save_screenshot);
}

void RenderTuner::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    set_mouse_filter(MOUSE_FILTER_IGNORE);
    make_settings();
    build_layout();
    RenderingServer::get_singleton()->viewport_set_measure_render_time(get_viewport()->get_viewport_rid(), true);
    last_frame_usec_ = Time::get_singleton()->get_ticks_usec();
    // The board view adds the theme (environment, lights) as it shows the
    // first board, so the first scan waits a moment.
    get_tree()->create_timer(0.5)->connect("timeout", callable_mp(this, &RenderTuner::rebuild));
    gpu_profile_ = OS::get_singleton()->get_cmdline_args().has("--gpu-profile");
    for (const String& arg : OS::get_singleton()->get_cmdline_user_args()) {
        if (arg.begins_with("--tuner-shot=")) shot_path_ = arg.substr(13);
        if (arg.begins_with("--tuner-print=")) {
            print_seconds_ = arg.substr(14).to_float();
            panel_->set_visible(false);
        }
        if (arg.begins_with("--tuner-quit=")) quit_seconds_ = arg.substr(13).to_float();
        if (arg.begins_with("--tuner-off=")) {
            // Starts with these custom-rendering toggles off (a shader name, "effects", "flicker" or "timer").
            for (const String& key : arg.substr(12).split(",", false)) set_custom(key.strip_edges(), false);
        }
        if (arg == "--tuner-selftest")
            get_tree()->create_timer(2.0)->connect("timeout", callable_mp(this, &RenderTuner::run_self_test));
    }
    set_process(true);
    set_process_input(true);
    get_tree()->connect("node_added", callable_mp(this, &RenderTuner::apply_custom));
}

void RenderTuner::save_screenshot(const String& path) {
    const Ref<Image> image = get_viewport()->get_texture()->get_image();
    ERR_FAIL_COND(image.is_null());
    DirAccess::make_dir_recursive_absolute(path.get_base_dir());
    if (image->save_png(path) == OK) UtilityFunctions::print("RenderTuner: saved ", path);
    else ERR_PRINT("RenderTuner: could not save " + path);
}

void RenderTuner::_exit_tree() {
    if (SceneTree* tree = get_tree(); tree && tree->is_connected("node_added", callable_mp(this, &RenderTuner::apply_custom)))
        tree->disconnect("node_added", callable_mp(this, &RenderTuner::apply_custom));
    if (Viewport* vp = get_viewport())
        RenderingServer::get_singleton()->viewport_set_measure_render_time(vp->get_viewport_rid(), false);
}

void RenderTuner::_input(const Ref<InputEvent>& event) {
    const Ref<InputEventKey> key = event;
    if (!key.is_valid() || !key->is_pressed() || key->is_echo()) return;
    if (key->get_keycode() == KEY_F1) {
        panel_->set_visible(!panel_->is_visible());
        get_viewport()->set_input_as_handled();
    } else if (key->get_keycode() == KEY_F12) {
        save_screenshot(ProjectSettings::get_singleton()->globalize_path("user://tuner/shot-" +
                                                                         String::num_int64(++shot_number_) + ".png"));
        get_viewport()->set_input_as_handled();
    }
}

void RenderTuner::make_settings() {
    RenderingServer* rs = RenderingServer::get_singleton();

    // On/off for everything the project draws on top of Godot's own rendering.
    custom_.instantiate();
    const auto toggle = [this](const String& key, const String& label) {
        custom_->add(
            PropertyInfo(Variant::BOOL, key), [this, key] { return !custom_off_.has(key); },
            [this, key](const Variant& v) { set_custom(key, v); }, [label] { return label; });
    };
    custom_->add_group("Our code");
    toggle("timer", "Level timer (Match3Game)");
    toggle("effects", "Match effects (BoardEffects)");
    toggle("flicker", "Brazier light flicker (FlickerLight3D)");
    custom_->add_group("Our surface shaders (off: stock material)");
    for (const char* name : kSurfaceShaders) toggle(name, name);
    custom_->add_group("Our effect shaders (off: hidden)");
    for (const char* name : kEffectShaders) toggle(name, name);

    // Window and frame pacing.
    display_.instantiate();
    DisplayServer* ds = DisplayServer::get_singleton();
    display_->add(
        PropertyInfo(Variant::INT, "vsync", PROPERTY_HINT_ENUM, "Off,On,Adaptive,Mailbox"),
        [ds] { return static_cast<int64_t>(ds->window_get_vsync_mode()); },
        [ds](const Variant& v) { ds->window_set_vsync_mode(static_cast<DisplayServer::VSyncMode>(int64_t(v))); },
        [] { return String("V-Sync"); });
    display_->add(
        PropertyInfo(Variant::INT, "max_fps", PROPERTY_HINT_RANGE, "0,500,1"),
        [] { return Engine::get_singleton()->get_max_fps(); },
        [](const Variant& v) { Engine::get_singleton()->set_max_fps(v); },
        [] { return String("Max FPS (0 = no cap)"); });
    display_->add(
        PropertyInfo(Variant::INT, "window_mode", PROPERTY_HINT_ENUM,
                     "Windowed:0,Maximized:2,Fullscreen:3,Exclusive fullscreen:4"),
        [ds] { return static_cast<int64_t>(ds->window_get_mode()); },
        [ds](const Variant& v) { ds->window_set_mode(static_cast<DisplayServer::WindowMode>(int64_t(v))); });

    // RenderingServer's global quality settings: setters with no getters, so
    // the values are kept here, seeded from the project settings.
    renderer_.instantiate();
    const auto row = [this](const char* name, const PropertyInfo& info, const Variant& initial,
                            std::function<void()> apply, const char* label = nullptr) {
        renderer_state_[name] = initial;
        const String key = name;
        renderer_->add(
            info, [this, key] { return renderer_state_.has(key) ? renderer_state_[key] : Variant(); },
            [this, key, apply](const Variant& v) {
                renderer_state_[key] = v;
                apply();
            },
            label ? TunerSettings::LabelFn([l = String(label)] { return l; }) : TunerSettings::LabelFn());
    };
    const auto st = [this](const char* key) { return renderer_state_[key]; };

    renderer_->add_group("Shadows");
    const auto sun_atlas = [rs, st] { rs->directional_shadow_atlas_set_size(st("sun_shadow_size"), st("sun_shadow_16_bits")); };
    row("sun_shadow_size", PropertyInfo(Variant::INT, "sun_shadow_size", PROPERTY_HINT_ENUM, "1024:1024,2048:2048,4096:4096,8192:8192,16384:16384"),
        2048, sun_atlas, "Sun shadow atlas");
    row("sun_shadow_16_bits", PropertyInfo(Variant::BOOL, "sun_shadow_16_bits"), true, sun_atlas, "Sun shadow 16-bit");
    const char* kShadowQuality = "Hard,Very Low,Low,Medium,High,Ultra";
    row("sun_soft_shadows", PropertyInfo(Variant::INT, "sun_soft_shadows", PROPERTY_HINT_ENUM, kShadowQuality), 2,
        [rs, st] { rs->directional_soft_shadow_filter_set_quality(static_cast<RenderingServer::ShadowQuality>(int64_t(st("sun_soft_shadows")))); },
        "Sun soft shadow filter");
    row("positional_soft_shadows", PropertyInfo(Variant::INT, "positional_soft_shadows", PROPERTY_HINT_ENUM, kShadowQuality), 2,
        [rs, st] { rs->positional_soft_shadow_filter_set_quality(static_cast<RenderingServer::ShadowQuality>(int64_t(st("positional_soft_shadows")))); },
        "Omni/spot soft shadow filter");

    const char* kQuality5 = "Very Low,Low,Medium,High,Ultra";
    renderer_->add_group("SSAO");
    const auto ssao = [rs, st] {
        rs->environment_set_ssao_quality(static_cast<RenderingServer::EnvironmentSSAOQuality>(int64_t(st("ssao_quality"))),
                                         st("ssao_half_size"), st("ssao_adaptive_target"), st("ssao_blur_passes"),
                                         st("ssao_fadeout_from"), st("ssao_fadeout_to"));
    };
    row("ssao_quality", PropertyInfo(Variant::INT, "ssao_quality", PROPERTY_HINT_ENUM, kQuality5), setting("rendering/environment/ssao/quality", 2), ssao);
    row("ssao_half_size", PropertyInfo(Variant::BOOL, "ssao_half_size"), setting("rendering/environment/ssao/half_size", true), ssao);
    row("ssao_adaptive_target", PropertyInfo(Variant::FLOAT, "ssao_adaptive_target", PROPERTY_HINT_RANGE, "0,1,0.01"), setting("rendering/environment/ssao/adaptive_target", 0.5), ssao);
    row("ssao_blur_passes", PropertyInfo(Variant::INT, "ssao_blur_passes", PROPERTY_HINT_RANGE, "0,6,1"), setting("rendering/environment/ssao/blur_passes", 2), ssao);
    row("ssao_fadeout_from", PropertyInfo(Variant::FLOAT, "ssao_fadeout_from", PROPERTY_HINT_RANGE, "0,512,0.1,or_greater"), setting("rendering/environment/ssao/fadeout_from", 50.0), ssao);
    row("ssao_fadeout_to", PropertyInfo(Variant::FLOAT, "ssao_fadeout_to", PROPERTY_HINT_RANGE, "64,65536,0.1,or_greater"), setting("rendering/environment/ssao/fadeout_to", 300.0), ssao);

    renderer_->add_group("SSIL");
    const auto ssil = [rs, st] {
        rs->environment_set_ssil_quality(static_cast<RenderingServer::EnvironmentSSILQuality>(int64_t(st("ssil_quality"))),
                                         st("ssil_half_size"), st("ssil_adaptive_target"), st("ssil_blur_passes"),
                                         st("ssil_fadeout_from"), st("ssil_fadeout_to"));
    };
    row("ssil_quality", PropertyInfo(Variant::INT, "ssil_quality", PROPERTY_HINT_ENUM, kQuality5), setting("rendering/environment/ssil/quality", 2), ssil);
    row("ssil_half_size", PropertyInfo(Variant::BOOL, "ssil_half_size"), setting("rendering/environment/ssil/half_size", true), ssil);
    row("ssil_adaptive_target", PropertyInfo(Variant::FLOAT, "ssil_adaptive_target", PROPERTY_HINT_RANGE, "0,1,0.01"), setting("rendering/environment/ssil/adaptive_target", 0.5), ssil);
    row("ssil_blur_passes", PropertyInfo(Variant::INT, "ssil_blur_passes", PROPERTY_HINT_RANGE, "0,6,1"), setting("rendering/environment/ssil/blur_passes", 4), ssil);
    row("ssil_fadeout_from", PropertyInfo(Variant::FLOAT, "ssil_fadeout_from", PROPERTY_HINT_RANGE, "0,512,0.1,or_greater"), setting("rendering/environment/ssil/fadeout_from", 50.0), ssil);
    row("ssil_fadeout_to", PropertyInfo(Variant::FLOAT, "ssil_fadeout_to", PROPERTY_HINT_RANGE, "64,65536,0.1,or_greater"), setting("rendering/environment/ssil/fadeout_to", 300.0), ssil);

    renderer_->add_group("Screen-space reflections");
    row("ssr_half_size", PropertyInfo(Variant::BOOL, "ssr_half_size"), false, [rs, st] { rs->environment_set_ssr_half_size(st("ssr_half_size")); });

    renderer_->add_group("Glow");
    row("glow_bicubic_upscale", PropertyInfo(Variant::BOOL, "glow_bicubic_upscale"),
        int64_t(setting("rendering/environment/glow/upscale_mode", 1)) == 1,
        [rs, st] { rs->environment_glow_set_use_bicubic_upscale(st("glow_bicubic_upscale")); });

    renderer_->add_group("Global illumination");
    row("gi_half_resolution", PropertyInfo(Variant::BOOL, "gi_half_resolution"), setting("rendering/global_illumination/gi/use_half_resolution", false),
        [rs, st] { rs->gi_set_use_half_resolution(st("gi_half_resolution")); }, "GI at half resolution");
    row("voxel_gi_quality", PropertyInfo(Variant::INT, "voxel_gi_quality", PROPERTY_HINT_ENUM, "Low,High"), setting("rendering/global_illumination/voxel_gi/quality", 0),
        [rs, st] { rs->voxel_gi_set_quality(static_cast<RenderingServer::VoxelGIQuality>(int64_t(st("voxel_gi_quality")))); }, "VoxelGI quality");
    row("sdfgi_ray_count", PropertyInfo(Variant::INT, "sdfgi_ray_count", PROPERTY_HINT_ENUM, "4,8,16,32,64,96,128"), setting("rendering/global_illumination/sdfgi/probe_ray_count", 1),
        [rs, st] { rs->environment_set_sdfgi_ray_count(static_cast<RenderingServer::EnvironmentSDFGIRayCount>(int64_t(st("sdfgi_ray_count")))); }, "SDFGI rays per probe");
    row("sdfgi_frames_to_converge", PropertyInfo(Variant::INT, "sdfgi_frames_to_converge", PROPERTY_HINT_ENUM, "5,10,15,20,25,30"), setting("rendering/global_illumination/sdfgi/frames_to_converge", 5),
        [rs, st] { rs->environment_set_sdfgi_frames_to_converge(static_cast<RenderingServer::EnvironmentSDFGIFramesToConverge>(int64_t(st("sdfgi_frames_to_converge")))); }, "SDFGI frames to converge");
    row("sdfgi_frames_to_update_light", PropertyInfo(Variant::INT, "sdfgi_frames_to_update_light", PROPERTY_HINT_ENUM, "1,2,4,8,16"), setting("rendering/global_illumination/sdfgi/frames_to_update_lights", 2),
        [rs, st] { rs->environment_set_sdfgi_frames_to_update_light(static_cast<RenderingServer::EnvironmentSDFGIFramesToUpdateLight>(int64_t(st("sdfgi_frames_to_update_light")))); }, "SDFGI frames to update lights");

    renderer_->add_group("Volumetric fog");
    const auto fog_size = [rs, st] { rs->environment_set_volumetric_fog_volume_size(st("volumetric_fog_size"), st("volumetric_fog_depth")); };
    row("volumetric_fog_size", PropertyInfo(Variant::INT, "volumetric_fog_size", PROPERTY_HINT_RANGE, "16,512,1"), 64, fog_size, "Froxel grid width/height");
    row("volumetric_fog_depth", PropertyInfo(Variant::INT, "volumetric_fog_depth", PROPERTY_HINT_RANGE, "16,512,1"), 64, fog_size, "Froxel grid depth");
    row("volumetric_fog_filter", PropertyInfo(Variant::BOOL, "volumetric_fog_filter"), int64_t(setting("rendering/environment/volumetric_fog/use_filter", 1)) != 0,
        [rs, st] { rs->environment_set_volumetric_fog_filter_active(st("volumetric_fog_filter")); }, "Froxel filter");

    renderer_->add_group("Materials");
    row("subsurface_quality", PropertyInfo(Variant::INT, "subsurface_quality", PROPERTY_HINT_ENUM, "Disabled,Low,Medium,High"), 0,
        [rs, st] { rs->sub_surface_scattering_set_quality(static_cast<RenderingServer::SubSurfaceScatteringQuality>(int64_t(st("subsurface_quality")))); }, "Subsurface scattering quality");
    const auto sss_scale = [rs, st] { rs->sub_surface_scattering_set_scale(st("subsurface_scale"), st("subsurface_depth_scale")); };
    row("subsurface_scale", PropertyInfo(Variant::FLOAT, "subsurface_scale", PROPERTY_HINT_RANGE, "0,1,0.001"), setting("rendering/environment/subsurface_scattering/subsurface_scattering_scale", 0.05), sss_scale);
    row("subsurface_depth_scale", PropertyInfo(Variant::FLOAT, "subsurface_depth_scale", PROPERTY_HINT_RANGE, "0,1,0.001"), setting("rendering/environment/subsurface_scattering/subsurface_scattering_depth_scale", 0.01), sss_scale);
    const auto limiter = [rs, st] { rs->screen_space_roughness_limiter_set_active(st("roughness_limiter"), st("roughness_limiter_amount"), st("roughness_limiter_limit")); };
    row("roughness_limiter", PropertyInfo(Variant::BOOL, "roughness_limiter"), setting("rendering/anti_aliasing/screen_space_roughness_limiter/enabled", true), limiter, "Specular roughness limiter");
    row("roughness_limiter_amount", PropertyInfo(Variant::FLOAT, "roughness_limiter_amount", PROPERTY_HINT_RANGE, "0.01,4,0.01"), setting("rendering/anti_aliasing/screen_space_roughness_limiter/amount", 0.25), limiter);
    row("roughness_limiter_limit", PropertyInfo(Variant::FLOAT, "roughness_limiter_limit", PROPERTY_HINT_RANGE, "0.01,1,0.01"), setting("rendering/anti_aliasing/screen_space_roughness_limiter/limit", 0.18), limiter);

}

void RenderTuner::build_layout() {
    auto* panel = memnew(PanelContainer);
    panel->add_theme_stylebox_override("panel", panel_style());
    panel->set_anchors_and_offsets_preset(PRESET_RIGHT_WIDE);
    panel->set_offset(SIDE_LEFT, -kPanelWidth);
    panel->set_mouse_filter(MOUSE_FILTER_STOP);
    add_child(panel);
    panel_ = panel;

    auto* column = memnew(VBoxContainer);
    column->add_theme_constant_override("separation", 6);
    panel->add_child(column);

    auto* header = memnew(HBoxContainer);
    column->add_child(header);
    auto* title = memnew(Label);
    title->set_text("Render tuning  (F1 hides)");
    title->set_h_size_flags(SIZE_EXPAND_FILL);
    title->set_clip_text(true);
    header->add_child(title);
    auto* rescan = memnew(Button);
    rescan->set_text("Rescan");
    rescan->set_tooltip_text("Find the scene's lights, environments and materials again");
    rescan->connect("pressed", callable_mp(this, &RenderTuner::rebuild));
    header->add_child(rescan);
    auto* reset = memnew(Button);
    reset->set_text("Reset all");
    reset->set_tooltip_text("Put every setting back to how the scene started");
    reset->connect("pressed", callable_mp(this, &RenderTuner::reset_all));
    header->add_child(reset);

    readout_ = memnew(Label);
    readout_->add_theme_font_size_override("font_size", kFontSize);
    readout_->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    column->add_child(readout_);
    graph_ = memnew(FrameGraph);
    graph_->set_window(&window_);
    graph_->set_custom_minimum_size(Vector2(0, 110));
    column->add_child(graph_);
    passes_ = memnew(Label);
    passes_->add_theme_font_size_override("font_size", kFontSize - 1);
    passes_->add_theme_color_override("font_color", Color(0.75f, 0.75f, 0.8f));
    passes_->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    column->add_child(passes_);

    auto* scroll = memnew(ScrollContainer);
    scroll->set_v_size_flags(SIZE_EXPAND_FILL);
    scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
    column->add_child(scroll);
    section_box_ = memnew(VBoxContainer);
    section_box_->set_h_size_flags(SIZE_EXPAND_FILL);
    section_box_->add_theme_constant_override("separation", 4);
    scroll->add_child(section_box_);
}

FoldableContainer* RenderTuner::add_section(VBoxContainer* parent, const String& title, Builder build, bool folded) {
    auto* section = memnew(FoldableContainer);
    section->set_title(title);
    section->add_theme_font_size_override("font_size", kFontSize + 1);
    const int64_t index = static_cast<int64_t>(builders_.size());
    builders_.push_back(std::move(build));
    sections_.push_back(section);
    auto* box = memnew(VBoxContainer);
    box->add_theme_constant_override("separation", 2);
    section->add_child(box);
    parent->add_child(section);
    const bool open = !folded || open_titles_.has(title);
    section->set_folded(!open);
    section->connect("folding_changed", callable_mp(this, &RenderTuner::on_folding_changed).bind(index));
    if (open) on_folding_changed(false, index);
    return section;
}

// Sections build their rows the first time they open, so a scene with many
// lights and materials costs nothing until someone looks.
void RenderTuner::on_folding_changed(bool folded, int64_t index) {
    if (index < 0 || index >= static_cast<int64_t>(sections_.size())) return;
    FoldableContainer* section = sections_[static_cast<std::size_t>(index)];
    if (folded) {
        open_titles_.erase(section->get_title());
        return;
    }
    open_titles_[section->get_title()] = true;
    Builder build = std::move(builders_[static_cast<std::size_t>(index)]);
    builders_[static_cast<std::size_t>(index)] = nullptr;
    if (build) build(Object::cast_to<VBoxContainer>(section->get_child(0)));
}

void RenderTuner::scan() {
    for (auto* list : {&environments_, &camera_attributes_, &compositor_effects_, &lights_, &gi_nodes_, &fog_volumes_,
                       &particles_, &decals_, &shader_materials_})
        list->clear();
    HashMap<uint64_t, bool> seen;
    titles_.clear();
    const auto add_unique = [&seen](std::vector<ObjectID>& list, Object* object) {
        if (!object) return;
        const uint64_t id = object->get_instance_id();
        if (seen.has(id)) return;
        seen[id] = true;
        list.push_back(ObjectID(id));
    };
    const auto add_material = [&](const Ref<Material>& material, Node* owner) {
        if (!Object::cast_to<ShaderMaterial>(material.ptr())) return;
        if (!titles_.has(material->get_instance_id())) titles_[material->get_instance_id()] = owner->get_name();
        add_unique(shader_materials_, material.ptr());
    };
    const auto add_compositor = [&](const Ref<Compositor>& compositor) {
        if (compositor.is_null()) return;
        const TypedArray<CompositorEffect> effects = compositor->get_compositor_effects();
        for (int64_t i = 0; i < effects.size(); ++i) add_unique(compositor_effects_, Object::cast_to<Object>(effects[i]));
    };

    std::vector<Node*> stack{get_tree()->get_root()};
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        if (node == this) continue;
        for (int32_t i = node->get_child_count(true) - 1; i >= 0; --i) stack.push_back(node->get_child(i, true));
        const auto name_after = [this, node](const Ref<Resource>& r) {
            if (r.is_valid() && !titles_.has(r->get_instance_id())) titles_[r->get_instance_id()] = node->get_name();
        };
        if (auto* world = Object::cast_to<WorldEnvironment>(node)) {
            name_after(world->get_environment());
            name_after(world->get_camera_attributes());
            add_unique(environments_, world->get_environment().ptr());
            add_unique(camera_attributes_, world->get_camera_attributes().ptr());
            add_compositor(world->get_compositor());
        } else if (auto* camera = Object::cast_to<Camera3D>(node)) {
            name_after(camera->get_environment());
            name_after(camera->get_attributes());
            add_unique(environments_, camera->get_environment().ptr());
            add_unique(camera_attributes_, camera->get_attributes().ptr());
            add_compositor(camera->get_compositor());
        } else if (Object::cast_to<Light3D>(node)) {
            add_unique(lights_, node);
        } else if (Object::cast_to<VoxelGI>(node) || Object::cast_to<LightmapGI>(node) ||
                   Object::cast_to<ReflectionProbe>(node)) {
            add_unique(gi_nodes_, node);
        } else if (auto* fog = Object::cast_to<FogVolume>(node)) {
            add_unique(fog_volumes_, node);
            add_material(fog->get_material(), node);
        } else if (Object::cast_to<Decal>(node)) {
            add_unique(decals_, node);
        }
        if (auto* particles = Object::cast_to<GPUParticles3D>(node)) {
            add_unique(particles_, node);
            add_material(particles->get_process_material(), node);
        } else if (Object::cast_to<CPUParticles3D>(node)) {
            add_unique(particles_, node);
        }
        if (auto* geometry = Object::cast_to<GeometryInstance3D>(node)) {
            add_material(geometry->get_material_override(), node);
            add_material(geometry->get_material_overlay(), node);
            if (auto* mesh = Object::cast_to<MeshInstance3D>(node)) {
                for (int32_t s = 0; s < mesh->get_surface_override_material_count(); ++s)
                    add_material(mesh->get_active_material(s), node);
            }
        }
    }
}

void RenderTuner::rebuild() {
    if (!section_box_) return;
    scan();
    bindings_.clear();
    builders_.clear();
    sections_.clear();
    for (int32_t i = section_box_->get_child_count() - 1; i >= 0; --i) {
        Node* child = section_box_->get_child(i);
        section_box_->remove_child(child);
        child->queue_free();
    }

    VBoxContainer* box = section_box_;
    add_section(box, "Custom rendering (on/off)", [this](VBoxContainer* b) { add_properties(b, custom_.ptr(), {}); },
                false);
    add_section(box, "Display and frame pacing", [this](VBoxContainer* b) { add_properties(b, display_.ptr(), {}); });
    add_section(box, "Viewport: resolution, AA, debug views", [this](VBoxContainer* b) {
        add_properties(b, get_tree()->get_root(), [](const String& n) { return viewport_property(n); });
    });
    add_section(box, "Renderer quality (shadows, SSAO, SSIL, GI, fog)",
                [this](VBoxContainer* b) { add_properties(b, renderer_.ptr(), {}); });

    // A resource's file name, or the node it was found on when it is embedded in a scene.
    const auto resource_title = [this](const String& kind, Object* object) {
        const Resource* r = Object::cast_to<Resource>(object);
        const String path = r ? r->get_path() : String();
        const uint64_t id = object->get_instance_id();
        String name = path.get_file();
        if (path.is_empty() || path.contains("::"))
            name = titles_.has(id) ? "on " + titles_[id] : String("#") + String::num_uint64(id);
        return kind + String(": ") + name;
    };
    const auto skip_resource = [](const String& n) { return !n.begins_with("resource_") && n != "script"; };
    for (const ObjectID id : environments_) {
        if (Object* env = ObjectDB::get_instance(id))
            add_section(box, resource_title("Environment", env), [this, id, skip_resource](VBoxContainer* b) {
                if (Object* o = ObjectDB::get_instance(id)) add_properties(b, o, skip_resource);
            });
    }
    for (const ObjectID id : camera_attributes_) {
        if (Object* attributes = ObjectDB::get_instance(id))
            add_section(box, resource_title("Camera / auto exposure", attributes),
                        [this, id, skip_resource](VBoxContainer* b) {
                            if (Object* o = ObjectDB::get_instance(id)) add_properties(b, o, skip_resource);
                        });
    }
    if (!compositor_effects_.empty()) {
        add_section(box, "Custom effects (compositor)", [this](VBoxContainer* b) {
            for (const ObjectID id : compositor_effects_) {
                Object* effect = ObjectDB::get_instance(id);
                if (!effect) continue;
                add_section(b, effect->get_class(), [this, id](VBoxContainer* inner) {
                    if (Object* o = ObjectDB::get_instance(id))
                        add_properties(inner, o, [](const String& n) {
                            return !n.begins_with("resource_") && n != "script" && !n.begins_with("access_resolved") &&
                                   !n.begins_with("needs_") && n != "effect_callback_type";
                        });
                });
            }
        });
    }
    const auto node_group = [this, box](const String& title, const std::vector<ObjectID>* nodes, const String& stop) {
        if (nodes->empty()) return;
        add_section(box, title + String(" (") + String::num_int64(static_cast<int64_t>(nodes->size())) + ")",
                    [this, nodes, stop](VBoxContainer* b) { add_node_sections(b, *nodes, stop); });
    };
    node_group("Lights", &lights_, "VisualInstance3D");
    node_group("GI and reflection probes", &gi_nodes_, "VisualInstance3D");
    node_group("Fog volumes", &fog_volumes_, "VisualInstance3D");
    node_group("Particles", &particles_, "GeometryInstance3D");
    node_group("Decals", &decals_, "VisualInstance3D");
    if (!shader_materials_.empty()) {
        add_section(box, "Shader parameters (" + String::num_int64(static_cast<int64_t>(shader_materials_.size())) + " materials)",
                    [this, resource_title](VBoxContainer* b) {
                        for (const ObjectID id : shader_materials_) {
                            Object* material = ObjectDB::get_instance(id);
                            if (!material) continue;
                            add_section(b, resource_title("Material", material), [this, id](VBoxContainer* inner) {
                                if (Object* o = ObjectDB::get_instance(id))
                                    add_properties(inner, o, [](const String& n) {
                                        return n.begins_with("shader_parameter/") || n == "render_priority";
                                    });
                            });
                        }
                    });
    }
}

void RenderTuner::add_node_sections(VBoxContainer* box, const std::vector<ObjectID>& nodes, const String& stop_class) {
    ClassDBSingleton* db = ClassDBSingleton::get_singleton();
    HashMap<String, bool> inherited;
    const TypedArray<Dictionary> base = db->class_get_property_list(stop_class, false);
    for (int64_t i = 0; i < base.size(); ++i) inherited[String(Dictionary(base[i])["name"])] = true;
    for (const ObjectID id : nodes) {
        auto* node = Object::cast_to<Node>(ObjectDB::get_instance(id));
        if (!node) continue;
        add_section(box, String(node->get_name()) + "  [" + node->get_class() + "]",
                    [this, id, inherited](VBoxContainer* b) {
                        Object* o = ObjectDB::get_instance(id);
                        if (!o) return;
                        add_properties(b, o, [](const String& n) { return n == "visible"; });
                        add_properties(b, o, [&inherited](const String& n) { return !inherited.has(n) && n != "script"; });
                    });
    }
}

void RenderTuner::add_properties(VBoxContainer* box, Object* object, const Filter& filter) {
    const TypedArray<Dictionary> properties = object->get_property_list();
    const auto* settings = Object::cast_to<TunerSettings>(object);
    String group, group_prefix, shown_group;
    for (int64_t i = 0; i < properties.size(); ++i) {
        const Dictionary p = properties[i];
        const String name = p["name"];
        const int64_t usage = p["usage"];
        if (usage & (PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP)) {
            group = name;
            group_prefix = p["hint_string"];
            continue;
        }
        if (usage & PROPERTY_USAGE_CATEGORY) continue;
        if (!(usage & PROPERTY_USAGE_EDITOR) || (usage & PROPERTY_USAGE_READ_ONLY)) continue;
        const int64_t type = p["type"];
        if (type != Variant::BOOL && type != Variant::INT && type != Variant::FLOAT && type != Variant::COLOR) continue;
        const int64_t hint = p["hint"];
        if (type == Variant::INT && hint != PROPERTY_HINT_NONE && hint != PROPERTY_HINT_RANGE &&
            hint != PROPERTY_HINT_ENUM)
            continue;  // flags, layers and the like
        if (filter && !filter(name)) continue;
        if (!group.is_empty() && group != shown_group) {
            auto* heading = memnew(Label);
            heading->set_text(group);
            heading->add_theme_font_size_override("font_size", kFontSize);
            heading->add_theme_color_override("font_color", Color(0.95f, 0.78f, 0.4f));
            box->add_child(heading);
            shown_group = group;
        }
        String label;
        if (settings) {
            label = settings->label_of(name);
        } else {
            String shown = name.trim_prefix("shader_parameter/");
            if (!group_prefix.is_empty() && shown.begins_with(group_prefix) && shown.length() > group_prefix.length())
                shown = shown.substr(group_prefix.length());
            label = shown.capitalize();
        }
        add_property_row(box, object, p, label);
    }
}

void RenderTuner::remember_original(Object* object, const StringName& property) {
    const String key = String::num_uint64(object->get_instance_id()) + ":" + String(property);
    if (seen_.has(key)) return;
    seen_[key] = true;
    originals_.push_back({ObjectID(object->get_instance_id()), property, object->get(property)});
}

void RenderTuner::add_property_row(VBoxContainer* box, Object* object, const Dictionary& property, const String& label) {
    const StringName name = property["name"];
    const auto type = static_cast<Variant::Type>(int64_t(property["type"]));
    const int64_t hint = property["hint"];
    const String hint_string = property["hint_string"];
    const Variant value = object->get(name);
    remember_original(object, name);

    auto* row = memnew(HBoxContainer);
    box->add_child(row);
    auto* text = memnew(Label);
    text->set_text(label);
    text->set_tooltip_text(String(name));
    text->set_mouse_filter(MOUSE_FILTER_PASS);
    text->set_h_size_flags(SIZE_EXPAND_FILL);
    text->set_clip_text(true);
    text->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
    text->add_theme_font_size_override("font_size", kFontSize);
    row->add_child(text);

    Binding b;
    b.object = ObjectID(object->get_instance_id());
    b.property = name;
    b.type = type;
    b.text = text;
    const int64_t index = static_cast<int64_t>(bindings_.size());

    if (type == Variant::BOOL) {
        auto* check = memnew(CheckBox);
        check->set_pressed_no_signal(value);
        // Spelled out: an empty box is hard to see on the dark panel.
        check->set_text(bool(value) ? "On" : "Off");
        check->connect("toggled", callable_mp(this, &RenderTuner::on_toggled).bind(index));
        row->add_child(check);
        b.control = check;
    } else if (type == Variant::COLOR) {
        auto* picker = memnew(ColorPickerButton);
        picker->set_pick_color(value);
        picker->set_custom_minimum_size(Vector2(90, 0));
        picker->connect("color_changed", callable_mp(this, &RenderTuner::on_color_changed).bind(index));
        row->add_child(picker);
        b.control = picker;
    } else if (type == Variant::INT && hint == PROPERTY_HINT_ENUM) {
        auto* options = memnew(OptionButton);
        options->add_theme_font_size_override("font_size", kFontSize);
        for (const tuner::EnumEntry& e : tuner::parse_enum_hint(hint_string.utf8().get_data())) {
            if (e.value < 0) continue;  // OptionButton ids must be >= 0
            options->add_item(String::utf8(e.label.c_str()), static_cast<int32_t>(e.value));
        }
        options->select(options->get_item_index(static_cast<int32_t>(int64_t(value))));
        options->set_h_size_flags(SIZE_EXPAND_FILL);
        options->set_clip_text(true);
        options->connect("item_selected", callable_mp(this, &RenderTuner::on_item_selected).bind(index));
        row->add_child(options);
        b.control = options;
    } else {
        const bool integer = type == Variant::INT;
        b.integer_range = integer;
        const tuner::RangeHint range =
            hint == PROPERTY_HINT_RANGE ? tuner::parse_range_hint(hint_string.utf8().get_data()) : tuner::RangeHint{};
        auto* spin = memnew(SpinBox);
        spin->add_theme_font_size_override("font_size", kFontSize);
        spin->set_custom_minimum_size(Vector2(86, 0));
        spin->set_select_all_on_focus(true);
        if (range.valid) {
            const double step = range.step > 0.0 ? range.step : tuner::default_step(range.min, range.max, integer);
            auto* slider = memnew(HSlider);
            slider->set_h_size_flags(SIZE_EXPAND_FILL);
            slider->set_custom_minimum_size(Vector2(90, 0));
            slider->set_v_size_flags(SIZE_SHRINK_CENTER);
            for (Range* r : {static_cast<Range*>(slider), static_cast<Range*>(spin)}) {
                r->set_min(range.min);
                r->set_max(range.max);
                r->set_step(step);
                r->set_allow_greater(range.or_greater);
                r->set_allow_lesser(range.or_less);
                if (range.exp && range.min > 0.0) r->set_exp_ratio(true);
            }
            // Typed values may go past the slider's ends when the hint allows it.
            slider->set_value_no_signal(value);
            slider->connect("value_changed", callable_mp(this, &RenderTuner::on_slider_changed).bind(index));
            row->add_child(slider);
            b.control = slider;
        } else {
            spin->set_min(-100000.0);
            spin->set_max(100000.0);
            spin->set_step(integer ? 1.0 : 0.001);
        }
        spin->set_value_no_signal(value);
        spin->connect("value_changed", callable_mp(this, &RenderTuner::on_spin_changed).bind(index));
        row->add_child(spin);
        b.spin = spin;
        if (!b.control) b.control = spin;
    }
    bindings_.push_back(b);
}

void RenderTuner::apply_binding(int64_t index, const Variant& value) {
    if (index < 0 || index >= static_cast<int64_t>(bindings_.size())) return;
    const Binding& b = bindings_[static_cast<std::size_t>(index)];
    Object* object = ObjectDB::get_instance(b.object);
    if (!object) return;
    object->set(b.property, value);
}

void RenderTuner::on_toggled(bool on, int64_t index) {
    if (index >= 0 && index < static_cast<int64_t>(bindings_.size()))
        if (auto* check = Object::cast_to<CheckBox>(bindings_[static_cast<std::size_t>(index)].control))
            check->set_text(on ? "On" : "Off");
    apply_binding(index, on);
}

void RenderTuner::on_item_selected(int64_t item, int64_t index) {
    if (index < 0 || index >= static_cast<int64_t>(bindings_.size())) return;
    auto* options = Object::cast_to<OptionButton>(bindings_[static_cast<std::size_t>(index)].control);
    if (options) apply_binding(index, static_cast<int64_t>(options->get_item_id(static_cast<int32_t>(item))));
}

void RenderTuner::on_color_changed(const Color& color, int64_t index) { apply_binding(index, color); }

void RenderTuner::on_slider_changed(double value, int64_t index) {
    if (index < 0 || index >= static_cast<int64_t>(bindings_.size())) return;
    const Binding& b = bindings_[static_cast<std::size_t>(index)];
    if (auto* spin = Object::cast_to<SpinBox>(b.spin)) spin->set_value_no_signal(value);
    apply_binding(index, b.integer_range ? Variant(static_cast<int64_t>(std::llround(value))) : Variant(value));
}

void RenderTuner::on_spin_changed(double value, int64_t index) {
    if (index < 0 || index >= static_cast<int64_t>(bindings_.size())) return;
    const Binding& b = bindings_[static_cast<std::size_t>(index)];
    if (b.control != b.spin) {
        if (auto* slider = Object::cast_to<Range>(b.control)) slider->set_value_no_signal(value);
    }
    apply_binding(index, b.integer_range ? Variant(static_cast<int64_t>(std::llround(value))) : Variant(value));
}

// Shows what each setting holds now: other settings and the
// engine (a resize changes the 3D scale) can change values the panel shows.
void RenderTuner::sync_controls() {
    for (const Binding& b : bindings_) {
        Object* object = ObjectDB::get_instance(b.object);
        if (!object) continue;
        const Variant value = object->get(b.property);
        if (const auto* settings = Object::cast_to<TunerSettings>(object); settings && b.text)
            b.text->set_text(settings->label_of(b.property));
        if (auto* check = Object::cast_to<CheckBox>(b.control)) {
            if (check->is_pressed() != bool(value)) check->set_pressed_no_signal(value);
            check->set_text(bool(value) ? "On" : "Off");
        } else if (auto* options = Object::cast_to<OptionButton>(b.control)) {
            const int32_t item = options->get_item_index(static_cast<int32_t>(int64_t(value)));
            if (options->get_selected() != item) options->select(item);
        } else if (auto* picker = Object::cast_to<ColorPickerButton>(b.control)) {
            if (!picker->get_popup()->is_visible()) picker->set_pick_color(value);
        } else {
            const double v = value;
            for (Control* c : {b.control, b.spin}) {
                auto* r = Object::cast_to<Range>(c);
                if (r && !r->has_focus() && std::abs(r->get_value() - v) > 1e-9) r->set_value_no_signal(v);
            }
        }
    }
}

void RenderTuner::reset_all() {
    for (const Original& o : originals_) {
        if (Object* object = ObjectDB::get_instance(o.object)) object->set(o.property, o.value);
    }
    sync_controls();
}

void RenderTuner::_process(double delta) {
    const uint64_t now = Time::get_singleton()->get_ticks_usec();
    RenderingServer* rs = RenderingServer::get_singleton();
    const RID vp = get_viewport()->get_viewport_rid();
    tuner::FrameSample s;
    s.frame_ms = static_cast<double>(now - last_frame_usec_) / 1000.0;
    // The render values are the previous frame's, the newest the engine has.
    s.cpu_ms = rs->get_frame_setup_time_cpu() + rs->viewport_get_measured_render_time_cpu(vp);
    s.gpu_ms = rs->viewport_get_measured_render_time_gpu(vp);
    last_frame_usec_ = now;
    window_.push(s);
    if (panel_ && panel_->is_visible()) graph_->queue_redraw();

    if (!shot_path_.is_empty()) {
        shot_timer_ += delta;
        if (shot_timer_ >= kShotSeconds) {
            save_screenshot(shot_path_);
            shot_path_ = String();
            get_tree()->quit();
        }
    }
    run_timer_ += delta;
    if (quit_seconds_ > 0.0 && run_timer_ >= quit_seconds_) {
        quit_seconds_ = 0.0;
        get_tree()->quit();
        return;
    }
    if (print_seconds_ > 0.0) {
        print_timer_ += delta;
        if (print_timer_ >= print_seconds_) {
            print_timer_ = 0.0;
            update_readout();
            UtilityFunctions::print("RenderTuner ", String::num(run_timer_, 1), " s\n", readout_->get_text(), "\n",
                                    passes_->get_text());
        }
    }
    readout_timer_ += delta;
    sync_timer_ += delta;
    if (readout_timer_ >= kReadoutSeconds) {
        readout_timer_ = 0.0;
        if (panel_ && panel_->is_visible()) update_readout();
    }
    if (sync_timer_ >= kSyncSeconds) {
        sync_timer_ = 0.0;
        if (panel_ && panel_->is_visible()) sync_controls();
    }
}

void RenderTuner::update_readout() {
    RenderingServer* rs = RenderingServer::get_singleton();
    Performance* perf = Performance::get_singleton();
    DisplayServer* ds = DisplayServer::get_singleton();
    Window* root = get_tree()->get_root();
    const RID vp = root->get_viewport_rid();

    std::size_t frames = 0;
    double covered = 0.0;
    while (frames < window_.size() && covered < kAverageMs) covered += window_.at(window_.size() - 1 - frames++).frame_ms;
    const tuner::WindowStats w = window_.stats(frames);

    const Ref<ViewportTexture> texture = root->get_texture();
    const Vector2i out = texture.is_valid() ? Vector2i(texture->get_size()) : root->get_size();
    const float scale = root->get_scaling_3d_scale();
    const Vector2i inner(static_cast<int>(std::lround(out.x * scale)), static_cast<int>(std::lround(out.y * scale)));
    const auto draws = [&](RenderingServer::ViewportRenderInfoType type) {
        return String::num_int64(rs->viewport_get_render_info(vp, type, RenderingServer::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME));
    };
    const double hz = ds->screen_get_refresh_rate(ds->window_get_current_screen());

    String text;
    text += rs->get_current_rendering_method() + " | " +
            rs->get_current_rendering_driver_name() + " | " + rs->get_video_adapter_name() + "\n";
    text += "Output " + String::num_int64(out.x) + "x" + String::num_int64(out.y) + " | 3D " +
            String::num_int64(inner.x) + "x" + String::num_int64(inner.y) + " (" + String::num(scale * 100.0, 0) +
            "%, " + scaling_name(root->get_scaling_3d_mode()) + (root->is_using_taa() ? ", TAA" : "") + ") | vsync " +
            vsync_name(ds->window_get_vsync_mode()) + (hz > 0.0 ? " | " + String::num(hz, 0) + " Hz" : String()) + "\n";
    text += String("MSAA 3D ") + msaa_name(root->get_msaa_3d()) + " | MSAA 2D " + msaa_name(root->get_msaa_2d()) +
            " | screen-space AA " + aa_name(root->get_screen_space_aa()) + (root->is_using_hdr_2d() ? " | HDR 2D" : "") +
            "\n";
    text += "FPS " + String::num(w.fps, 1) + " | frame " + ms(w.frame_mean) + " (p99 " + ms(w.frame_p99) +
            ", max " + ms(w.frame_max) + ")\n";
    text += "GPU " + (w.gpu_mean > 0.0 ? ms(w.gpu_mean) + " (p99 " + ms(w.gpu_p99) + ")" : String("n/a")) +
            " | CPU render " + ms(w.cpu_mean) + "\n";
    text += "Draw calls " + String::num_int64(int64_t(perf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME))) +
            " (scene " + draws(RenderingServer::VIEWPORT_RENDER_INFO_TYPE_VISIBLE) + ", shadow " +
            draws(RenderingServer::VIEWPORT_RENDER_INFO_TYPE_SHADOW) + ", 2D " +
            draws(RenderingServer::VIEWPORT_RENDER_INFO_TYPE_CANVAS) + ") | objects " +
            String::num_int64(int64_t(perf->get_monitor(Performance::RENDER_TOTAL_OBJECTS_IN_FRAME))) + " | triangles " +
            String::num(perf->get_monitor(Performance::RENDER_TOTAL_PRIMITIVES_IN_FRAME) / 1e6, 2) + " M\n";
    text += "VRAM " + mib(perf->get_monitor(Performance::RENDER_VIDEO_MEM_USED)) + " (textures " +
            mib(perf->get_monitor(Performance::RENDER_TEXTURE_MEM_USED)) + ", buffers " +
            mib(perf->get_monitor(Performance::RENDER_BUFFER_MEM_USED)) + ")";
    readout_->set_text(text);

    // Per-pass GPU times exist only when Godot captures timestamps
    // (--gpu-profile); without it, leave the RenderingDevice alone (it may
    // belong to a separate render thread).
    if (!gpu_profile_) {
        passes_->set_text("Per-pass GPU times: start Godot with --gpu-profile.");
        return;
    }
    RenderingDevice* rd = rs->get_rendering_device();
    const uint32_t count = rd ? rd->get_captured_timestamps_count() : 0;
    if (count < 2) {
        passes_->set_text("Per-pass GPU times: start Godot with --gpu-profile.");
        return;
    }
    // Godot marks each viewport's start; the span after it is the render up
    // to the next marker. --gpu-profile adds a marker per pass.
    std::vector<std::pair<double, String>> passes;
    passes.reserve(static_cast<std::size_t>(count));
    for (uint32_t i = 0; i + 1 < count; ++i) {
        const double ns = static_cast<double>(rd->get_captured_timestamp_gpu_time(i + 1) - rd->get_captured_timestamp_gpu_time(i));
        const String name = rd->get_captured_timestamp_name(i);
        passes.emplace_back(ns / 1e6, name.begins_with("vp_begin") ? String("viewport render") : name);
    }
    const std::size_t shown = std::min<std::size_t>(8, passes.size());
    std::partial_sort(passes.begin(), passes.begin() + static_cast<std::ptrdiff_t>(shown), passes.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });
    String lines = "Slowest GPU passes, last frame (each pass with --gpu-profile):";
    for (std::size_t i = 0; i < shown; ++i) lines += "\n  " + ms(passes[i].first) + "  " + passes[i].second;
    passes_->set_text(lines);
}

void RenderTuner::set_custom(const String& key, bool on) {
    if (on) custom_off_.erase(key);
    else custom_off_.insert(key, true);
    std::vector<Node*> stack{get_tree()->get_root()};
    while (!stack.empty()) {
        Node* n = stack.back();
        stack.pop_back();
        apply_custom_to(n, key, on);
        for (int32_t i = 0; i < n->get_child_count(true); ++i) stack.push_back(n->get_child(i, true));
    }
}

void RenderTuner::apply_custom(Node* node) {
    for (const KeyValue<String, bool>& off : custom_off_) apply_custom_to(node, off.key, false);
}

void RenderTuner::apply_custom_to(Node* node, const String& key, bool on) {
    if (key == "timer") {
        // Match3Game is in another module; its timer_enabled property is bound.
        if (node->is_class("Match3Game")) node->set("timer_enabled", on);
        return;
    }
    if (key == "flicker") {
        if (auto* light = Object::cast_to<FlickerLight3D>(node)) light->set_process(on);
        return;
    }
    if (key == "effects") {
        auto* fx = Object::cast_to<Node3D>(node);
        if (fx && node->get_name() == StringName("Effects") && node->get_parent() &&
            node->get_parent()->is_class("BoardView3D"))
            fx->set_visible(on);
        return;
    }
    if (!draws_with(node, key)) return;
    bool surface = false;
    for (const char* name : kSurfaceShaders) surface = surface || key == name;
    if (surface) {
        auto* geometry = Object::cast_to<GeometryInstance3D>(node);
        if (!geometry) return;
        if (!on && !geometry->has_meta(kStockMeta)) {
            if (stock_.is_null()) {
                Ref<StandardMaterial3D> m;
                m.instantiate();
                m->set_albedo(Color(0.6f, 0.6f, 0.6f));
                stock_ = m;
            }
            geometry->set_meta(kStockMeta, geometry->get_material_override());
            geometry->set_material_override(stock_);
        } else if (on && geometry->has_meta(kStockMeta)) {
            geometry->set_material_override(geometry->get_meta(kStockMeta));
            geometry->remove_meta(kStockMeta);
        }
    } else if (auto* n3 = Object::cast_to<Node3D>(node)) {
        n3->set_visible(on);
    }
}

int64_t RenderTuner::find_binding(Object* object, const StringName& property) const {
    for (std::size_t i = 0; i < bindings_.size(); ++i)
        if (uint64_t(bindings_[i].object) == object->get_instance_id() && bindings_[i].property == property)
            return static_cast<int64_t>(i);
    return -1;
}

void RenderTuner::run_self_test() {
    int checks = 0, failures = 0;
    const auto check = [&](bool ok, const String& what) {
        ++checks;
        if (!ok) {
            ++failures;
            UtilityFunctions::printerr("RenderTuner self-test FAIL: ", what);
        }
    };
    rebuild();
    // Open every section, nested ones included, as a person clicking through would.
    for (std::size_t i = 0; i < sections_.size(); ++i) {
        sections_[i]->set_folded(false);
        on_folding_changed(false, static_cast<int64_t>(i));
    }
    check(bindings_.size() > 100, "every section builds its rows (" + String::num_int64(int64_t(bindings_.size())) + ")");
    check(!environments_.empty() && !lights_.empty(), "the scan finds the theme's environment and lights");

    // Every control writes its own value back without changing it.
    int changed = 0;
    for (std::size_t i = 0; i < bindings_.size(); ++i) {
        Object* object = ObjectDB::get_instance(bindings_[i].object);
        if (!object || Object::cast_to<TunerSettings>(object) == display_.ptr()) continue;
        const Variant before = object->get(bindings_[i].property);
        apply_binding(static_cast<int64_t>(i), before);
        if (object->get(bindings_[i].property) != before) ++changed;
    }
    check(changed == 0, String::num_int64(changed) + " settings changed when written back unchanged");

    auto* env = Object::cast_to<Environment>(environments_.empty() ? nullptr : ObjectDB::get_instance(environments_[0]));
    if (env) {
        // A slider drives an Environment value.
        const double intensity = env->get_glow_intensity();
        const int64_t slider_row = find_binding(env, "glow_intensity");
        auto* slider = slider_row < 0 ? nullptr : Object::cast_to<Range>(bindings_[std::size_t(slider_row)].control);
        check(slider != nullptr, "glow intensity has a slider");
        if (slider) {
            slider->set_value(intensity + 0.5);
            check(std::abs(env->get_glow_intensity() - (intensity + 0.5)) < 1e-3, "the slider sets glow intensity");
        }
        reset_all();
        check(std::abs(env->get_glow_intensity() - intensity) < 1e-6, "reset puts glow intensity back");
    }
    UtilityFunctions::print("RenderTuner self-test: ", checks, " checks, ", failures, " failures");
    get_tree()->quit(failures == 0 ? 0 : 1);
}

}  // namespace montezuma::godot_binding

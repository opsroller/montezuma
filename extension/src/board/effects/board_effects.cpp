// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "board_effects.hpp"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "../../render/quality_tiers.hpp"
#include "../feel.hpp"
#include "../piece_library.hpp"

using namespace godot;

namespace montezuma::godot_binding {

namespace {

real_t r(double v) { return static_cast<real_t>(v); }

constexpr int kGemSlot = 13;
constexpr int kSteelSlot = 14;
constexpr int kChainSlot = 15;
constexpr int kPlacedSlot = 16;

// Totem colours follow their kind (red, orange, yellow, green, blue,
// purple, white); power-up colours are their own.
const Color kTotemColors[7] = {Color(1.0f, 0.38f, 0.1f), Color(1.0f, 0.62f, 0.2f), Color(1.0f, 0.84f, 0.3f),
                               Color(0.4f, 0.95f, 0.45f), Color(0.35f, 0.65f, 1.0f), Color(0.78f, 0.45f, 1.0f),
                               Color(1.0f, 1.0f, 1.0f)};
const Color kPowerUpColors[6] = {Color(1.0f, 0.42f, 0.12f), Color(0.85f, 0.55f, 1.0f), Color(0.75f, 0.92f, 1.0f),
                                 Color(0.35f, 0.9f, 1.0f),  Color(1.0f, 0.82f, 0.25f), Color(0.45f, 1.0f, 0.55f)};
const Color kGemColor(0.55f, 0.95f, 1.0f);
const Color kSteelColor(0.56f, 0.58f, 0.62f);
const Color kChainColor(0.22f, 0.21f, 0.2f);

String seconds_text(double s) { return "+" + String::num_int64(static_cast<int64_t>(std::lround(s))) + " s"; }

}  // namespace

void BoardEffects::load(double cell, const PieceLibrary& library) {
    library_ = &library;
    cell_ = r(cell);
    counts_.fill(0);
    // Mobile and web carry half the sparks (visual target, particle budgets).
    particle_scale_ = 1.0f;
    if (const QualityTiers* q = QualityTiers::get_singleton()) {
        const String tier = q->get_tier();
        if (tier == "mobile" || tier == "web") particle_scale_ = 0.5f;
    }
    Ref<SphereMesh> sphere;
    sphere.instantiate();
    sphere->set_radius(0.5f);
    sphere->set_height(1.0f);
    sphere->set_radial_segments(16);
    sphere->set_rings(8);
    sphere_ = sphere;
    Ref<SphereMesh> octa;
    octa.instantiate();
    octa->set_radius(0.5f);
    octa->set_height(1.0f);
    octa->set_radial_segments(4);
    octa->set_rings(1);
    octahedron_ = octa;
    Ref<TorusMesh> ring;
    ring.instantiate();
    ring->set_inner_radius(0.42f);
    ring->set_outer_radius(0.5f);
    ring->set_rings(32);
    ring->set_ring_segments(6);
    ring_ = ring;
    Ref<CylinderMesh> disc;
    disc.instantiate();
    disc->set_top_radius(0.5f);
    disc->set_bottom_radius(0.5f);
    disc->set_height(0.02f);
    disc->set_radial_segments(24);
    disc_ = disc;
    Ref<BoxMesh> box;
    box.instantiate();
    box->set_size(Vector3(1, 1, 1));
    box_ = box;
    Ref<CylinderMesh> barrel;
    barrel.instantiate();
    barrel->set_top_radius(0.5f);
    barrel->set_bottom_radius(0.5f);
    barrel->set_height(1.0f);
    barrel->set_radial_segments(16);
    barrel_ = barrel;
    Ref<CylinderMesh> cone;
    cone.instantiate();
    cone->set_top_radius(0.0f);
    cone->set_bottom_radius(0.5f);
    cone->set_height(1.0f);
    cone->set_radial_segments(3);  // a triangle seen from above
    cone_ = cone;
    Ref<BoxMesh> shard;
    shard.instantiate();
    shard->set_size(Vector3(0.08f, 0.02f, 0.05f) * cell_);
    shard_ = shard;
}

Node3D* BoardEffects::override_for(const String& name) const {
    if (!library_) return nullptr;
    const Ref<PackedScene> scene = library_->entry(name);
    if (scene.is_null()) return nullptr;
    Node* node = scene->instantiate();
    if (auto* n3 = Object::cast_to<Node3D>(node)) return n3;
    UtilityFunctions::push_warning("Asset manifest entry '", name, "' is not a Node3D scene; using a placeholder.");
    if (node) memdelete(node);
    return nullptr;
}

Ref<Material> BoardEffects::glow(Color c, float energy, bool transparent) const {
    Ref<StandardMaterial3D> m;
    m.instantiate();
    m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
    // Additive effects stay translucent, so the piece under them still reads
    // (readability rules 4 and 6).
    m->set_albedo(transparent ? Color(c.r, c.g, c.b, 0.55f) : c);
    m->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
    m->set_emission(c);
    m->set_emission_energy_multiplier(energy);
    if (transparent) {
        m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        m->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
    }
    m->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    return m;
}

Ref<Material> BoardEffects::solid(Color c, float metallic, float roughness) const {
    Ref<StandardMaterial3D> m;
    m.instantiate();
    m->set_albedo(c);
    m->set_metallic(metallic);
    m->set_roughness(roughness);
    return m;
}

Node3D* BoardEffects::mesh_node(const Ref<Mesh>& mesh, const Ref<Material>& mat) const {
    auto* mi = memnew(MeshInstance3D);
    mi->set_mesh(mesh);
    mi->set_material_override(mat);
    mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
    return mi;
}

Node3D* BoardEffects::label(const String& text, Color c) const {
    auto* l = memnew(Label3D);
    l->set_text(text);
    l->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
    l->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
    l->set_font_size(64);
    l->set_outline_size(14);
    l->set_pixel_size(0.6f * cell_ / 64.0f);
    l->set_modulate(c);
    l->set_outline_modulate(Color(0.08f, 0.05f, 0.02f, 0.9f));
    return l;
}

Node3D* BoardEffects::burst(Color c, int amount, double lifetime, float speed, const Ref<Mesh>& mesh) const {
    auto* p = memnew(CPUParticles3D);
    p->set_emitting(false);
    p->set_one_shot(true);
    p->set_amount(std::max(1, static_cast<int>(std::lround(amount * particle_scale_))));
    p->set_lifetime(lifetime);
    p->set_explosiveness_ratio(1.0f);
    p->set_direction(Vector3(0, 1, 0));
    p->set_spread(80.0f);
    p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, speed * 0.5f * cell_);
    p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, speed * cell_);
    p->set_gravity(Vector3(0, -9.0f * cell_, 0));
    p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.5f);
    p->set_param_max(CPUParticles3D::PARAM_SCALE, 1.0f);
    p->set_color(c);
    p->set_mesh(mesh);
    if (Ref<PrimitiveMesh> prim = mesh; prim.is_valid() && prim->get_material().is_null()) {
        // Particles of a shared mesh take the material from the instance.
        p->set_material_override(glow(c, 1.6f, false));
    }
    p->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
    return p;
}

void BoardEffects::schedule(Node3D* node, Node3D* root, const Ref<Tween>& tween, double start, double seconds) const {
    node->set_visible(false);
    root->add_child(node);
    tween->tween_callback(Callable(node, "set_visible").bind(true))->set_delay(start);
    if (auto* p = Object::cast_to<CPUParticles3D>(node)) {
        tween->tween_callback(Callable(p, "restart"))->set_delay(start);
        seconds = std::max(seconds, p->get_lifetime());
    } else if (auto* g = Object::cast_to<GPUParticles3D>(node)) {
        tween->tween_callback(Callable(g, "restart"))->set_delay(start);
        seconds = std::max(seconds, g->get_lifetime());
    }
    tween->tween_callback(Callable(node, "queue_free"))->set_delay(start + seconds);
}

void BoardEffects::fade(Node3D* node, const Ref<Tween>& tween, double start, double seconds) const {
    tween->tween_property(node, "scale", Vector3(0.001f, 0.001f, 0.001f), seconds)
        ->set_delay(start)
        ->set_trans(Tween::TRANS_QUAD)
        ->set_ease(Tween::EASE_IN);
}

// ---- Markers ----------------------------------------------------------------

Node3D* BoardEffects::power_up_badge(int id) const {
    if (Node3D* n = override_for("bonus_" + String(fx::kPowerUpNames[static_cast<size_t>(id)].data()))) return n;
    const Color c = kPowerUpColors[id];
    auto* badge = memnew(Node3D);
    // A dark backing disc with a rim in the badge colour, so it reads on any piece.
    Node3D* back = mesh_node(disc_, solid(Color(0.08f, 0.06f, 0.05f), 0.0f, 0.6f));
    back->set_scale(Vector3(0.34f, 1.0f, 0.34f) * cell_);
    badge->add_child(back);
    Node3D* rim = mesh_node(ring_, glow(c, 1.2f, false));
    rim->set_scale(Vector3(0.36f, 0.5f, 0.36f) * cell_);
    badge->add_child(rim);
    auto add = [&](const Ref<Mesh>& mesh, Vector3 scale, Vector3 pos, real_t yaw) {
        Node3D* n = mesh_node(mesh, glow(c, 1.6f, false));
        n->set_scale(scale * cell_);
        n->set_position(pos * cell_);
        n->set_rotation(Vector3(0, yaw, 0));
        badge->add_child(n);
    };
    const real_t q = static_cast<real_t>(Math::PI / 4.0);
    switch (id) {
        case fx::kTnt:  // a barrel: a filled circle from above
            add(barrel_, Vector3(0.18f, 0.16f, 0.18f), Vector3(0, 0.08f, 0), 0);
            break;
        case fx::kColorClear:  // a six-pointed asterisk
            for (int i = 0; i < 3; ++i) add(box_, Vector3(0.26f, 0.04f, 0.05f), Vector3(0, 0.04f, 0), q * 4 / 3 * i);
            break;
        case fx::kLightning:  // a zigzag bolt
            add(box_, Vector3(0.05f, 0.04f, 0.16f), Vector3(0.03f, 0.04f, -0.06f), -0.5f);
            add(box_, Vector3(0.05f, 0.04f, 0.16f), Vector3(-0.03f, 0.04f, 0.06f), -0.5f);
            add(box_, Vector3(0.12f, 0.04f, 0.04f), Vector3(0, 0.04f, 0), 0);
            break;
        case fx::kClock:  // a dial with one hand
            add(ring_, Vector3(0.24f, 0.4f, 0.24f), Vector3(0, 0.04f, 0), 0);
            add(box_, Vector3(0.03f, 0.04f, 0.1f), Vector3(0, 0.04f, -0.04f), 0);
            break;
        case fx::kMultiplier:  // an X
            add(box_, Vector3(0.26f, 0.04f, 0.05f), Vector3(0, 0.04f, 0), q);
            add(box_, Vector3(0.26f, 0.04f, 0.05f), Vector3(0, 0.04f, 0), -q);
            break;
        default:  // hint: a triangle
            add(cone_, Vector3(0.24f, 0.08f, 0.24f), Vector3(0, 0.05f, 0), 0);
            break;
    }
    return badge;
}

Node3D* BoardEffects::make_marks(int m) const {
    if (m == fx::marks::kNone) return nullptr;
    auto* root = memnew(Node3D);
    root->set_name("Marks");
    const real_t top = 0.5f * cell_;
    if (m & fx::marks::kSteel) {
        // A metal frame around the cell.
        Node3D* frame = override_for("marker_steel");
        if (!frame) {
            frame = memnew(Node3D);
            const Ref<Material> metal = solid(kSteelColor, 0.9f, 0.35f);
            for (int i = 0; i < 4; ++i) {
                Node3D* bar = mesh_node(box_, metal);
                const bool along_x = i < 2;
                const real_t side = (i % 2 == 0 ? -0.43f : 0.43f) * cell_;
                bar->set_scale(along_x ? Vector3(0.94f * cell_, 0.18f * cell_, 0.08f * cell_)
                                       : Vector3(0.08f * cell_, 0.18f * cell_, 0.94f * cell_));
                bar->set_position(along_x ? Vector3(0, 0.09f * cell_, side) : Vector3(side, 0.09f * cell_, 0));
                frame->add_child(bar);
            }
        }
        root->add_child(frame);
    }
    if (m & fx::marks::kChain) {
        // Two iron bars crossed over the piece.
        Node3D* chain = override_for("marker_chain");
        if (!chain) {
            chain = memnew(Node3D);
            const Ref<Material> iron = solid(kChainColor, 0.8f, 0.5f);
            for (real_t yaw : {static_cast<real_t>(Math::PI / 4.0), static_cast<real_t>(-Math::PI / 4.0)}) {
                Node3D* bar = mesh_node(box_, iron);
                bar->set_scale(Vector3(1.0f * cell_, 0.07f * cell_, 0.09f * cell_));
                bar->set_position(Vector3(0, top + 0.05f * cell_, 0));
                bar->set_rotation(Vector3(0, yaw, 0));
                chain->add_child(bar);
            }
        }
        chain->set_name("Chain");
        root->add_child(chain);
    }
    if (m & fx::marks::kGem) {
        // A bright gem at the cell's far-left corner.
        Node3D* gem = override_for("marker_gem");
        if (!gem) {
            gem = mesh_node(octahedron_, glow(kGemColor, 2.0f, false));
            gem->set_scale(Vector3(0.3f, 0.38f, 0.3f) * cell_);
        }
        gem->set_name("Gem");
        gem->set_position(Vector3(-0.3f * cell_, top, -0.3f * cell_));
        root->add_child(gem);
    }
    if (const int id = fx::marks::power_up(m); id >= 0 && id < 6) {
        Node3D* badge = power_up_badge(id);
        badge->set_name("PowerUp");
        badge->set_position(Vector3(0.28f * cell_, top, -0.28f * cell_));
        root->add_child(badge);
    }
    return root;
}

Node3D* BoardEffects::make_hint_ring() const {
    if (Node3D* n = override_for("fx_hint")) return n;
    Node3D* ring = mesh_node(ring_, glow(Color(1.0f, 0.95f, 0.55f), 1.5f, true));
    ring->set_scale(Vector3(0.92f, 0.6f, 0.92f) * cell_);
    ring->set_position(Vector3(0, 0.03f * cell_, 0));
    return ring;
}

// ---- Beats -------------------------------------------------------------------

void BoardEffects::play(const fx::Beat& beat, const Ref<Tween>& tween, Node3D* root,
                        const std::function<Vector3(fx::Cell)>& at, Vector3 centre, double span) const {
    using Kind = fx::Beat::Kind;
    const double t0 = beat.start;
    const double len = std::max(beat.duration, 0.05);
    const real_t c = cell_;
    auto slot = [&]() -> int {
        switch (beat.kind) {
            case Kind::Totem: return beat.id;
            case Kind::PowerUp: return 7 + beat.id;
            case Kind::GemCollect: return kGemSlot;
            case Kind::SteelBreak: return kSteelSlot;
            case Kind::ChainBreak: return kChainSlot;
            case Kind::MarkPlaced: return kPlacedSlot;
        }
        return -1;
    }();
    if ((beat.kind == Kind::Totem && (beat.id < 0 || beat.id > 6)) ||
        (beat.kind == Kind::PowerUp && (beat.id < 0 || beat.id > 5)))
        return;
    if (slot >= 0 && slot < kCountSlots) ++counts_[slot];

    // A theme's own scene replaces the placeholder for this beat.
    String name;
    switch (beat.kind) {
        case Kind::Totem: name = "fx_totem_" + String(fx::kTotemNames[static_cast<size_t>(beat.id)].data()); break;
        case Kind::PowerUp: name = "fx_power_up_" + String(fx::kPowerUpNames[static_cast<size_t>(beat.id)].data()); break;
        case Kind::GemCollect: name = "fx_gem_collect"; break;
        case Kind::SteelBreak: name = "fx_steel_break"; break;
        case Kind::ChainBreak: name = "fx_chain_break"; break;
        case Kind::MarkPlaced: name = "fx_power_up_placed"; break;
    }
    if (Node3D* n = override_for(name)) {
        n->set_position(beat.kind == Kind::Totem ? centre : at(beat.cell));
        schedule(n, root, tween, t0, len);
        return;
    }

    auto flash_cells = [&](const std::vector<fx::Cell>& cells, Color col, double start, double seconds) {
        for (const fx::Cell& cell : cells) {
            Node3D* f = mesh_node(disc_, glow(col, 0.8f));
            f->set_position(at(cell) + Vector3(0, 0.04f * c, 0));
            f->set_scale(Vector3(0.2f, 1, 0.2f) * c);
            schedule(f, root, tween, start, seconds);
            tween->tween_property(f, "scale", Vector3(0.8f, 1, 0.8f) * c, seconds * 0.35)->set_delay(start)->set_ease(Tween::EASE_OUT);
            fade(f, tween, start + seconds * 0.45, seconds * 0.55);
        }
    };
    auto rising_label = [&](const String& text, Color col, Vector3 from) {
        Node3D* l = label(text, col);
        l->set_position(from + Vector3(0, 0.8f * c, 0));
        schedule(l, root, tween, t0, len);
        tween->tween_property(l, "position", from + Vector3(0, r(0.8 + feel::kLabelRiseCells) * c, 0), len)
            ->set_delay(t0)
            ->set_ease(Tween::EASE_OUT);
        tween->tween_property(l, "modulate:a", 0.0, len * 0.4)->set_delay(t0 + len * 0.6);
    };
    auto ring_wave = [&](Vector3 from, Color col, double radius, double start, double seconds) {
        Node3D* ring = mesh_node(ring_, glow(col, 1.2f));
        ring->set_position(from + Vector3(0, 0.06f * c, 0));
        ring->set_scale(Vector3(0.2f, 0.4f, 0.2f) * c);
        schedule(ring, root, tween, start, seconds);
        const real_t d = r(radius * 2.0);
        tween->tween_property(ring, "scale", Vector3(d, 0.4f * c, d), seconds)->set_delay(start)->set_ease(Tween::EASE_OUT);
    };

    switch (beat.kind) {
        case Kind::Totem: {
            const Color col = kTotemColors[beat.id];
            const double lead = std::min(feel::kTotemLeadSeconds, len);
            switch (beat.id) {
                case fx::kFire:
                    // Fireballs fall from above onto each target during the
                    // lead and burst as the clear starts.
                    for (const fx::Cell& cell : beat.cells) {
                        const Vector3 to = at(cell) + Vector3(0, 0.3f * c, 0);
                        Node3D* ball = mesh_node(sphere_, glow(col, 3.0f, false));
                        ball->set_scale(Vector3(0.5f, 0.5f, 0.5f) * c);
                        ball->set_position(to + Vector3(0.6f * c, r(feel::kFireballDropCells) * c, -1.2f * c));
                        schedule(ball, root, tween, t0, lead);
                        tween->tween_property(ball, "position", to, lead)->set_delay(t0)->set_trans(Tween::TRANS_QUAD)->set_ease(Tween::EASE_IN);
                        Node3D* embers = burst(col, 10, 0.35, 3.0f, sphere_);
                        Object::cast_to<CPUParticles3D>(embers)->set_param_min(CPUParticles3D::PARAM_SCALE, 0.05f * c);
                        Object::cast_to<CPUParticles3D>(embers)->set_param_max(CPUParticles3D::PARAM_SCALE, 0.1f * c);
                        embers->set_position(to);
                        schedule(embers, root, tween, t0 + lead, std::max(0.05, len - lead));
                    }
                    flash_cells(beat.cells, col, t0 + lead, std::max(0.05, len - lead));
                    break;
                case fx::kTime:
                    rising_label(seconds_text(beat.value), col, centre);
                    ring_wave(centre, col, span * 0.5, t0, len);
                    break;
                case fx::kGold: {
                    rising_label("+" + String::num_int64(static_cast<int64_t>(beat.value)), col, centre);
                    Node3D* coins = burst(col, 36, len, 4.0f, barrel_);
                    auto* p = Object::cast_to<CPUParticles3D>(coins);
                    p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
                    p->set_emission_box_extents(Vector3(r(span * 0.4), 0.1f, r(span * 0.4)));
                    p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.08f * c);
                    p->set_param_max(CPUParticles3D::PARAM_SCALE, 0.14f * c);
                    coins->set_position(centre + Vector3(0, 0.4f * c, 0));
                    schedule(coins, root, tween, t0, len);
                    break;
                }
                case fx::kHarvest:
                case fx::kWard:
                    // A wave across the whole board; the gems, chains and
                    // steel it touches play their own beats.
                    ring_wave(centre, col, span * 0.5, t0, len);
                    flash_cells(beat.cells, col, t0 + lead * 0.5, std::max(0.05, len - lead * 0.5));
                    break;
                case fx::kGift:
                    for (const fx::Cell& cell : beat.cells) {
                        Node3D* sparkle = burst(col, 12, 0.4, 2.5f, octahedron_);
                        auto* p = Object::cast_to<CPUParticles3D>(sparkle);
                        p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.05f * c);
                        p->set_param_max(CPUParticles3D::PARAM_SCALE, 0.09f * c);
                        sparkle->set_position(at(cell) + Vector3(0, 0.5f * c, 0));
                        schedule(sparkle, root, tween, t0 + lead * 0.5, len - lead * 0.5);
                    }
                    ring_wave(centre, col, span * 0.5, t0, len);
                    break;
                default:  // paint: a white wash over each repainted piece as it changes
                    flash_cells(beat.cells, col, t0, lead);
                    break;
            }
            break;
        }
        case Kind::PowerUp: {
            const Color col = kPowerUpColors[beat.id];
            const Vector3 here = at(beat.cell);
            switch (beat.id) {
                case fx::kTnt: {
                    // A blast shell over its area, then embers.
                    double reach = 0.5;
                    for (const fx::Cell& cell : beat.cells)
                        reach = std::max(reach, static_cast<double>(std::max(std::abs(cell.x - beat.cell.x), std::abs(cell.y - beat.cell.y))) + 0.5);
                    // A shock ring out to the blast's edge over its cells.
                    ring_wave(here, col, reach, t0, len);
                    flash_cells(beat.cells, col, t0, len);
                    Node3D* embers = burst(col, 16, 0.4, 4.0f, sphere_);
                    auto* p = Object::cast_to<CPUParticles3D>(embers);
                    p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.05f * c);
                    p->set_param_max(CPUParticles3D::PARAM_SCALE, 0.1f * c);
                    embers->set_position(here);
                    schedule(embers, root, tween, t0, len);
                    break;
                }
                case fx::kLightning:
                    // Beams along each line it clears.
                    for (const fx::Cell& cell : beat.cells) {
                        if (cell == beat.cell) continue;
                        const bool row = cell.y == beat.cell.y;
                        const bool col_line = cell.x == beat.cell.x;
                        if (!row && !col_line) continue;
                        const Vector3 mid = (here + at(cell)) * 0.5f + Vector3(0, 0.45f * c, 0);
                        const real_t length = here.distance_to(at(cell));
                        Node3D* beam = mesh_node(box_, glow(col, 3.0f));
                        beam->set_position(mid);
                        beam->set_scale(row ? Vector3(length, 0.06f * c, 0.12f * c) : Vector3(0.12f * c, 0.06f * c, length));
                        schedule(beam, root, tween, t0, len);
                        fade(beam, tween, t0 + len * 0.4, len * 0.6);
                    }
                    flash_cells(beat.cells, col, t0, len);
                    break;
                case fx::kColorClear:
                    ring_wave(here, col, 1.5, t0, len);
                    flash_cells(beat.cells, col, t0, len);
                    break;
                case fx::kClock:
                    rising_label(seconds_text(beat.value), col, here);
                    break;
                case fx::kMultiplier:
                    rising_label(String::utf8("×") + String::num_int64(static_cast<int64_t>(beat.value)), col, here);
                    break;
                default:  // hint: a ring; the hint itself shows on the board
                    ring_wave(here, col, 1.0, t0, len);
                    break;
            }
            break;
        }
        case Kind::GemCollect: {
            // The gem flies up and away.
            const Vector3 from = at(beat.cell) + Vector3(-0.3f * c, 0.5f * c, -0.3f * c);
            Node3D* gem = mesh_node(octahedron_, glow(kGemColor, 2.0f, false));
            gem->set_scale(Vector3(0.3f, 0.38f, 0.3f) * c);
            gem->set_position(from);
            schedule(gem, root, tween, t0, len);
            tween->tween_property(gem, "position", from + Vector3(0, r(feel::kGemFlyCells) * c, -0.5f * c), len)
                ->set_delay(t0)
                ->set_ease(Tween::EASE_OUT);
            tween->tween_property(gem, "rotation", Vector3(0, r(Math::PI * 2.0), 0), len)->set_delay(t0);
            fade(gem, tween, t0 + len * 0.5, len * 0.5);
            break;
        }
        case Kind::SteelBreak: {
            Node3D* shards = burst(kSteelColor, 14, len, 3.5f, shard_);
            Object::cast_to<CPUParticles3D>(shards)->set_material_override(solid(kSteelColor, 0.9f, 0.35f));
            shards->set_position(at(beat.cell) + Vector3(0, 0.15f * c, 0));
            schedule(shards, root, tween, t0, len);
            break;
        }
        case Kind::ChainBreak:
            for (real_t yaw : {static_cast<real_t>(Math::PI / 4.0), static_cast<real_t>(-Math::PI / 4.0)}) {
                Node3D* bar = mesh_node(box_, solid(kChainColor, 0.8f, 0.5f));
                const Vector3 from = at(beat.cell) + Vector3(0, 0.55f * c, 0);
                bar->set_scale(Vector3(1.0f * c, 0.07f * c, 0.09f * c));
                bar->set_position(from);
                bar->set_rotation(Vector3(0, yaw, 0));
                schedule(bar, root, tween, t0, len);
                tween->tween_property(bar, "position", from + Vector3(r(yaw) * 0.4f * c, -0.3f * c, 0.3f * c), len)
                    ->set_delay(t0)
                    ->set_ease(Tween::EASE_IN);
                fade(bar, tween, t0 + len * 0.3, len * 0.7);
            }
            break;
        case Kind::MarkPlaced: {
            // A sparkle where a badge or gem appears; the marker pops in on
            // the piece itself.
            const int id = fx::marks::power_up(beat.id);
            const Color col = id >= 0 && id < 6 ? kPowerUpColors[id] : kGemColor;
            Node3D* sparkle = burst(col, 8, 0.35, 2.0f, octahedron_);
            auto* p = Object::cast_to<CPUParticles3D>(sparkle);
            p->set_param_min(CPUParticles3D::PARAM_SCALE, 0.04f * c);
            p->set_param_max(CPUParticles3D::PARAM_SCALE, 0.07f * c);
            sparkle->set_position(at(beat.cell) + Vector3(0, 0.5f * c, 0));
            schedule(sparkle, root, tween, t0, len);
            break;
        }
    }
}

}  // namespace montezuma::godot_binding

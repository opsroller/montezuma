// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/compositor.hpp>
#include <godot_cpp/classes/compositor_effect.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/render_data.hpp>
#include <godot_cpp/variant/rid.hpp>

#include "rt_math.hpp"

namespace montezuma::godot_binding {

// Hardware ray-traced reflections between the board's pieces (WP-54
// spike), built only with MONTEZUMA_RAY_TRACING and run only with
// `-- --rt-reflections` on Forward+ where the GPU has ray-tracing pipelines.
//
// Each frame the pieces under nodes in the "ray_traced" group (meshes and
// the piece batcher's multimeshes) become one acceleration structure, one
// BLAS per mesh. After transparents, a ray-tracing pass casts a ray from the
// camera to each pixel's depth: where it lands on a piece, it traces that
// surface's mirror reflection through the pieces. A hit the screen shows
// takes its lit colour from the frame; one it hides takes the hit piece's
// albedo. The traced colour replaces the engine's reflection by Fresnel
// times the material's roughness weight. Stone, water and the backdrop
// keep the engine's screen-space reflections and probes.
// Reference: docs/reference/ray-traced-reflections.md.
class RtReflections : public godot::CompositorEffect {
    GDCLASS(RtReflections, godot::CompositorEffect)

public:
    RtReflections();

    void _render_callback(int32_t p_effect_callback_type, godot::RenderData* p_render_data) override;

    // The run's --rt-* options, read once.
    static const rt::Options& options();
    // Forward+ with a RenderingDevice that has ray-tracing pipelines.
    static bool is_supported();
    // `compositor` with an RtReflections in it when the run asked for one and
    // the GPU can run it (made if missing); otherwise `compositor` unchanged.
    // Warns once when asked for on a GPU that cannot.
    static godot::Ref<godot::Compositor> attach(godot::Ref<godot::Compositor> compositor);
    // The RtReflections among `compositor`'s effects, or null.
    static godot::Ref<RtReflections> in(const godot::Ref<godot::Compositor>& compositor);
    // True when an RtReflections traced in one of the last few frames.
    static bool tracing();

    void set_strength(float strength);
    float get_strength() const { return strength_; }
    // Pieces in the last scene it traced, and BLASes it holds.
    int get_instance_count() const { return instance_count_; }
    int get_blas_count() const { return static_cast<int>(meshes_.size()); }
    int get_traced_frames() const { return traced_frames_; }
    // Meshes it freed after they left the board.
    int get_evicted_count() const { return evicted_; }

protected:
    static void _bind_methods();
    void _notification(int p_what);

private:
    struct GpuMesh {
        godot::RID vertices;
        godot::RID indices;
        godot::RID blas;
        std::uint32_t first_triangle = 0;
        std::vector<float> normals;  // its packed triangle normals, to repack after an eviction
    };

    bool ensure_pipelines();
    void compile_on_render_thread();
    void free_all();
    bool upload_meshes();

    float strength_ = rt::kDefaultStrength;
    godot::RID trace_shader_;
    godot::RID trace_pipeline_;
    godot::RID hit_table_;
    std::int64_t hit_range_ = 0;
    godot::RID resolve_shader_;
    godot::RID resolve_pipeline_;
    godot::RID nearest_sampler_;
    godot::RID tlas_;
    godot::RID instance_buffer_;
    godot::RID normal_buffer_;
    godot::RID frame_buffer_;
    std::unordered_map<std::uint64_t, GpuMesh> meshes_;
    std::vector<float> normals_;  // every mesh's packed triangle normals, in upload order
    bool normals_dirty_ = false;
    bool failed_ = false;
    int instance_count_ = 0;
    int traced_frames_ = 0;
    int evicted_ = 0;
};

}  // namespace montezuma::godot_binding

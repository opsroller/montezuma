// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

// Ray-traced reflections: options, attach rule, reflectance and packing.
// No Godot, no framework; run via ctest. The GPU half is checked by
// `-- --rt-selftest` (docs/reference/ray-traced-reflections.md).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "rt/rt_math.hpp"

using namespace montezuma::godot_binding::rt;

namespace {

int checks = 0;
int failures = 0;

void check(bool cond, const std::string& label) {
    ++checks;
    if (!cond) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", label.c_str());
    }
}

bool near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }

void test_options() {
    const Options none = parse_options({});
    check(!none.enabled && !none.self_test, "off without flags");
    check(near(none.strength, kDefaultStrength), "default strength");

    const Options on = parse_options({"--quality=desktop-high", "--rt-reflections"});
    check(on.enabled && !on.self_test, "--rt-reflections turns it on");

    const Options test = parse_options({"--rt-selftest"});
    check(test.enabled && test.self_test, "--rt-selftest turns it on and tests");
    check(!test.debug && parse_options({"--rt-debug"}).debug && parse_options({"--rt-debug"}).enabled,
          "--rt-debug turns it on and paints");

    check(near(parse_options({"--rt-reflections", "--rt-strength=0.5"}).strength, 0.5f), "strength parses");
    check(near(parse_options({"--rt-strength=7"}).strength, kMaxStrength), "strength clamps high");
    check(near(parse_options({"--rt-strength=-1"}).strength, 0.0f), "strength clamps low");
    check(near(parse_options({"--rt-strength=lots"}).strength, kDefaultStrength), "bad strength keeps default");
    check(near(parse_options({"--rt-strength="}).strength, kDefaultStrength), "empty strength keeps default");
    check(!parse_options({"--rt-strength=1"}).enabled, "strength alone does not turn it on");
}

void test_attach() {
    const Options on = parse_options({"--rt-reflections"});
    const Options off = parse_options({});
    check(should_attach(true, on, "forward_plus", true), "built, asked, Forward+, ray tracing");
    check(!should_attach(false, on, "forward_plus", true), "not built");
    check(!should_attach(true, off, "forward_plus", true), "not asked");
    check(!should_attach(true, on, "forward_plus", false), "no ray tracing");
    check(!should_attach(true, on, "mobile", true), "Mobile renderer");
    check(!should_attach(true, on, "gl_compatibility", true), "Compatibility renderer");
    check(!should_attach(true, on, "", true), "no renderer");
}

void test_reflectance() {
    check(near(dielectric_f0(1.5f), 0.04f), "glass F0 is 4%");
    check(near(dielectric_f0(1.76f), 0.0758f, 1e-4f), "ruby F0");
    check(near(dielectric_f0(1.0f), 0.0f), "air has none");
    check(near(dielectric_f0(0.0f), dielectric_f0(kDefaultIor)), "no ior uses the default");

    const Rgb gold{1.0f, 0.8f, 0.3f};
    const Rgb metal = material_f0(gold, 1.0f, 1.5f);
    check(near(metal.r, 1.0f) && near(metal.g, 0.8f) && near(metal.b, 0.3f), "metal F0 is its albedo");
    const Rgb gem = material_f0(gold, 0.0f, 1.76f);
    check(near(gem.r, dielectric_f0(1.76f)) && near(gem.b, dielectric_f0(1.76f)), "dielectric F0 is grey");
    const Rgb half = material_f0(gold, 0.5f, 1.5f);
    check(near(half.g, 0.5f * 0.04f + 0.5f * 0.8f), "metalness blends");
    const Rgb clamped = material_f0(gold, 3.0f, 1.5f);
    check(near(clamped.g, 0.8f), "metalness clamps");

    const Rgb f0{0.04f, 0.04f, 0.04f};
    check(near(schlick(f0, 1.0f).r, 0.04f), "head on is F0");
    check(near(schlick(f0, 0.0f).r, 1.0f), "grazing is 1");
    check(schlick(f0, 0.5f).r > 0.04f && schlick(f0, 0.5f).r < 1.0f, "between in between");
    check(near(schlick(f0, -1.0f).r, 1.0f), "back facing clamps to grazing");

    check(near(roughness_weight(0.0f), 1.0f), "mirror takes it all");
    check(near(roughness_weight(kRoughnessCutoff), 0.0f), "cutoff takes none");
    check(near(roughness_weight(0.9f), 0.0f), "rough takes none");
    check(roughness_weight(0.1f) > roughness_weight(0.3f), "falls with roughness");
    check(near(roughness_weight(0.2f, 0.0f), 0.0f), "zero cutoff takes none");
}

void test_packing() {
    const std::vector<float> empty = pack_instances({});
    check(empty.empty(), "no instances, no data");

    InstanceMaterial a;
    a.albedo = {0.1f, 0.2f, 0.3f};
    a.f0 = {0.04f, 0.05f, 0.06f};
    a.first_triangle = 12;
    InstanceMaterial b = a;
    b.weight = 0.0f;
    b.first_triangle = 40;
    const std::vector<float> packed = pack_instances({a, b});
    check(packed.size() == 2 * kFloatsPerInstance, "two vec4s each");
    check(near(packed[0], 0.1f) && near(packed[2], 0.3f), "albedo first");
    check(near(packed[3], 12.0f), "first triangle in w");
    check(near(packed[4], 0.04f) && near(packed[6], 0.06f), "f0 second");
    check(near(packed[7], 1.0f) && near(packed[kFloatsPerInstance + 7], 0.0f), "trace weight last");
    check(near(packed[kFloatsPerInstance + 3], 40.0f), "second instance's triangle");

    // One quad: two triangles sharing an edge, vertex normals up.
    const std::vector<float> pos{0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1};
    const std::vector<float> up{0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
    const std::vector<std::uint32_t> idx{0, 2, 1, 0, 3, 2};
    const std::vector<float> n = pack_triangle_normals(pos, up, idx);
    check(n.size() == 2 * kFloatsPerTriangle, "three vec4s a triangle");
    check(near(n[1], 1.0f) && near(n[5], 1.0f) && near(n[kFloatsPerTriangle + 9], 1.0f), "vertex normals kept");

    // No normals: each corner gets the face normal, unit length, winding as Godot's (clockwise front).
    const std::vector<float> face = pack_triangle_normals(pos, {}, idx);
    check(face.size() == 2 * kFloatsPerTriangle, "face normals fill in");
    const float len = std::sqrt(face[0] * face[0] + face[1] * face[1] + face[2] * face[2]);
    check(near(len, 1.0f), "face normal is unit length");
    check(near(std::fabs(face[1]), 1.0f), "face normal is along y");

    // A zero normal is replaced by the face normal.
    std::vector<float> broken = up;
    broken[0] = broken[1] = broken[2] = 0.0f;
    const std::vector<float> fixed = pack_triangle_normals(pos, broken, idx);
    check(near(std::fabs(fixed[1]), 1.0f), "zero normal replaced");

    // Out-of-range indices and a trailing partial triangle are dropped.
    const std::vector<float> bad = pack_triangle_normals(pos, up, {0, 2, 9, 0, 3, 2, 1});
    check(bad.size() == kFloatsPerTriangle * 2, "one entry per whole triangle, bad ones included");
}

}  // namespace

int main() {
    test_options();
    test_attach();
    test_reflectance();
    test_packing();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

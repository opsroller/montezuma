// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

// Board-to-world placement and camera framing, kept free of Godot types so
// it is unit-tested on its own (extension/src/board/tests/).
//
// World layout: the board lies on the XZ plane centred on the view's origin.
// Cell x runs along +X, cell y (rows, pieces fall toward higher y) along +Z,
// so row 0 is the far edge and appears at the top of the screen.

namespace montezuma::godot_binding::layout {

struct Grid {
    int width = 8;
    int height = 8;
    double cell = 1.0;
};

struct Point {
    double x = 0.0;
    double z = 0.0;
};

// Centre of a cell on the board plane.
inline Point cell_center(const Grid& g, int x, int y) {
    return {(x - (g.width - 1) * 0.5) * g.cell, (y - (g.height - 1) * 0.5) * g.cell};
}

// Cell under a point on the board plane; false when the point is off the board.
inline bool cell_at(const Grid& g, Point p, int& out_x, int& out_y) {
    const int x = static_cast<int>(std::floor(p.x / g.cell + g.width * 0.5));
    const int y = static_cast<int>(std::floor(p.z / g.cell + g.height * 0.5));
    if (x < 0 || y < 0 || x >= g.width || y >= g.height) return false;
    out_x = x;
    out_y = y;
    return true;
}

// How far a drawn board reaching from (min_x, min_z) to (max_x, max_z)
// extends past the outer cells, in cells; 0 when it lies inside them.
inline double board_padding(const Grid& g, double min_x, double min_z, double max_x, double max_z) {
    const double half_w = 0.5 * g.width * g.cell;
    const double half_h = 0.5 * g.height * g.cell;
    const double reach = std::max({-min_x - half_w, max_x - half_w, -min_z - half_h, max_z - half_h});
    return std::max(0.0, reach / g.cell);
}

struct Fit {
    double half_width = 4.0;     // half the framed area along X (world units)
    double half_depth = 4.0;     // half the framed area along Z
    double height = 0.5;         // tallest point above the board plane to keep in frame
    double tilt_degrees = 15.0;  // camera pitch away from straight down, toward +Z
    double vfov_degrees = 40.0;  // vertical field of view
    double aspect = 16.0 / 9.0;  // viewport width / height
    double margin = 0.08;        // fraction of the viewport kept empty on each side
};

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

// Direction from the board centre to the camera (unit length).
inline Vec3 camera_direction(double tilt_degrees) {
    const double t = tilt_degrees * std::numbers::pi / 180.0;
    return {0.0, std::cos(t), std::sin(t)};
}

// True when every corner of the framed box is inside the view, margin
// included, for a camera `distance` from the centre looking at it.
inline bool fits(const Fit& f, double distance) {
    const Vec3 back = camera_direction(f.tilt_degrees);  // camera looks along -back
    // Camera basis: right = +X, up = right x forward.
    const Vec3 up{0.0, back.z, -back.y};
    const double scale = 1.0 - 2.0 * std::clamp(f.margin, 0.0, 0.45);
    const double tan_v = std::tan(f.vfov_degrees * std::numbers::pi / 360.0) * scale;
    const double tan_h = std::tan(f.vfov_degrees * std::numbers::pi / 360.0) * f.aspect * scale;
    for (double sx : {-1.0, 1.0}) {
        for (double sz : {-1.0, 1.0}) {
            for (double h : {0.0, f.height}) {
                // Corner relative to the camera position.
                const Vec3 v{sx * f.half_width - back.x * distance, h - back.y * distance,
                             sz * f.half_depth - back.z * distance};
                const double depth = -(v.x * back.x + v.y * back.y + v.z * back.z);
                if (depth <= 0.0) return false;
                const double sx_view = v.x / depth;
                const double sy_view = (v.x * up.x + v.y * up.y + v.z * up.z) / depth;
                if (std::abs(sx_view) > tan_h || std::abs(sy_view) > tan_v) return false;
            }
        }
    }
    return true;
}

// Smallest camera distance from the board centre that frames the whole box
// at this aspect ratio: the board fits by width on tall phones and by height
// on wide screens, and is never cropped.
inline double fit_distance(const Fit& f) {
    double lo = 0.0;
    double hi = 1.0;
    while (!fits(f, hi) && hi < 1e6) hi *= 2.0;
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (lo + hi);
        (fits(f, mid) ? hi : lo) = mid;
    }
    return hi;
}

}  // namespace montezuma::godot_binding::layout

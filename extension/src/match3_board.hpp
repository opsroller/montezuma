// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "montezuma/board.hpp"

namespace montezuma::godot_binding {

// Exposes the engine-independent montezuma::Board to Godot.
class Match3Board : public godot::RefCounted {
    GDCLASS(Match3Board, godot::RefCounted)

public:
    void setup(int width, int height, int kinds, int64_t seed);

    int get_width() const { return board_.width(); }
    int get_height() const { return board_.height(); }
    int get_kinds() const { return board_.kinds(); }
    int get_cell(int x, int y) const;
    godot::PackedInt32Array get_cells() const;
    bool is_legal_swap(godot::Vector2i a, godot::Vector2i b);
    bool has_legal_move();
    // Empty Dictionary for an illegal swap; otherwise
    // { steps: Array[Dictionary kind->count], matched_kind, shuffled, stuck, score }.
    godot::Dictionary try_swap(godot::Vector2i a, godot::Vector2i b);

protected:
    static void _bind_methods();

private:
    // 8x8, 7 kinds, seed 0 is a fixed, known-playable deal (covered by the
    // core tests), so this default construction cannot fail.
    Board board_{8, 8, 7, 0};
};

}  // namespace montezuma::godot_binding

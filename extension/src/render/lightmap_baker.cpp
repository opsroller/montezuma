// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "lightmap_baker.hpp"

#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/editor_file_dialog.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/lightmap_gi.hpp>
#include <godot_cpp/classes/lightmap_gi_data.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

constexpr const char* kArg = "--bake-lightmaps=";
constexpr const char* kButtonText = "Bake Lightmaps";
// Frames the editor must stay idle (no scan or import) before baking.
constexpr int kSettleFrames = 30;
// A bake of every theme on a software rasterizer stays far below this.
constexpr double kTimeoutSeconds = 1800.0;
constexpr double kStepTimeoutSeconds = 60.0;

// The editor reports a failed bake in a dialog, not the log; copy it there.
void print_dialogs(Node* node) {
    if (auto* dialog = Object::cast_to<AcceptDialog>(node)) {
        if (dialog->is_visible() && !dialog->get_text().is_empty()) {
            UtilityFunctions::printerr("LIGHTMAP_BAKE editor says: ", dialog->get_text());
        }
    }
    for (int i = 0; i < node->get_child_count(); ++i) print_dialogs(node->get_child(i));
}

// The first press of "Bake Lightmaps" on a node without data only asks
// where to save, in a file dialog that proposes <owner scene>.lmbake.
EditorFileDialog* find_save_dialog(Node* node) {
    if (auto* dialog = Object::cast_to<EditorFileDialog>(node)) {
        if (dialog->is_visible() && dialog->get_current_path().ends_with(".lmbake")) return dialog;
    }
    for (int i = 0; i < node->get_child_count(); ++i) {
        if (EditorFileDialog* found = find_save_dialog(node->get_child(i))) return found;
    }
    return nullptr;
}

}  // namespace

void LightmapBaker::_ready() {
    const PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
    for (const String& arg : args) {
        if (arg.begins_with(kArg)) scenes_ = arg.substr(String(kArg).length()).split(",", false);
    }
    set_process(!scenes_.is_empty());
    if (!scenes_.is_empty()) step_ = Step::WaitImport;
}

void LightmapBaker::_process(double delta) {
    elapsed_ += delta;
    step_elapsed_ += delta;
    if (elapsed_ > kTimeoutSeconds) {
        failures_ += static_cast<int>(scenes_.size()) - index_;
        UtilityFunctions::printerr("LIGHTMAP_BAKE fail: timed out");
        step_ = Step::Done;
    }
    EditorInterface* editor = EditorInterface::get_singleton();
    switch (step_) {
        case Step::Idle:
        case Step::Baking:  // re-entered from the bake's progress dialog, which runs frames
            return;
        case Step::WaitImport: {
            EditorFileSystem* fs = editor->get_resource_filesystem();
            wait_frames_ = fs->is_scanning() || fs->is_importing() ? 0 : wait_frames_ + 1;
            if (wait_frames_ >= kSettleFrames) step_ = Step::Open;
            return;
        }
        case Step::Open:
            UtilityFunctions::print("LIGHTMAP_BAKE open ", scenes_[index_]);
            editor->open_scene_from_path(scenes_[index_]);
            step_ = Step::Select;
            step_elapsed_ = 0.0;
            wait_frames_ = 0;
            return;
        case Step::Select: {
            Node* root = editor->get_edited_scene_root();
            Node* lightmap = root ? root->find_child("Lightmap", true, false) : nullptr;
            if (!Object::cast_to<LightmapGI>(lightmap) || root->get_scene_file_path() != scenes_[index_]) {
                if (step_elapsed_ > kStepTimeoutSeconds) finish(false, "no LightmapGI named Lightmap");
                return;
            }
            editor->edit_node(lightmap);
            step_ = Step::Bake;
            step_elapsed_ = 0.0;
            return;
        }
        case Step::Bake: {
            // The plugin shows its button once the node is selected.
            Button* button = find_bake_button(editor->get_base_control());
            EditorFileSystem* fs = editor->get_resource_filesystem();
            if (!button || !button->is_visible_in_tree() || fs->is_scanning() || fs->is_importing()) {
                if (step_elapsed_ > kStepTimeoutSeconds) finish(false, "no Bake Lightmaps button");
                return;
            }
            auto* lightmap = Object::cast_to<LightmapGI>(editor->get_edited_scene_root()->find_child("Lightmap", true, false));
            lightmap->set_light_data(Ref<LightmapGIData>());
            step_ = Step::Baking;
            button->emit_signal("pressed");
            // Accept the proposed path; the bake then runs synchronously.
            EditorFileDialog* dialog = find_save_dialog(get_tree()->get_root());
            if (!dialog) {
                finish(false, "no save dialog after Bake Lightmaps");
                return;
            }
            const String path = dialog->get_current_path();
            dialog->hide();
            const uint64_t start = Time::get_singleton()->get_ticks_msec();
            dialog->emit_signal("file_selected", path);
            const uint64_t ms = Time::get_singleton()->get_ticks_msec() - start;
            finish(lightmap->get_light_data().is_valid(), path + String(" in ") + String::num_int64(static_cast<int64_t>(ms)) + String(" ms"));
            return;
        }
        case Step::Done:
            set_process(false);
            get_tree()->quit(failures_ == 0 ? 0 : 1);
            return;
    }
}

void LightmapBaker::finish(bool ok, const String& why) {
    if (ok) {
        UtilityFunctions::print("LIGHTMAP_BAKE ok ", scenes_[index_], " ", why);
    } else if (!retried_) {
        UtilityFunctions::printerr("LIGHTMAP_BAKE retry ", scenes_[index_], " ", why);
        print_dialogs(get_tree()->get_root());
        retried_ = true;
        step_ = Step::WaitImport;
        wait_frames_ = 0;
        step_elapsed_ = 0.0;
        return;
    } else {
        ++failures_;
        UtilityFunctions::printerr("LIGHTMAP_BAKE fail ", scenes_[index_], " ", why);
        print_dialogs(get_tree()->get_root());
    }
    ++index_;
    retried_ = false;
    step_ = index_ < scenes_.size() ? Step::Open : Step::Done;
    step_elapsed_ = 0.0;
}

Button* LightmapBaker::find_bake_button(Node* node) const {
    if (!node) return nullptr;
    if (auto* button = Object::cast_to<Button>(node)) {
        if (button->get_text() == kButtonText) return button;
    }
    for (int i = 0; i < node->get_child_count(); ++i) {
        if (Button* found = find_bake_button(node->get_child(i))) return found;
    }
    return nullptr;
}

}  // namespace montezuma::godot_binding

// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#pragma once

#include <cstdint>
#include <vector>

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace montezuma::godot_binding {

// Plays sounds by cue name. Code says what happened (`play("match")`);
// the cue table (res://assets/audio/cues.json) says what that sounds like:
// files, bus, level, randomisation, voice limit and priority. Runs as the
// GameAudio autoload; C++ callers reach it through get_singleton().
//
// A cue the table does not know warns once and stays silent, so code can
// call cues before their sounds exist. A file the table names but cannot
// load is an error at startup.
class GameAudio : public godot::Node {
    GDCLASS(GameAudio, godot::Node)

public:
    enum class Kind { OneShot, Music, Ambience };

    static GameAudio* get_singleton() { return singleton_; }

    GameAudio();
    ~GameAudio() override;

    void _enter_tree() override;
    void _exit_tree() override;
    void _ready() override;
    void _process(double delta) override;
    void _notification(int what);

    // One-shots play on a pooled voice. Music cues cross-fade the music
    // player; ambience cues cross-fade the ambience bed. Loops run until
    // stop() or until another music or ambience cue replaces them.
    void play(const godot::StringName& name);
    void stop(const godot::StringName& name);
    void stop_music(double fade_seconds);
    bool has_cue(const godot::StringName& name) const;
    godot::PackedStringArray get_cue_names() const;

    // Player-facing volume per bus ("Master", "Music", "Ambience", "SFX",
    // "UI"), linear 0..1. The settings screen saves and restores these.
    void set_volume(const godot::StringName& bus, double linear);
    double get_volume(const godot::StringName& bus) const;
    void set_muted(const godot::StringName& bus, bool muted);
    bool is_muted(const godot::StringName& bus) const;

    void set_cue_table_path(const godot::String& path) { cue_table_path_ = path; }
    godot::String get_cue_table_path() const { return cue_table_path_; }
    void set_music_fade(double seconds) { music_fade_ = seconds; }
    double get_music_fade() const { return music_fade_; }

    // Voices currently playing a one-shot, for tests and the debug overlay.
    int get_active_voice_count() const;

    // Quits the game once every sound has stopped and the audio server has
    // let go of it, or after a short timeout. Use it instead of
    // SceneTree::quit(); closing the window goes through it too, and so does
    // the `--quit-after-frames=<n>` user argument, which tests and captures
    // use instead of Godot's --quit-after. Godot frees
    // a stopped sound only on a later audio mix and frame, which never come
    // once the tree is torn down, so quitting with a sound playing leaks its
    // stream ("resources still in use at exit").
    void quit(int exit_code);

protected:
    static void _bind_methods();

private:
    struct Cue {
        godot::StringName name;
        Kind kind = Kind::OneShot;
        std::vector<godot::Ref<godot::AudioStream>> streams;
        godot::StringName bus;
        double volume_db = 0.0;
        double volume_random_db = 0.0;
        double pitch_random_semitones = 0.0;
        int max_voices = 4;
        int priority = 2;  // 1 critical, 2 feedback, 3 flavour
        bool duck_music = false;
        int last_variation = -1;
    };

    struct Voice {
        godot::AudioStreamPlayer* player = nullptr;
        int cue = -1;
        int priority = 3;
        uint64_t started = 0;
    };

    // Fades one loop player (music or ambience) in and its partner out.
    struct LoopDeck {
        godot::AudioStreamPlayer* players[2] = {nullptr, nullptr};
        int current = 0;
        int cue = -1;
        double fade = 1.0;
        double gain[2] = {0.0, 0.0};
        double target[2] = {0.0, 0.0};
        double base_db[2] = {0.0, 0.0};
    };

    void ensure_buses();
    bool load_cue_table();
    void start_one_shot(int cue_index);
    void start_loop(LoopDeck& deck, int cue_index, double fade_seconds);
    void stop_loop(LoopDeck& deck, double fade_seconds);
    void update_deck(LoopDeck& deck, double delta);
    int pick_variation(Cue& cue);
    Voice* pick_voice(int cue_index, int priority);
    void apply_bus_volume(int bus_index);
    void run_self_test();
    // Stops every voice and loop at once and keeps their playbacks until
    // the audio server drops them; released() says when it has.
    void release_all();
    bool released() const;

    static GameAudio* singleton_;

    godot::String cue_table_path_ = "res://assets/audio/cues.json";
    double music_fade_ = 1.5;

    std::vector<Cue> cues_;
    godot::HashMap<godot::StringName, int> cue_index_;
    godot::HashSet<godot::StringName> warned_;
    std::vector<Voice> voices_;
    LoopDeck music_;
    LoopDeck ambience_;
    godot::Ref<godot::RandomNumberGenerator> rng_;
    uint64_t play_counter_ = 0;

    // Short, shallow duck of the music and ambience buses while a
    // priority-1 cue plays: felt, not heard.
    double duck_db_ = 0.0;
    double duck_hold_ = 0.0;
    int music_bus_ = -1;
    int ambience_bus_ = -1;
    godot::HashMap<godot::StringName, double> volumes_;

    // Quitting (quit(), the window closing or `--quit-after-frames=<n>`).
    std::vector<godot::Ref<godot::AudioStreamPlayback>> releasing_;
    bool releasing_all_ = false;
    int quit_code_ = -1;  // set while quit() waits for the release
    uint64_t quit_started_msec_ = 0;
    uint64_t quit_after_frames_ = 0;  // 0 without --quit-after-frames
};

}  // namespace montezuma::godot_binding

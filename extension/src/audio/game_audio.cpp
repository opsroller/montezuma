// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

#include "game_audio.hpp"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/audio_effect_hard_limiter.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace montezuma::godot_binding {

namespace {

constexpr int kVoiceCount = 24;            // global one-shot voice budget
constexpr double kDuckDepthDb = -4.0;      // music and ambience under priority-1 cues
constexpr double kDuckHoldSeconds = 0.6;
constexpr double kDuckAttackDbPerSecond = 40.0;
constexpr double kDuckReleaseDbPerSecond = 8.0;
constexpr double kSilentDb = -80.0;
// How long quit() waits for the audio server to free the stopped sounds.
// A stopped sound needs two audio mix steps and one frame to be freed.
constexpr uint64_t kQuitTimeoutMsec = 500;

// The frame count of the `--quit-after-frames=<n>` user argument, else 0.
uint64_t quit_after_frames() {
    const PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
    for (const String& arg : args) {
        if (!arg.begins_with("--quit-after-frames=")) continue;
        const String value = arg.trim_prefix("--quit-after-frames=");
        if (value.is_valid_int() && value.to_int() > 0) return static_cast<uint64_t>(value.to_int());
    }
    return 0;
}

const char* const kBuses[] = {"Music", "Ambience", "SFX", "UI"};

double to_db(double linear) {
    return linear <= 0.0001 ? kSilentDb : 20.0 * std::log10(linear);
}

double number(const Dictionary& d, const char* key, double fallback) {
    const Variant v = d.get(key, fallback);
    return v.get_type() == Variant::FLOAT || v.get_type() == Variant::INT ? double(v) : fallback;
}

}  // namespace

GameAudio* GameAudio::singleton_ = nullptr;

GameAudio::GameAudio() {
    rng_.instantiate();
}

GameAudio::~GameAudio() {
    if (singleton_ == this) singleton_ = nullptr;
}

void GameAudio::_bind_methods() {
    ClassDB::bind_method(D_METHOD("play", "name"), &GameAudio::play);
    ClassDB::bind_method(D_METHOD("stop", "name"), &GameAudio::stop);
    ClassDB::bind_method(D_METHOD("stop_music", "fade_seconds"), &GameAudio::stop_music);
    ClassDB::bind_method(D_METHOD("has_cue", "name"), &GameAudio::has_cue);
    ClassDB::bind_method(D_METHOD("get_cue_names"), &GameAudio::get_cue_names);
    ClassDB::bind_method(D_METHOD("set_volume", "bus", "linear"), &GameAudio::set_volume);
    ClassDB::bind_method(D_METHOD("get_volume", "bus"), &GameAudio::get_volume);
    ClassDB::bind_method(D_METHOD("set_muted", "bus", "muted"), &GameAudio::set_muted);
    ClassDB::bind_method(D_METHOD("is_muted", "bus"), &GameAudio::is_muted);
    ClassDB::bind_method(D_METHOD("get_active_voice_count"), &GameAudio::get_active_voice_count);
    ClassDB::bind_method(D_METHOD("quit", "exit_code"), &GameAudio::quit, DEFVAL(0));

    ClassDB::bind_method(D_METHOD("set_cue_table_path", "path"), &GameAudio::set_cue_table_path);
    ClassDB::bind_method(D_METHOD("get_cue_table_path"), &GameAudio::get_cue_table_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "cue_table_path", PROPERTY_HINT_FILE, "*.json"),
                 "set_cue_table_path", "get_cue_table_path");
    ClassDB::bind_method(D_METHOD("set_music_fade", "seconds"), &GameAudio::set_music_fade);
    ClassDB::bind_method(D_METHOD("get_music_fade"), &GameAudio::get_music_fade);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "music_fade", PROPERTY_HINT_RANGE, "0,10,0.1,suffix:s"),
                 "set_music_fade", "get_music_fade");
}

void GameAudio::_enter_tree() {
    if (singleton_ == nullptr) singleton_ = this;
}

void GameAudio::_exit_tree() {
    // Stop every playback before the players go, so none outlives the tree.
    for (Voice& voice : voices_) voice.player->stop();
    stop_loop(music_, 0.0);
    stop_loop(ambience_, 0.0);
    if (singleton_ == this) singleton_ = nullptr;
}

void GameAudio::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    ensure_buses();

    voices_.reserve(kVoiceCount);
    for (int i = 0; i < kVoiceCount; ++i) {
        auto* player = memnew(AudioStreamPlayer);
        add_child(player);
        voices_.push_back(Voice{player});
    }
    for (LoopDeck* deck : {&music_, &ambience_}) {
        for (auto*& player : deck->players) {
            player = memnew(AudioStreamPlayer);
            player->set_bus(deck == &music_ ? "Music" : "Ambience");
            add_child(player);
        }
    }

    load_cue_table();
    // The window's close button quits through quit(), so sounds stop first.
    get_tree()->set_auto_accept_quit(false);
    quit_after_frames_ = quit_after_frames();
    if (OS::get_singleton()->get_cmdline_user_args().has("--audio-selftest")) run_self_test();
}

void GameAudio::_notification(int what) {
    if (what == NOTIFICATION_WM_CLOSE_REQUEST && !Engine::get_singleton()->is_editor_hint()) quit(0);
}

void GameAudio::quit(int exit_code) {
    if (quit_code_ >= 0) return;
    quit_code_ = exit_code;
    quit_started_msec_ = Time::get_singleton()->get_ticks_msec();
    release_all();
    // _process quits once the playbacks are gone; it runs even while the
    // game is paused (process_mode Always in game_audio.tscn).
    set_process(true);
}

void GameAudio::release_all() {
    releasing_all_ = true;
    auto release = [this](AudioStreamPlayer* player) {
        if (player == nullptr) return;
        if (player->has_stream_playback()) releasing_.push_back(player->get_stream_playback());
        player->stop();
    };
    for (Voice& voice : voices_) release(voice.player);
    for (LoopDeck* deck : {&music_, &ambience_}) {
        for (AudioStreamPlayer* player : deck->players) release(player);
    }
    stop_loop(music_, 0.0);
    stop_loop(ambience_, 0.0);
}

bool GameAudio::released() const {
    // Each playback is ours alone once the audio server has freed its entry.
    return std::all_of(releasing_.begin(), releasing_.end(),
                       [](const Ref<AudioStreamPlayback>& p) { return p->get_reference_count() <= 1; });
}

void GameAudio::ensure_buses() {
    AudioServer* server = AudioServer::get_singleton();
    for (const char* name : kBuses) {
        if (server->get_bus_index(name) >= 0) continue;
        const int index = server->get_bus_count();
        server->add_bus(index);
        server->set_bus_name(index, name);
        server->set_bus_send(index, "Master");
    }
    // A safety net on the master, not the loudness control: files are
    // levelled so it rarely engages.
    if (server->get_bus_effect_count(0) == 0) {
        Ref<AudioEffectHardLimiter> limiter;
        limiter.instantiate();
        limiter->set_ceiling_db(-1.0);
        server->add_bus_effect(0, limiter);
    }
    music_bus_ = server->get_bus_index("Music");
    ambience_bus_ = server->get_bus_index("Ambience");
    for (const char* name : {"Master", "Music", "Ambience", "SFX", "UI"}) {
        if (!volumes_.has(name)) volumes_[name] = 1.0;
    }
}

bool GameAudio::load_cue_table() {
    const String text = FileAccess::get_file_as_string(cue_table_path_);
    if (text.is_empty()) {
        UtilityFunctions::push_error("GameAudio: cannot read cue table ", cue_table_path_);
        return false;
    }
    const Variant parsed = JSON::parse_string(text);
    if (parsed.get_type() != Variant::DICTIONARY) {
        UtilityFunctions::push_error("GameAudio: cue table is not a JSON object: ", cue_table_path_);
        return false;
    }
    const Dictionary root = parsed;
    const String base = cue_table_path_.get_base_dir() + "/";
    const Dictionary table = root.get("cues", Dictionary());
    const Array names = table.keys();

    bool ok = true;
    cues_.clear();
    cue_index_.clear();
    cues_.reserve(names.size());
    for (int64_t i = 0; i < names.size(); ++i) {
        const String name = names[i];
        const Variant entry = table[name];
        if (entry.get_type() != Variant::DICTIONARY) {
            UtilityFunctions::push_error("GameAudio: cue '", name, "' is not an object");
            ok = false;
            continue;
        }
        const Dictionary d = entry;
        Cue cue;
        cue.name = name;
        const String kind = d.get("type", "one_shot");
        cue.kind = kind == "music" ? Kind::Music : kind == "ambience" ? Kind::Ambience : Kind::OneShot;
        cue.bus = String(d.get("bus", cue.kind == Kind::Music      ? "Music"
                                      : cue.kind == Kind::Ambience ? "Ambience"
                                                                   : "SFX"));
        cue.volume_db = number(d, "volume_db", 0.0);
        cue.volume_random_db = number(d, "volume_random_db", 0.0);
        cue.pitch_random_semitones = number(d, "pitch_random_semitones", 0.0);
        cue.max_voices = std::max(1, int(number(d, "max_voices", 4)));
        cue.priority = std::clamp(int(number(d, "priority", 2)), 1, 3);
        cue.duck_music = bool(d.get("duck_music", cue.priority == 1));

        const Array files = d.get("files", Array());
        for (int64_t f = 0; f < files.size(); ++f) {
            const String path = base + String(files[f]);
            Ref<AudioStream> stream = ResourceLoader::get_singleton()->load(path);
            if (stream.is_null()) {
                UtilityFunctions::push_error("GameAudio: cue '", name, "' cannot load ", path);
                ok = false;
                continue;
            }
            if (cue.kind != Kind::OneShot) stream->set("loop", true);  // Ogg Vorbis loops
            cue.streams.push_back(stream);
        }
        if (cue.streams.empty()) {
            UtilityFunctions::push_error("GameAudio: cue '", name, "' has no playable files");
            ok = false;
            continue;
        }
        cue_index_[cue.name] = int(cues_.size());
        cues_.push_back(std::move(cue));
    }
    return ok;
}

void GameAudio::play(const StringName& name) {
    if (releasing_all_) return;  // quitting
    const auto found = cue_index_.find(name);
    if (found == cue_index_.end()) {
        if (!warned_.has(name)) {
            warned_.insert(name);
            UtilityFunctions::push_warning("GameAudio: no cue named '", name, "'; staying silent");
        }
        return;
    }
    const int index = found->value;
    switch (cues_[index].kind) {
        case Kind::Music: start_loop(music_, index, music_fade_); break;
        case Kind::Ambience: start_loop(ambience_, index, music_fade_); break;
        case Kind::OneShot: start_one_shot(index); break;
    }
}

void GameAudio::stop(const StringName& name) {
    const auto found = cue_index_.find(name);
    if (found == cue_index_.end()) return;
    const int index = found->value;
    if (music_.cue == index) stop_loop(music_, music_fade_);
    if (ambience_.cue == index) stop_loop(ambience_, music_fade_);
    for (Voice& voice : voices_) {
        if (voice.cue == index && voice.player->is_playing()) voice.player->stop();
    }
}

void GameAudio::stop_music(double fade_seconds) {
    stop_loop(music_, fade_seconds);
}

bool GameAudio::has_cue(const StringName& name) const {
    return cue_index_.has(name);
}

PackedStringArray GameAudio::get_cue_names() const {
    PackedStringArray out;
    for (const Cue& cue : cues_) out.push_back(cue.name);
    return out;
}

int GameAudio::pick_variation(Cue& cue) {
    const int count = int(cue.streams.size());
    if (count == 1) return 0;
    // Random without an immediate repeat.
    int pick = int(rng_->randi_range(0, count - 2));
    if (pick >= cue.last_variation && cue.last_variation >= 0) ++pick;
    cue.last_variation = pick;
    return pick;
}

GameAudio::Voice* GameAudio::pick_voice(int cue_index, int priority) {
    Voice* oldest_same = nullptr;
    Voice* free_voice = nullptr;
    Voice* victim = nullptr;
    int same = 0;
    for (Voice& voice : voices_) {
        if (!voice.player->is_playing()) {
            if (free_voice == nullptr) free_voice = &voice;
            continue;
        }
        if (voice.cue == cue_index) {
            ++same;
            if (oldest_same == nullptr || voice.started < oldest_same->started) oldest_same = &voice;
        }
        // Steal the least important, then the oldest, never a more important one.
        if (voice.priority >= priority &&
            (victim == nullptr || voice.priority > victim->priority ||
             (voice.priority == victim->priority && voice.started < victim->started))) {
            victim = &voice;
        }
    }
    if (same >= cues_[cue_index].max_voices) return oldest_same;
    return free_voice != nullptr ? free_voice : victim;
}

void GameAudio::start_one_shot(int cue_index) {
    Cue& cue = cues_[cue_index];
    Voice* voice = pick_voice(cue_index, cue.priority);
    if (voice == nullptr) return;  // every voice is busy with something more important

    AudioStreamPlayer* player = voice->player;
    player->stop();
    player->set_stream(cue.streams[pick_variation(cue)]);
    player->set_bus(cue.bus);
    const auto spread_db = static_cast<float>(cue.volume_random_db);
    player->set_volume_db(static_cast<float>(cue.volume_db + rng_->randf_range(-spread_db, spread_db)));
    const auto spread_st = static_cast<float>(cue.pitch_random_semitones);
    const double semitones = rng_->randf_range(-spread_st, spread_st);
    player->set_pitch_scale(static_cast<float>(std::pow(2.0, semitones / 12.0)));
    player->play();
    voice->cue = cue_index;
    voice->priority = cue.priority;
    voice->started = ++play_counter_;

    if (cue.duck_music) duck_hold_ = kDuckHoldSeconds;
}

void GameAudio::start_loop(LoopDeck& deck, int cue_index, double fade_seconds) {
    if (deck.cue == cue_index && deck.players[deck.current]->is_playing()) return;
    Cue& cue = cues_[cue_index];
    // Take the idle player so a loop still fading out is never cut off.
    const int next = deck.players[deck.current]->is_playing() ? 1 - deck.current : deck.current;
    AudioStreamPlayer* player = deck.players[next];
    player->stop();
    player->set_stream(cue.streams[pick_variation(cue)]);
    player->set_bus(cue.bus);
    deck.base_db[next] = cue.volume_db;
    deck.gain[next] = fade_seconds > 0.0 ? 0.0 : 1.0;
    deck.target[next] = 1.0;
    if (next != deck.current) deck.target[deck.current] = 0.0;
    deck.fade = fade_seconds;
    deck.current = next;
    deck.cue = cue_index;
    player->set_volume_db(static_cast<float>(deck.base_db[next] + to_db(deck.gain[next])));
    player->play();
}

void GameAudio::stop_loop(LoopDeck& deck, double fade_seconds) {
    deck.target[0] = deck.target[1] = 0.0;
    deck.fade = fade_seconds;
    deck.cue = -1;
    if (fade_seconds <= 0.0) {
        for (int i = 0; i < 2; ++i) {
            deck.gain[i] = 0.0;
            if (deck.players[i] != nullptr) deck.players[i]->stop();
        }
    }
}

void GameAudio::update_deck(LoopDeck& deck, double delta) {
    const double step = deck.fade > 0.0 ? delta / deck.fade : 1.0;
    for (int i = 0; i < 2; ++i) {
        AudioStreamPlayer* player = deck.players[i];
        if (player == nullptr || !player->is_playing() || deck.gain[i] == deck.target[i]) continue;
        deck.gain[i] = deck.gain[i] < deck.target[i] ? std::min(deck.target[i], deck.gain[i] + step)
                                                     : std::max(deck.target[i], deck.gain[i] - step);
        if (deck.gain[i] <= 0.0) {
            player->stop();
        } else {
            player->set_volume_db(static_cast<float>(deck.base_db[i] + to_db(deck.gain[i])));
        }
    }
}

void GameAudio::_process(double delta) {
    if (quit_after_frames_ > 0 && Engine::get_singleton()->get_process_frames() >= quit_after_frames_) quit(0);
    if (quit_code_ >= 0 &&
        (released() || Time::get_singleton()->get_ticks_msec() - quit_started_msec_ >= kQuitTimeoutMsec)) {
        releasing_.clear();
        get_tree()->quit(quit_code_);
        return;
    }
    if (releasing_all_) return;  // nothing plays again on the way out
    update_deck(music_, delta);
    update_deck(ambience_, delta);

    const double target = duck_hold_ > 0.0 ? kDuckDepthDb : 0.0;
    duck_hold_ = std::max(0.0, duck_hold_ - delta);
    if (duck_db_ != target) {
        duck_db_ = duck_db_ > target ? std::max(target, duck_db_ - kDuckAttackDbPerSecond * delta)
                                     : std::min(target, duck_db_ + kDuckReleaseDbPerSecond * delta);
        apply_bus_volume(music_bus_);
        apply_bus_volume(ambience_bus_);
    }
}

void GameAudio::apply_bus_volume(int bus_index) {
    if (bus_index < 0) return;
    AudioServer* server = AudioServer::get_singleton();
    const StringName name = server->get_bus_name(bus_index);
    const auto found = volumes_.find(name);
    const double linear = found == volumes_.end() ? 1.0 : found->value;
    const bool ducked = bus_index == music_bus_ || bus_index == ambience_bus_;
    server->set_bus_volume_db(bus_index, static_cast<float>(to_db(linear) + (ducked ? duck_db_ : 0.0)));
}

void GameAudio::set_volume(const StringName& bus, double linear) {
    const int index = AudioServer::get_singleton()->get_bus_index(bus);
    if (index < 0) {
        UtilityFunctions::push_warning("GameAudio: no audio bus named '", bus, "'");
        return;
    }
    volumes_[bus] = std::clamp(linear, 0.0, 1.0);
    apply_bus_volume(index);
}

double GameAudio::get_volume(const StringName& bus) const {
    const auto found = volumes_.find(bus);
    return found == volumes_.end() ? 1.0 : found->value;
}

void GameAudio::set_muted(const StringName& bus, bool muted) {
    const int index = AudioServer::get_singleton()->get_bus_index(bus);
    if (index >= 0) AudioServer::get_singleton()->set_bus_mute(index, muted);
}

bool GameAudio::is_muted(const StringName& bus) const {
    const int index = AudioServer::get_singleton()->get_bus_index(bus);
    return index >= 0 && AudioServer::get_singleton()->is_bus_mute(index);
}

int GameAudio::get_active_voice_count() const {
    int count = 0;
    for (const Voice& voice : voices_) count += voice.player->is_playing() ? 1 : 0;
    return count;
}

void GameAudio::run_self_test() {
    // Started with `-- --audio-selftest`: play every cue once so a headless
    // run proves each file loads and starts, then check the voice limits.
    int failures = 0;
    for (const Cue& cue : cues_) {
        play(cue.name);
        const bool started = cue.kind == Kind::OneShot ? get_active_voice_count() > 0
                             : cue.kind == Kind::Music ? music_.cue >= 0
                                                       : ambience_.cue >= 0;
        if (!started) {
            UtilityFunctions::push_error("GameAudio self-test: cue '", cue.name, "' did not start");
            ++failures;
        }
    }
    const auto swap = cue_index_.find("swap");
    if (swap != cue_index_.end()) {
        for (int i = 0; i < kVoiceCount; ++i) play("swap");
        int playing = 0;
        for (const Voice& voice : voices_) playing += voice.cue == swap->value && voice.player->is_playing() ? 1 : 0;
        if (playing > cues_[swap->value].max_voices) {
            UtilityFunctions::push_error("GameAudio self-test: 'swap' passed its voice limit");
            ++failures;
        }
    }
    play("no_such_cue_for_self_test");
    for (Voice& voice : voices_) voice.player->stop();
    stop_loop(music_, 0.0);
    stop_loop(ambience_, 0.0);
    UtilityFunctions::print("GameAudio self-test: ", int64_t(cues_.size()), " cues, ", failures, " failures");
}

}  // namespace montezuma::godot_binding

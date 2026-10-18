class_name LevelState
extends RefCounted
## Quotas, timer, score and the same-kind streak that charges Power Totems.

enum Outcome { PLAYING, WON, LOST }

var quotas: Dictionary  # kind -> pieces still needed
var time_left: float
var score := 0
var streak_kind := -1
var streak := 0
var outcome := Outcome.PLAYING


func _init(level_quotas: Dictionary, time_limit: float) -> void:
	quotas = level_quotas.duplicate()
	time_left = time_limit


func apply_move(result: MoveResult) -> void:
	if outcome != Outcome.PLAYING:
		return
	score += result.score()
	var cleared := result.cleared_by_kind()
	for kind in quotas:
		quotas[kind] = maxi(0, quotas[kind] - cleared.get(kind, 0))
	if result.matched_kind == streak_kind:
		streak += 1
	else:
		streak_kind = result.matched_kind
		streak = 1
	if quotas.values().all(func(n): return n == 0):
		outcome = Outcome.WON


func tick(delta: float) -> void:
	if outcome != Outcome.PLAYING:
		return
	time_left = maxf(0.0, time_left - delta)
	if time_left == 0.0:
		outcome = Outcome.LOST

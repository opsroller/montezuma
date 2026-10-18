extends SceneTree
## Headless test runner: godot --headless --script res://tests/run_tests.gd

var failures := 0
var checks := 0


func _init() -> void:
	test_fresh_board_has_no_matches_and_a_move()
	test_illegal_swaps_are_rejected()
	test_legal_swap_clears_and_refills()
	test_matches_count_overlapping_cells_once()
	test_cascade_scoring()
	test_level_win_and_streak()
	test_level_loses_on_timeout()
	test_seed_is_deterministic()
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)


func check(cond: bool, label: String) -> void:
	checks += 1
	if not cond:
		failures += 1
		push_error("FAIL: " + label)


func board_from(rows: Array) -> Board:
	var b := Board.new(rows[0].length(), rows.size(), 7, 1)
	for y in rows.size():
		for x in rows[y].length():
			b.set_cell(x, y, int(rows[y][x]))
	return b


func test_fresh_board_has_no_matches_and_a_move() -> void:
	for s in 50:
		var b := Board.new(8, 8, 7, s)
		check(b.find_matches().is_empty(), "fresh board seed %d has no matches" % s)
		check(b.has_legal_move(), "fresh board seed %d has a move" % s)


func test_illegal_swaps_are_rejected() -> void:
	var b := board_from(["0123", "1230", "2301", "3012"])
	var before := b.cells.duplicate()
	check(b.try_swap(Vector2i(0, 0), Vector2i(2, 0)) == null, "non-adjacent swap rejected")
	check(b.try_swap(Vector2i(0, 0), Vector2i(1, 0)) == null, "non-matching swap rejected")
	check(b.try_swap(Vector2i(3, 3), Vector2i(4, 3)) == null, "out of bounds rejected")
	check(b.cells == before, "rejected swaps leave board unchanged")


func test_legal_swap_clears_and_refills() -> void:
	var b := board_from(["0010", "1234", "2345", "3456"])
	var r := b.try_swap(Vector2i(2, 0), Vector2i(3, 0))
	check(r != null, "swap making 000 is legal")
	check(r.steps.size() >= 1 and r.steps[0].get(0, 0) == 3, "first step clears three of kind 0")
	check(r.matched_kind == 0, "matched kind is 0")
	check(not b.cells.has(Board.EMPTY), "board fully refilled")
	check(b.find_matches().is_empty(), "no matches left after resolve")


func test_matches_count_overlapping_cells_once() -> void:
	var b := board_from(["00012", "01345", "02456", "13561", "24612"])
	check(b.find_matches().size() == 5, "L-shape of 5 cells counted once each")


func test_cascade_scoring() -> void:
	var r := MoveResult.new()
	r.steps = [{0: 3}, {1: 4}]
	check(r.score() == 3 * 10 * 1 + 4 * 10 * 2, "cascade multiplier applied")
	check(r.cleared_by_kind() == {0: 3, 1: 4}, "cleared totals per kind")


func test_level_win_and_streak() -> void:
	var level := LevelState.new({0: 5, 1: 3}, 120.0)
	var a := MoveResult.new()
	a.steps = [{0: 3}]
	a.matched_kind = 0
	level.apply_move(a)
	level.apply_move(a)
	check(level.streak == 2 and level.streak_kind == 0, "same-kind matches build a streak")
	check(level.quotas[0] == 0 and level.outcome == LevelState.Outcome.PLAYING, "partial quotas keep playing")
	var b := MoveResult.new()
	b.steps = [{1: 3}]
	b.matched_kind = 1
	level.apply_move(b)
	check(level.streak == 1 and level.streak_kind == 1, "different kind resets the streak")
	check(level.outcome == LevelState.Outcome.WON, "all quotas met wins")


func test_level_loses_on_timeout() -> void:
	var level := LevelState.new({0: 5}, 2.0)
	level.tick(1.5)
	check(level.outcome == LevelState.Outcome.PLAYING, "still playing with time left")
	level.tick(1.0)
	check(level.outcome == LevelState.Outcome.LOST and level.time_left == 0.0, "timeout loses")


func test_seed_is_deterministic() -> void:
	check(Board.new(8, 8, 7, 42).cells == Board.new(8, 8, 7, 42).cells, "same seed, same board")

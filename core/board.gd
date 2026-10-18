class_name Board
extends RefCounted
## Pure match-3 board model. No rendering; the 3D layer observes results.

const EMPTY := -1

var width: int
var height: int
var kinds: int
var cells: PackedInt32Array
var rng: RandomNumberGenerator


func _init(w: int = 8, h: int = 8, kind_count: int = 7, seed_value: int = 0) -> void:
	width = w
	height = h
	kinds = kind_count
	rng = RandomNumberGenerator.new()
	rng.seed = seed_value
	cells = PackedInt32Array()
	cells.resize(width * height)
	fill_fresh()


func get_cell(x: int, y: int) -> int:
	return cells[y * width + x]


func set_cell(x: int, y: int, kind: int) -> void:
	cells[y * width + x] = kind


func in_bounds(x: int, y: int) -> bool:
	return x >= 0 and y >= 0 and x < width and y < height


## Fills the board with no pre-existing matches and at least one legal move.
func fill_fresh() -> void:
	while true:
		for y in height:
			for x in width:
				set_cell(x, y, _pick_non_matching(x, y))
		if has_legal_move():
			return


func _pick_non_matching(x: int, y: int) -> int:
	var kind := rng.randi_range(0, kinds - 1)
	for _i in kinds:
		var row := x >= 2 and get_cell(x - 1, y) == kind and get_cell(x - 2, y) == kind
		var col := y >= 2 and get_cell(x, y - 1) == kind and get_cell(x, y - 2) == kind
		if not row and not col:
			return kind
		kind = (kind + 1) % kinds
	return kind


## Returns the set of matched cell indices as a Dictionary used as a set.
func find_matches() -> Dictionary:
	var matched := {}
	for y in height:
		var x := 0
		while x < width:
			var kind := get_cell(x, y)
			var end := x + 1
			while end < width and kind != EMPTY and get_cell(end, y) == kind:
				end += 1
			if kind != EMPTY and end - x >= 3:
				for i in range(x, end):
					matched[y * width + i] = true
			x = end
	for x in width:
		var y := 0
		while y < height:
			var kind := get_cell(x, y)
			var end := y + 1
			while end < height and kind != EMPTY and get_cell(x, end) == kind:
				end += 1
			if kind != EMPTY and end - y >= 3:
				for i in range(y, end):
					matched[i * width + x] = true
			y = end
	return matched


func _swap_cells(a: Vector2i, b: Vector2i) -> void:
	var t := get_cell(a.x, a.y)
	set_cell(a.x, a.y, get_cell(b.x, b.y))
	set_cell(b.x, b.y, t)


func is_adjacent(a: Vector2i, b: Vector2i) -> bool:
	return absi(a.x - b.x) + absi(a.y - b.y) == 1


## A swap is legal when the cells are adjacent and it creates a match.
func is_legal_swap(a: Vector2i, b: Vector2i) -> bool:
	if not in_bounds(a.x, a.y) or not in_bounds(b.x, b.y) or not is_adjacent(a, b):
		return false
	_swap_cells(a, b)
	var ok := not find_matches().is_empty()
	_swap_cells(a, b)
	return ok


func has_legal_move() -> bool:
	for y in height:
		for x in width:
			var here := Vector2i(x, y)
			if x + 1 < width and is_legal_swap(here, Vector2i(x + 1, y)):
				return true
			if y + 1 < height and is_legal_swap(here, Vector2i(x, y + 1)):
				return true
	return false


## Applies a swap and resolves all cascades. Returns null for an illegal swap,
## otherwise a MoveResult describing every cascade step for the view to animate.
func try_swap(a: Vector2i, b: Vector2i) -> MoveResult:
	if not is_legal_swap(a, b):
		return null
	_swap_cells(a, b)
	var result := MoveResult.new()
	result.matched_kind = _dominant_kind(find_matches())
	while true:
		var matched := find_matches()
		if matched.is_empty():
			break
		var step := {}
		for idx in matched:
			var kind := cells[idx]
			step[kind] = step.get(kind, 0) + 1
			cells[idx] = EMPTY
		result.steps.append(step)
		_collapse_and_refill()
	if not has_legal_move():
		shuffle()
		result.shuffled = true
	return result


func _dominant_kind(matched: Dictionary) -> int:
	var counts := {}
	var best := EMPTY
	for idx in matched:
		var kind := cells[idx]
		counts[kind] = counts.get(kind, 0) + 1
		if best == EMPTY or counts[kind] > counts[best]:
			best = kind
	return best


func _collapse_and_refill() -> void:
	for x in width:
		var write := height - 1
		for y in range(height - 1, -1, -1):
			var kind := get_cell(x, y)
			if kind != EMPTY:
				set_cell(x, write, kind)
				write -= 1
		for y in range(write, -1, -1):
			set_cell(x, y, rng.randi_range(0, kinds - 1))


## Reorders existing pieces until no match shows and a legal move exists.
func shuffle() -> void:
	var pieces := Array(cells)
	while true:
		for i in range(pieces.size() - 1, 0, -1):
			var j := rng.randi_range(0, i)
			var t = pieces[i]
			pieces[i] = pieces[j]
			pieces[j] = t
		cells = PackedInt32Array(pieces)
		if find_matches().is_empty() and has_legal_move():
			return

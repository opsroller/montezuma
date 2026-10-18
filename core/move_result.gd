class_name MoveResult
extends RefCounted
## Outcome of one legal swap, ordered for animation.

## One Dictionary per cascade step: artifact kind -> pieces cleared.
var steps: Array[Dictionary] = []
## Kind that made up most of the swap's own match; drives Power Totem streaks.
var matched_kind: int = -1
var shuffled := false


func cleared_by_kind() -> Dictionary:
	var total := {}
	for step in steps:
		for kind in step:
			total[kind] = total.get(kind, 0) + step[kind]
	return total


## 10 points per piece, multiplied by the cascade step (1-based).
func score() -> int:
	var points := 0
	for i in steps.size():
		for kind in steps[i]:
			points += steps[i][kind] * 10 * (i + 1)
	return points

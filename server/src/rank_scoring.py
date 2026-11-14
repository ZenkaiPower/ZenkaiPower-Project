"""Ranked point scaling. Inputs are whole displayed points, frozen before a battle."""

FALLBACK_TOLERANCE = 250
MIN_POPULATION = 20
SCALE = 1000
MAX_POINTS = 9999


def percentile(values, fraction):
    """Linear interpolation at (n - 1) * fraction; values must be sorted."""
    position = (len(values) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(values) - 1)
    return values[lower] + (values[upper] - values[lower]) * (position - lower)


def population_tolerance(points):
    if len(points) < MIN_POPULATION:
        return FALLBACK_TOLERANCE
    values = sorted(points)
    spread = percentile(values, 0.75) - percentile(values, 0.25)
    return min(500, max(100, int(spread * 0.10 + 0.5)))


def adjusted_points(start_points, winner, normal_changes, tolerance):
    """Return signed changes; ties award no points. Round halves upward."""
    if winner == "tie":
        return (0, 0)
    win = 0 if winner == "p1" else 1
    lose = 1 - win
    gap = max(0, start_points[win] - start_points[lose] - tolerance)
    denominator = SCALE + gap
    changes = [0, 0]
    for side in (win, lose):
        base = abs(normal_changes[side])
        magnitude = max(1, (base * SCALE + denominator // 2) // denominator)
        limit = MAX_POINTS - start_points[side] if side == win else start_points[side]
        magnitude = min(max(0, limit), magnitude)
        changes[side] = magnitude if side == win else -magnitude
    return tuple(changes)

"""Geometry-only checks; does not compile or launch the game.

This independently checks the authored layout rules, not the C++ runtime simulation.
Run the game's opt-in --verify suite after building to check the real simulation.
"""
from collections import deque
import itertools
import math
from pathlib import Path
import random
import re

ROOT = Path(__file__).resolve().parents[1]
RULES = (ROOT / "SimpleGame/Planet.h").read_text(encoding="utf-8")
SOURCE = (ROOT / "SimpleGame/PlanetGeneration.cpp").read_text(encoding="utf-8")


def constant(name):
    return int(re.search(rf"constexpr int {name} = (\d+)", RULES)[1])


def verify(seed, extreme=None):
    rng = random.Random(seed)
    jitter = lambda: extreme if extreme is not None else rng.randrange(-2, 3)
    speed = 155
    spacing = round(speed * 30 / math.hypot(42, 21))
    centers = [(-7, 1), (-7 + spacing + jitter(), 1 + jitter()),
               (-7 + spacing + jitter(), 1 - spacing + jitter()),
               (-7 + spacing * 2 + jitter(), 1 + jitter()),
               (-7 + spacing * 2 + jitter(), 1 - spacing + jitter())]
    slots = [float(v.strip()) for v in re.search(r"slots\[\] = \{([^}]+)\}", SOURCE)[1].split(",")]
    assert len(slots) ** 2 == constant("HomesPerVillage")
    assert constant("VillageCount") == len(centers)
    assert constant("ResidentsPerVillage") == 30
    radius = constant("VillageRadius")
    assert max(abs(v) for v in slots) + 2 < radius
    bounds = (-30, -125, spacing * 2 + 17, 24)
    x0, y0, x1, y1 = bounds
    obstacles, entrances = [], []
    for vx, vy in centers:
        assert x0 <= vx - radius and vx + radius <= x1
        assert y0 <= vy - radius and vy + radius <= y1
        for sx, sy in itertools.product(slots, repeat=2):
            obstacles.append((vx + sx, vy + sy, 1.8, 1.6))
            entrances.append((vx + sx, vy + sy + 1.5))
        # Read the real authored scenery rows, so added buildings are included in reachability checks.
        number = r"(-?(?:\d+(?:\.\d*)?|\.\d+))f?"
        rows = re.findall(
            r"\{PropKind::\w+,\s*\{" + number + r",\s*" + number + r"\},\s*" +
            number + r",\s*" + number + r",\s*" + number +
            r',\s*"([^"]+)",\s*(true|false)\}', SOURCE)
        assert len(rows) == 9, "Scenery parser must cover every authored village prop"
        for sx, sy, width, depth, height, binding, solid in rows:
            if solid == "true":
                obstacles.append((vx + float(sx), vy + float(sy), float(width), float(depth)))
    obstacles.append((centers[0][0]-7, centers[0][1]+.5, 3, 2.5))
    obstacles.append((centers[4][0] - 2, centers[4][1] + 1.5, .8, .8))
    for vx, vy in centers[1:4]:
        obstacles.append((vx + 1.5, vy - 1.5, .55, .55))
    buckets = {}
    for obstacle in obstacles:
        x, y, w, d = obstacle
        for bx in range(math.floor((x-w/2)/8), math.floor((x+w/2)/8)+1):
            for by in range(math.floor((y-d/2)/8), math.floor((y+d/2)/8)+1):
                buckets.setdefault((bx, by), []).append(obstacle)

    def walkable(x, y, r=.28):
        if not (x0-.5+r <= x <= x1+.5-r and y0-.5+r <= y <= y1+.5-r):
            return False
        for bx in range(math.floor((x-r)/8), math.floor((x+r)/8)+1):
            for by in range(math.floor((y-r)/8), math.floor((y+r)/8)+1):
                for ox, oy, w, d in buckets.get((bx, by), []):
                    if abs(x-ox) < w/2+r and abs(y-oy) < d/2+r:
                        return False
        return True

    def clear(a, b):
        steps = max(1, math.ceil(math.dist(a, b) / .12))
        return all(walkable(a[0] + (b[0]-a[0])*i/steps,
                            a[1] + (b[1]-a[1])*i/steps) for i in range(steps+1))

    opened = {(x, y) for x in range(x0, x1+1) for y in range(y0, y1+1) if walkable(x, y)}
    visited = {(-5, 1)}
    pending = deque(visited)
    while pending:
        a = pending.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            b = (a[0]+dx, a[1]+dy)
            if b in opened and b not in visited and clear(a, b):
                visited.add(b)
                pending.append(b)
    assert visited == opened, f"seed {seed}: disconnected navigation pocket"
    for point in entrances + centers:
        assert walkable(*point), f"seed {seed}: obstructed entrance {point}"
        assert any((x, y) in visited and clear(point, (x, y))
                   for x in range(round(point[0])-2, round(point[0])+3)
                   for y in range(round(point[1])-2, round(point[1])+3)), point
    interactables = [(centers[0][0]-7, centers[0][1]+.5, 2.6),
                     (centers[4][0]-2, centers[4][1]+1.5, 1.8)]
    for vx, vy in centers[1:4]:
        interactables.extend(((vx+1.5, vy-1.5, 1.8), (vx-2, vy-2, 1.8)))
    for px, py, reach in interactables:
        assert any((x, y) in visited and math.dist((px, py), (x, y)) < reach
                   for x in range(round(px)-3, round(px)+4)
                   for y in range(round(py)-3, round(py)+4)), (px, py)
    times = []
    for a, b in ((0, 1), (1, 2), (1, 3), (3, 4), (2, 4)):
        dx, dy = abs(centers[a][0]-centers[b][0]), abs(centers[a][1]-centers[b][1])
        seconds = (dx+dy)*math.hypot(42, 21)/speed
        assert 27 <= seconds <= 33, seconds
        times.append(seconds)
    return min(times), max(times), (x1-x0+1)*(y1-y0+1)


if __name__ == "__main__":
    results = [verify(seed) for seed in range(1, 9)]
    results.extend(verify(0, extreme) for extreme in (-2, 2))
    print(f"PASS geometry model: {len(results)} layouts, {results[0][2]:,} tiles, "
          f"150 residents / 180 homes, connected floor and all home entrances")
    print(f"PASS linked village center travel: {min(r[0] for r in results):.2f}"
          f"..{max(r[1] for r in results):.2f}s at base walking speed")
    print("NOTE: C++ runtime, animation and rendering were not built or executed.")

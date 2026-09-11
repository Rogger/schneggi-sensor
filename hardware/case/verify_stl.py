#!/usr/bin/env python3
"""Basic STL sanity checks for the generated case parts."""

from __future__ import annotations

import re
from pathlib import Path


CASE_DIR = Path(__file__).resolve().parent
EXPECTED_BOUNDS = {
    "bottom.stl": ((-19.8, -17.3, 0.0), (19.8, 17.3, 11.5)),
    "top.stl": ((-19.8, -17.3, 10.7), (19.8, 17.3, 13.1)),
    "pcb.stl": ((-17.5, -15.0, 3.8), (17.5, 15.0, 10.4)),
    "assembly.stl": ((-19.8, -17.3, 0.0), (19.8, 17.3, 13.1)),
}
HOLE_CENTERS = ((-15.3, -12.8), (15.3, -12.8), (15.3, 12.8), (-15.3, 12.8))
SCREW_CLEARANCE_RADIUS = 1.125
LID_UNDERSIDE_Z = 11.5


def read_vertices(path: Path) -> list[tuple[float, float, float]]:
    vertices = []
    pattern = re.compile(r"vertex\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)")
    for line in path.read_text(encoding="ascii", errors="ignore").splitlines():
        match = pattern.search(line)
        if match:
            vertices.append(tuple(float(match.group(i)) for i in range(1, 4)))
    return vertices


def close(a: float, b: float, tolerance: float = 0.05) -> bool:
    return abs(a - b) <= tolerance


def main() -> None:
    for name, (expected_min, expected_max) in EXPECTED_BOUNDS.items():
        path = CASE_DIR / name
        vertices = read_vertices(path)
        if len(vertices) < 100:
            raise SystemExit(f"{name}: mesh is unexpectedly small ({len(vertices)} vertices)")
        mins = tuple(min(v[i] for v in vertices) for i in range(3))
        maxs = tuple(max(v[i] for v in vertices) for i in range(3))
        for actual, expected in zip(mins, expected_min):
            if not close(actual, expected):
                raise SystemExit(f"{name}: min bound {mins} != {expected_min}")
        for actual, expected in zip(maxs, expected_max):
            if not close(actual, expected):
                raise SystemExit(f"{name}: max bound {maxs} != {expected_max}")
        if name == "pcb.stl" and maxs[2] >= LID_UNDERSIDE_Z:
            raise SystemExit(f"{name}: component envelope reaches lid underside at {LID_UNDERSIDE_Z} mm")
        for cx, cy in HOLE_CENTERS:
            found = any(
                close(((x - cx) ** 2 + (y - cy) ** 2) ** 0.5, SCREW_CLEARANCE_RADIUS, 0.03)
                for x, y, _ in vertices
            )
            if not found:
                raise SystemExit(f"{name}: missing screw clearance ring at {(cx, cy)}")
        print(f"{name}: {len(vertices) // 3} facets, bounds {mins} to {maxs}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Parametric case generator for the Schneggi base sensor board.

The intended CAD backend is build123d.  The small STL backend below keeps the
checked-in parts reproducible on machines where build123d is not installed yet.
Install build123d from requirements.txt before extending the model with richer
boolean geometry.
"""

from __future__ import annotations

from dataclasses import dataclass
from math import cos, pi, sin, sqrt
from pathlib import Path


OUT_DIR = Path(__file__).resolve().parent


@dataclass(frozen=True)
class Board:
    width: float = 35.0
    depth: float = 30.0
    corner_radius: float = 1.0
    thickness: float = 1.6
    origin_x: float = 15.0
    origin_y: float = 15.0
    holes: tuple[tuple[float, float], ...] = (
        (17.2, 17.2),
        (47.8, 17.2),
        (47.8, 42.8),
        (17.2, 42.8),
    )
    shtc3: tuple[float, float] = (32.5, 18.25)
    e73_center: tuple[float, float] = (42.25, 29.25)
    e73_size: tuple[float, float] = (18.0, 13.0)

    def local(self, xy: tuple[float, float]) -> tuple[float, float]:
        return (xy[0] - self.origin_x, xy[1] - self.origin_y)

    def centered(self, xy: tuple[float, float]) -> tuple[float, float]:
        x, y = self.local(xy)
        return (x - self.width / 2, y - self.depth / 2)


@dataclass(frozen=True)
class Component:
    name: str
    center: tuple[float, float]
    size: tuple[float, float]
    height: float
    bottom: bool = False


@dataclass(frozen=True)
class Case:
    wall: float = 2.0
    fit_clearance: float = 0.3
    floor: float = 1.6
    bottom_wall_height: float = 11.5
    lid_top_thickness: float = 1.6
    lid_skirt_depth: float = 0.8
    lid_fit_clearance: float = 0.35
    top_component_clearance: float = 4.0
    boss_outer_diameter: float = 5.2
    screw_clearance_diameter: float = 2.25
    screw_head_counterbore_diameter: float = 4.2
    standoff_height: float = 2.2
    standoff_outer_diameter: float = 4.6
    vent_slot_width: float = 1.2
    vent_slot_length: float = 8.0
    vent_pitch: float = 2.2
    vent_count: int = 4
    segment_count: int = 12


BOARD = Board()
CASE = Case()
TOP_COMPONENTS: tuple[Component, ...] = (
    Component("E73 module", BOARD.e73_center, (13.0, 18.0), 3.0),
    Component("SHTC3", BOARD.shtc3, (2.0, 2.0), 0.8),
    Component("X1 AFC20", (20.0875, 30.0), (6.0, 2.9), 4.3),
    Component("SW1", (40.7, 18.4), (5.2, 5.2), 1.5),
    Component("RESET1", (25.1, 41.1), (5.2, 5.2), 1.5),
    Component("CN1 FTSH", (40.8, 40.7), (13.0, 6.0), 5.0),
    Component("CN2 SM04B", (24.8, 18.1), (6.0, 4.0), 4.3),
    Component("LED1", (34.5, 41.5), (2.0, 1.25), 0.8),
    Component("LDO1", (20.25, 25.25), (3.0, 1.7), 1.1),
    Component("Q1", (17.0, 23.0), (1.6, 1.2), 0.6),
    Component("X1 crystal", (29.5, 18.0), (3.2, 1.5), 0.9),
    Component("X2 crystal", (26.75, 36.5), (3.2, 1.5), 0.9),
)
BOTTOM_COMPONENTS: tuple[Component, ...] = ()


Point = tuple[float, float]
Vertex = tuple[float, float, float]
Face = tuple[Vertex, Vertex, Vertex]


def rounded_rect_points(width: float, depth: float, radius: float, segments: int) -> list[Point]:
    radius = min(radius, width / 2, depth / 2)
    centers = (
        (width / 2 - radius, depth / 2 - radius, 0.0),
        (-width / 2 + radius, depth / 2 - radius, pi / 2),
        (-width / 2 + radius, -depth / 2 + radius, pi),
        (width / 2 - radius, -depth / 2 + radius, 3 * pi / 2),
    )
    points: list[Point] = []
    for cx, cy, start in centers:
        for i in range(segments + 1):
            a = start + i * (pi / 2) / segments
            points.append((cx + radius * cos(a), cy + radius * sin(a)))
    return points


def tri(a: Vertex, b: Vertex, c: Vertex) -> Face:
    return (a, b, c)


def normal(face: Face) -> Vertex:
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = face
    ux, uy, uz = bx - ax, by - ay, bz - az
    vx, vy, vz = cx - ax, cy - ay, cz - az
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    length = sqrt(nx * nx + ny * ny + nz * nz) or 1.0
    return (nx / length, ny / length, nz / length)


def translate(faces: list[Face], dx: float, dy: float, dz: float) -> list[Face]:
    return [tuple((x + dx, y + dy, z + dz) for x, y, z in face) for face in faces]  # type: ignore[return-value]


def box(cx: float, cy: float, cz: float, sx: float, sy: float, sz: float) -> list[Face]:
    x0, x1 = cx - sx / 2, cx + sx / 2
    y0, y1 = cy - sy / 2, cy + sy / 2
    z0, z1 = cz - sz / 2, cz + sz / 2
    v = {
        "000": (x0, y0, z0),
        "100": (x1, y0, z0),
        "110": (x1, y1, z0),
        "010": (x0, y1, z0),
        "001": (x0, y0, z1),
        "101": (x1, y0, z1),
        "111": (x1, y1, z1),
        "011": (x0, y1, z1),
    }
    quads = (
        ("000", "100", "110", "010"),
        ("001", "011", "111", "101"),
        ("000", "001", "101", "100"),
        ("100", "101", "111", "110"),
        ("110", "111", "011", "010"),
        ("010", "011", "001", "000"),
    )
    faces: list[Face] = []
    for a, b, c, d in quads:
        faces.extend((tri(v[a], v[b], v[c]), tri(v[a], v[c], v[d])))
    return faces


def extruded_polygon(points: list[Point], z0: float, z1: float) -> list[Face]:
    faces: list[Face] = []
    bottom = [(x, y, z0) for x, y in points]
    top = [(x, y, z1) for x, y in points]
    center_bottom = (0.0, 0.0, z0)
    center_top = (0.0, 0.0, z1)
    count = len(points)
    for i in range(count):
        j = (i + 1) % count
        faces.extend((tri(bottom[i], bottom[j], top[j]), tri(bottom[i], top[j], top[i])))
        faces.append(tri(center_bottom, bottom[i], bottom[j]))
        faces.append(tri(center_top, top[j], top[i]))
    return faces


def extruded_ring(outer: list[Point], inner: list[Point], z0: float, z1: float) -> list[Face]:
    if len(outer) != len(inner):
        raise ValueError("outer and inner loops must have the same vertex count")
    faces: list[Face] = []
    count = len(outer)
    for i in range(count):
        j = (i + 1) % count
        ob0, ob1 = (*outer[i], z0), (*outer[j], z0)
        ot0, ot1 = (*outer[i], z1), (*outer[j], z1)
        ib0, ib1 = (*inner[i], z0), (*inner[j], z0)
        it0, it1 = (*inner[i], z1), (*inner[j], z1)
        faces.extend((tri(ob0, ob1, ot1), tri(ob0, ot1, ot0)))
        faces.extend((tri(ib0, it1, ib1), tri(ib0, it0, it1)))
        faces.extend((tri(ot0, ot1, it1), tri(ot0, it1, it0)))
        faces.extend((tri(ob0, ib1, ob1), tri(ob0, ib0, ib1)))
    return faces


def cylinder(cx: float, cy: float, z0: float, z1: float, radius: float, segments: int) -> list[Face]:
    pts = [(cx + radius * cos(2 * pi * i / segments), cy + radius * sin(2 * pi * i / segments)) for i in range(segments)]
    return extruded_polygon(pts, z0, z1)


def annular_cylinder(
    cx: float,
    cy: float,
    z0: float,
    z1: float,
    outer_radius: float,
    inner_radius: float,
    segments: int,
) -> list[Face]:
    outer = [(cx + outer_radius * cos(2 * pi * i / segments), cy + outer_radius * sin(2 * pi * i / segments)) for i in range(segments)]
    inner = [(cx + inner_radius * cos(2 * pi * i / segments), cy + inner_radius * sin(2 * pi * i / segments)) for i in range(segments)]
    return extruded_ring(outer, inner, z0, z1)


def write_ascii_stl(path: Path, faces: list[Face], name: str) -> None:
    with path.open("w", encoding="ascii") as stl:
        stl.write(f"solid {name}\n")
        for face in faces:
            nx, ny, nz = normal(face)
            stl.write(f"  facet normal {nx:.6f} {ny:.6f} {nz:.6f}\n")
            stl.write("    outer loop\n")
            for x, y, z in face:
                stl.write(f"      vertex {x:.6f} {y:.6f} {z:.6f}\n")
            stl.write("    endloop\n")
            stl.write("  endfacet\n")
        stl.write(f"endsolid {name}\n")


def bottom_mesh() -> list[Face]:
    board_inner_w = BOARD.width + 2 * CASE.fit_clearance
    board_inner_d = BOARD.depth + 2 * CASE.fit_clearance
    outer_w = board_inner_w + 2 * CASE.wall
    outer_d = board_inner_d + 2 * CASE.wall
    outer_r = BOARD.corner_radius + CASE.wall + CASE.fit_clearance
    inner_r = BOARD.corner_radius + CASE.fit_clearance
    outer = rounded_rect_points(outer_w, outer_d, outer_r, CASE.segment_count)
    inner = rounded_rect_points(board_inner_w, board_inner_d, inner_r, CASE.segment_count)

    faces = extruded_polygon(outer, 0.0, CASE.floor)
    faces += extruded_ring(outer, inner, CASE.floor, CASE.bottom_wall_height)

    for x, y in (BOARD.local(hole) for hole in BOARD.holes):
        cx = x - BOARD.width / 2
        cy = y - BOARD.depth / 2
        faces += annular_cylinder(
            cx,
            cy,
            CASE.floor,
            CASE.floor + CASE.standoff_height,
            CASE.standoff_outer_diameter / 2,
            CASE.screw_clearance_diameter / 2,
            CASE.segment_count * 2,
        )
        faces += annular_cylinder(
            cx,
            cy,
            CASE.floor + CASE.standoff_height,
            CASE.bottom_wall_height,
            CASE.boss_outer_diameter / 2,
            CASE.screw_clearance_diameter / 2,
            CASE.segment_count * 2,
        )

    support_y = -BOARD.depth / 2 + 15.0
    faces += box(0.0, support_y, CASE.floor + 0.6, 18.0, 1.0, 1.2)
    return faces


def top_mesh() -> list[Face]:
    board_inner_w = BOARD.width + 2 * CASE.fit_clearance
    board_inner_d = BOARD.depth + 2 * CASE.fit_clearance
    outer_w = board_inner_w + 2 * CASE.wall
    outer_d = board_inner_d + 2 * CASE.wall
    outer_r = BOARD.corner_radius + CASE.wall + CASE.fit_clearance
    skirt_w = board_inner_w - 2 * CASE.lid_fit_clearance
    skirt_d = board_inner_d - 2 * CASE.lid_fit_clearance
    skirt_outer = rounded_rect_points(skirt_w, skirt_d, BOARD.corner_radius, CASE.segment_count)
    skirt_inner = rounded_rect_points(skirt_w - 2 * CASE.wall, skirt_d - 2 * CASE.wall, max(0.5, BOARD.corner_radius - CASE.wall), CASE.segment_count)

    z0 = CASE.bottom_wall_height
    z1 = z0 + CASE.lid_top_thickness
    faces: list[Face] = []

    sht_x, sht_y = BOARD.local(BOARD.shtc3)
    vent_cx = sht_x - BOARD.width / 2
    vent_cy = sht_y - BOARD.depth / 2
    vent_area_w = CASE.vent_count * CASE.vent_slot_width + (CASE.vent_count - 1) * (CASE.vent_pitch - CASE.vent_slot_width)
    vent_area_h = CASE.vent_slot_length
    margin = 1.4

    left_w = (outer_w - (vent_area_w + 2 * margin)) / 2 + vent_cx
    right_w = outer_w - left_w - vent_area_w - 2 * margin
    faces += box(-outer_w / 2 + left_w / 2, 0.0, (z0 + z1) / 2, left_w, outer_d, CASE.lid_top_thickness)
    faces += box(outer_w / 2 - right_w / 2, 0.0, (z0 + z1) / 2, right_w, outer_d, CASE.lid_top_thickness)
    faces += box(vent_cx, outer_d / 2 - (outer_d / 2 - vent_cy - vent_area_h / 2 - margin) / 2, (z0 + z1) / 2, vent_area_w + 2 * margin, outer_d / 2 - vent_cy - vent_area_h / 2 - margin, CASE.lid_top_thickness)
    faces += box(vent_cx, -outer_d / 2 + (vent_cy - vent_area_h / 2 - margin + outer_d / 2) / 2, (z0 + z1) / 2, vent_area_w + 2 * margin, vent_cy - vent_area_h / 2 - margin + outer_d / 2, CASE.lid_top_thickness)

    x_start = vent_cx - vent_area_w / 2
    for i in range(CASE.vent_count + 1):
        if i == 0:
            cx = x_start - margin / 2
            sx = margin
        elif i == CASE.vent_count:
            cx = x_start + vent_area_w + margin / 2
            sx = margin
        else:
            gap = CASE.vent_pitch - CASE.vent_slot_width
            cx = x_start + i * CASE.vent_pitch - gap / 2
            sx = gap
        faces += box(cx, vent_cy, (z0 + z1) / 2, sx, vent_area_h + 2 * margin, CASE.lid_top_thickness)

    # Rounded corner caps restore the exterior silhouette around the box-composed vent plate.
    corner_r = outer_r
    for sx in (-1, 1):
        for sy in (-1, 1):
            faces += cylinder(
                sx * (outer_w / 2 - corner_r),
                sy * (outer_d / 2 - corner_r),
                z0,
                z1,
                corner_r,
                CASE.segment_count,
            )

    faces += extruded_ring(skirt_outer, skirt_inner, z0 - CASE.lid_skirt_depth, z0)

    for x, y in (BOARD.local(hole) for hole in BOARD.holes):
        cx = x - BOARD.width / 2
        cy = y - BOARD.depth / 2
        faces += annular_cylinder(
            cx,
            cy,
            z0 - CASE.lid_skirt_depth,
            z1,
            CASE.boss_outer_diameter / 2,
            CASE.screw_clearance_diameter / 2,
            CASE.segment_count * 2,
        )
        faces += annular_cylinder(
            cx,
            cy,
            z1 - 0.7,
            z1,
            CASE.screw_head_counterbore_diameter / 2,
            CASE.screw_clearance_diameter / 2,
            CASE.segment_count * 2,
        )

    return faces


def pcb_mesh() -> list[Face]:
    board_z0 = CASE.floor + CASE.standoff_height
    board_z1 = board_z0 + BOARD.thickness
    board = rounded_rect_points(BOARD.width, BOARD.depth, BOARD.corner_radius, CASE.segment_count)
    faces = extruded_polygon(board, board_z0, board_z1)

    for x, y in (BOARD.centered(hole) for hole in BOARD.holes):
        faces += annular_cylinder(
            x,
            y,
            board_z1,
            board_z1 + 0.15,
            1.7,
            CASE.screw_clearance_diameter / 2,
            CASE.segment_count * 2,
        )

    for component in TOP_COMPONENTS:
        x, y = BOARD.centered(component.center)
        sx, sy = component.size
        faces += box(x, y, board_z1 + component.height / 2, sx, sy, component.height)

    for component in BOTTOM_COMPONENTS:
        x, y = BOARD.centered(component.center)
        sx, sy = component.size
        faces += box(x, y, board_z0 - component.height / 2, sx, sy, component.height)

    return faces


def generate() -> None:
    bottom = bottom_mesh()
    top = top_mesh()
    pcb = pcb_mesh()
    write_ascii_stl(OUT_DIR / "bottom.stl", bottom, "schneggi_base_bottom")
    write_ascii_stl(OUT_DIR / "top.stl", top, "schneggi_base_top")
    write_ascii_stl(OUT_DIR / "pcb.stl", pcb, "schneggi_base_pcb_components")
    write_ascii_stl(OUT_DIR / "assembly.stl", bottom + pcb + top, "schneggi_base_fit_check_assembly")


if __name__ == "__main__":
    generate()

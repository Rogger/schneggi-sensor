#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pyrender", "trimesh", "scipy", "pillow", "numpy"]
# [tool.uv]
# override-dependencies = ["pyopengl==3.1.9"]
# ///
"""Render the README images in docs/img from the KiCad boards and case STLs.

Needs kicad-cli (KiCad 9+ for `pcb render`), ImageMagick and an EGL-capable OpenGL driver.
Run from anywhere:  uv run scripts/render_images.py
"""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

os.environ.setdefault('PYOPENGL_PLATFORM', 'egl')

import numpy as np
import pyrender
import trimesh
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'docs' / 'img'
BOARDS = {
    'base': ROOT / 'hardware/base/E73-2G4M08S1C-52840.kicad_pcb',
    'co2': ROOT / 'hardware/co2/schneggi_sensor_kicad7.kicad_pcb',
}
CASE_TOP = ROOT / 'hardware/case/top.stl'
CASE_BOTTOM = ROOT / 'hardware/case/bottom.stl'

# Case STLs are Y-up in mm; the boards rest on the standoffs at y = 5 mm,
# covering x 0..35 and z -30..0. KiCad GLB exports are Y-up in metres with
# the board spanning z 0..30 from the grid origin.
STANDOFF_Y = 5.0
BOARD_Z_SHIFT = -30.0
LID_COLOR = [0.16, 0.16, 0.17]
SHELL_COLOR = [0.50, 0.50, 0.52]
BOARD_THICKNESS = 1.6

# Shared board layout (board-local mm, origin at the KiCad grid origin).
MOUNTING_HOLES = [(2.2, 2.2), (32.8, 2.2), (2.2, 27.8), (32.8, 27.8)]
STANDOFF_LENGTH = 10.0
STANDOFF_ACROSS_FLATS = 3.2  # M2 male-female hex standoff
STANDOFF_STUD_LENGTH = 6.0  # threaded M2 stud, pointing down into the base board
STANDOFF_COLOR = [0.80, 0.64, 0.32]  # brass
# CN2: JST SH SM04B-SRSS-TB, side entry, socket facing the board edge (y = 0).
I2C_X = 9.8
I2C_FACE_Y = 0.5
I2C_PIN_HEIGHT = 3.0  # above the board surface
I2C_WIRES = [(-1.5, [0.08, 0.08, 0.08]), (-0.5, [0.80, 0.12, 0.10]),
             (0.5, [0.12, 0.30, 0.85]), (1.5, [0.95, 0.80, 0.10])]
PLUG_COLOR = [0.93, 0.91, 0.85]
LABEL_GREY = (136, 136, 136, 255)
FONT_CANDIDATES = [
    '/usr/share/fonts/noto/NotoSans-Medium.ttf',
    '/usr/share/fonts/truetype/noto/NotoSans-Medium.ttf',
    '/usr/share/fonts/TTF/DejaVuSans.ttf',
    '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
]


def run(*cmd):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def trim(src, dst, size, border=0):
    run('magick', str(src), '-trim', '+repage', '-resize', size,
        '-bordercolor', 'none', '-border', str(border), '-strip',
        '-define', 'png:compression-level=9', str(dst))


def render_board_images(tmp):
    """Raytraced isometric view of each board, straight from KiCad."""
    for name, pcb in BOARDS.items():
        raw = tmp / f'{name}_raw.png'
        run('kicad-cli', 'pcb', 'render', str(pcb), '-o', str(raw), '-w', '1200', '-h', '800',
            '--quality', 'high', '--rotate', '-40,0,30', '--perspective', '--zoom', '0.72',
            '--background', 'transparent')
        trim(raw, OUT / f'{name}.png', '600x400')


def export_glb(tmp, name):
    glb = tmp / f'{name}.glb'
    run('kicad-cli', 'pcb', 'export', 'glb', str(BOARDS[name]), '-o', str(glb), '--subst-models',
        '--include-tracks', '--include-pads', '--include-soldermask', '--include-silkscreen',
        '--grid-origin')
    return glb


def pose(dx=0.0, dy=0.0, dz=0.0, scale=1.0):
    m = np.eye(4)
    m[:3, :3] *= scale
    m[:3, 3] = [dx, dy, dz]
    return m


def look_at(eye, target, up=(0, 1, 0)):
    f = target - eye
    f /= np.linalg.norm(f)
    r = np.cross(f, up)
    r /= np.linalg.norm(r)
    u = np.cross(r, f)
    m = np.eye(4)
    m[:3, 0], m[:3, 1], m[:3, 2], m[:3, 3] = r, u, -f, eye
    return m


def mesh_color(geometry):
    visual = geometry.visual
    try:
        material = visual.material
        color = getattr(material, 'baseColorFactor', None)
        if color is None:
            color = material.main_color
    except AttributeError:
        color = visual.main_color
    color = np.asarray(color, float)
    if color.max() > 1:
        color = color / 255
    return tuple(np.round(color[:3], 3))


class Stage:
    def __init__(self):
        self.scene = pyrender.Scene(bg_color=[0, 0, 0, 0], ambient_light=[0.25] * 3)

    def add_stl(self, path, y, rgb):
        material = pyrender.MetallicRoughnessMaterial(
            baseColorFactor=rgb + [1], metallicFactor=0.0, roughnessFactor=0.7)
        mesh = pyrender.Mesh.from_trimesh(trimesh.load(path), material=material, smooth=False)
        self.scene.add(mesh, pose=pose(dy=y))

    def add_mesh(self, mesh, rgb, metallic=0.0, roughness=0.6):
        material = pyrender.MetallicRoughnessMaterial(
            baseColorFactor=list(rgb) + [1], metallicFactor=metallic, roughnessFactor=roughness)
        self.scene.add(pyrender.Mesh.from_trimesh(mesh, material=material, smooth=False))

    def add_board(self, glb, y):
        # Merge the thousands of KiCad part meshes by colour to keep rendering fast.
        groups = {}
        for g in trimesh.load(glb).dump(concatenate=False):
            groups.setdefault(mesh_color(g), []).append(
                trimesh.Trimesh(g.vertices, g.faces, process=False))
        for color, parts in groups.items():
            material = pyrender.MetallicRoughnessMaterial(
                baseColorFactor=list(color) + [1], metallicFactor=0.15, roughnessFactor=0.55)
            mesh = pyrender.Mesh.from_trimesh(
                trimesh.util.concatenate(parts), material=material, smooth=False)
            self.scene.add(mesh, pose=pose(dy=y, dz=BOARD_Z_SHIFT) @ pose(scale=1000))

    def render(self, center, size, distance=640, fov=13, azimuth=-35, elevation=28):
        az, el = np.radians(azimuth), np.radians(elevation)
        eye = center + distance * np.array(
            [np.sin(az) * np.cos(el), np.sin(el), np.cos(az) * np.cos(el)])
        camera_pose = look_at(eye, center)
        self.scene.add(pyrender.PerspectiveCamera(
            yfov=np.radians(fov), znear=distance - 190, zfar=distance + 210), pose=camera_pose)
        for direction, intensity in (((-0.5, -1, -0.3), 2.2), ((0.6, -0.4, -0.8), 0.9),
                                     ((0, 0.3, 1), 0.4)):
            d = np.array(direction, float)
            d /= np.linalg.norm(d)
            self.scene.add(pyrender.DirectionalLight(intensity=intensity),
                           pose=look_at(-d * 100, np.zeros(3)))
        width, height = size
        renderer = pyrender.OffscreenRenderer(width, height)
        color, _ = renderer.render(self.scene, flags=pyrender.RenderFlags.RGBA)
        renderer.delete()

        view = np.linalg.inv(camera_pose)
        focal = 1 / np.tan(np.radians(fov) / 2) * height / 2

        def project(point):
            q = view @ np.append(point, 1.0)
            return focal * q[0] / -q[2] + width / 2, -focal * q[1] / -q[2] + height / 2

        return Image.fromarray(color), project


def board_point(x, y_local, height):
    """Board-local (x, y) at a height in the case frame."""
    return np.array([x, height, y_local + BOARD_Z_SHIFT])


def tube(points, radius, sections=12):
    """Round wire along a polyline: cylinders between points, spheres at joints."""
    parts = []
    for a, b in zip(points[:-1], points[1:]):
        if np.linalg.norm(b - a) > 1e-6:
            parts.append(trimesh.creation.cylinder(radius=radius, segment=[a, b], sections=sections))
    for p in points:
        sphere = trimesh.creation.icosphere(subdivisions=1, radius=radius)
        sphere.apply_translation(p)
        parts.append(sphere)
    return trimesh.util.concatenate(parts)


def standoffs(bottom_y):
    """Male-female M2 hex standoffs: hex body from bottom_y up, threaded stud below."""
    radius = STANDOFF_ACROSS_FLATS / np.sqrt(3)
    pitch = 0.4  # M2 coarse thread
    meshes = []
    for x, y in MOUNTING_HOLES:
        a = board_point(x, y, bottom_y)
        meshes.append(trimesh.creation.cylinder(
            radius=radius, segment=[a, a + [0, STANDOFF_LENGTH, 0]], sections=6))
        # Thread suggested by alternating crest/root rings.
        for i in range(int(STANDOFF_STUD_LENGTH / pitch)):
            top = a - [0, i * pitch, 0]
            r = 1.0 if i % 2 == 0 else 0.82
            meshes.append(trimesh.creation.cylinder(
                radius=r, segment=[top - [0, pitch, 0], top], sections=20))
    return trimesh.util.concatenate(meshes)


def i2c_cable(base_y, co2_y, plug_gap=1.2, bend=2.5):
    """JST SH plugs in front of both I2C sockets, joined by four wires that loop
    around the board edge."""
    plug_depth, plug_width, plug_height = 4.0, 6.0, 2.6
    plugs, wires = [], {tuple(c): [] for _, c in I2C_WIRES}
    front_y = I2C_FACE_Y - plug_gap               # plug face, board-local y
    back_y = front_y - plug_depth                 # wire exit, board-local y
    for top in (base_y, co2_y):
        h = top + BOARD_THICKNESS + I2C_PIN_HEIGHT
        box = trimesh.creation.box([plug_width, plug_height, plug_depth])
        box.apply_translation(board_point(I2C_X, (front_y + back_y) / 2, h))
        plugs.append(box)
    lo = base_y + BOARD_THICKNESS + I2C_PIN_HEIGHT
    hi = co2_y + BOARD_THICKNESS + I2C_PIN_HEIGHT
    loop_z = back_y - 3.0                          # outermost wire run, board-local y
    for dx, color in I2C_WIRES:
        x = I2C_X + dx
        # Fan the wires a little so the inner ones nest inside the outer bends.
        inset = (dx + 1.5) * 0.35
        z_out = loop_z + inset
        # Out from the plug, quarter bend up, straight run, quarter bend back in.
        path = [board_point(x, back_y, lo)]
        c_low = board_point(x, z_out + bend, lo + bend)
        c_high = board_point(x, z_out + bend, hi - bend)
        path += [c_low + bend * np.array([0, -np.cos(a), -np.sin(a)])
                 for a in np.radians(np.linspace(0, 90, 10))]
        path += [c_high + bend * np.array([0, np.sin(a), -np.cos(a)])
                 for a in np.radians(np.linspace(0, 90, 10))]
        path.append(board_point(x, back_y, hi))
        wires[tuple(color)].append(tube(np.array(path), 0.4))
    return trimesh.util.concatenate(plugs), wires, board_point(I2C_X - 1.5, loop_z, (lo + hi) / 2)


def label(image, project, anchors, width, left_anchors=None):
    font_path = next((f for f in FONT_CANDIDATES if Path(f).exists()), None)
    font = ImageFont.truetype(font_path, 46) if font_path else ImageFont.load_default()
    left_anchors = left_anchors or {}
    def text_width(texts):
        return max((font.getlength(s) for s in texts), default=0)

    left_pad = int(text_width(left_anchors) + 140) if left_anchors else 0
    right_pad = int(text_width(anchors) + 80)
    canvas = Image.new('RGBA', (left_pad + width + right_pad, image.height), (0, 0, 0, 0))
    canvas.paste(image, (left_pad, 0))
    draw = ImageDraw.Draw(canvas)

    def dot(x, y):
        draw.ellipse([x - 10, y - 10, x + 10, y + 10], fill=LABEL_GREY)

    text_x = left_pad + width + 40
    for text, point in anchors.items():
        x, y = project(np.array(point, float))
        x += left_pad
        draw.line([(x + 15, y), (text_x - 15, y)], fill=LABEL_GREY, width=4)
        dot(x + 15, y)
        draw.text((text_x, y), text, font=font, fill=LABEL_GREY, anchor='lm')
    for text, point in left_anchors.items():
        x, y = project(np.array(point, float))
        x += left_pad
        draw.line([(x - 15, y), (left_pad - 25, y)], fill=LABEL_GREY, width=4)
        dot(x - 15, y)
        draw.text((left_pad - 40, y), text, font=font, fill=LABEL_GREY, anchor='rm')
    return canvas


def render_exploded(tmp):
    """Exploded stack: bottom shell, base board, standoffs and I2C cable,
    CO2 board, lid."""
    lift = {'base': 42, 'co2': 76, 'lid': 92}
    base_y = STANDOFF_Y + lift['base']
    co2_y = STANDOFF_Y + lift['co2']
    standoff_y = (base_y + BOARD_THICKNESS + co2_y - STANDOFF_LENGTH + STANDOFF_STUD_LENGTH) / 2
    stage = Stage()
    stage.add_stl(CASE_BOTTOM, 0, SHELL_COLOR)
    stage.add_board(export_glb(tmp, 'base'), base_y)
    stage.add_mesh(standoffs(standoff_y), STANDOFF_COLOR, metallic=0.6, roughness=0.35)
    plugs, wires, cable_anchor = i2c_cable(base_y, co2_y)
    stage.add_mesh(plugs, PLUG_COLOR)
    for color, meshes in wires.items():
        stage.add_mesh(trimesh.util.concatenate(meshes), color, roughness=0.45)
    stage.add_board(export_glb(tmp, 'co2'), co2_y)
    stage.add_stl(CASE_TOP, lift['lid'], LID_COLOR)
    size = (1300, 2200)
    image, project = stage.render(np.array([20, (35 + lift['lid']) / 2, -15]), size)
    right_hole = board_point(*MOUNTING_HOLES[3], standoff_y + STANDOFF_LENGTH / 2)
    labelled = label(image, project, {
        'Lid': (45, lift['lid'] + 28, -20),
        'CO2 board (optional)': (35, co2_y + 1, -15),
        'M2 male-female standoffs': right_hole + [STANDOFF_ACROSS_FLATS / 2, 0, 0],
        'Base board': (38, base_y + 1, -15),
        'Bottom shell': (45, 12, -20),
    }, size[0], left_anchors={'I2C cable': cable_anchor})
    labelled.save(tmp / 'exploded_raw.png')
    trim(tmp / 'exploded_raw.png', OUT / 'exploded.png', '1000x', border=12)


def main():
    for tool in ('kicad-cli', 'magick'):
        if not shutil.which(tool):
            sys.exit(f'{tool} not found on PATH')
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        render_board_images(tmp)
        render_exploded(tmp)
    print(f'Wrote {", ".join(sorted(p.name for p in OUT.glob("*.png")))} to {OUT}')


if __name__ == '__main__':
    main()

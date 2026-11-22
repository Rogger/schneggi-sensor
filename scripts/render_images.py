#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pyrender", "trimesh", "scipy", "pillow", "numpy"]
# [tool.uv]
# override-dependencies = ["pyopengl==3.1.9"]
# ///
"""Render the README images in docs/img from the KiCad boards and case STLs.

Needs kicad-cli (KiCad 8+), ImageMagick and an EGL-capable OpenGL driver.
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
LABEL_GREY = (136, 136, 136, 255)
FONT_CANDIDATES = [
    '/usr/share/fonts/noto/NotoSans-Medium.ttf',
    '/usr/share/fonts/truetype/noto/NotoSans-Medium.ttf',
    '/usr/share/fonts/TTF/DejaVuSans.ttf',
    '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
]


def run(*cmd):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def trim(src, dst, size):
    run('magick', str(src), '-trim', '+repage', '-resize', size, '-strip',
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


def label(image, project, anchors, width):
    font_path = next((f for f in FONT_CANDIDATES if Path(f).exists()), None)
    font = ImageFont.truetype(font_path, 46) if font_path else ImageFont.load_default()
    canvas = Image.new('RGBA', (width + 520, image.height), (0, 0, 0, 0))
    canvas.paste(image, (0, 0))
    draw = ImageDraw.Draw(canvas)
    text_x = width + 40
    for text, point in anchors.items():
        x, y = project(np.array(point, float))
        draw.line([(x + 15, y), (text_x - 15, y)], fill=LABEL_GREY, width=4)
        draw.ellipse([x + 5, y - 10, x + 25, y + 10], fill=LABEL_GREY)
        draw.text((text_x, y), text, font=font, fill=LABEL_GREY, anchor='lm')
    return canvas


def render_case(tmp):
    """Case with the lid lifted off the bottom shell."""
    stage = Stage()
    lift = 14
    stage.add_stl(CASE_BOTTOM, 0, SHELL_COLOR)
    stage.add_stl(CASE_TOP, lift, LID_COLOR)
    image, _ = stage.render(np.array([20, (35 + lift) / 2, -15]), (1000, 1100), distance=480)
    image.save(tmp / 'case_raw.png')
    trim(tmp / 'case_raw.png', OUT / 'case.png', '600x400')


def render_exploded(tmp):
    """Exploded stack: bottom shell, base board, CO2 board, lid."""
    lift = {'base': 42, 'co2': 72, 'lid': 88}
    stage = Stage()
    stage.add_stl(CASE_BOTTOM, 0, SHELL_COLOR)
    stage.add_board(export_glb(tmp, 'base'), STANDOFF_Y + lift['base'])
    stage.add_board(export_glb(tmp, 'co2'), STANDOFF_Y + lift['co2'])
    stage.add_stl(CASE_TOP, lift['lid'], LID_COLOR)
    size = (1300, 2100)
    image, project = stage.render(np.array([20, (35 + lift['lid']) / 2, -15]), size)
    labelled = label(image, project, {
        'Lid': (45, lift['lid'] + 28, -20),
        'CO2 board (optional)': (35, STANDOFF_Y + lift['co2'] + 1, -15),
        'Base board': (38, STANDOFF_Y + lift['base'] + 1, -15),
        'Bottom shell': (45, 12, -20),
    }, size[0])
    labelled.save(tmp / 'exploded_raw.png')
    trim(tmp / 'exploded_raw.png', OUT / 'exploded.png', '900x')


def main():
    for tool in ('kicad-cli', 'magick'):
        if not shutil.which(tool):
            sys.exit(f'{tool} not found on PATH')
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        render_board_images(tmp)
        render_case(tmp)
        render_exploded(tmp)
    print(f'Wrote {", ".join(sorted(p.name for p in OUT.glob("*.png")))} to {OUT}')


if __name__ == '__main__':
    main()

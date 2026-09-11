# Schneggi base sensor case

Parametric FDM-printable enclosure for the base board variant in
`hardware/base/E73-2G4M08S1C-52840.kicad_pcb`.

The generated parts are:

- `bottom.stl`: tray with PCB standoffs, M2 screw bosses, board clearance, and support rib.
- `top.stl`: screw-down lid with vent slots over the SHTC3 sensor and no LED, button, USB, or programming openings.
- `pcb.stl`: non-printable fit-check model with the PCB slab and simplified top-side component bodies.
- `assembly.stl`: non-printable combined model for uploaders that require one parent file.

## Board references

- PCB outline: 35 mm x 30 mm rounded rectangle, KiCad coordinates `(15,15)` to `(50,45)`.
- Mounting holes: `(17.2,17.2)`, `(47.8,17.2)`, `(47.8,42.8)`, `(17.2,42.8)`.
- SHTC3: `(32.5,18.25)`, used for lid vent placement.
- E73 module: centered at `(42.25,29.25)`, tracked as a keep-out in `case.py`.

## Regenerating

Create a local Python environment and install the CAD dependency:

```sh
cd hardware/case
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
```

Then regenerate and sanity-check the STL files:

```sh
python3 case.py
python3 verify_stl.py
```

`case.py` keeps dimensions centralized near the top of the file. The current
script includes a deterministic STL fallback so the checked-in meshes can be
rebuilt even before `build123d` is installed locally.

For clearance review, load `assembly.stl` directly, or load `bottom.stl`,
`top.stl`, and `pcb.stl` together in a CAD viewer or slicer. Keep `pcb.stl` and
`assembly.stl` out of print jobs; they are only envelope models for fit
checking.

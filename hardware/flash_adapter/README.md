# Flash Adapter PCB

This KiCad project is intended to be exported for JLCPCB with KiCad 9.

Use the KiCad 9 AppImage entrypoint:

```sh
/tmp/kicad_appimage_run/AppRun kicad-cli version
```

The expected version for the checked-in production files is KiCad CLI `9.0.9`.

## Generate JLCPCB Files

These commands follow JLCPCB's KiCad 9 Gerber/drill preparation guidance:

- Run DRC before generating manufacturing files.
- Export the standard fabrication layers: copper, paste, silkscreen, solder mask, and `Edge.Cuts`.
- Use Protel filename extensions for Gerbers.
- Use extended X2 format and include netlist attributes.
- Generate Excellon drill output with absolute origin, millimeters, decimal zeros format, and alternate oval-hole mode.
- Use the drill/place-file origin so Gerber, drill, and CPL coordinates are positive and board-relative.
- Format the BOM as `Comment,Designator,Footprint,JLCPCB Part #（optional）`.
- Format the CPL as `Designator,Mid X,Mid Y,Layer,Rotation`, with `mm` coordinate suffixes.
- Zip only the Gerber layers and merged Excellon drill file before upload.

Reference: <https://jlcpcb.com/help/article/how-to-generate-gerber-and-drill-files-in-kicad-9>
BOM sample: <https://static.jlcpcb.com//video/Sample-BOM_JLCSMT.xlsx>
CPL sample: <https://static.jlcpcb.com//video/JLCSMT_Sample_CPL1.xlsx>

From `hardware/flash_adapter`:

```sh
make jlcpcb
```

The `Makefile` runs the full sequence below.

```sh
/tmp/kicad_appimage_run/AppRun kicad-cli pcb drc --severity-error --schematic-parity --output flash_adapter-drc.rpt flash_adapter.kicad_pcb
/tmp/kicad_appimage_run/AppRun kicad-cli sch erc --severity-all --output flash_adapter-erc.rpt flash_adapter.kicad_sch

rm -f jlcpcb/gerber/*
/tmp/kicad_appimage_run/AppRun kicad-cli pcb export gerbers --output jlcpcb/gerber --layers F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts --use-drill-file-origin flash_adapter.kicad_pcb
/tmp/kicad_appimage_run/AppRun kicad-cli pcb export drill --output jlcpcb/gerber --format excellon --drill-origin plot --excellon-units mm --excellon-zeros-format decimal --excellon-oval-format alternate flash_adapter.kicad_pcb
/tmp/kicad_appimage_run/AppRun kicad-cli sch export bom --fields Value,Reference,Footprint,'LCSC Part' --labels Comment,Designator,Footprint,'JLCPCB Part #（optional）' --output jlcpcb/production_files/BOM-flash_adapter.csv flash_adapter.kicad_sch
sed -i 's/"//g' jlcpcb/production_files/BOM-flash_adapter.csv

/tmp/kicad_appimage_run/AppRun kicad-cli pcb export pos --side front --format csv --units mm --smd-only --use-drill-file-origin --output /tmp/CPL-flash_adapter.raw.csv flash_adapter.kicad_pcb
awk -F, 'BEGIN { print "Designator,Mid X,Mid Y,Layer,Rotation" } NR > 1 { gsub(/"/, ""); layer = ($7 == "top" ? "Top" : ($7 == "bottom" ? "Bottom" : $7)); printf "%s,%.4fmm,%.4fmm,%s,%.0f\n", $1, $4, $5, layer, $6 }' /tmp/CPL-flash_adapter.raw.csv > jlcpcb/production_files/CPL-flash_adapter.csv

rm -f jlcpcb/production_files/GERBER-flash_adapter.zip
zip -j -q jlcpcb/production_files/GERBER-flash_adapter.zip jlcpcb/gerber/*.gtl jlcpcb/gerber/*.gbl jlcpcb/gerber/*.gtp jlcpcb/gerber/*.gbp jlcpcb/gerber/*.gto jlcpcb/gerber/*.gbo jlcpcb/gerber/*.gts jlcpcb/gerber/*.gbs jlcpcb/gerber/*.gm1 jlcpcb/gerber/*.drl
```

The Gerber package uses JLCPCB-style Protel extensions and a single merged Excellon drill file.
The BOM and CPL match the JLCPCB sample column names and column order.

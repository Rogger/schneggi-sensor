KICAD_CLI ?= /tmp/kicad_appimage_run/AppRun kicad-cli

PROJECT := flash_adapter
PCB := $(PROJECT).kicad_pcb
SCH := $(PROJECT).kicad_sch

GERBER_DIR := jlcpcb/gerber
PRODUCTION_DIR := jlcpcb/production_files
BOM := $(PRODUCTION_DIR)/BOM-$(PROJECT).csv
CPL := $(PRODUCTION_DIR)/CPL-$(PROJECT).csv
GERBER_ZIP := $(PRODUCTION_DIR)/GERBER-$(PROJECT).zip
CPL_RAW := /tmp/CPL-$(PROJECT).raw.csv

GERBER_LAYERS := F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts
GERBER_FILES := \
	$(GERBER_DIR)/$(PROJECT)-F_Cu.gtl \
	$(GERBER_DIR)/$(PROJECT)-B_Cu.gbl \
	$(GERBER_DIR)/$(PROJECT)-F_Paste.gtp \
	$(GERBER_DIR)/$(PROJECT)-B_Paste.gbp \
	$(GERBER_DIR)/$(PROJECT)-F_Silkscreen.gto \
	$(GERBER_DIR)/$(PROJECT)-B_Silkscreen.gbo \
	$(GERBER_DIR)/$(PROJECT)-F_Mask.gts \
	$(GERBER_DIR)/$(PROJECT)-B_Mask.gbs \
	$(GERBER_DIR)/$(PROJECT)-Edge_Cuts.gm1 \
	$(GERBER_DIR)/$(PROJECT).drl

.PHONY: all jlcpcb check gerbers bom cpl zip clean

all: jlcpcb

jlcpcb: check gerbers bom cpl zip

check:
	$(KICAD_CLI) pcb drc --severity-error --schematic-parity --output $(PROJECT)-drc.rpt $(PCB)
	$(KICAD_CLI) sch erc --severity-all --output $(PROJECT)-erc.rpt $(SCH)

gerbers:
	mkdir -p $(GERBER_DIR)
	rm -f $(GERBER_DIR)/*
	$(KICAD_CLI) pcb export gerbers --output $(GERBER_DIR) --layers $(GERBER_LAYERS) --use-drill-file-origin $(PCB)
	$(KICAD_CLI) pcb export drill --output $(GERBER_DIR) --format excellon --drill-origin plot --excellon-units mm --excellon-zeros-format decimal --excellon-oval-format alternate $(PCB)
	rm -f $(GERBER_DIR)/*-drl_map.* $(GERBER_DIR)/*-job.gbrjob

bom:
	mkdir -p $(PRODUCTION_DIR)
	$(KICAD_CLI) sch export bom --fields Value,Reference,Footprint,'LCSC Part' --labels Comment,Designator,Footprint,'JLCPCB Part #（optional）' --output $(BOM) $(SCH)
	sed -i 's/"//g' $(BOM)

cpl:
	mkdir -p $(PRODUCTION_DIR)
	$(KICAD_CLI) pcb export pos --side front --format csv --units mm --smd-only --use-drill-file-origin --output $(CPL_RAW) $(PCB)
	awk -F, 'BEGIN { print "Designator,Mid X,Mid Y,Layer,Rotation" } NR > 1 { gsub(/"/, ""); layer = ($$7 == "top" ? "Top" : ($$7 == "bottom" ? "Bottom" : $$7)); printf "%s,%.4fmm,%.4fmm,%s,%.0f\n", $$1, $$4, $$5, layer, $$6 }' $(CPL_RAW) > $(CPL)

zip:
	rm -f $(GERBER_ZIP)
	zip -j -q $(GERBER_ZIP) $(GERBER_FILES)

clean:
	rm -f $(PROJECT)-drc.rpt $(PROJECT)-erc.rpt $(CPL_RAW)
	rm -f $(GERBER_DIR)/*
	rm -f $(BOM) $(CPL) $(GERBER_ZIP)

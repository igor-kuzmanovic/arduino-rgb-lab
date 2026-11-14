.DEFAULT_GOAL := help
.PHONY: help run

# Discover sketches stored as <name>/<name>.ino in the project root.
SKETCHES := $(foreach name,$(patsubst %/,%,$(wildcard */)),$(if $(wildcard $(name)/$(name).ino),$(name)))

# Bare `make` lists sketches without building or accessing the board.
help:
	@echo 'Usage: make run SKETCH=<name>'
	@echo 'Available sketches: $(strip $(SKETCHES))'

# Build and upload the selected sketch, then open its serial monitor.
# Close an existing monitor with Ctrl+C before running this again.
run:
	$(if $(strip $(SKETCH)),,$(error Specify a sketch: make run SKETCH=<name>))
	$(HOME)/.local/bin/arduino-cli compile --upload --fqbn arduino:avr:uno --port /dev/ttyUSB0 "$(SKETCH)"
	$(HOME)/.local/bin/arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200

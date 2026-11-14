# Arduino Sketches

Arduino Uno sketches with a shared Makefile for uploading and serial monitoring.

| Sketch | Description |
|---|---|
| [IRReceiver](IRReceiver/IRReceiver.ino) | Control an RGB LED with a 24-button IR remote: 16 colors, brightness, power, and four effects |

## IRReceiver hardware

- Arduino Uno R3 or compatible ATmega328P board
- 38 kHz IR receiver
- Common-cathode RGB LED module with current-limiting resistors
- 24-button NEC RGB remote, USB data cable, and jumper wires

### Wiring

Disconnect USB power before changing connections. Identify the receiver's pins
from its datasheet; their physical order depends on the part.

| Component | Pin | Uno |
|---|---|---|
| IR receiver | OUT | D2 |
| IR receiver | GND | GND |
| IR receiver | VCC | 5V, for a 5V-compatible receiver |
| RGB module | R | D9 |
| RGB module | G | D10 |
| RGB module | B | D6 |
| RGB module | − | GND |

## Setup

Requires Arduino CLI at `~/.local/bin/arduino-cli` and GNU Make.
Install the board package and library:

```bash
~/.local/bin/arduino-cli core update-index
~/.local/bin/arduino-cli core install arduino:avr
~/.local/bin/arduino-cli lib install IRremote
```

From the project directory, list available sketches:

```bash
make
```

Select a sketch explicitly to compile, upload, and open its serial monitor:

```bash
make run SKETCH=IRReceiver
```

This uploads through `/dev/ttyUSB0` and opens the serial
monitor at 115200 baud. Press **Ctrl+C** to close the monitor before uploading
again. Change the port in [Makefile](Makefile) if needed.

### Adding a sketch

Create `<Name>/<Name>.ino` in the project root, then run
`make run SKETCH=<Name>`. Sketches appear automatically in the `make` listing.
The shared Makefile targets the Uno and uses 115200 baud for serial monitoring.

### WSL 2

The Uno must be attached to WSL using `usbipd-win`. After reconnecting USB, run
this in Windows PowerShell, replacing `<BUSID>` with the Uno's ID from
`usbipd list`:

```powershell
usbipd attach --wsl --busid <BUSID>
```

The device must first be shared once with
`usbipd bind --busid <BUSID>` in Administrator PowerShell.

## IRReceiver remote

<img src="remote.svg" alt="24-button remote layout with hexadecimal command codes" width="400">

Protocol: **NEC**. Address: **`0xEF00`**. Commands below are hexadecimal,
arranged in the same rows and columns as the remote.

| Left | Center-left | Center-right | Right |
|---|---|---|---|
| `00` Brighter | `01` Dimmer | `02` OFF | `03` ON |
| `04` Red | `05` Green | `06` Blue | `07` White |
| `08` Red-orange | `09` Lime | `0A` Light blue | `0B` FLASH |
| `0C` Orange-red | `0D` Turquoise | `0E` Violet | `0F` STROBE |
| `10` Orange | `11` Cyan | `12` Magenta | `13` FADE |
| `14` Yellow | `15` Sky blue | `16` Pink | `17` SMOOTH |

RGB presets approximate the printed button colors.

### Controls

- Brightness has 15 nonzero levels. Hold a brightness button to repeat.
- OFF disables output and ignores all buttons except ON.
- ON restores the selected color, brightness, and mode.
- Selecting a color stops the effect.
- Startup defaults to red at full brightness. Settings reset on power loss.

### Effects

| Button | Behavior |
|---|---|
| FLASH | Cycle through seven colors, changing every 500 ms |
| STROBE | Alternate white and off, 100 ms each |
| FADE | Blend through seven colors, 2 seconds per transition |
| SMOOTH | Blend through red, green, and blue, 2 seconds per transition |

The seven-color sequence is red, yellow, green, cyan, blue, magenta, and white.
Effects run without blocking remote input.

## Code

[IRReceiver/IRReceiver.ino](IRReceiver/IRReceiver.ino) contains the pin assignments,
command map, RGB presets, and effect timings as named constants and tables.

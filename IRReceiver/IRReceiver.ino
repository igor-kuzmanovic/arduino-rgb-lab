#include <IRremote.hpp>

// Wiring: the RGB module uses a shared ground and three PWM outputs.
constexpr byte IR_RECEIVER_PIN = 2;
constexpr byte RED_PIN = 9;
constexpr byte GREEN_PIN = 10;
constexpr byte BLUE_PIN = 6;
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr uint16_t REMOTE_ADDRESS = 0xEF00;

// Brightness has 15 nonzero levels. Use OFF to turn the LED off completely.
constexpr byte MAX_BRIGHTNESS = 255;
constexpr byte BRIGHTNESS_STEP = 17;
constexpr byte MIN_BRIGHTNESS = BRIGHTNESS_STEP;

// All effects run without delay(), so the remote remains responsive.
constexpr unsigned long FRAME_INTERVAL_MS = 20;
constexpr unsigned long FLASH_INTERVAL_MS = 500;
constexpr unsigned long STROBE_INTERVAL_MS = 100;
constexpr unsigned long FADE_INTERVAL_MS = 2000;

struct Color {
  byte red;
  byte green;
  byte blue;
};

constexpr Color BLACK = {0, 0, 0};
constexpr Color RED = {255, 0, 0};
constexpr Color GREEN = {0, 255, 0};
constexpr Color BLUE = {0, 0, 255};
constexpr Color WHITE = {255, 255, 255};

// NEC command codes, in the same row order as remote.svg.
enum Command : byte {
  BRIGHTER = 0x00, DIMMER = 0x01, POWER_OFF = 0x02, POWER_ON = 0x03,
  COLOR_RED = 0x04, COLOR_GREEN = 0x05, COLOR_BLUE = 0x06, COLOR_WHITE = 0x07,
  COLOR_RED_ORANGE = 0x08, COLOR_LIME = 0x09, COLOR_LIGHT_BLUE = 0x0A, EFFECT_FLASH = 0x0B,
  COLOR_ORANGE_RED = 0x0C, COLOR_TURQUOISE = 0x0D, COLOR_VIOLET = 0x0E, EFFECT_STROBE = 0x0F,
  COLOR_ORANGE = 0x10, COLOR_CYAN = 0x11, COLOR_MAGENTA = 0x12, EFFECT_FADE = 0x13,
  COLOR_YELLOW = 0x14, COLOR_SKY_BLUE = 0x15, COLOR_PINK = 0x16, EFFECT_SMOOTH = 0x17
};

struct ColorButton {
  Command command;
  Color color;
};

// RGB values approximate the colors printed on the remote.
const ColorButton COLOR_BUTTONS[] = {
  {COLOR_RED, RED},
  {COLOR_GREEN, GREEN},
  {COLOR_BLUE, BLUE},
  {COLOR_WHITE, WHITE},
  {COLOR_RED_ORANGE, {255, 32, 0}},
  {COLOR_LIME, {64, 255, 0}},
  {COLOR_LIGHT_BLUE, {0, 96, 255}},
  {COLOR_ORANGE_RED, {255, 64, 0}},
  {COLOR_TURQUOISE, {0, 255, 128}},
  {COLOR_VIOLET, {128, 0, 255}},
  {COLOR_ORANGE, {255, 128, 0}},
  {COLOR_CYAN, {0, 255, 255}},
  {COLOR_MAGENTA, {255, 0, 255}},
  {COLOR_YELLOW, {255, 255, 0}},
  {COLOR_SKY_BLUE, {0, 128, 255}},
  {COLOR_PINK, {255, 0, 128}}
};

// FLASH and FADE use seven colors; SMOOTH transitions between R, G, and B.
const Color RAINBOW_COLORS[] = {
  RED, {255, 255, 0}, GREEN, {0, 255, 255}, BLUE, {255, 0, 255}, WHITE
};
const Color PRIMARY_COLORS[] = {RED, GREEN, BLUE};
constexpr byte RAINBOW_COLOR_COUNT = sizeof(RAINBOW_COLORS) / sizeof(Color);
constexpr byte PRIMARY_COLOR_COUNT = sizeof(PRIMARY_COLORS) / sizeof(Color);
constexpr byte STROBE_PHASE_COUNT = 2;  // Alternating white and black.

enum Mode { SOLID, FLASH, STROBE, FADE, SMOOTH };

// Start red at full brightness. OFF preserves these settings for ON.
Color selectedColor = RED;
Mode mode = SOLID;
bool powerOn = true;
byte brightness = MAX_BRIGHTNESS;
unsigned long effectStartedAt = 0;
unsigned long lastFrameAt = 0;

// Scale each channel by the overall brightness before writing PWM values.
void render(Color color) {
  if (!powerOn) color = BLACK;
  // Widen before multiplication: 255 * 255 exceeds an AVR signed int.
  analogWrite(RED_PIN, (uint16_t)color.red * brightness / MAX_BRIGHTNESS);
  analogWrite(GREEN_PIN, (uint16_t)color.green * brightness / MAX_BRIGHTNESS);
  analogWrite(BLUE_PIN, (uint16_t)color.blue * brightness / MAX_BRIGHTNESS);
}

// Interpolate one channel over a single fade interval using integer math.
byte blendChannel(byte from, byte to, unsigned long elapsedInFade) {
  return ((uint32_t)from * (FADE_INTERVAL_MS - elapsedInFade) +
          (uint32_t)to * elapsedInFade) / FADE_INTERVAL_MS;
}

// Calculate the current effect frame from elapsed time, without blocking IR.
void updateLed(unsigned long now) {
  // Unsigned subtraction also handles millis() wrapping around.
  unsigned long elapsed = now - effectStartedAt;
  Color color = selectedColor;

  switch (mode) {
    case SOLID:
      break;
    case FLASH:
      color = RAINBOW_COLORS[(elapsed / FLASH_INTERVAL_MS) % RAINBOW_COLOR_COUNT];
      break;
    case STROBE: {
      bool darkPhase = (elapsed / STROBE_INTERVAL_MS) % STROBE_PHASE_COUNT;
      color = darkPhase ? BLACK : WHITE;
      break;
    }
    case FADE:
    case SMOOTH: {
      const Color* palette = mode == FADE ? RAINBOW_COLORS : PRIMARY_COLORS;
      byte count = mode == FADE ? RAINBOW_COLOR_COUNT : PRIMARY_COLOR_COUNT;
      byte index = (elapsed / FADE_INTERVAL_MS) % count;
      Color from = palette[index];
      Color to = palette[(index + 1) % count];
      unsigned long elapsedInFade = elapsed % FADE_INTERVAL_MS;
      color = {
        blendChannel(from.red, to.red, elapsedInFade),
        blendChannel(from.green, to.green, elapsedInFade),
        blendChannel(from.blue, to.blue, elapsedInFade)
      };
      break;
    }
  }
  render(color);
}

// Select a solid color if the command matches one of the color buttons.
bool selectColor(uint16_t command) {
  for (const ColorButton& button : COLOR_BUTTONS) {
    if (button.command == command) {
      selectedColor = button.color;
      mode = SOLID;
      return true;
    }
  }
  return false;
}

// Apply one press. Only brightness buttons repeat when held down.
void handleButton(uint16_t command, bool repeat) {
  // OFF locks the controls until ON is pressed.
  if (!powerOn && command != POWER_ON) return;
  if (repeat && command != BRIGHTER && command != DIMMER) return;

  Serial.print(F("Button 0x"));
  Serial.println(command, HEX);
  switch (command) {
    // Return early for controls that preserve the current color and effect.
    case BRIGHTER:
      brightness = min((int)MAX_BRIGHTNESS, (int)brightness + BRIGHTNESS_STEP);
      return;
    case DIMMER:
      brightness = max((int)MIN_BRIGHTNESS, (int)brightness - BRIGHTNESS_STEP);
      return;
    case POWER_OFF:
      powerOn = false;
      return;
    case POWER_ON:
      powerOn = true;
      return;
    case EFFECT_FLASH:
      mode = FLASH;
      break;
    case EFFECT_STROBE:
      mode = STROBE;
      break;
    case EFFECT_FADE:
      mode = FADE;
      break;
    case EFFECT_SMOOTH:
      mode = SMOOTH;
      break;
    default:
      if (!selectColor(command)) {
        Serial.println(F("Unmapped command."));
        return;
      }
      break;
  }

  // Selecting a color or effect restarts the effect clock.
  effectStartedAt = millis();
}

// Initialize the outputs, serial monitor, and IR receiver once at startup.
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  render(selectedColor);
  IrReceiver.begin(IR_RECEIVER_PIN, DISABLE_LED_FEEDBACK);
  Serial.println(F("Ready."));
}

// Process this remote's NEC messages and refresh the LED at a fixed interval.
void loop() {
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol == NEC &&
        IrReceiver.decodedIRData.address == REMOTE_ADDRESS) {
      bool repeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
      handleButton(IrReceiver.decodedIRData.command, repeat);
      updateLed(millis());
    }
    // Resume reception even when a message belongs to another remote.
    IrReceiver.resume();
  }

  unsigned long now = millis();
  if (now - lastFrameAt >= FRAME_INTERVAL_MS) {
    lastFrameAt = now;
    updateLed(now);
  }
}

#ifndef CONFIG_H
#define CONFIG_H

// ===== I2C CONFIGURATION =====
#define I2C_ADDRESS 0x42

// ===== BUTTON CONFIGURATION =====
#define NUM_BUTTONS 2

// Button input pins (active LOW with internal pullup)
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
  13, 6
};

// Button LED output pins
const uint8_t BUTTON_LED_PINS[NUM_BUTTONS] = {
  2, 3
};

// ===== TOWER CONFIGURATION =====
#define NUM_TOWERS 3
#define NUM_TOWER_ROWS 5

// Tower LED pins [tower][row]
const uint8_t TOWER_LED_PINS[NUM_TOWERS][NUM_TOWER_ROWS] = {
  // Tower 0
  {23, 24, 25, 26, 27},
  // Tower 1
  {28, 29, 30, 31, 32},
  // Tower 2
  {33, 34, 35, 36, 37}
};

// ===== MAX7219 DISPLAY CONFIGURATION =====
#define MAX_CLK 11
#define MAX_CS 10
#define MAX_DIN 9

// ===== DISPLAY JUSTIFICATION =====
#define LEFT  0
#define RIGHT 1

// Display settings
#define DISPLAY_INTENSITY 0x08  // Brightness (0x00-0x0F)

#endif
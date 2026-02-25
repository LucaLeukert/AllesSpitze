#include <Wire.h>
#include "max7219.h"
#include "config.h"
#include "logger.h"

// ===== I2C CONFIGURATION =====
#define I2C_ADDRESS 0x42

// ===== PROTOCOL COMMANDS =====
#define CMD_INIT 0x01
#define CMD_HEALTHCHECK 0x02
#define CMD_POLL_BUTTON_EVENTS 0x03
#define CMD_HIGHLIGHT_BUTTON 0x04
#define CMD_HIGHLIGHT_TOWER 0x05
#define CMD_UPDATE_USER_NAME 0x06
#define CMD_UPDATE_USER_BALANCE 0x07

// Response Commands (Command + 0x80)
#define RSP_INIT 0x81
#define RSP_HEALTHCHECK 0x82
#define RSP_POLL_BUTTON_EVENTS 0x83
#define RSP_HIGHLIGHT_BUTTON 0x84
#define RSP_HIGHLIGHT_TOWER 0x85
#define RSP_UPDATE_USER_NAME 0x86
#define RSP_UPDATE_USER_BALANCE 0x87

// ===== STATUS CODES =====
#define STATUS_SUCCESS 0x00
#define STATUS_ERROR_INVALID_PACKET 0x01
#define STATUS_ERROR_CHECKSUM 0x02
#define STATUS_ERROR_INVALID_COMMAND 0x03
#define STATUS_ERROR_INVALID_PARAM 0x04

// ===== BUFFER SIZES =====
#define RX_BUFFER_SIZE 256
#define TX_BUFFER_SIZE 256
#define MAX_BUTTON_EVENTS 16
#define MAX_USERNAME_LENGTH 32

// ===== GLOBAL STATE =====
struct {
  uint8_t rxBuffer[RX_BUFFER_SIZE];
  uint8_t rxIndex;
  bool rxComplete;

  uint8_t txBuffer[TX_BUFFER_SIZE];
  uint8_t txLength;

  uint8_t buttonEvents[MAX_BUTTON_EVENTS];
  uint8_t buttonEventCount;

  char username[MAX_USERNAME_LENGTH + 1];
  int32_t balanceCents;               // Balance in cents (divide by 100 for display)
  int32_t lastDisplayedBalanceCents;  // Track last displayed balance in cents

  bool initialized;
} state;


// Button state tracking
bool buttonStates[NUM_BUTTONS];
bool lastButtonStates[NUM_BUTTONS];

// MAX7219 Display instance
MAX7219 display = MAX7219();

// Forward declarations
void initializeHardware();
void scanButtons();
void addButtonEvent(uint8_t buttonId);
void receiveEvent(int numBytes);
void requestEvent();
void processPacket();
void handleInit();
void handleHealthCheck();
void handlePollButtonEvents();
void handleHighlightButton(uint8_t length);
void handleHighlightTower(uint8_t length);
void handleUpdateUserName(uint8_t length);
void handleUpdateUserBalance(uint8_t length);
void sendResponse(uint8_t responseCmd, uint8_t status);
void sendErrorResponse(uint8_t command, uint8_t errorCode);
void printStatus();
void updateDisplay();
void initializeDisplay();

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  LOG_INFO(LOG_CAT_SYSTEM, "=== I2C Slave Protocol Device ===");
  logInfo2Hex(LOG_CAT_I2C, "I2C Address: 0x", I2C_ADDRESS);

  // Initialize state
  memset(&state, 0, sizeof(state));
  state.initialized = false;
  state.lastDisplayedBalanceCents = -999999.0;  // Force initial update

  // Initialize hardware
  initializeHardware();

  // Initialize display
  initializeDisplay();

  // Setup I2C
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  LOG_INFO(LOG_CAT_SYSTEM, "Ready!");
}

// ===== MAIN LOOP =====
void loop() {
  // Process received packet
  if (state.rxComplete) {
    processPacket();
    state.rxComplete = false;
    state.rxIndex = 0;
  }

  // Scan buttons for events
  scanButtons();

  // Update display if balance changed
  updateDisplay();

  // Debug output
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    printStatus();
    lastDebug = millis();
  }
}

// ===== DISPLAY FUNCTIONS =====
void initializeDisplay() {
  display.Begin();
  display.MAX7219_SetBrightness(DISPLAY_INTENSITY);

  // Display initial message
  display.DisplayText("--------", LEFT);

  LOG_INFO(LOG_CAT_SYSTEM, "Display initialized");
}

void updateDisplay() {
  if (!state.initialized) {
    display.DisplayText("--------", LEFT);
    return;
  }

  // Only update if balance has changed
  if (state.balanceCents != state.lastDisplayedBalanceCents) {
    display.Clear();

    // Format balance with 2 decimal places
    char balanceStr[16];
    int32_t wholePart = state.balanceCents / 100;
    int32_t decimalPart = abs(state.balanceCents % 100);

    // Format as "XXX.XX"
    snprintf(balanceStr, sizeof(balanceStr), "%ld.%02ld", wholePart, decimalPart);

    // Display the balance right-justified
    display.DisplayText(balanceStr, RIGHT);

    state.lastDisplayedBalanceCents = state.balanceCents;

    logInfo2Str(LOG_CAT_STATE, "Display updated: ", balanceStr);
  }
}

// ===== HARDWARE INITIALIZATION =====
void initializeHardware() {
  // Initialize button pins
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    pinMode(BUTTON_LED_PINS[i], OUTPUT);
    digitalWrite(BUTTON_LED_PINS[i], LOW);
    buttonStates[i] = false;
    lastButtonStates[i] = false;
  }

  // Initialize tower LED pins
  for (int t = 0; t < NUM_TOWERS; t++) {
    for (int r = 0; r < NUM_TOWER_ROWS; r++) {
      pinMode(TOWER_LED_PINS[t][r], OUTPUT);
      digitalWrite(TOWER_LED_PINS[t][r], LOW);
    }
  }

  LOG_INFO(LOG_CAT_SYSTEM, "Hardware initialized");
}

// ===== BUTTON SCANNING =====
void scanButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttonStates[i] = !digitalRead(BUTTON_PINS[i]);  // Active LOW

    // Detect button press (rising edge)
    if (buttonStates[i] && !lastButtonStates[i]) {
      addButtonEvent(i);
    }

    lastButtonStates[i] = buttonStates[i];
  }
}

void addButtonEvent(uint8_t buttonId) {
  if (state.buttonEventCount < MAX_BUTTON_EVENTS) {
    state.buttonEvents[state.buttonEventCount++] = buttonId;
    logInfo2(LOG_CAT_BUTTON, "Button pressed: ", buttonId);
  } else {
    LOG_ERROR(LOG_CAT_BUTTON, "Button event queue full!");
  }
}

// ===== I2C RECEIVE EVENT =====
void receiveEvent(int numBytes) {
  state.rxIndex = 0;
  logDebug2Dec(LOG_CAT_I2C, "I2C receive: ", numBytes);

  while (Wire.available() && state.rxIndex < RX_BUFFER_SIZE) {
    state.rxBuffer[state.rxIndex] = Wire.read();
    state.rxIndex++;
  }

  // Mark packet as complete
  if (state.rxIndex >= 3) {
    state.rxComplete = true;
    logHexDump(LOG_CAT_I2C, "RX", state.rxBuffer, state.rxIndex);
  }
}

// ===== I2C REQUEST EVENT =====
void requestEvent() {
  if (state.txLength > 0) {
    Wire.write(state.txBuffer, state.txLength);
    logHexDump(LOG_CAT_I2C, "TX", state.txBuffer, state.txLength);
    state.txLength = 0;
  } else {
    LOG_WARN(LOG_CAT_I2C, "Request but no response ready!");
  }
}

// ===== PACKET PROCESSING =====
void processPacket() {
  if (state.rxIndex < 3) {
    LOG_ERROR(LOG_CAT_PROTOCOL, "Packet too short!");
    return;
  }

  uint8_t command = state.rxBuffer[0];
  uint8_t length = state.rxBuffer[1];

  if (state.rxIndex < (2 + length + 1)) {
    LOG_ERROR(LOG_CAT_PROTOCOL, "Incomplete packet!");
    return;
  }

  uint8_t checksum = state.rxBuffer[2 + length];
  uint8_t calculatedChecksum = 0;
  for (int i = 0; i < (2 + length); i++) {
    calculatedChecksum ^= state.rxBuffer[i];
  }

  if (calculatedChecksum != checksum) {
    logError2Hex(
      LOG_CAT_PROTOCOL,
      "Checksum error! Expected: 0x",
      calculatedChecksum,
      ", got: 0x",
      checksum);
    sendErrorResponse(command, STATUS_ERROR_CHECKSUM);
    return;
  }

  logDebug2(LOG_CAT_PROTOCOL, "Processing command: 0x", command);

  switch (command) {
    case CMD_INIT:
      handleInit();
      break;
    case CMD_HEALTHCHECK:
      handleHealthCheck();
      break;
    case CMD_POLL_BUTTON_EVENTS:
      handlePollButtonEvents();
      break;
    case CMD_HIGHLIGHT_BUTTON:
      handleHighlightButton(length);
      break;
    case CMD_HIGHLIGHT_TOWER:
      handleHighlightTower(length);
      break;
    case CMD_UPDATE_USER_NAME:
      handleUpdateUserName(length);
      break;
    case CMD_UPDATE_USER_BALANCE:
      handleUpdateUserBalance(length);
      break;
    default:
      logError2(LOG_CAT_PROTOCOL, "Unknown command: 0x", command);
      sendErrorResponse(command, STATUS_ERROR_INVALID_COMMAND);
      break;
  }
}

// ===== COMMAND HANDLERS =====

void handleInit() {
  LOG_INFO(LOG_CAT_PROTOCOL, "INIT command received");

  // Reset state
  state.buttonEventCount = 0;
  memset(state.username, 0, sizeof(state.username));
  state.balanceCents = 0;
  state.lastDisplayedBalanceCents = -999999;  // Force display update
  state.initialized = true;

  // Turn off all LEDs
  for (int i = 0; i < NUM_BUTTONS; i++) {
    digitalWrite(BUTTON_LED_PINS[i], LOW);
  }
  for (int t = 0; t < NUM_TOWERS; t++) {
    for (int r = 0; r < NUM_TOWER_ROWS; r++) {
      digitalWrite(TOWER_LED_PINS[t][r], LOW);
    }
  }

  // Reset display
  display.DisplayText("--------", LEFT);

  sendResponse(RSP_INIT, STATUS_SUCCESS);
}

void handleHealthCheck() {
  LOG_DEBUG(LOG_CAT_PROTOCOL, "HEALTHCHECK command received");

  uint8_t status =
    state.initialized ? STATUS_SUCCESS : STATUS_ERROR_INVALID_PARAM;
  sendResponse(RSP_HEALTHCHECK, status);
}

void handlePollButtonEvents() {
  // Build response: [RSP] [LENGTH] [COUNT] [ID1] ... [IDn] [CHECKSUM]
  state.txBuffer[0] = RSP_POLL_BUTTON_EVENTS;
  state.txBuffer[1] = state.buttonEventCount + 1;  // +1 for count byte
  state.txBuffer[2] = state.buttonEventCount;

  for (int i = 0; i < state.buttonEventCount; i++) {
    state.txBuffer[3 + i] = state.buttonEvents[i];
  }

  uint8_t checksum = 0;
  for (int i = 0; i < (3 + state.buttonEventCount); i++) {
    checksum ^= state.txBuffer[i];
  }
  state.txBuffer[3 + state.buttonEventCount] = checksum;
  state.txLength = 4 + state.buttonEventCount;

  // Clear button events after preparing response
  state.buttonEventCount = 0;
}

void handleHighlightButton(uint8_t length) {
  if (length != 2) {
    LOG_ERROR(LOG_CAT_PROTOCOL, "HIGHLIGHT_BUTTON: Invalid length");
    sendErrorResponse(CMD_HIGHLIGHT_BUTTON, STATUS_ERROR_INVALID_PACKET);
    return;
  }

  uint8_t buttonId = state.rxBuffer[2];
  uint8_t ledState = state.rxBuffer[3];

  logInfo3(
    LOG_CAT_LED,
    "HIGHLIGHT_BUTTON: ID=",
    buttonId,
    " State=",
    ledState);

  if (buttonId >= NUM_BUTTONS) {
    logError2(LOG_CAT_LED, "Invalid button ID: ", buttonId);
    sendErrorResponse(CMD_HIGHLIGHT_BUTTON, STATUS_ERROR_INVALID_PARAM);
    return;
  }

  digitalWrite(BUTTON_LED_PINS[buttonId], ledState ? HIGH : LOW);
  sendResponse(RSP_HIGHLIGHT_BUTTON, STATUS_SUCCESS);
}

void handleHighlightTower(uint8_t length) {
  if (length != 2) {
    LOG_ERROR(LOG_CAT_PROTOCOL, "HIGHLIGHT_TOWER: Invalid length");
    sendErrorResponse(CMD_HIGHLIGHT_TOWER, STATUS_ERROR_INVALID_PACKET);
    return;
  }

  uint8_t towerId = state.rxBuffer[2];
  uint8_t row = state.rxBuffer[3];
  Serial.println("tower");

  logInfo3(LOG_CAT_LED, "HIGHLIGHT_TOWER: Tower=", towerId, " Row=", row);

  if (towerId >= NUM_TOWERS || row >= NUM_TOWER_ROWS + 1) {
    logTimestamp();
    logCategory("ERROR");
    Serial.print("Invalid tower/row: ");
    Serial.print(towerId);
    Serial.print("/");
    Serial.println(row);
    sendErrorResponse(CMD_HIGHLIGHT_TOWER, STATUS_ERROR_INVALID_PARAM);
    return;
  }

  if (row == 0) {
    // Turn off all rows for this tower
    for (int r = 0; r < NUM_TOWER_ROWS; r++) {
      digitalWrite(TOWER_LED_PINS[towerId][r], LOW);
    }
    sendResponse(RSP_HIGHLIGHT_TOWER, STATUS_SUCCESS);
    return;
  }

  row -= 1;

  // Turn off all rows for this tower
  for (int r = 0; r < NUM_TOWER_ROWS; r++) {
    digitalWrite(TOWER_LED_PINS[towerId][r], LOW);
  }

  // Turn on specified row
  for (int r = 0; r <= row; r++) {
    digitalWrite(TOWER_LED_PINS[towerId][r], HIGH);
  }
  Serial.print("highlight tower pin: ");
  Serial.println(TOWER_LED_PINS[towerId][row]);
  sendResponse(RSP_HIGHLIGHT_TOWER, STATUS_SUCCESS);
}

void handleUpdateUserName(uint8_t length) {
  if (length > MAX_USERNAME_LENGTH) {
    logError2(LOG_CAT_PROTOCOL, "Username too long: ", length);
    sendErrorResponse(CMD_UPDATE_USER_NAME, STATUS_ERROR_INVALID_PARAM);
    return;
  }

  memset(state.username, 0, sizeof(state.username));
  memcpy(state.username, &state.rxBuffer[2], length);
  state.username[length] = '\0';

  logInfo2Str(LOG_CAT_STATE, "UPDATE_USER_NAME: ", state.username);

  sendResponse(RSP_UPDATE_USER_NAME, STATUS_SUCCESS);
}

void handleUpdateUserBalance(uint8_t length) {
  if (length != 4) {
    LOG_ERROR(LOG_CAT_PROTOCOL, "UPDATE_USER_BALANCE: Invalid length");
    sendErrorResponse(
      CMD_UPDATE_USER_BALANCE,
      STATUS_ERROR_INVALID_PACKET);
    return;
  }

  // Receive balance in cents (already multiplied by 100 on sender side)
  state.balanceCents = (int32_t)((uint32_t)state.rxBuffer[2] | ((uint32_t)state.rxBuffer[3] << 8) | ((uint32_t)state.rxBuffer[4] << 16) | ((uint32_t)state.rxBuffer[5] << 24));

  logTimestamp();
  logCategory("INFO");
  Serial.print("UPDATE_USER_BALANCE (cents): ");
  Serial.println(state.balanceCents);

  sendResponse(RSP_UPDATE_USER_BALANCE, STATUS_SUCCESS);

  // Display will be updated in main loop
}

// ===== RESPONSE HELPERS =====

void sendResponse(uint8_t responseCmd, uint8_t status) {
  state.txBuffer[0] = responseCmd;
  state.txBuffer[1] = 1;  // Length: 1 byte (status)
  state.txBuffer[2] = status;

  uint8_t checksum = responseCmd ^ 1 ^ status;
  state.txBuffer[3] = checksum;
  state.txLength = 4;
}

void sendErrorResponse(uint8_t command, uint8_t errorCode) {
  uint8_t responseCmd = command | 0x80;
  sendResponse(responseCmd, errorCode);
}

// ===== DEBUG OUTPUT =====

void printStatus() {
  LOG_INFO(LOG_CAT_STATE, "=== Status ===");

  logTimestamp();
  logCategory("INFO");
  Serial.print("Initialized: ");
  Serial.println(state.initialized ? "YES" : "NO");

  logTimestamp();
  logCategory("INFO");
  Serial.print("Username: ");
  Serial.println(state.username);

  logTimestamp();
  logCategory("INFO");
  Serial.print("Balance (cents): ");
  Serial.println(state.balanceCents);

  logTimestamp();
  logCategory("INFO");
  Serial.print("Button Events: ");
  Serial.println(state.buttonEventCount);

  LOG_DEBUG(LOG_CAT_BUTTON, "Button states:");
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (i % 8 == 0 && i > 0)
      Serial.println();
    Serial.print(buttonStates[i] ? "1" : "0");
  }
  Serial.println();
}
// ===== LOGGING SYSTEM =====

// Log levels
#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_DEBUG 4
#define LOG_LEVEL_TRACE 5

// Set the active log level (change this to control verbosity)
#define ACTIVE_LOG_LEVEL LOG_LEVEL_INFO

// Log categories (bitflags for granular control)
#define LOG_CAT_SYSTEM (1 << 0)
#define LOG_CAT_I2C (1 << 1)
#define LOG_CAT_BUTTON (1 << 2)
#define LOG_CAT_LED (1 << 3)
#define LOG_CAT_PROTOCOL (1 << 4)
#define LOG_CAT_STATE (1 << 5)

// Enable/disable categories (OR together categories you want)
#define ACTIVE_LOG_CATEGORIES \
  (LOG_CAT_SYSTEM | LOG_CAT_I2C | LOG_CAT_BUTTON | LOG_CAT_PROTOCOL | LOG_CAT_LED)

// Timestamp options
#define LOG_TIMESTAMP_MILLIS 1  // Set to 0 to disable timestamps

// Helper function to print timestamp
inline void logTimestamp() {
#if LOG_TIMESTAMP_MILLIS
  unsigned long ms = millis();
  unsigned long seconds = ms / 1000;
  unsigned long mins = seconds / 60;
  unsigned long hours = mins / 60;

  Serial.print('[');
  if (hours < 10) Serial.print('0');
  Serial.print(hours);
  Serial.print(':');
  if ((mins % 60) < 10) Serial.print('0');
  Serial.print(mins % 60);
  Serial.print(':');
  if ((seconds % 60) < 10) Serial.print('0');
  Serial.print(seconds % 60);
  Serial.print('.');
  unsigned long ms_part = ms % 1000;
  if (ms_part < 100) Serial.print('0');
  if (ms_part < 10) Serial.print('0');
  Serial.print(ms_part);
  Serial.print("] ");
#endif
}

// Helper function to print category name
inline void logCategory(const char* category) {
  Serial.print('[');
  Serial.print(category);
  Serial.print("] ");
}

// Helper to print hex byte
inline void printHexByte(uint8_t value) {
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
}

// Main logging macros
#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(category, msg)                                       \
  do {                                                                 \
    if (category & ACTIVE_LOG_CATEGORIES) {                            \
      logTimestamp();                                                  \
      logCategory("ERROR");                                            \
      Serial.println(msg);                                             \
    }                                                                  \
  } while (0)
#else
#define LOG_ERROR(category, msg)
#endif

#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_WARNING
#define LOG_WARN(category, msg)                                        \
  do {                                                                 \
    if (category & ACTIVE_LOG_CATEGORIES) {                            \
      logTimestamp();                                                  \
      logCategory("WARN");                                             \
      Serial.println(msg);                                             \
    }                                                                  \
  } while (0)
#else
#define LOG_WARN(category, msg)
#endif

#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(category, msg)                                        \
  do {                                                                 \
    if (category & ACTIVE_LOG_CATEGORIES) {                            \
      logTimestamp();                                                  \
      logCategory("INFO");                                             \
      Serial.println(msg);                                             \
    }                                                                  \
  } while (0)
#else
#define LOG_INFO(category, msg)
#endif

#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(category, msg)                                       \
  do {                                                                 \
    if (category & ACTIVE_LOG_CATEGORIES) {                            \
      logTimestamp();                                                  \
      logCategory("DEBUG");                                            \
      Serial.println(msg);                                             \
    }                                                                  \
  } while (0)
#else
#define LOG_DEBUG(category, msg)
#endif

#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_TRACE
#define LOG_TRACE(category, msg)                                       \
  do {                                                                 \
    if (category & ACTIVE_LOG_CATEGORIES) {                            \
      logTimestamp();                                                  \
      logCategory("TRACE");                                            \
      Serial.println(msg);                                             \
    }                                                                  \
  } while (0)
#else
#define LOG_TRACE(category, msg)
#endif

// Hex dump helper
#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG
inline void logHexDump(
  uint16_t category,
  const char* label,
  const uint8_t* data,
  size_t length) {
  if (!(category & ACTIVE_LOG_CATEGORIES))
    return;

  logTimestamp();
  logCategory("HEX");
  Serial.print(label);
  Serial.print(" (");
  Serial.print(length);
  Serial.print(" bytes): ");

  for (size_t i = 0; i < length; i++) {
    printHexByte(data[i]);
    Serial.print(' ');
  }
  Serial.println();
}
#else
#define logHexDump(category, label, data, length)
#endif

// Formatted logging helpers
#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_ERROR
inline void logError2(uint16_t category, const char* msg, int val) {
  if (category & ACTIVE_LOG_CATEGORIES) {
    logTimestamp();
    logCategory("ERROR");
    Serial.print(msg);
    Serial.println(val);
  }
}
inline void logError2Hex(
  uint16_t category,
  const char* msg1,
  uint8_t val1,
  const char* msg2,
  uint8_t val2) {
  if (category & ACTIVE_LOG_CATEGORIES) {
    logTimestamp();
    logCategory("ERROR");
    Serial.print(msg1);
    printHexByte(val1);
    Serial.print(msg2);
    printHexByte(val2);
    Serial.println();
  }
}
#else
#define logError2(category, msg, val)
#define logError2Hex(category, msg1, val1, msg2, val2)
#endif

#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_INFO
inline void logInfo2(uint16_t category, const char* msg, int val) {
  if (category & ACTIVE_LOG_CATEGORIES) {
    logTimestamp();
    logCategory("INFO");
    Serial.print(msg);
    Serial.println(val);
  }
}
inline void logInfo2Hex(
  uint16_t category,
  const char* msg,
  uint8_t val) {
  if (category & ACTIVE_LOG_CATEGORIES) {
    logTimestamp();
    logCategory("INFO");
    Serial.print(msg);
    printHexByte(val);
    Serial.println();
  }
}
inline void logInfo2Str(
  uint16_t category,
  const char* msg,
  const char* str) {
  if (category & ACTIVE_LOG_CATEGORIES) {
    logTimestamp();
    logCategory("INFO");
    Serial.print(msg);
    Serial.println(str);
  }
}
inline void logInfo3(
  uint16_t category,
  const char* msg1,
  int val1,
  const char* msg2,
  int val2) {
  if (category & ACTIVE_LOG_CATEGORIES) {
    logTimestamp();
    logCategory("INFO");
    Serial.print(msg1);
    Serial.print(val1);
    Serial.print(msg2);
    Serial.println(val2);
  }
}
#else
#define logInfo2(category, msg, val)
#define logInfo2Hex(category, msg, val)
#define logInfo2Str(category, msg, str)
#define logInfo3(category, msg1, val1, msg2, val2)
#endif

#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG
inline void logDebug2(
  uint16_t category,
  const char* msg,
  uint8_t val) {
  if (category & ACTIVE_LOG_CATEGORIES) {
    logTimestamp();
    logCategory("DEBUG");
    Serial.print(msg);
    printHexByte(val);
    Serial.println();
  }
}
inline void logDebug2Dec(
  uint16_t category,
  const char* msg,
  int val) {
  if (category & ACTIVE_LOG_CATEGORIES) {
    logTimestamp();
    logCategory("DEBUG");
    Serial.print(msg);
    Serial.println(val);
  }
}
#else
#define logDebug2(category, msg, val)
#define logDebug2Dec(category, msg, val)
#endif
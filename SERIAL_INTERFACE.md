# AllesSpitze Serial Interface Documentation

## Overview

The AllesSpitze slot machine now supports remote control via serial communication over USB. This allows you to control the machine from a PC using a USB null modem cable and terminal software like PuTTY.

## Hardware Setup

### Requirements
- USB null modem cable (USB-to-Serial adapter)
- Raspberry Pi running AllesSpitze
- PC with PuTTY or similar terminal software

### Connection
1. Connect the USB null modem cable between the PC and Raspberry Pi
2. The application will automatically detect and open the serial port on startup
3. Common device names:
   - Linux/Raspberry Pi: `/dev/ttyUSB0`, `/dev/ttyACM0`
   - macOS: `/dev/cu.usbserial-*`, `/dev/cu.usbmodem-*`
   - Windows: `COM3`, `COM4`, etc.

## PuTTY Configuration

### Settings
- **Connection type**: Serial
- **Serial line**: 
  - Windows: `COM3` (check Device Manager)
  - Linux: `/dev/ttyUSB0` (check with `ls /dev/tty*`)
- **Speed (baud rate)**: `115200`
- **Data bits**: `8`
- **Stop bits**: `1`
- **Parity**: `None`
- **Flow control**: `None`

### Steps to Connect
1. Open PuTTY
2. Select "Serial" as connection type
3. Enter the serial port name (e.g., `COM3` on Windows)
4. Set speed to `115200`
5. Click "Open"
6. You should see a welcome message:
   ```
   # AllesSpitze Serial Interface Ready
   # Commands: POWER_ON, POWER_OFF, SET_BALANCE <value>, SET_PROB <json>, STATUS
   ```

## Available Commands

### 1. Power Control

#### Power On
```
POWER_ON
```
or
```
ON
```

**Response**: `OK: Powering on`

**Effect**: 
- Turns on the display (shows current balance)
- Enables all buttons
- Enables tower LEDs
- Allows normal game operation

#### Power Off
```
POWER_OFF
```
or
```
OFF
```

**Response**: `OK: Powering off`

**Effect**:
- Clears the display (shows 0)
- Disables all buttons (LEDs off)
- Turns off all tower LEDs
- Ignores button presses
- Shows "POWERED OFF" on screen (QML UI)

### 2. Balance Management

#### Set Balance
```
SET_BALANCE <amount>
```
or
```
BALANCE <amount>
```

**Examples**:
```
SET_BALANCE 100.5
BALANCE 500
SET_BALANCE 1000.75
```

**Response**: `OK: Balance set to <amount>`

**Effect**:
- Sets the player's balance to the specified amount
- Automatically saves to persistent storage
- Updates the Arduino display immediately

### 3. Probability Configuration

#### Set Symbol Probabilities
```
SET_PROB <json>
```
or
```
PROBABILITIES <json>
```

**JSON Format**:
```json
{
  "coin": <weight>,
  "kleeblatt": <weight>,
  "marienkaefer": <weight>,
  "sonne": <weight>,
  "teufel": <weight>
}
```

**Examples**:
```
SET_PROB {"coin":10,"kleeblatt":15,"marienkaefer":20,"sonne":5,"teufel":8}
```

```
SET_PROB {"coin":5,"marienkaefer":30}
```

**Response**: `OK: Probabilities updated`

**Effect**:
- Updates the symbol weights in the slot reel
- Higher weight = more frequent appearance
- Only specified symbols are updated (others keep current values)
- Takes effect on the next spin

**Note**: The weight values represent relative frequencies, not absolute percentages.

### 4. Status Query

#### Get System Status
```
STATUS
```
or
```
?
```

**Response**:
```
=== AllesSpitze Status ===
Power: ON/OFF
Balance: <current_balance>
Bet: <current_bet>
Current Prize: <accumulated_prize>
Session Active: YES/NO
Risk Mode: YES/NO
Risk Level: <0-7>
Risk Prize: <risk_amount>
==========================
```

**Example Response**:
```
=== AllesSpitze Status ===
Power: ON
Balance: 1044.8
Bet: 1.0
Current Prize: 15.5
Session Active: YES
Risk Mode: NO
Risk Level: 0
Risk Prize: 0
==========================
```

## Usage Examples

### Example Session 1: Basic Control
```
> STATUS
=== AllesSpitze Status ===
Power: ON
Balance: 100.0
Bet: 1.0
Current Prize: 0
Session Active: NO
Risk Mode: NO
Risk Level: 0
Risk Prize: 0
==========================

> SET_BALANCE 500
OK: Balance set to 500

> STATUS
=== AllesSpitze Status ===
Power: ON
Balance: 500.0
...
```

### Example Session 2: Power Cycle
```
> POWER_OFF
OK: Powering off

> STATUS
=== AllesSpitze Status ===
Power: OFF
Balance: 500.0
...

> POWER_ON
OK: Powering on
```

### Example Session 3: Adjust Probabilities
```
> SET_PROB {"coin":20,"marienkaefer":10}
OK: Probabilities updated

> STATUS
=== AllesSpitze Status ===
Power: ON
Balance: 500.0
...
```

## Power State Details

### When Powered ON:
- ✅ Display shows current balance
- ✅ Buttons are active (LEDs indicate availability)
- ✅ Tower LEDs show current levels
- ✅ Players can spin, bet, and cash out
- ✅ All game features are available

### When Powered OFF:
- ❌ Display shows "--------" or 0
- ❌ All button LEDs are OFF
- ❌ All tower LEDs are OFF
- ❌ Button presses are ignored
- ❌ Screen shows "POWERED OFF"
- ⚠️ Game state is preserved (balance, towers, prizes)

**Important**: Powering off does NOT reset the game state. When you power back on, all towers, prizes, and balance are restored.

## Troubleshooting

### Serial Port Not Found
- **Linux**: Check permissions with `sudo chmod 666 /dev/ttyUSB0`
- **Windows**: Verify COM port in Device Manager
- Check that the USB cable is properly connected
- Try unplugging and replugging the USB cable

### No Response to Commands
- Verify baud rate is set to `115200`
- Check that line ending is set to `LF` or `CR+LF`
- Ensure the application is running on the Raspberry Pi
- Check the application logs for errors

### Commands Not Working
- Commands are case-insensitive (POWER_ON = power_on)
- Each command must be on a separate line
- JSON must be valid (use online JSON validator if unsure)
- Check for typos in command names

## Technical Details

### Serial Configuration
- **Baud Rate**: 115200
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: None
- **Line Ending**: LF (`\n`) or CR+LF (`\r\n`)

### Thread Safety
All serial commands are processed in a dedicated thread and safely forwarded to the main application thread.

### Auto-Detection
The application automatically searches for USB serial devices on startup using these patterns:
- Linux: `ttyUSB*`, `ttyACM*`
- macOS: `cu.usbserial*`, `cu.usbmodem*`
- Manufacturers: FTDI, Prolific, and others

### Logging
All serial commands and responses are logged in the application log file:
```
~/.local/share/AllesSpitzeQt/debug_YYYY-MM-DD_HH-MM-SS.log
```

## Integration with I2C/Arduino

The power state and balance updates are automatically synchronized with the Arduino:
- Power ON/OFF controls button LEDs and tower LEDs
- Balance changes are sent to the 7-segment display
- All synchronization happens automatically

## Future Enhancements

Potential future commands:
- `RESET_GAME` - Reset all towers and prizes
- `SET_BET <amount>` - Change bet amount remotely
- `FORCE_SPIN` - Trigger a spin programmatically
- `GET_STATISTICS` - Retrieve play statistics


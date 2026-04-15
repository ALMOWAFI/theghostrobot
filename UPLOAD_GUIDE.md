# How to Upload — The Ghost Robot

Complete step-by-step guide to embed the code into both devices.

---

## What You Need

- Cursor (already installed)
- PlatformIO extension (already installed)
- USB cable (USB-A to USB-B for Arduino R3, USB-A to Micro-USB for ESP32)
- Both devices unpowered from batteries during upload

---

## Step 1 — Open the Project in Cursor

```
File → Open Folder → C:/Users/almwa/theghostrobot
```

PlatformIO will auto-detect both projects (`esp32/` and `arduino/`).  
Wait for it to finish indexing (bottom status bar shows progress).

---

## Step 2 — Install Bluepad32 Library (ESP32 only, one time)

1. In Cursor terminal:
```
cd esp32
pio lib install "https://github.com/ricardoquesada/bluepad32-arduino.git"
```
Or PlatformIO will auto-install it on first build from `platformio.ini`.

---

## Step 3 — Upload Arduino R3 (Flipper System)

### A. Set TEST_MODE on
Open `arduino/src/main.cpp` and set:
```cpp
#define TEST_MODE   1
```

### B. Connect Arduino R3 via USB

### C. Find the COM port
In Cursor terminal:
```
pio device list
```
Look for something like `COM3` or `COM4` — that is your Arduino.

### D. Upload
```
cd arduino
pio run --target upload
```
Or click the **→ Upload** arrow in the PlatformIO toolbar at the bottom of Cursor.

### E. Open Serial Monitor
```
pio device monitor --baud 115200
```
Or click the **plug icon** in PlatformIO toolbar.

### F. Read the test output
The serial monitor will show:
```
[GHOST] Flipper system starting...
[TEST] === ACTUATOR TEST ===
[TEST] Extending...
[TEST] Retracting...
[TEST] === IR SENSOR TEST (10 seconds) ===
[TEST] L=clear   C=clear   R=clear
```
Wave your hand in front of each IR sensor — it should show `DETECT`.

### G. Fix any issues
| Problem | Fix |
|---|---|
| Actuator extends when should retract | Set `INVERT_ACTUATOR 1` |
| IR shows DETECT when nothing there | Set `IR_ACTIVE_HIGH 1` |
| IR never shows DETECT | Check wiring + sensor LED indicator |
| Nothing happens | Check power and COM port |

### H. Set TEST_MODE off and re-upload
```cpp
#define TEST_MODE   0
```
Upload again. Arduino R3 is done.

---

## Step 4 — Upload ESP32 (Drive System)

### A. Set TEST_MODE on
Open `esp32/src/main.cpp` and set:
```cpp
#define TEST_MODE   1
```

### B. Disconnect Arduino, connect ESP32 via USB

### C. Find the COM port
```
pio device list
```

### D. Upload
```
cd ../esp32
pio run --target upload
```

> **Note:** If upload fails with "connecting..." — hold the **BOOT button** on the ESP32 while upload starts, release after it says "Uploading".

### E. Open Serial Monitor
```
pio device monitor --baud 115200
```

### F. Read the test output
```
[GHOST] Drive system starting...
[TEST] Left motor FORWARD 1s
[TEST] Left motor BACKWARD 1s
[TEST] Right motor FORWARD 1s
[TEST] Right motor BACKWARD 1s
[TEST] Done.
[GHOST] Ready. Pair Xbox controller now.
```

### G. Fix any issues
| Problem | Fix |
|---|---|
| Left motor spins wrong direction | Set `INVERT_LEFT_MOTOR 1` |
| Right motor spins wrong direction | Set `INVERT_RIGHT_MOTOR 1` |
| Both motors wrong | Swap motor + and - wires on L298N outputs |
| No motor movement | Check L298N power (12V pin) and ENA/ENB jumpers |
| Upload fails | Hold BOOT button on ESP32 during upload |

### H. Pair Xbox Controller
After upload with TEST_MODE off:
1. Turn on Xbox controller
2. Hold the **pair button** (top of controller, small button) until light flashes
3. Serial monitor shows: `[BT] Xbox connected`
4. Test drive — left stick forward/back, right stick to turn
5. Hold **Y button** for turbo

### I. Set TEST_MODE off and re-upload
```cpp
#define TEST_MODE   0
```
Upload again. ESP32 is done.

---

## Step 5 — Final Integration Test

1. Power both systems from batteries (disconnect USB)
2. Turn on Xbox controller → ESP32 connects automatically
3. Arduino R3 boots → waits 5s → calibrates IR → pre-extends wedge
4. Drive the robot around, verify:
   - Motors respond to controller
   - Wedge is extended and low to ground
   - Wave hand near front IR sensors → actuator fires

---

## Step 6 — Tuning Before Competition

Open serial monitor on Arduino R3 and check calibration output:
```
[CAL] mean=0.980 stddev=0.140 threshold=0.700
```
If detection is too sensitive or misses opponent — adjust the IR sensor potentiometers (small screw on each sensor board) until detection distance is ~15cm.

---

## Quick Reference — PlatformIO Commands

| Action | Command |
|---|---|
| Build only (no upload) | `pio run` |
| Build + upload | `pio run --target upload` |
| Serial monitor | `pio device monitor --baud 115200` |
| List connected devices | `pio device list` |
| Clean build cache | `pio run --target clean` |

---

## File Structure

```
theghostrobot/
├── esp32/
│   ├── platformio.ini       ← ESP32 build config
│   └── src/
│       └── main.cpp         ← ESP32 drive code
├── arduino/
│   ├── platformio.ini       ← Arduino build config
│   └── src/
│       └── main.cpp         ← Arduino flipper code
├── wiring/
│   └── wiring_notes.md      ← Full wiring reference
├── README.md                ← Project overview
└── UPLOAD_GUIDE.md          ← This file
```

---

## Troubleshooting

**PlatformIO not detecting board**
- Unplug and replug USB
- Try different USB cable (some are charge-only, no data)
- Install CH340 driver (common for Arduino clones): search "CH340 driver Windows"

**ESP32 keeps restarting (boot loop)**
- Check Serial Monitor for error message
- Most common: Bluepad32 not installed → run `pio lib install` again

**Arduino compiles but actuator does nothing**
- Check D7 and D8 are OUTPUT (already set in code)
- Measure voltage on L298N output pins with multimeter — should be ~7.4V when extending

**Xbox controller connects then immediately disconnects**
- Run `BP32.forgetBluetoothKeys()` is already in the code — should fix pairing issues
- Try fully powering off ESP32, then pairing fresh

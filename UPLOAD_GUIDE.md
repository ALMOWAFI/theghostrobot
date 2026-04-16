# How to Upload — The Ghost Robot

Complete step-by-step guide to flash the ESP32.

---

## What You Need

- Cursor (already installed)
- PlatformIO extension (already installed)
- USB cable (USB-A to Micro-USB for ESP32)
- ESP32 unpowered from batteries during upload

---

## Step 1 — Open the Project in Cursor

```
File → Open Folder → C:/Users/almwa/theghostrobot
```

PlatformIO will auto-detect the esp32 project.
Wait for it to finish indexing (bottom status bar shows progress).

---

## Step 2 — Upload ESP32

### A. Set TEST_MODE on
Open `esp32/src/main.cpp` and set:
```cpp
#define TEST_MODE   1
```

### B. Connect ESP32 via USB

### C. Find the COM port
In Cursor terminal:
```
pio device list
```
Look for something like `COM3` or `COM4`.

### D. Upload
```
cd esp32
pio run --target upload
```
Or click the **→ Upload** arrow in the PlatformIO toolbar.

> **If upload fails with "connecting..."** — hold the **BOOT button** on the ESP32 while upload starts, release after it says "Uploading".

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
[TEST] Actuator EXTEND 1s
[TEST] Actuator RETRACT 1s
[TEST] Done.
[GHOST] Ready. Pair Xbox controller now.
```

### G. Fix any issues
| Problem | Fix |
|---|---|
| Left motor spins wrong direction | Set `INVERT_LEFT_MOTOR 1` |
| Right motor spins wrong direction | Set `INVERT_RIGHT_MOTOR 1` |
| Actuator extends when should retract | Set `INVERT_ACTUATOR 1` |
| No motor movement | Check TB6612 VM power and STBY pin (GPIO13 → HIGH) |
| Actuator does nothing | Check MX1616H VM power (buck converter set to 6V) |
| Upload fails | Hold BOOT button on ESP32 during upload |

### H. Set TEST_MODE off and re-upload
```cpp
#define TEST_MODE   0
```
Upload again.

---

## Step 3 — Pair Xbox Controller

1. Turn on Xbox controller
2. Hold the **pair button** (top of controller, small button) until light flashes
3. Serial monitor shows: `[BT] Xbox connected`
4. Test drive — left stick forward/back, right stick to turn
5. Hold **Y button** for turbo
6. Hold **R1** to extend actuator, **L1** to retract

---

## Step 4 — Final Integration Test

1. Power ESP32 from batteries (disconnect USB)
2. Turn on Xbox controller → ESP32 connects automatically
3. Drive the robot around, verify:
   - Motors respond to controller
   - R1 extends wedge, L1 retracts it
   - Y button gives full speed burst

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
│       └── main.cpp         ← ESP32 code (drive + actuator)
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
- Install CH340 driver if needed: search "CH340 driver Windows"

**ESP32 keeps restarting (boot loop)**
- Check Serial Monitor for error message
- Most common: Bluepad32 not installed → PlatformIO will auto-install on first build

**Motors don't move**
- Check TB6612 STBY pin is wired to GPIO13
- Check VM pin has 14.8V from series batteries

**Actuator doesn't move**
- Check buck converter output is ~6V
- Check MX1616H VM connected to buck converter output
- Measure voltage on MX1616H output pins — should be ~3-5V when R1 held

**Xbox controller connects then immediately disconnects**
- `BP32.forgetBluetoothKeys()` is already in the code — should fix pairing
- Try fully powering off ESP32, then pairing fresh

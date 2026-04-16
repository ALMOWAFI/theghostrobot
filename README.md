# The Ghost Robot — Sumo Combat Robot

## The Story

The Ghost Robot is a sumo combat robot built by a team from **Constructor University Bremen**, competing in the **Makers Club Sumo Bot Event** — an official university robotics competition with a **1kg weight limit** and **20x20cm size limit**.

The robot is designed around a two-system architecture: a manual drive system controlled via Xbox controller, and a fully autonomous flipper system that detects opponents using IR sensors and fires a linear actuator wedge. The name "Ghost" reflects the robot's aggressive, fast, and unpredictable nature in the ring.

The 3D-printed chassis is based on the open-source **Sumo Robot Combate** design from Cults3D, heavily modified with enlarged front holes to accommodate the flipper mechanism. All structural parts are printed in PETG for strength and heat resistance, while the wheels use SBR solid rubber grip strips for maximum traction on the sumo mat.

---

## Competition Details

- **Event:** Makers Club Sumo Bot Competition
- **Organizer:** Constructor University Bremen — Makers Club
- **Weight limit:** 1kg maximum
- **Size limit:** 20x20cm maximum
- **Ring:** Standard sumo dohyo (154cm diameter, black mat with white border edge)
- **Rules:** Standard sumo — push opponent out of ring. Active weapons allowed.

---

## Robot Specs

| Spec | Value |
|---|---|
| Weight (printed parts) | 300g |
| Total estimated weight | ~838g |
| Size | 20x20cm |
| Drive motors | 2x 25GA-370 12V 100RPM (~133RPM at 14.8V) |
| Motor driver | L298N Dual H-Bridge |
| Main controller | ESP32 (Bluetooth) |
| Flipper controller | Arduino R3 |
| Flipper mechanism | Linear actuator 12V 60N 25mm stroke |
| Wheel grip | SBR solid rubber self-adhesive strips |
| Batteries | 2x 7.4V 2200mAh 50C LiPo (series = 14.8V for drive) + 11.1V 3S LiPo for flipper |
| Wireless | Xbox controller via Bluetooth (Bluepad32) |

---

## Architecture

The robot runs two completely independent embedded systems:

### System 1 — Drive (ESP32)
```
Xbox Controller (Bluetooth)
        |
      ESP32
        |
    L298N Motor Driver
      /         \
Left Motor    Right Motor
(JGA25-370)   (JGA25-370)
```
- 100% manual control via Xbox controller
- FreeRTOS dual-core: Core 0 handles Bluetooth, Core 1 handles motor control
- 20kHz PWM via ESP32 LEDC hardware
- Physics-based acceleration ramp (no wheel slip)
- Battery voltage compensation (consistent speed through full match)
- Turbo button: Xbox Y button → 100% boost mode

### System 2 — Flipper (Arduino R3)
```
IR Sensor (center)
       |
   Arduino R3
       |
L298N Mini Motor Driver
       |
Linear Actuator (wedge)
```
- 100% autonomous — no connection to ESP32
- Hardware interrupt on IR sensor D3 (INT1) — ~1 microsecond reaction time
- Pre-extends wedge at match start
- Fires actuator on detection, auto-retracts after push with cooldown timer
- AVR assembly actuator trigger (187ns at 16MHz)

---

## Wiring

### System 1 — ESP32 + L298N + Motors

| L298N Pin | ESP32 GPIO | Function |
|---|---|---|
| ENA | GPIO 14 | Left motor PWM |
| IN1 | GPIO 27 | Left motor direction A |
| IN2 | GPIO 26 | Left motor direction B |
| IN3 | GPIO 25 | Right motor direction A |
| IN4 | GPIO 33 | Right motor direction B |
| ENB | GPIO 32 | Right motor PWM |
| 12V | Series battery (+) = 14.8V | Power |
| GND | Series battery (-) + ESP32 GND | Ground |

### System 2 — Arduino R3 + IR Sensor + Linear Actuator

| Component | Arduino Pin | Function |
|---|---|---|
| IR Sensor | D3 | Hardware interrupt (INT1) |
| Actuator IN1 | D7 | Actuator extend |
| Actuator IN2 | D8 | Actuator retract |
| IR Sensor VCC | 5V | Power |
| IR Sensor GND | GND | Ground |
| Actuator Driver 12V | 11.1V 3S LiPo (+) | Power |
| Actuator Driver GND | 11.1V 3S LiPo (-) + Arduino GND | Ground |

---

## Components List

### Purchased
| Component | Spec | Qty |
|---|---|---|
| Drive motors | 25GA-370 12V 100RPM | 2 |
| Linear actuator | 12V 60N 15mm/s 25mm stroke | 1 |
| Motor driver (drive) | L298N Dual H-Bridge | 1 |
| Motor driver (actuator) | L298N Mini | 1 |
| IR sensor | 3-wire obstacle avoidance | 1 |
| Batteries | 7.4V 2200mAh 50C LiPo | 2 |
| Wheel grip | SBR solid rubber self-adhesive 1mm | 1 sheet |

### Already Have
| Component | Notes |
|---|---|
| ESP32 | Main drive controller |
| Arduino R3 | Flipper controller |
| Xbox controller | Drive input via Bluetooth |
| 3D printed chassis | Modified Sumo Robot Combate design |

---

## Code Guidelines & Implementation

### Tools
- **IDE:** Cursor + PlatformIO extension
- **Language:** C/C++ (Arduino framework) + AVR inline assembly (ISR only)
- **ESP32 framework:** Arduino Core with FreeRTOS
- **Key library:** Bluepad32 (Xbox Bluetooth)

### Coding Advantages Implemented

#### ESP32
1. **FreeRTOS dual-core** — Bluetooth on Core 0, motor control on Core 1. Zero interference.
2. **20kHz PWM (LEDC)** — Optimal switching frequency for L298N + JGA25-370. Less heat, more torque.
3. **Physics-based acceleration ramp** — Based on μ=0.65 (SBR rubber), mass=838g. Max acceleration = 6.3 m/s². Prevents wheel slip.
4. **Voltage compensation** — Reads battery level, scales PWM to maintain constant speed.
5. **Turbo mode** — Xbox Y button bypasses ramp and goes to 100% PWM instantly.
6. **Tank drive mixing** — Left stick Y = forward/back, Right stick X = turning.

#### Arduino R3
1. **Hardware interrupt** — Single IR sensor on INT1 (D3). Reaction time ~1 microsecond.
2. **AVR assembly ISR** — Actuator trigger written in AVR assembly. Executes in 3 clock cycles (187ns at 16MHz).
3. **IR logic** — Detection → extend actuator. No detection + extended → retract.
4. **Startup sequence** — 5 second delay, then pre-extend actuator.
5. **Cooldown timer** — 2.5 second cooldown between actuator triggers to prevent jamming.

### Priority Order (Both Systems)
```
1. Startup sequence (5s delay)
2. Calibration
3. Normal operation loop
```

---

## File Structure

```
theghostrobot/
├── esp32/
│   ├── platformio.ini
│   └── src/
│       └── main.cpp          # ESP32 drive code
├── arduino/
│   ├── platformio.ini
│   └── src/
│       └── main.cpp          # Arduino flipper code
├── wiring/
│   └── wiring_notes.md       # Detailed wiring reference
└── README.md                 # This file
```

---

## TODO

- [ ] Write ESP32 drive code (`esp32/src/main.cpp`)
- [ ] Write Arduino flipper code (`arduino/src/main.cpp`)
- [ ] Create PlatformIO configs (`platformio.ini` for both)
- [ ] Test motor driver wiring
- [ ] Test IR sensor calibration
- [ ] Test linear actuator extend/retract
- [ ] Tune acceleration ramp values
- [ ] Tune IR detection threshold
- [ ] Full system integration test
- [ ] Competition day tuning

---

## 3D Model

Base design: [Sumo Robot Combate — Cults3D](https://cults3d.com/en/3d-model/gadget/sumo-robot-combate)
Modified: Front holes enlarged for linear actuator arm clearance
Material: PETG (all structural parts), SBR rubber strips (wheel grip)
GitHub (CAD + STL): [sumobot-ghost](https://github.com/ALMOWAFI/sumobot-ghost)

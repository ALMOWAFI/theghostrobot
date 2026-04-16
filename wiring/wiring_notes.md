# Wiring Notes

## System 1 — ESP32 Drive System

### Power
- Battery 1 + Battery 2 in **series = 14.8V** → L298N 12V pin + GND
- L298N 5V output → ESP32 VIN (powers ESP32 from motor battery)

### TB6612FNG → ESP32
```
TB6612 PWMA  →  ESP32 GPIO 14   (Left motor PWM - LEDC channel 0)
TB6612 AIN1  →  ESP32 GPIO 27   (Left motor forward)
TB6612 AIN2  →  ESP32 GPIO 26   (Left motor backward)
TB6612 BIN1  →  ESP32 GPIO 25   (Right motor forward)
TB6612 BIN2  →  ESP32 GPIO 33   (Right motor backward)
TB6612 PWMB  →  ESP32 GPIO 32   (Right motor PWM - LEDC channel 1)
TB6612 STBY  →  ESP32 GPIO 13   (Standby — must be HIGH to enable motors)
TB6612 VCC   →  ESP32 3.3V      (Logic power)
TB6612 GND   →  Series battery (-) + ESP32 GND
TB6612 VM    →  Series battery (+) = 14.8V (Motor power)
```

### TB6612FNG → Motors
```
TB6612 AO1 + AO2  →  Left motor  (25GA-370)
TB6612 BO1 + BO2  →  Right motor (25GA-370)
```
Note: If motors spin wrong direction, swap AO1/AO2 or BO1/BO2 wires.
Or set INVERT_LEFT_MOTOR / INVERT_RIGHT_MOTOR in esp32/src/main.cpp.

---

## System 2 — Arduino R3 Flipper System

### Power
- 11.1V 3S LiPo (or buck converter output set to 12V) → L298N Mini [12V] + [GND]
- L298N Mini [5V] output → Arduino R3 VIN
- Arduino R3 powers IR sensor from its 5V pin

### IR Sensor → Arduino R3
```
IR Sensor VCC  →  Arduino 5V
IR Sensor GND  →  Arduino GND
IR Sensor OUT  →  Arduino D3   (Hardware Interrupt INT1)
```
- Only one IR sensor used
- Sensor outputs LOW when obstacle detected (default)
- If sensor outputs HIGH on detection → set IR_ACTIVE_HIGH 1 in code
- Adjust potentiometer on sensor for ~15cm detection range

### L298N Mini → Arduino R3
```
L298N Mini IN1  →  Arduino D7   (Actuator extend)
L298N Mini IN2  →  Arduino D8   (Actuator retract)
L298N Mini GND  →  Arduino GND + Power GND
L298N Mini 12V  →  11.1V battery (+) or buck converter output
```

### L298N Mini → Linear Actuator
```
L298N Mini OUT1  →  Actuator wire 1
L298N Mini OUT2  →  Actuator wire 2
```
Note: If actuator extends when it should retract → set INVERT_ACTUATOR 1 in code.

---

## Full Wiring Diagram (Text)

```
┌─────────────────────────────────────────────────────┐
│              SYSTEM 1 — DRIVE                        │
│                                                      │
│  [7.4V Bat1(+)]──[7.4V Bat2(-)]  ← Series = 14.8V  │
│  [7.4V Bat1(-)]                  ← GND              │
│  [7.4V Bat2(+)]                  ← 14.8V (+)        │
│                                                      │
│  14.8V (+) ──→ TB6612 VM                            │
│  14.8V (-) ──→ TB6612 GND ──→ ESP32 GND             │
│  ESP32 3.3V ──→ TB6612 VCC                          │
│  ESP32 GPIO13 ──→ TB6612 STBY (HIGH = enabled)      │
│                                                      │
│  ESP32 GPIO14 ──→ TB6612 PWMA                       │
│  ESP32 GPIO27 ──→ TB6612 AIN1                       │
│  ESP32 GPIO26 ──→ TB6612 AIN2                       │
│  ESP32 GPIO25 ──→ TB6612 BIN1                       │
│  ESP32 GPIO33 ──→ TB6612 BIN2                       │
│  ESP32 GPIO32 ──→ TB6612 PWMB                       │
│                                                      │
│  TB6612 AO1/AO2 ──→ Left Motor                     │
│  TB6612 BO1/BO2 ──→ Right Motor                    │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│              SYSTEM 2 — FLIPPER                      │
│                                                      │
│  11.1V battery (or buck 12V output)                 │
│  (+) ──→ L298N Mini [12V]                           │
│  (-) ──→ L298N Mini [GND] ──→ Arduino GND           │
│  L298N Mini [5V] ──→ Arduino VIN                    │
│                                                      │
│  IR Sensor VCC ──→ Arduino 5V                       │
│  IR Sensor GND ──→ Arduino GND                      │
│  IR Sensor OUT ──→ Arduino D3                       │
│                                                      │
│  Arduino D7 ──→ L298N Mini IN1                      │
│  Arduino D8 ──→ L298N Mini IN2                      │
│                                                      │
│  L298N Mini OUT1 ──→ Actuator wire 1                │
│  L298N Mini OUT2 ──→ Actuator wire 2                │
└─────────────────────────────────────────────────────┘

NOTE: System 1 and System 2 share NO connections.
      Completely isolated power and signal lines.
```

---

## Important Notes

1. **Systems are fully isolated** — no shared GND, no shared power, no signal wires between ESP32 and Arduino.

2. **Series battery wiring** — connect Battery 1 (+) to Battery 2 (-). Use Battery 1 (-) as GND and Battery 2 (+) as 14.8V positive.

3. **IR sensor orientation** — face forward at ~30–40mm height (center height of opponent robot). Adjust potentiometer for ~15cm range.

4. **Actuator current** — draws ~1A when running. L298N Mini rated 2A — fine.

5. **Motor current** — each JGA25-370 draws ~0.5A running, up to 2A stall. L298N rated 2A per channel — do not stall for extended time.

6. **LiPo safety** — never discharge below 3.0V per cell:
   - 2S (7.4V nominal) → stop at 6.0V
   - 3S (11.1V nominal) → stop at 9.0V

# Wiring Notes

## System 1 — ESP32 Drive System

### Power
- Battery 1 (7.4V LiPo) → L298N 12V pin + GND
- L298N 5V output → ESP32 VIN (powers ESP32 from motor battery)
- Do NOT connect both battery GNDs together (systems are isolated)

### L298N → ESP32
```
L298N ENA  →  ESP32 GPIO 14   (Left motor PWM - LEDC channel 0)
L298N IN1  →  ESP32 GPIO 27   (Left motor forward)
L298N IN2  →  ESP32 GPIO 26   (Left motor backward)
L298N IN3  →  ESP32 GPIO 25   (Right motor forward)
L298N IN4  →  ESP32 GPIO 33   (Right motor backward)
L298N ENB  →  ESP32 GPIO 32   (Right motor PWM - LEDC channel 1)
L298N GND  →  Battery 1 (-)  +  ESP32 GND
L298N 12V  →  Battery 1 (+)
```

### L298N → Motors
```
L298N OUT1 + OUT2  →  Left motor (JGA25-370)
L298N OUT3 + OUT4  →  Right motor (JGA25-370)
```
Note: If motors spin wrong direction, swap OUT1/OUT2 or OUT3/OUT4 wires.

---

## System 2 — Arduino R3 Flipper System

### Power
- Battery 2 (7.4V LiPo) → Actuator L298N Mini 12V pin + GND
- Actuator L298N Mini 5V output → Arduino R3 VIN
- Systems share NO power or signal lines

### IR Sensors → Arduino R3
```
IR Left   VCC  →  Arduino 5V
IR Left   GND  →  Arduino GND
IR Left   OUT  →  Arduino D2    (Hardware Interrupt INT0)

IR Center VCC  →  Arduino 5V
IR Center GND  →  Arduino GND
IR Center OUT  →  Arduino D3    (Hardware Interrupt INT1)

IR Right  VCC  →  Arduino 5V
IR Right  GND  →  Arduino GND
IR Right  OUT  →  Arduino D5    (Pin Change Interrupt PCINT21)
```
Note: IR sensors output LOW when obstacle detected, HIGH when clear.
Adjust sensitivity potentiometer on each sensor to ~15cm detection range.

### L298N Mini → Arduino R3
```
L298N IN1  →  Arduino D7   (Actuator extend)
L298N IN2  →  Arduino D8   (Actuator retract)
L298N GND  →  Arduino GND
L298N 12V  →  Battery 2 (+)
```

### L298N Mini → Linear Actuator
```
L298N OUT1 + OUT2  →  Linear Actuator wires
```
Note: If actuator extends when it should retract, swap the two wires.

---

## Important Notes

1. **Never connect Battery 1 and Battery 2 GNDs** — systems must remain isolated to prevent ground loops and protect Arduino from ESP32 Bluetooth noise.

2. **IR sensor orientation** — sensors must face forward, parallel to ground, at the height of the opponent robot (roughly center height ~30-40mm from ground).

3. **Actuator current** — linear actuator draws up to 1A. L298N Mini can handle this (2A rated).

4. **Motor current** — each JGA25-370 draws up to 0.5A running, 2A stall. L298N can handle 2A per channel. Do not stall motors for extended periods.

5. **LiPo safety** — never discharge below 3.0V per cell (6.0V total for 2S). Monitor battery voltage during testing.

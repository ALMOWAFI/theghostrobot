# Wiring Notes — The Ghost Robot

Single ESP32 system. No Arduino. No IR sensor.

---

## Power

```
[7.4V LiPo #1 (+)] ──→ [7.4V LiPo #2 (-)]   ← connect these together (series)
[7.4V LiPo #1 (-)] ──→ GND rail              ← this is your GND
[7.4V LiPo #2 (+)] ──→ 14.8V rail            ← this is your 14.8V

14.8V rail ──→ TB6612 VM         (motor power)
14.8V rail ──→ Buck converter IN (steps down to 6V for actuator driver)
GND rail   ──→ TB6612 GND
GND rail   ──→ Buck converter GND
GND rail   ──→ ESP32 GND

Buck converter OUT (6V) ──→ MX1616H VM
Buck converter GND      ──→ MX1616H GND

ESP32 powered from USB during testing.
For competition: use a 5V phone charger or power bank for ESP32 VIN.
```

---

## ESP32 → TB6612FNG (Drive Motors)

```
ESP32 3.3V   ──→ TB6612 VCC    (logic power)
ESP32 GND    ──→ TB6612 GND
ESP32 GPIO13 ──→ TB6612 STBY   (must be HIGH — if LOW, motors disabled)
ESP32 GPIO14 ──→ TB6612 PWMA   (left motor PWM)
ESP32 GPIO27 ──→ TB6612 AIN1   (left motor direction A)
ESP32 GPIO26 ──→ TB6612 AIN2   (left motor direction B)
ESP32 GPIO25 ──→ TB6612 BIN1   (right motor direction A)
ESP32 GPIO33 ──→ TB6612 BIN2   (right motor direction B)
ESP32 GPIO32 ──→ TB6612 PWMB   (right motor PWM)
```

## TB6612FNG → Motors

```
TB6612 AO1 + AO2  ──→  Left motor  (25GA-370)
TB6612 BO1 + BO2  ──→  Right motor (25GA-370)
```
If a motor spins wrong direction → swap AO1/AO2 or BO1/BO2 wires,
or set `INVERT_LEFT_MOTOR 1` / `INVERT_RIGHT_MOTOR 1` in code.

---

## ESP32 → MX1616H (Linear Actuator)

```
ESP32 GPIO16 ──→ MX1616H IN1   (actuator extend)
ESP32 GPIO17 ──→ MX1616H IN2   (actuator retract)
ESP32 GND    ──→ MX1616H GND
Buck 6V out  ──→ MX1616H VM    (motor power — max 7V, do not exceed)
```

## MX1616H → Linear Actuator

```
MX1616H OUT1  ──→  Actuator wire 1
MX1616H OUT2  ──→  Actuator wire 2
```
If actuator direction is wrong → set `INVERT_ACTUATOR 1` in code.

---

## Full Wiring Diagram

```
┌──────────────────────────────────────────────────────────┐
│                   SINGLE ESP32 SYSTEM                     │
│                                                           │
│  [7.4V Bat1(+)]──[7.4V Bat2(-)]   ← series = 14.8V      │
│  [7.4V Bat1(-)] = GND                                    │
│  [7.4V Bat2(+)] = 14.8V                                  │
│                                                           │
│  14.8V ──→ TB6612 VM                                     │
│  14.8V ──→ Buck converter IN                             │
│  GND   ──→ TB6612 GND ──→ ESP32 GND ──→ MX1616H GND     │
│                                                           │
│  Buck OUT (6V) ──→ MX1616H VM                            │
│                                                           │
│  ESP32 3.3V   ──→ TB6612 VCC                             │
│  ESP32 GPIO13 ──→ TB6612 STBY                            │
│  ESP32 GPIO14 ──→ TB6612 PWMA                            │
│  ESP32 GPIO27 ──→ TB6612 AIN1                            │
│  ESP32 GPIO26 ──→ TB6612 AIN2                            │
│  ESP32 GPIO25 ──→ TB6612 BIN1                            │
│  ESP32 GPIO33 ──→ TB6612 BIN2                            │
│  ESP32 GPIO32 ──→ TB6612 PWMB                            │
│                                                           │
│  ESP32 GPIO16 ──→ MX1616H IN1                            │
│  ESP32 GPIO17 ──→ MX1616H IN2                            │
│                                                           │
│  TB6612 AO1/AO2 ──→ Left Motor                          │
│  TB6612 BO1/BO2 ──→ Right Motor                         │
│  MX1616H OUT1/OUT2 ──→ Linear Actuator                  │
└──────────────────────────────────────────────────────────┘
```

---

## Important Notes

1. **MX1616H max voltage is 7V** — always power from buck converter, never directly from battery.

2. **TB6612 STBY must be HIGH** — connected to GPIO13. If motors don't move, check this wire first.

3. **Series battery wiring** — connect Bat1 (+) to Bat2 (-). Use Bat1 (-) as GND and Bat2 (+) as 14.8V.

4. **ESP32 power for competition** — use a small USB power bank or 5V regulator from the battery into ESP32 VIN.

5. **Motor current** — each 25GA-370 draws ~0.5A running, up to 2A stall. TB6612 rated 1.2A continuous per channel — do not stall for extended time.

6. **Actuator current** — draws ~1A. MX1616H rated 1.5A per channel — fine.

7. **LiPo safety** — never discharge below 3.0V per cell:
   - 2S (7.4V nominal) → stop at 6.0V
   - Fully charged 2S = 8.4V per cell = 16.8V series

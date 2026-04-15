/**
 * THE GHOST ROBOT — ESP32 Drive System
 * ─────────────────────────────────────
 * Controller : Xbox via Bluetooth (Bluepad32)
 * Hardware   : L298N + 2x JGA25-370 12V 133RPM
 * Battery    : 7.4V 2200mAh LiPo
 *
 * Architecture:
 *   Core 0 — Bluetooth polling (dedicated, no interference)
 *   Core 1 — Motor control at fixed 500Hz tick
 *
 * Optimizations:
 *   - FreeRTOS dual-core isolation
 *   - 20kHz LEDC PWM (optimal for L298N + JGA25-370)
 *   - Physics-based acceleration ramp (no wheel slip)
 *   - Battery voltage compensation (consistent speed)
 *   - Turbo mode via Y button
 */

#include <Arduino.h>
#include <Bluepad32.h>

// ─── Motor Driver Pins ───────────────────────────────────────────────────────
#define ENA_PIN     14    // Left motor PWM  — LEDC channel 0
#define IN1_PIN     27    // Left motor forward
#define IN2_PIN     26    // Left motor backward
#define IN3_PIN     25    // Right motor forward
#define IN4_PIN     33    // Right motor backward
#define ENB_PIN     32    // Right motor PWM  — LEDC channel 1

// ─── Battery Monitoring ──────────────────────────────────────────────────────
// Connect battery + through voltage divider (100kΩ / 47kΩ) to GPIO 34
#define BATT_PIN        34
#define BATT_FULL_V     7.4f
#define BATT_EMPTY_V    6.0f
#define BATT_DIVIDER    (147.0f / 47.0f)   // (R1+R2)/R2

// ─── LEDC PWM Config ─────────────────────────────────────────────────────────
#define PWM_FREQ        20000   // 20kHz — beyond audible range, optimal for L298N
#define PWM_RES         8       // 8-bit resolution (0–255)
#define LEDC_CH_L       0       // Left motor LEDC channel
#define LEDC_CH_R       1       // Right motor LEDC channel

// ─── Drive Config ────────────────────────────────────────────────────────────
// Physics: μ(SBR)=0.65, m=0.838kg, g=9.8 → max_accel=6.37m/s²
// Wheel r=0.018m → max_angular_accel=354 rad/s²
// At 133RPM → ω=13.93 rad/s → time to full speed = 39.3ms
// Ramp: 20 steps × 2ms = 40ms — just at slip threshold
#define RAMP_STEPS      20
#define RAMP_MS         2       // ms per ramp step
#define MAX_PWM         255
#define STICK_MAX       512
#define DEADZONE        30      // Ignore stick noise below this

// ─── Gamepad State ───────────────────────────────────────────────────────────
GamepadPtr gp = nullptr;

void onConnected(GamepadPtr gamepad) {
    gp = gamepad;
    Serial.println("[BT] Xbox controller connected");
}

void onDisconnected(GamepadPtr gamepad) {
    gp = nullptr;
    Serial.println("[BT] Xbox controller disconnected");
}

// ─── Motor State ─────────────────────────────────────────────────────────────
volatile int leftCurrent  = 0;   // Current PWM (-255 to 255)
volatile int rightCurrent = 0;

// ─── Low-level Motor Output ──────────────────────────────────────────────────
void IRAM_ATTR driveMotor(int ledcCh, int pinA, int pinB, int pwm) {
    if (pwm > 0) {
        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, LOW);
    } else if (pwm < 0) {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, HIGH);
        pwm = -pwm;
    } else {
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
    }
    ledcWrite(ledcCh, constrain(pwm, 0, MAX_PWM));
}

// ─── Battery Voltage ─────────────────────────────────────────────────────────
float readBatteryVoltage() {
    int raw = analogRead(BATT_PIN);
    return (raw / 4095.0f) * 3.3f * BATT_DIVIDER;
}

// Scale PWM up as battery drops to maintain consistent speed
float voltageCompensation() {
    float v = constrain(readBatteryVoltage(), BATT_EMPTY_V, BATT_FULL_V);
    return BATT_FULL_V / v;
}

// ─── Core 0: Bluetooth Task ──────────────────────────────────────────────────
void bluetoothTask(void* param) {
    for (;;) {
        BP32.update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ─── Core 1: Motor Control Task (500Hz) ──────────────────────────────────────
void motorTask(void* param) {
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        int leftTarget = 0, rightTarget = 0;

        if (gp && gp->isConnected()) {
            // Read sticks — negate Y so forward = positive
            int ly = -gp->axisY();    // Left stick  Y: forward/back
            int rx =  gp->axisRX();   // Right stick X: turning
            bool turbo = gp->y();     // Y button: instant full speed

            // Deadzone
            if (abs(ly) < DEADZONE) ly = 0;
            if (abs(rx) < DEADZONE) rx = 0;

            // Tank drive mixing
            int left  = constrain(ly + rx, -STICK_MAX, STICK_MAX);
            int right = constrain(ly - rx, -STICK_MAX, STICK_MAX);

            // Map to PWM range, preserve sign
            leftTarget  = map(abs(left),  0, STICK_MAX, 0, MAX_PWM) * (left  < 0 ? -1 : 1);
            rightTarget = map(abs(right), 0, STICK_MAX, 0, MAX_PWM) * (right < 0 ? -1 : 1);

            if (turbo) {
                // Bypass ramp — instant full power
                leftCurrent  = leftTarget;
                rightCurrent = rightTarget;
            } else {
                // Physics-based ramp — stay below slip threshold
                int lStep = (leftTarget  - leftCurrent)  / RAMP_STEPS;
                int rStep = (rightTarget - rightCurrent) / RAMP_STEPS;
                if (lStep == 0 && leftTarget  != leftCurrent)  lStep  = (leftTarget  > leftCurrent)  ? 1 : -1;
                if (rStep == 0 && rightTarget != rightCurrent) rStep  = (rightTarget > rightCurrent) ? 1 : -1;
                leftCurrent  += lStep;
                rightCurrent += rStep;
            }
        } else {
            // No controller — coast to stop
            leftCurrent  = leftCurrent  * 3 / 4;
            rightCurrent = rightCurrent * 3 / 4;
        }

        // Battery compensation
        float comp   = voltageCompensation();
        int leftOut  = constrain((int)(leftCurrent  * comp), -MAX_PWM, MAX_PWM);
        int rightOut = constrain((int)(rightCurrent * comp), -MAX_PWM, MAX_PWM);

        driveMotor(LEDC_CH_L, IN1_PIN, IN2_PIN, leftOut);
        driveMotor(LEDC_CH_R, IN3_PIN, IN4_PIN, rightOut);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(RAMP_MS));
    }
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[GHOST] Drive system starting...");

    // Direction pins
    pinMode(IN1_PIN, OUTPUT); pinMode(IN2_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT); pinMode(IN4_PIN, OUTPUT);

    // 20kHz LEDC PWM
    ledcSetup(LEDC_CH_L, PWM_FREQ, PWM_RES);
    ledcSetup(LEDC_CH_R, PWM_FREQ, PWM_RES);
    ledcAttachPin(ENA_PIN, LEDC_CH_L);
    ledcAttachPin(ENB_PIN, LEDC_CH_R);

    // Motors off
    driveMotor(LEDC_CH_L, IN1_PIN, IN2_PIN, 0);
    driveMotor(LEDC_CH_R, IN3_PIN, IN4_PIN, 0);

    // Bluepad32 — fresh pair on every boot
    BP32.setup(&onConnected, &onDisconnected);
    BP32.forgetBluetoothKeys();

    // Pin Bluetooth task on Core 0, motor task on Core 1
    xTaskCreatePinnedToCore(bluetoothTask, "BT",    4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(motorTask,     "Motor", 4096, NULL, 2, NULL, 1);

    Serial.println("[GHOST] Ready. Pair Xbox controller now.");
    vTaskDelete(NULL);  // Kill setup task — everything runs in RTOS
}

void loop() {
    // Unreachable — FreeRTOS owns execution
}

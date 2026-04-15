/**
 * THE GHOST ROBOT — ESP32 Drive System
 * ─────────────────────────────────────
 * Controller : Xbox via Bluetooth (Bluepad32)
 * Hardware   : L298N + 2x JGA25-370 12V 133RPM
 * Battery    : 7.4V 2200mAh LiPo
 *
 * ─── CONFIGURATION ───────────────────────────────────────────────────────────
 * Edit the flags below before uploading.
 * No need to touch anything else.
 */

// Set to 1 to run motor test on boot (verifies wiring before fighting)
#define TEST_MODE               0

// Set to 1 if left motor spins the wrong direction
#define INVERT_LEFT_MOTOR       0

// Set to 1 if right motor spins the wrong direction
#define INVERT_RIGHT_MOTOR      0

// Set to 1 only if you have built the voltage divider circuit on GPIO 34
// (100kΩ from battery+ to GPIO34, 47kΩ from GPIO34 to GND)
// Leave 0 if unsure — safe default
#define ENABLE_BATTERY_MONITOR  0

// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <Bluepad32.h>

// ─── Motor Driver Pins ───────────────────────────────────────────────────────
#define ENA_PIN     14    // Left motor PWM  — LEDC channel 0
#define IN1_PIN     27    // Left motor forward
#define IN2_PIN     26    // Left motor backward
#define IN3_PIN     25    // Right motor forward
#define IN4_PIN     33    // Right motor backward
#define ENB_PIN     32    // Right motor PWM  — LEDC channel 1

// ─── Battery Monitor (only used if ENABLE_BATTERY_MONITOR = 1) ───────────────
#define BATT_PIN        34
#define BATT_FULL_V     7.4f
#define BATT_EMPTY_V    6.0f
#define BATT_DIVIDER    (147.0f / 47.0f)  // (R1+R2)/R2 with 100k/47k divider

// ─── LEDC PWM ────────────────────────────────────────────────────────────────
#define PWM_FREQ        20000   // 20kHz — optimal for L298N + JGA25-370
#define PWM_RES         8       // 8-bit (0–255)
#define LEDC_CH_L       0
#define LEDC_CH_R       1

// ─── Drive Config ────────────────────────────────────────────────────────────
// Physics: μ(SBR)=0.65, m=0.838kg → max_accel=6.37m/s²
// Ramp: 20 steps × 2ms = 40ms to full speed — just at slip threshold
#define RAMP_STEPS      20
#define RAMP_MS         2
#define MAX_PWM         255
#define STICK_MAX       512
#define DEADZONE        30

// ─── LEDC Compatibility (ESP32 Arduino Core v2.x vs v3.x) ───────────────────
void setupLEDC(uint8_t channel, uint32_t freq, uint8_t res, uint8_t pin) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // Core v3.x API
    ledcAttachChannel(pin, freq, res, channel);
#else
    // Core v2.x API
    ledcSetup(channel, freq, res);
    ledcAttachPin(pin, channel);
#endif
}

// ─── Gamepad ─────────────────────────────────────────────────────────────────
GamepadPtr gp = nullptr;

void onConnected(GamepadPtr gamepad) {
    gp = gamepad;
    Serial.println("[BT] Xbox connected");
}
void onDisconnected(GamepadPtr gamepad) {
    gp = nullptr;
    Serial.println("[BT] Xbox disconnected");
}

// ─── Motor State ─────────────────────────────────────────────────────────────
volatile int leftCurrent  = 0;
volatile int rightCurrent = 0;

// ─── Motor Output ────────────────────────────────────────────────────────────
void IRAM_ATTR driveMotor(int ch, int pinA, int pinB, int pwm, bool invert) {
    if (invert) pwm = -pwm;
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
    ledcWrite(ch, constrain(pwm, 0, MAX_PWM));
}

// ─── Battery Compensation ────────────────────────────────────────────────────
float voltageCompensation() {
#if ENABLE_BATTERY_MONITOR
    int raw = analogRead(BATT_PIN);
    float v = (raw / 4095.0f) * 3.3f * BATT_DIVIDER;
    v = constrain(v, BATT_EMPTY_V, BATT_FULL_V);
    return BATT_FULL_V / v;
#else
    return 1.0f;  // Disabled — no scaling applied
#endif
}

// ─── Test Mode ───────────────────────────────────────────────────────────────
#if TEST_MODE
void runTestMode() {
    Serial.println("[TEST] Starting motor test...");
    Serial.println("[TEST] Left motor FORWARD 1s");
    driveMotor(LEDC_CH_L, IN1_PIN, IN2_PIN, 180, INVERT_LEFT_MOTOR);
    delay(1000);
    driveMotor(LEDC_CH_L, IN1_PIN, IN2_PIN, 0, false);
    delay(500);

    Serial.println("[TEST] Left motor BACKWARD 1s");
    driveMotor(LEDC_CH_L, IN1_PIN, IN2_PIN, -180, INVERT_LEFT_MOTOR);
    delay(1000);
    driveMotor(LEDC_CH_L, IN1_PIN, IN2_PIN, 0, false);
    delay(500);

    Serial.println("[TEST] Right motor FORWARD 1s");
    driveMotor(LEDC_CH_R, IN3_PIN, IN4_PIN, 180, INVERT_RIGHT_MOTOR);
    delay(1000);
    driveMotor(LEDC_CH_R, IN3_PIN, IN4_PIN, 0, false);
    delay(500);

    Serial.println("[TEST] Right motor BACKWARD 1s");
    driveMotor(LEDC_CH_R, IN3_PIN, IN4_PIN, -180, INVERT_RIGHT_MOTOR);
    delay(1000);
    driveMotor(LEDC_CH_R, IN3_PIN, IN4_PIN, 0, false);

    Serial.println("[TEST] Done. If any motor spun wrong direction,");
    Serial.println("[TEST] set INVERT_LEFT_MOTOR or INVERT_RIGHT_MOTOR to 1.");
}
#endif

// ─── Core 0: Bluetooth Task ──────────────────────────────────────────────────
void bluetoothTask(void* param) {
    for (;;) {
        BP32.update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ─── Core 1: Motor Control Task ──────────────────────────────────────────────
void motorTask(void* param) {
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        int leftTarget = 0, rightTarget = 0;

        if (gp && gp->isConnected()) {
            int ly    = -gp->axisY();          // Left stick Y (forward = positive)
            int rx    =  gp->axisRX();          // Right stick X
            bool turbo = (gp->buttons() & 0x0008) != 0;  // Y button bitmask

            if (abs(ly) < DEADZONE) ly = 0;
            if (abs(rx) < DEADZONE) rx = 0;

            // Tank drive mixing
            int left  = constrain(ly + rx, -STICK_MAX, STICK_MAX);
            int right = constrain(ly - rx, -STICK_MAX, STICK_MAX);

            leftTarget  = map(abs(left),  0, STICK_MAX, 0, MAX_PWM) * (left  < 0 ? -1 : 1);
            rightTarget = map(abs(right), 0, STICK_MAX, 0, MAX_PWM) * (right < 0 ? -1 : 1);

            if (turbo) {
                // Bypass ramp — instant full power
                leftCurrent  = leftTarget;
                rightCurrent = rightTarget;
            } else {
                // Physics-based ramp
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

        float comp   = voltageCompensation();
        int leftOut  = constrain((int)(leftCurrent  * comp), -MAX_PWM, MAX_PWM);
        int rightOut = constrain((int)(rightCurrent * comp), -MAX_PWM, MAX_PWM);

        driveMotor(LEDC_CH_L, IN1_PIN, IN2_PIN, leftOut,  INVERT_LEFT_MOTOR);
        driveMotor(LEDC_CH_R, IN3_PIN, IN4_PIN, rightOut, INVERT_RIGHT_MOTOR);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(RAMP_MS));
    }
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[GHOST] Drive system starting...");

    pinMode(IN1_PIN, OUTPUT); pinMode(IN2_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT); pinMode(IN4_PIN, OUTPUT);

    setupLEDC(LEDC_CH_L, PWM_FREQ, PWM_RES, ENA_PIN);
    setupLEDC(LEDC_CH_R, PWM_FREQ, PWM_RES, ENB_PIN);

    driveMotor(LEDC_CH_L, IN1_PIN, IN2_PIN, 0, false);
    driveMotor(LEDC_CH_R, IN3_PIN, IN4_PIN, 0, false);

#if TEST_MODE
    runTestMode();
    Serial.println("[TEST] Entering normal drive mode...");
#endif

    BP32.setup(&onConnected, &onDisconnected);
    BP32.forgetBluetoothKeys();

    xTaskCreatePinnedToCore(bluetoothTask, "BT",    4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(motorTask,     "Motor", 4096, NULL, 2, NULL, 1);

    Serial.println("[GHOST] Ready. Pair Xbox controller now.");
}

void loop() {
    vTaskDelete(NULL);
}

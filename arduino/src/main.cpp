/**
 * THE GHOST ROBOT — Arduino R3 Flipper System
 * ─────────────────────────────────────────────
 * Hardware : 3x IR obstacle sensors + Linear actuator via L298N mini
 * Battery  : 7.4V 2200mAh LiPo (isolated from ESP32 system)
 *
 * ─── CONFIGURATION ───────────────────────────────────────────────────────────
 * Edit the flags below before uploading.
 * No need to touch anything else.
 */

// Set to 1 to run component test on boot (tests actuator + prints IR readings)
#define TEST_MODE           0

// Set to 1 if your IR sensors output HIGH when obstacle detected
// (most common sensors output LOW — leave 0 if unsure)
#define IR_ACTIVE_HIGH      0

// Set to 1 if actuator retracts when it should extend (swaps IN1/IN2 logic)
#define INVERT_ACTUATOR     0

// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>

// ─── Pin Definitions ─────────────────────────────────────────────────────────
#define IR_LEFT     2    // Hardware INT0  — must stay D2
#define IR_CENTER   3    // Hardware INT1  — must stay D3
#define IR_RIGHT    5    // PCINT21 — Pin Change Interrupt

#define ACT_IN1     7    // PORTD bit 7
#define ACT_IN2     8    // PORTB bit 0

// ─── Timing ──────────────────────────────────────────────────────────────────
#define STARTUP_DELAY_MS    5000
#define CALIBRATION_SAMPLES 100
#define EXTEND_MS           600
#define RETRACT_MS          600
#define COOLDOWN_MS         2500
#define LOOP_DELAY_MS       5

// ─── IR Trigger Edge ─────────────────────────────────────────────────────────
#if IR_ACTIVE_HIGH
  #define IR_EDGE RISING
  #define IR_DETECTED(pin) (digitalRead(pin) == HIGH)
#else
  #define IR_EDGE FALLING
  #define IR_DETECTED(pin) (digitalRead(pin) == LOW)
#endif

// ─── Prediction ──────────────────────────────────────────────────────────────
#define HISTORY_SIZE    5

struct IRSample {
    bool     detected;
    uint32_t ts;
};

static IRSample history[HISTORY_SIZE];
static uint8_t  histIdx = 0;

// ─── Volatile ISR Flags ───────────────────────────────────────────────────────
volatile bool flagLeft   = false;
volatile bool flagCenter = false;
volatile bool flagRight  = false;

// ─── State ───────────────────────────────────────────────────────────────────
static bool     extended    = false;
static uint32_t lastTrigger = 0;

// ─── AVR Assembly: Actuator Trigger ──────────────────────────────────────────
// Executes in 2–3 clock cycles = ~187ns at 16MHz
// D7 = PORTD bit 7 (ACT_IN1), D8 = PORTB bit 0 (ACT_IN2)

static inline void __attribute__((always_inline)) asmExtend() {
#if INVERT_ACTUATOR
    asm volatile (
        "cbi %[pd], 7 \n\t"
        "sbi %[pb], 0 \n\t"
        : : [pd] "I" (_SFR_IO_ADDR(PORTD)), [pb] "I" (_SFR_IO_ADDR(PORTB))
    );
#else
    asm volatile (
        "sbi %[pd], 7 \n\t"
        "cbi %[pb], 0 \n\t"
        : : [pd] "I" (_SFR_IO_ADDR(PORTD)), [pb] "I" (_SFR_IO_ADDR(PORTB))
    );
#endif
}

static inline void __attribute__((always_inline)) asmRetract() {
#if INVERT_ACTUATOR
    asm volatile (
        "sbi %[pd], 7 \n\t"
        "cbi %[pb], 0 \n\t"
        : : [pd] "I" (_SFR_IO_ADDR(PORTD)), [pb] "I" (_SFR_IO_ADDR(PORTB))
    );
#else
    asm volatile (
        "cbi %[pd], 7 \n\t"
        "sbi %[pb], 0 \n\t"
        : : [pd] "I" (_SFR_IO_ADDR(PORTD)), [pb] "I" (_SFR_IO_ADDR(PORTB))
    );
#endif
}

static inline void __attribute__((always_inline)) asmStop() {
    asm volatile (
        "cbi %[pd], 7 \n\t"
        "cbi %[pb], 0 \n\t"
        : : [pd] "I" (_SFR_IO_ADDR(PORTD)), [pb] "I" (_SFR_IO_ADDR(PORTB))
    );
}

// ─── ISR Handlers (static — no lambdas, safe on AVR) ─────────────────────────
static void isrLeft()   { flagLeft   = true; }
static void isrCenter() { flagCenter = true; }

// Pin Change ISR — detects falling edge on D5 only
ISR(PCINT2_vect) {
    static uint8_t prev = 0xFF;
    uint8_t curr = PIND;
    // Falling edge on PD5 = D5 went LOW
    if ((prev & (1 << PD5)) && !(curr & (1 << PD5))) {
        flagRight = true;
    }
    prev = curr;
}

// ─── IR Calibration ──────────────────────────────────────────────────────────
void calibrateIR() {
    float sum = 0, sumSq = 0;
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        float v = (float)digitalRead(IR_CENTER);
        sum   += v;
        sumSq += v * v;
        delay(5);
    }
    float mean   = sum / CALIBRATION_SAMPLES;
    float stddev = sqrt((sumSq / CALIBRATION_SAMPLES) - (mean * mean));
    Serial.print("[CAL] mean="); Serial.print(mean, 3);
    Serial.print(" stddev=");    Serial.print(stddev, 3);
    Serial.print(" threshold="); Serial.println(mean - 2.0f * stddev, 3);
}

// ─── IR Approach Prediction ──────────────────────────────────────────────────
int predictFire() {
    int detected = 0;
    uint32_t oldest = UINT32_MAX, newest = 0;
    for (int i = 0; i < HISTORY_SIZE; i++) {
        if (history[i].detected) {
            detected++;
            if (history[i].ts < oldest) oldest = history[i].ts;
            if (history[i].ts > newest) newest = history[i].ts;
        }
    }
    if (detected < 2) return 0;
    uint32_t span = newest - oldest;
    if (span < 150) return 0;
    if (span < 350) return 30;
    return 60;
}

// ─── Actuator Operations ─────────────────────────────────────────────────────
void doExtend() {
    asmExtend();
    delay(EXTEND_MS);
    asmStop();
    extended    = true;
    lastTrigger = millis();
    Serial.println("[ACT] Extended");
}

void doRetract() {
    asmRetract();
    delay(RETRACT_MS);
    asmStop();
    extended = false;
    Serial.println("[ACT] Retracted");
}

// ─── Test Mode ───────────────────────────────────────────────────────────────
#if TEST_MODE
void runTestMode() {
    Serial.println("[TEST] === ACTUATOR TEST ===");
    Serial.println("[TEST] Extending...");
    asmExtend(); delay(EXTEND_MS); asmStop();
    delay(1000);
    Serial.println("[TEST] Retracting...");
    asmRetract(); delay(RETRACT_MS); asmStop();
    delay(500);
    Serial.println("[TEST] If actuator moved wrong direction, set INVERT_ACTUATOR=1");

    Serial.println("[TEST] === IR SENSOR TEST (10 seconds) ===");
    Serial.println("[TEST] Wave hand in front of each sensor to verify:");
    unsigned long start = millis();
    while (millis() - start < 10000) {
        Serial.print("[TEST] L=");
        Serial.print(IR_DETECTED(IR_LEFT)   ? "DETECT" : "clear ");
        Serial.print("  C=");
        Serial.print(IR_DETECTED(IR_CENTER) ? "DETECT" : "clear ");
        Serial.print("  R=");
        Serial.println(IR_DETECTED(IR_RIGHT)  ? "DETECT" : "clear ");
        delay(200);
    }
    Serial.println("[TEST] Done. If sensors show wrong values, set IR_ACTIVE_HIGH=1");
    Serial.println("[TEST] Entering fight mode...");
}
#endif

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[GHOST] Flipper system starting...");

    // Actuator pins
    pinMode(ACT_IN1, OUTPUT);
    pinMode(ACT_IN2, OUTPUT);
    asmStop();

    // IR sensor pins
    pinMode(IR_LEFT,   INPUT);
    pinMode(IR_CENTER, INPUT);
    pinMode(IR_RIGHT,  INPUT);

    // Hardware interrupts with static handlers (no lambda — safe on AVR)
    attachInterrupt(digitalPinToInterrupt(IR_LEFT),   isrLeft,   IR_EDGE);
    attachInterrupt(digitalPinToInterrupt(IR_CENTER), isrCenter, IR_EDGE);

    // Pin Change Interrupt for D5 (PCINT21)
    PCICR  |= (1 << PCIE2);
    PCMSK2 |= (1 << PCINT21);
    sei();

    // Initialize history buffer
    for (int i = 0; i < HISTORY_SIZE; i++) history[i] = {false, 0};

#if TEST_MODE
    runTestMode();
#endif

    // Competition: wait 5s for match start signal
    Serial.println("[GHOST] Waiting 5s for match start...");
    delay(STARTUP_DELAY_MS);

    // Calibrate IR sensors against ambient conditions
    Serial.println("[GHOST] Calibrating IR...");
    calibrateIR();

    // Pre-extend wedge — sits at ground level ready to scoop
    Serial.println("[GHOST] Pre-extending wedge...");
    doExtend();

    Serial.println("[GHOST] Fight!");
}

// ─── Main Loop ───────────────────────────────────────────────────────────────
void loop() {
    // Read and clear flags atomically
    cli();
    bool left   = flagLeft;
    bool center = flagCenter;
    bool right  = flagRight;
    flagLeft = flagCenter = flagRight = false;
    sei();

    // Also poll current state (catches sustained detections between interrupts)
    left   |= IR_DETECTED(IR_LEFT);
    center |= IR_DETECTED(IR_CENTER);
    right  |= IR_DETECTED(IR_RIGHT);

    // Update prediction history
    history[histIdx % HISTORY_SIZE] = {center, millis()};
    histIdx++;

    bool cooldown = (millis() - lastTrigger) > COOLDOWN_MS;

    if (cooldown) {
        if (center || (left && right)) {
            int leadMs = predictFire();
            if (leadMs > 0) delay(leadMs);
            doExtend();
        } else if (left || right) {
            doExtend();
        } else if (extended) {
            doRetract();
        }
    }

    delay(LOOP_DELAY_MS);
}

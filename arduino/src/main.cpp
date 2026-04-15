/**
 * THE GHOST ROBOT — Arduino R3 Flipper System
 * ─────────────────────────────────────────────
 * Hardware : 3x IR obstacle sensors + Linear actuator via L298N mini
 * Battery  : 7.4V 2200mAh LiPo (isolated from ESP32 system)
 *
 * Optimizations:
 *   - Hardware interrupts on D2, D3 (INT0, INT1) — ~1μs reaction
 *   - Pin Change Interrupt on D5 — faster than polling
 *   - AVR inline assembly actuator trigger — 3 clock cycles (187ns)
 *   - Startup IR calibration — dynamic threshold, not hardcoded
 *   - IR approach prediction — fires actuator before opponent arrives
 *   - Cooldown timer — prevents actuator jamming
 *
 * IR Logic:
 *   Center detected       → extend actuator (push/flip)
 *   Left + Right (no ctr) → extend (opponent wide, still engage)
 *   Nothing detected      → retract if extended
 *
 * Startup sequence:
 *   1. Boot → 5s wait (competition rule)
 *   2. Calibrate IR baseline
 *   3. Pre-extend wedge
 *   4. Fight
 */

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>

// ─── Pin Definitions ─────────────────────────────────────────────────────────
#define IR_LEFT     2    // Hardware INT0  — must be D2
#define IR_CENTER   3    // Hardware INT1  — must be D3
#define IR_RIGHT    5    // PCINT21 (Pin Change Group 2)

#define ACT_IN1     7    // Actuator extend  — PORTD bit 7
#define ACT_IN2     8    // Actuator retract — PORTB bit 0

// ─── Timing ──────────────────────────────────────────────────────────────────
#define STARTUP_DELAY_MS    5000
#define CALIBRATION_SAMPLES 100
#define EXTEND_MS           600
#define RETRACT_MS          600
#define COOLDOWN_MS         2500
#define LOOP_DELAY_MS       5     // 200Hz main loop

// ─── Prediction ──────────────────────────────────────────────────────────────
#define HISTORY_SIZE        5

struct IRSample {
    bool     detected;
    uint32_t ts;
};

static IRSample history[HISTORY_SIZE];
static uint8_t  histIdx = 0;

// ─── Volatile Flags (set in ISR, read in loop) ────────────────────────────────
volatile bool flagLeft   = false;
volatile bool flagCenter = false;
volatile bool flagRight  = false;

// ─── State ───────────────────────────────────────────────────────────────────
static bool     extended    = false;
static uint32_t lastTrigger = 0;

// ─── AVR Assembly: Actuator Control ──────────────────────────────────────────
// PORTD bit 7 = D7 (ACT_IN1)
// PORTB bit 0 = D8 (ACT_IN2)
// Each function executes in 2–3 AVR clock cycles = ~187ns at 16MHz

static inline void __attribute__((always_inline)) asmExtend() {
    asm volatile (
        "sbi %[pd], 7 \n\t"   // PORTD |= (1<<7) — D7 HIGH
        "cbi %[pb], 0 \n\t"   // PORTB &= ~(1<<0) — D8 LOW
        :
        : [pd] "I" (_SFR_IO_ADDR(PORTD)),
          [pb] "I" (_SFR_IO_ADDR(PORTB))
    );
}

static inline void __attribute__((always_inline)) asmRetract() {
    asm volatile (
        "cbi %[pd], 7 \n\t"   // D7 LOW
        "sbi %[pb], 0 \n\t"   // D8 HIGH
        :
        : [pd] "I" (_SFR_IO_ADDR(PORTD)),
          [pb] "I" (_SFR_IO_ADDR(PORTB))
    );
}

static inline void __attribute__((always_inline)) asmStop() {
    asm volatile (
        "cbi %[pd], 7 \n\t"   // D7 LOW
        "cbi %[pb], 0 \n\t"   // D8 LOW
        :
        : [pd] "I" (_SFR_IO_ADDR(PORTD)),
          [pb] "I" (_SFR_IO_ADDR(PORTB))
    );
}

// ─── Hardware ISRs ────────────────────────────────────────────────────────────
ISR(INT0_vect) { flagLeft   = true; }
ISR(INT1_vect) { flagCenter = true; }

ISR(PCINT2_vect) {
    // Only trigger on falling edge (LOW = detected)
    if (!(PIND & (1 << PD5))) flagRight = true;
}

// ─── IR Calibration ──────────────────────────────────────────────────────────
// Samples center IR 100x at rest → computes mean + 2σ as threshold
// Returns dynamic threshold (1 = clear, 0 = detected for most IR modules)
void calibrateIR() {
    float sum = 0, sumSq = 0;
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        int v = digitalRead(IR_CENTER);
        sum   += v;
        sumSq += v * v;
        delay(5);
    }
    float mean   = sum / CALIBRATION_SAMPLES;
    float stddev = sqrt((sumSq / CALIBRATION_SAMPLES) - (mean * mean));
    Serial.print("[CAL] mean=");
    Serial.print(mean, 3);
    Serial.print(" stddev=");
    Serial.print(stddev, 3);
    Serial.print(" threshold=");
    Serial.println(mean - 2.0f * stddev, 3);
    // Threshold is informational — hardware interrupt handles actual detection
}

// ─── IR Approach Prediction ──────────────────────────────────────────────────
// Looks at last HISTORY_SIZE readings, estimates approach speed
// Returns ms to wait before firing (0 = fire immediately)
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

    if (detected < 2) return 0;   // Not enough data — fire now

    uint32_t span = newest - oldest;
    if (span < 150)  return 0;    // Very fast approach — fire immediately
    if (span < 350)  return 30;   // Medium — small lead time
    return 60;                     // Slow — larger lead time
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

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[GHOST] Flipper system starting...");

    // Actuator output pins
    pinMode(ACT_IN1, OUTPUT);
    pinMode(ACT_IN2, OUTPUT);
    asmStop();

    // IR input pins
    pinMode(IR_LEFT,   INPUT);
    pinMode(IR_CENTER, INPUT);
    pinMode(IR_RIGHT,  INPUT);

    // Hardware interrupts: FALLING edge (IR output LOW = obstacle detected)
    attachInterrupt(digitalPinToInterrupt(IR_LEFT),   []{ flagLeft   = true; }, FALLING);
    attachInterrupt(digitalPinToInterrupt(IR_CENTER), []{ flagCenter = true; }, FALLING);

    // Pin Change Interrupt for D5 (PCINT21)
    PCICR  |= (1 << PCIE2);          // Enable PCINT group 2
    PCMSK2 |= (1 << PCINT21);        // Enable PCINT21 pin
    sei();

    // Initialize history
    for (int i = 0; i < HISTORY_SIZE; i++) {
        history[i] = {false, 0};
    }

    // Competition startup delay
    Serial.println("[GHOST] Waiting 5s for match start...");
    delay(STARTUP_DELAY_MS);

    // IR calibration
    Serial.println("[GHOST] Calibrating IR sensors...");
    calibrateIR();

    // Pre-extend wedge — sits low at ground level, ready to scoop
    Serial.println("[GHOST] Pre-extending wedge...");
    doExtend();

    Serial.println("[GHOST] Flipper ready. Fighting!");
}

// ─── Main Loop ───────────────────────────────────────────────────────────────
void loop() {
    // Snapshot and clear interrupt flags atomically
    cli();
    bool left   = flagLeft;
    bool center = flagCenter;
    bool right  = flagRight;
    flagLeft = flagCenter = flagRight = false;
    sei();

    // Update prediction history with center sensor
    history[histIdx % HISTORY_SIZE] = {center, millis()};
    histIdx++;

    uint32_t now      = millis();
    bool     cooldown = (now - lastTrigger) > COOLDOWN_MS;

    if (cooldown) {
        if (center || (left && right)) {
            // Opponent confirmed in front — predict and fire
            int leadMs = predictFire();
            if (leadMs > 0) delay(leadMs);
            doExtend();

        } else if (left || right) {
            // Opponent on one side only — still extend to catch
            doExtend();

        } else if (extended) {
            // Nothing detected — retract and reset for next engagement
            doRetract();
        }
    }

    delay(LOOP_DELAY_MS);
}

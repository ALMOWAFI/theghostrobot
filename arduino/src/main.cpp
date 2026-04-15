/**
 * THE GHOST ROBOT — Arduino R3 Flipper System
 * ─────────────────────────────────────────────
 * Hardware : 1x IR obstacle sensor + Linear actuator via L298N mini
 * Battery  : 11.1V 3S LiPo (or buck converter from main battery)
 *
 * IR Sensor → D3 (Hardware INT1)
 * Actuator  → D7 (extend) / D8 (retract) via L298N mini
 *
 * ─── CONFIGURATION ───────────────────────────────────────────────────────────
 */

// Set to 1 to run component test on boot
#define TEST_MODE           0

// Set to 1 if IR sensor outputs HIGH when obstacle detected
// (most sensors output LOW — leave 0 if unsure)
#define IR_ACTIVE_HIGH      0

// Set to 1 if actuator retracts when it should extend
#define INVERT_ACTUATOR     0

// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// ─── Pins ────────────────────────────────────────────────────────────────────
#define IR_PIN      3    // Hardware INT1 — must stay D3
#define ACT_IN1     7    // PORTD bit 7
#define ACT_IN2     8    // PORTB bit 0

// ─── Timing ──────────────────────────────────────────────────────────────────
#define STARTUP_DELAY_MS    5000
#define EXTEND_MS           600
#define RETRACT_MS          600
#define COOLDOWN_MS         2500
#define LOOP_DELAY_MS       5

// ─── IR Config ───────────────────────────────────────────────────────────────
#if IR_ACTIVE_HIGH
  #define IR_EDGE      RISING
  #define IR_DETECTED  (digitalRead(IR_PIN) == HIGH)
#else
  #define IR_EDGE      FALLING
  #define IR_DETECTED  (digitalRead(IR_PIN) == LOW)
#endif

// ─── State ───────────────────────────────────────────────────────────────────
volatile bool   irFlag      = false;
static bool     extended    = false;
static uint32_t lastTrigger = 0;

// ─── AVR Assembly: Actuator ──────────────────────────────────────────────────
// 3 clock cycles = 187ns at 16MHz
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

// ─── ISR ─────────────────────────────────────────────────────────────────────
static void isrIR() { irFlag = true; }

// ─── Actuator ────────────────────────────────────────────────────────────────
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
    Serial.println("[TEST] If wrong direction → set INVERT_ACTUATOR 1");

    Serial.println("[TEST] === IR SENSOR TEST (10 seconds) ===");
    Serial.println("[TEST] Wave hand in front of sensor:");
    unsigned long start = millis();
    while (millis() - start < 10000) {
        Serial.println(IR_DETECTED ? "[TEST] DETECTED" : "[TEST] clear");
        delay(200);
    }
    Serial.println("[TEST] If sensor never triggers → set IR_ACTIVE_HIGH 1");
    Serial.println("[TEST] Entering fight mode...");
}
#endif

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[GHOST] Flipper system starting...");

    pinMode(ACT_IN1, OUTPUT);
    pinMode(ACT_IN2, OUTPUT);
    asmStop();

    pinMode(IR_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(IR_PIN), isrIR, IR_EDGE);

#if TEST_MODE
    runTestMode();
#endif

    Serial.println("[GHOST] Waiting 5s...");
    delay(STARTUP_DELAY_MS);

    Serial.println("[GHOST] Pre-extending wedge...");
    doExtend();

    Serial.println("[GHOST] Fight!");
}

// ─── Main Loop ───────────────────────────────────────────────────────────────
void loop() {
    // Read and clear flag atomically
    cli();
    bool triggered = irFlag;
    irFlag = false;
    sei();

    // Also poll (catches sustained detection between interrupts)
    triggered |= IR_DETECTED;

    bool cooldown = (millis() - lastTrigger) > COOLDOWN_MS;

    if (triggered && cooldown) {
        doExtend();
    } else if (!triggered && extended && cooldown) {
        doRetract();
    }

    delay(LOOP_DELAY_MS);
}

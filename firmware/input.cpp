#include "input.h"

//  Кнопки — состояние

struct BtnState {
    uint8_t  pin;
    bool     stable;        // текущее стабильное состояние (нажата = true)
    bool     lastRead;      // последнее сырое чтение
    uint32_t lastChange;    // время последнего изменения (для дебаунса)
    uint32_t pressStart;    // когда началось нажатие
    bool     longFired;     // долгое уже сработало
    bool     waitingDouble; // ждём второе нажатие
    uint32_t firstRelease;  // время первого отпускания
};

static BtnState btns[NUM_BUTTONS];
static const uint8_t BTN_PINS[NUM_BUTTONS] = {
    PIN_BTN1, PIN_BTN2, PIN_BTN3, PIN_BTN4, PIN_BTN5
};

// Очередь событий (кольцевая)
static ButtonEvent eventQueue[8];
static uint8_t eventHead = 0;
static uint8_t eventTail = 0;

static void push_event(uint8_t btn, PressType type) {
    uint8_t next = (eventHead + 1) % 8;
    if (next == eventTail) return; // переполнение — теряем
    eventQueue[eventHead] = { btn, type, true };
    eventHead = next;
}

ButtonEvent input_get_event() {
    if (eventHead == eventTail) return { 0, PRESS_SHORT, false };
    ButtonEvent e = eventQueue[eventTail];
    eventTail = (eventTail + 1) % 8;
    return e;
}

//  LED

static const uint8_t LED_PINS[NUM_LEDS] = {
    PIN_LED_WHITE, PIN_LED_BLUE, PIN_LED_GREEN, PIN_LED_YELLOW, PIN_LED_RED
};

static bool     ledState[NUM_LEDS] = { false };
static bool     ledLocked = false;        // если true — led_update_* игнорируются
static uint32_t flashUntil = 0;
static uint32_t blinkUntil = 0;
static bool     blinkState = false;
static uint32_t blinkLast = 0;

//  Инициализация

void input_init() {
    // Кнопки
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BTN_PINS[i], INPUT_PULLUP);
        btns[i].pin          = BTN_PINS[i];
        btns[i].stable       = false;
        btns[i].lastRead     = false;
        btns[i].lastChange   = 0;
        btns[i].pressStart   = 0;
        btns[i].longFired    = false;
        btns[i].waitingDouble= false;
        btns[i].firstRelease = 0;
    }

    // LED
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
        ledState[i] = false;
    }

    Serial.println("[INPUT] buttons + LEDs initialized");
}

//  Обновление кнопок

void input_update() {
    uint32_t now = millis();

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        BtnState& b = btns[i];

        // Чтение с подтяжкой (нажата = LOW)
        bool raw = (digitalRead(b.pin) == LOW);

        // Дебаунс
        if (raw != b.lastRead) {
            b.lastChange = now;
            b.lastRead   = raw;
        }

        if ((now - b.lastChange) > BTN_DEBOUNCE_MS && raw != b.stable) {
            b.stable = raw;

            if (b.stable) {
                //  НАЖАТИЕ 
                b.pressStart = now;
                b.longFired  = false;

                // Если ждём второе — это двойное
                if (b.waitingDouble) {
                    push_event(i, PRESS_DOUBLE);
                    b.waitingDouble = false;
                }
            } else {
                //  ОТПУСКАНИЕ 
                uint32_t held = now - b.pressStart;

                if (b.longFired) {
                    // долгое уже сработало — ничего
                    b.waitingDouble = false;
                } else if (held < BTN_LONG_MS) {
                    // было короткое нажатие
                    if (b.waitingDouble) {
                        // уже ждали — значит это второе, но мы уже обработали в нажатии
                        // сюда попадаем после двойного, ничего не делаем
                        b.waitingDouble = false;
                    } else {
                        // начинаем ждать второе
                        b.waitingDouble = true;
                        b.firstRelease  = now;
                    }
                } else {
                    // долгое уже отработало выше, тут сброс
                    b.waitingDouble = false;
                }
            }
        }

        //  Долгое нажатие (пока держим) 
        if (b.stable && !b.longFired && (now - b.pressStart) >= BTN_LONG_MS) {
            push_event(i, PRESS_LONG);
            b.longFired     = true;
            b.waitingDouble = false;
        }

        //  Таймаут двойного 
        if (b.waitingDouble && (now - b.firstRelease) > BTN_DOUBLE_MS) {
            // не дождались второго — это было короткое
            push_event(i, PRESS_SHORT);
            b.waitingDouble = false;
        }
    }

    led_tick();
}

//  LED — базовое управление

void led_set_raw(uint8_t pin, bool on) {
    digitalWrite(pin, on ? HIGH : LOW);
}

void led_all_off() {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        digitalWrite(LED_PINS[i], LOW);
        ledState[i] = false;
    }
    ledLocked = false;
}

void led_set(uint8_t idx, bool on) {
    if (idx >= NUM_LEDS) return;
    digitalWrite(LED_PINS[idx], on ? HIGH : LOW);
    ledState[idx] = on;
}

//  LED — накопительный режим (5 ступеней по 20%)

// Порядок зажигания: белый → синий → зелёный → жёлтый → красный
void led_update_accumulative(uint8_t percent) {
    if (ledLocked) return;
    if (percent > 100) percent = 100;

    // Сколько LED горит (0..5)
    // 0–20%   → 1 (белый)
    // 21–40%  → 2
    // 41–60%  → 3
    // 61–80%  → 4
    // 81–100% → 5
    uint8_t on = (percent + 19) / 20;
    if (percent == 0) on = 0;
    if (on > 5) on = 5;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        bool want = (i < on);
        if (ledState[i] != want) {
            digitalWrite(LED_PINS[i], want ? HIGH : LOW);
            ledState[i] = want;
        }
    }
}

//  LED — один горит (для Пинг/Утечка/Климат)

void led_update_single(uint8_t idx) {
    if (ledLocked) return;
    if (idx >= NUM_LEDS) {
        led_all_off();
        return;
    }

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        bool want = (i == idx);
        if (ledState[i] != want) {
            digitalWrite(LED_PINS[i], want ? HIGH : LOW);
            ledState[i] = want;
        }
    }
}

//  LED — вспышка белым (нотификация)

void led_flash_white(uint16_t duration_ms) {
    ledLocked = true;
    led_all_off();
    digitalWrite(PIN_LED_WHITE, HIGH);
    flashUntil = millis() + duration_ms;
}

//  LED — мигание красным (подтверждение питания)

void led_blink_red(uint16_t duration_ms) {
    ledLocked = true;
    led_all_off();
    blinkUntil = millis() + duration_ms;
    blinkLast  = 0;
    blinkState = false;
}

//  LED — анимации (вызывается из input_update)

void led_tick() {
    uint32_t now = millis();

    // Вспышка белым
    if (flashUntil > 0 && now >= flashUntil) {
        digitalWrite(PIN_LED_WHITE, LOW);
        flashUntil = 0;
        ledLocked  = false;
    }

    // Мигание красным (1 раз в 500 мс)
    if (blinkUntil > 0) {
        if (now >= blinkUntil) {
            digitalWrite(PIN_LED_RED, LOW);
            blinkUntil = 0;
            ledLocked  = false;
        } else if (now - blinkLast > 500) {
            blinkState = !blinkState;
            digitalWrite(PIN_LED_RED, blinkState ? HIGH : LOW);
            blinkLast = now;
        }
    }
}

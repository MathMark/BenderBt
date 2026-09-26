#include "core0.h"

#include <EEManager.h>
#include <EncButton.h>
#include <FastLED.h>
#include <GyverMAX7219.h>
#include <VolAnalyzer.h>
#include <esp_a2dp_api.h>

#include "btAudio.h"
#include "tmr.h"

#define VOL_MAX 22

MAX7219<5, 1, MTRX_CS, MTRX_DAT, MTRX_CLK> mtrx;
Tmr square_tmr;
Data data;
EEManager memory(data);
extern btAudio btaudio;

volatile bool bt_connected = false;

/*
 * A2DP callback: следит за состоянием подключения телефона.
 */
void a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {
    if (event == ESP_A2D_CONNECTION_STATE_EVT) {
        bt_connected = (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED);
        Serial.printf("[BT] connected = %d\n", bt_connected);
    }
}

// ========================= MATRIX =========================
void upd_bright() {
    uint8_t m = data.bright_mouth, e = data.bright_eyes;
    uint8_t br[] = {m, m, m, e, e};
    mtrx.setBright(br);
}

void draw_vol(uint8_t v, uint8_t vmax) {
    mtrx.rect(0, 0, ANALYZ_WIDTH - 1, 7, GFX_CLEAR);

    mtrx.lineH(2, 0, ANALYZ_WIDTH - 1, GFX_FILL);
    mtrx.lineH(5, 0, ANALYZ_WIDTH - 1, GFX_FILL);
    mtrx.lineV(0, 2, 5, GFX_FILL);
    mtrx.lineV(ANALYZ_WIDTH - 1, 2, 5, GFX_FILL);

    uint8_t inner = ANALYZ_WIDTH - 2;
    uint8_t pos = 1 + (uint32_t)v * (inner - 1) / vmax;
    mtrx.lineV(pos, 3, 4, GFX_FILL);

    mtrx.update();
}

// ========================= EYES =========================
void draw_eye(uint8_t i) {
    uint8_t x = ANALYZ_WIDTH + i * 8;
    mtrx.rect(1 + x, 1, 6 + x, 6, GFX_FILL);
    mtrx.lineV(0 + x, 2, 5);
    mtrx.lineV(7 + x, 2, 5);
    mtrx.lineH(0, 2 + x, 5 + x);
    mtrx.lineH(7, 2 + x, 5 + x);
}

void draw_eyeb(uint8_t i, int x, int y, int w = 2) {
    x += ANALYZ_WIDTH + i * 8;
    mtrx.rect(x, y, x + w - 1, y + w - 1, GFX_CLEAR);
}

// Веки поверх нарисованного глаза. phase: 1 — чуть, 3 — почти закрыт.
static void draw_lids(uint8_t phase) {
    if (!phase) return;
    uint8_t x0 = ANALYZ_WIDTH, x1 = ANALYZ_WIDTH + 15;

    mtrx.lineH(0, x0, x1, GFX_CLEAR);
    mtrx.lineH(7, x0, x1, GFX_CLEAR);
    if (phase >= 2) {
        mtrx.lineH(1, x0, x1, GFX_CLEAR);
        mtrx.lineH(6, x0, x1, GFX_CLEAR);
    }
    if (phase >= 3) {
        mtrx.lineH(2, x0, x1, GFX_CLEAR);
        mtrx.lineH(5, x0, x1, GFX_CLEAR);
    }
}

void anim_search() {
    static int8_t pos = 4, dir = 1;
    static Tmr tmr(50);
    if (!tmr) return;

    pos += dir;
    if (pos >= 6) dir = -1;
    if (pos <= 0) dir = 1;

    mtrx.rect(ANALYZ_WIDTH, 0, ANALYZ_WIDTH + 15, 7, GFX_CLEAR);
    mtrx.rect(ANALYZ_WIDTH, 2, ANALYZ_WIDTH + 15, 5, GFX_FILL);
    draw_eyeb(0, pos, 3);
    draw_eyeb(1, pos, 3);
    mtrx.update();
}

void change_state() {
    mtrx.clear();
    if (data.state) {
        upd_bright();
        square_tmr.start(600);
        draw_eye(0);
        draw_eye(1);
        draw_eyeb(0, 2, 2, 4);
        draw_eyeb(1, 2, 2, 4);
    } else {
        mtrx.setBright((uint8_t)0);
        draw_eye(0);
        draw_eye(1);
        mtrx.rect(ANALYZ_WIDTH, 0, ANALYZ_WIDTH + 15, 3, GFX_CLEAR);
        draw_eyeb(0, 3, 5);
        draw_eyeb(1, 3, 5);
    }
    mtrx.update();
}

static uint32_t calcBlinkNext() {
    return millis() + random(3000, 8000);
}

// ========================= ANALYZ =========================
void analyz0(uint8_t vol) {
    static uint16_t offs;
    offs += 20 * vol / 100;
    for (uint8_t i = 0; i < ANALYZ_WIDTH; i++) {
        int16_t val = inoise8(i * 50, offs);
        val -= 128;
        val = val * vol / 100;
        val += 128;
        val = map(val, 45, 255 - 45, 0, 7);
        mtrx.dot(i, val);
    }
}

// ========================= ENCODER =========================
static bool handle_encoder(EncButton& eb, VolAnalyzer& sound,
                           Tmr& angry_tmr, Tmr& matrix_tmr) {
    if (!eb.tick()) return false;

    if (eb.turn()) {
        if (eb.pressing()) {
            switch (eb.getClicks()) {
                case 0:
                    data.bright_mouth = constrain(data.bright_mouth + eb.dir(), 0, 16);
                    upd_bright();
                    break;
                case 1:
                    data.bright_eyes = constrain(data.bright_eyes + eb.dir(), 0, 16);
                    upd_bright();
                    break;
            }
        } else if (data.state) {
            angry_tmr.start();
            data.vol = constrain(data.vol + eb.dir(), 0, VOL_MAX);
            btaudio.volume(data.vol / (float)VOL_MAX);
            draw_vol(data.vol, VOL_MAX);
            matrix_tmr.start();
        }
    }

    if (eb.hasClicks()) {
        switch (eb.getClicks()) {
            case 1:
                data.state = !data.state;
                matrix_tmr.stop();                     // убрать шкалу громкости
                btaudio.volume(data.state ? data.vol / (float)VOL_MAX : 0.0);
                change_state();
                break;
            case 2:                                    // калибровка порога
                data.trsh = sound.getMax() * 2 / 3;
                sound.setTrsh(data.trsh);
                Serial.printf("[cal] trsh = %u\n", data.trsh);
                break;
        }
    }

    memory.update();
    return true;
}

// ========================= CORE 0 =========================
void core0(void* p) {
    // ---------- SETUP ----------
    EncButton eb(ENC_S1, ENC_S2, ENC_BTN);
    VolAnalyzer sound(ANALYZ_PIN);
    sound.setAmpliDt(300);
    //sound.setTrsh(data.trsh);
    sound.setTrsh(400);
    sound.setPulseMin(40);
    sound.setPulseMax(80);

    Tmr eye_tmr(80);
    Tmr matrix_tmr(1000);
    Tmr angry_tmr(800);
    Tmr blink_step(40);
    square_tmr.timerMode(1);
    matrix_tmr.timerMode(1);
    angry_tmr.timerMode(1);
    bool pulse = 0;

    EEPROM.begin(memory.blockSize());
    memory.begin(0, 'b');
    data.mode         = constrain(data.mode, 0, 1);
    data.vol          = constrain(data.vol, 0, VOL_MAX);
    data.bright_mouth = constrain(data.bright_mouth, 0, 16);
    data.bright_eyes  = constrain(data.bright_eyes, 0, 16);

    mtrx.begin();
    upd_bright();
    mtrx.clear();
    mtrx.update();

    Serial.println(F("[BT] starting..."));
    btaudio.begin();
    btaudio.I2S(I2S_BCLK, I2S_DOUT, I2S_LRC);
    esp_a2d_register_callback(a2dp_cb);        // строго после begin()
    btaudio.reconnect();
    btaudio.volume(data.state ? data.vol / (float)VOL_MAX : 0.0);
    Serial.println(F("[BT] ready, waiting for phone"));

    bool was_connected = false;
    bool mouth_cleared = false;
    uint32_t blink_next = calcBlinkNext();
    int8_t blink_phase = 0;

    // ---------- LOOP ----------
    for (;;) {
        square_tmr.tick();
        matrix_tmr.tick();
        angry_tmr.tick();
        memory.tick();

        // ----- смена состояния подключения -----
        if (bt_connected != was_connected) {
            was_connected = bt_connected;
            mtrx.clear();
            if (bt_connected) change_state();
            mtrx.update();
        }

        // ----- отладка АЦП -----
        static uint32_t adc_dbg;
        if (millis() - adc_dbg > 500) {
            adc_dbg = millis();
            uint16_t mn = 4095, mx = 0;
            for (uint8_t i = 0; i < 64; i++) {
                uint16_t val = analogRead(ANALYZ_PIN);
                if (val < mn) mn = val;
                if (val > mx) mx = val;
            }
            Serial.printf("[adc] min=%u max=%u span=%u | raw=%u vol=%u max=%u trsh=%u\n",
                          mn, mx, mx - mn,
                          sound.getRaw(), sound.getVol(),
                          sound.getMax(), sound.getTrsh());
        }

        // ----- глаза -----
        if (!bt_connected) {
            anim_search();
        } else if (data.state && !square_tmr.state() && eye_tmr) {
            draw_eye(0);
            draw_eye(1);

            if (angry_tmr.state()) {
                draw_eyeb(0, 3, 3);
                draw_eyeb(1, 3, 3);
                mtrx.lineH(0, ANALYZ_WIDTH, ANALYZ_WIDTH + 15, GFX_CLEAR);
                mtrx.lineH(1, ANALYZ_WIDTH + 5, ANALYZ_WIDTH + 10, GFX_CLEAR);
                mtrx.lineH(2, ANALYZ_WIDTH + 6, ANALYZ_WIDTH + 9, GFX_CLEAR);
                mtrx.lineH(3, ANALYZ_WIDTH + 7, ANALYZ_WIDTH + 8, GFX_CLEAR);
            } else if (eb.pressing()) {
                draw_eyeb(0, 4, 3, 3);
                draw_eyeb(1, 1, 3, 3);
            } else {
                static uint16_t pos;
                pos += 15;
                uint8_t x = inoise8(pos);
                uint8_t y = inoise8(pos + UINT16_MAX / 4);
                x = map(constrain(x, 40, 215), 40, 215, 2, 5);
                y = map(constrain(y, 40, 215), 40, 215, 2, 5);

                if (pulse) {
                    pulse = 0;
                    draw_eyeb(0, x + random(-1, 1), y + random(-1, 2), 3);
                    draw_eyeb(1, x + random(-1, 1), y + random(-1, 2), 3);
                } else {
                    draw_eyeb(0, x, y);
                    draw_eyeb(1, x, y);
                }
            }

            // ----- моргание -----
            if (!blink_phase && millis() > blink_next) blink_phase = 1;
            if (blink_phase) {
                if (blink_step) {
                    blink_phase++;
                    if (blink_phase > 6) {
                        blink_phase = 0;
                        blink_next = calcBlinkNext();
                    }
                }
                draw_lids((blink_phase <= 3) ? blink_phase : (7 - blink_phase));
            }

            mtrx.update();
        }

        // ----- рот -----
        bool snd = sound.tick();
        if (snd && sound.pulse()) pulse = 1;

        if (matrix_tmr.state()) {
            mouth_cleared = false;                     // шкалу рисует энкодер
        } else if (bt_connected && data.state) {
            mouth_cleared = false;
            if (snd) {
                mtrx.rect(0, 0, ANALYZ_WIDTH - 1, 7, GFX_CLEAR);
                analyz0(sound.getVol());
                mtrx.update();
            }
        } else if (!mouth_cleared) {
            mtrx.rect(0, 0, ANALYZ_WIDTH - 1, 7, GFX_CLEAR);
            mtrx.update();
            mouth_cleared = true;
        }

        handle_encoder(eb, sound, angry_tmr, matrix_tmr);
        vTaskDelay(1);
    }
}
#include "core/touch_pad.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace input::touch {

namespace {

std::mutex g_lock;
bool g_active = false;
float g_lx = 0, g_ly = 0, g_rx = 0, g_ry = 0;
uint16_t g_buttons = 0;
uint8_t g_analog[8] = {};

float Clamp(float v) { return std::max(-1.0f, std::min(1.0f, v)); }

int16_t Axis(float v) {
    v = Clamp(v);
    // avoid the -32768/32767 asymmetry at the extremes
    return int16_t(std::lround(v * 32767.0f));
}

}  // namespace

void SetLeftStick(float x, float y) {
    std::lock_guard lock(g_lock);
    g_lx = Clamp(x);
    g_ly = Clamp(y);
    g_active = true;
}

void SetRightStick(float x, float y) {
    std::lock_guard lock(g_lock);
    g_rx = Clamp(x);
    g_ry = Clamp(y);
    g_active = true;
}

void SetButton(int index, bool down) {
    std::lock_guard lock(g_lock);
    g_active = true;
    switch (index) {
        case 0: case 1: case 2: case 3: case 4: case 5:
            g_analog[index] = down ? 0xFF : 0;
            break;
        case 6:
            if (down) g_buttons |= 1u << 4; else g_buttons &= ~(1u << 4);
            break;
        case 7:
            if (down) g_buttons |= 1u << 5; else g_buttons &= ~(1u << 5);
            break;
        default:
            break;
    }
}

void SetDpad(int index, bool down) {
    if (index < 0 || index > 3) return;
    std::lock_guard lock(g_lock);
    g_active = true;
    if (down) g_buttons |= uint16_t(1u << index); else g_buttons &= ~uint16_t(1u << index);
}

void SetThumb(int index, bool down) {
    if (index < 0 || index > 1) return;
    std::lock_guard lock(g_lock);
    g_active = true;
    const uint16_t bit = uint16_t(1u << (6 + index));
    if (down) g_buttons |= bit; else g_buttons &= ~bit;
}

void SetTrigger(int index, float value) {
    if (index < 0 || index > 1) return;
    std::lock_guard lock(g_lock);
    g_active = true;
    g_analog[6 + index] = uint8_t(std::lround(Clamp(value) * 255.0f));
}

void ReleaseAll() {
    std::lock_guard lock(g_lock);
    g_active = false;
    g_lx = g_ly = g_rx = g_ry = 0;
    g_buttons = 0;
    std::fill(std::begin(g_analog), std::end(g_analog), 0);
}

bool Active() {
    std::lock_guard lock(g_lock);
    return g_active;
}

Pad Held() {
    std::lock_guard lock(g_lock);
    Pad p;
    p.connected = g_active;
    p.buttons = g_buttons;
    for (int i = 0; i < 8; i++) p.analog[i] = g_analog[i];
    p.lx = Axis(g_lx);
    p.ly = Axis(g_ly);
    p.rx = Axis(g_rx);
    p.ry = Axis(g_ry);
    return p;
}

}  // namespace input::touch

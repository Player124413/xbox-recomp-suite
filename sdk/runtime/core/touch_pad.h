#pragma once
// A virtual Xbox controller fed by the Android touch overlay. The Java side
// calls these through JNI; input::Poll merges the state into controller 0 so
// the guest's XInput reads see it exactly like a physical pad.
#include "core/input.h"

namespace input::touch {

// -1..1, y up positive like the console's stick report
void SetLeftStick(float x, float y);
void SetRightStick(float x, float y);
// 0 A, 1 B, 2 X, 3 Y, 4 Black (L1), 5 White (R1), 6 Start, 7 Back
void SetButton(int index, bool down);
// 0 up, 1 down, 2 left, 3 right
void SetDpad(int index, bool down);
// 0 left stick click, 1 right stick click
void SetThumb(int index, bool down);
// 0 left trigger, 1 right trigger, 0..1
void SetTrigger(int index, float value);
void ReleaseAll();
bool Active();
Pad Held();

}  // namespace input::touch

// JNI bridge between the Android touch overlay and the runtime's virtual
// controller. The Java side never touches guest memory; it only publishes a
// small input state that input::Poll merges into controller 0.
#include <jni.h>

#include "core/touch_pad.h"

namespace {

inline bool Truthy(jboolean value) { return value != JNI_FALSE; }

}  // namespace

extern "C" {

JNIEXPORT void JNICALL
Java_org_xberecomp_TouchInput_setLeftStick(JNIEnv*, jclass, jfloat x, jfloat y) {
    input::touch::SetLeftStick(x, y);
}

JNIEXPORT void JNICALL
Java_org_xberecomp_TouchInput_setRightStick(JNIEnv*, jclass, jfloat x, jfloat y) {
    input::touch::SetRightStick(x, y);
}

JNIEXPORT void JNICALL
Java_org_xberecomp_TouchInput_setButton(JNIEnv*, jclass, jint index, jboolean down) {
    input::touch::SetButton(int(index), Truthy(down));
}

JNIEXPORT void JNICALL
Java_org_xberecomp_TouchInput_setDpad(JNIEnv*, jclass, jint index, jboolean down) {
    input::touch::SetDpad(int(index), Truthy(down));
}

JNIEXPORT void JNICALL
Java_org_xberecomp_TouchInput_setThumb(JNIEnv*, jclass, jint index, jboolean down) {
    input::touch::SetThumb(int(index), Truthy(down));
}

JNIEXPORT void JNICALL
Java_org_xberecomp_TouchInput_setTrigger(JNIEnv*, jclass, jint index, jfloat value) {
    input::touch::SetTrigger(int(index), value);
}

JNIEXPORT void JNICALL
Java_org_xberecomp_TouchInput_releaseAll(JNIEnv*, jclass) {
    input::touch::ReleaseAll();
}

}  // extern "C"

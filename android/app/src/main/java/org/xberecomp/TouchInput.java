package org.xberecomp;

/**
 * JNI bridge to the virtual Xbox controller in the native runtime.
 *
 * The overlay calls these on the UI thread; the native side takes a short
 * mutex and the guest's XInput reads see the state on its next poll. No game
 * data or input state is stored in Java.
 */
public final class TouchInput {
    static {
        // SDLActivity has already loaded the runtime; this keeps the bridge
        // usable if the overlay is created before that in a future change.
        try {
            System.loadLibrary("main");
        } catch (UnsatisfiedLinkError alreadyLoaded) {
            // The library is loaded by SDLActivity; ignore.
        }
    }

    private TouchInput() {}

    public static native void setLeftStick(float x, float y);
    public static native void setRightStick(float x, float y);
    /** 0 A, 1 B, 2 X, 3 Y, 4 Black (L1), 5 White (R1), 6 Start, 7 Back. */
    public static native void setButton(int index, boolean down);
    /** 0 up, 1 down, 2 left, 3 right. */
    public static native void setDpad(int index, boolean down);
    /** 0 left stick click, 1 right stick click. */
    public static native void setThumb(int index, boolean down);
    /** 0 left trigger, 1 right trigger, 0..1. */
    public static native void setTrigger(int index, float value);
    public static native void releaseAll();
}

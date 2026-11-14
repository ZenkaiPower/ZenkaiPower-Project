package org.libsdl.app;

/** Access SDL's Android joystick driver without changing its generated sources. */
public final class TouchPadBridge {
    private static final int DEVICE = 0x5a504144;
    private static boolean attached;

    public static void attach() {
        if (attached) return;
        SDLControllerManager.nativeAddJoystick(DEVICE, "ZenkaiPower Touch Pad", "zenkai-touch-pad",
                0, 0, 0x7fff, 6, 0x3f, 0, false, false, false, false);
        attached = true;
    }

    public static void button(int key, boolean down) {
        attach();
        if (down) SDLControllerManager.onNativePadDown(DEVICE, key, 0);
        else SDLControllerManager.onNativePadUp(DEVICE, key, 0);
    }

    public static void axis(int axis, float value) {
        attach();
        SDLControllerManager.onNativeJoy(DEVICE, axis, value);
    }
}

package com.zenkaipower.android;

import android.content.Context;
import android.os.SystemClock;
import android.os.VibrationEffect;
import android.os.Vibrator;

/** Bounded gameplay rumble, updated on the UI thread by the virtual gamepad. */
final class DeviceVibration {
    private final Vibrator vibrator;
    private int amplitude;
    private long sentAt;

    DeviceVibration(Context context) {
        vibrator = (Vibrator)context.getSystemService(Context.VIBRATOR_SERVICE);
    }

    void update(int requested) {
        if (vibrator == null || !vibrator.hasVibrator()) return;
        requested = Math.max(0, Math.min(255, requested));
        long now = SystemClock.uptimeMillis();
        if (requested == 0) {
            if (amplitude != 0) vibrator.cancel();
            amplitude = 0;
            return;
        }
        // Refresh before the short effect expires; avoid a Binder call per frame.
        if (requested == amplitude && now - sentAt < 96) return;
        int level = vibrator.hasAmplitudeControl() ? requested : VibrationEffect.DEFAULT_AMPLITUDE;
        vibrator.vibrate(VibrationEffect.createOneShot(160, level));
        amplitude = requested;
        sentAt = now;
    }
}

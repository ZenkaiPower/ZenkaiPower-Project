package com.zenkaipower.android;

import android.content.Intent;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import java.io.File;
import java.io.IOException;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** JNI adapter for the shared desktop installer; images are read in place. */
final class DiscInstaller {
    private static final Object PREPARE_LOCK = new Object();
    static {
        System.loadLibrary("SDL3");
        org.libsdl.app.SDL.setupJNI();
        System.loadLibrary("installer");
    }
    private final LauncherActivity activity;
    private final File root;
    private volatile boolean cancelled;
    private native void start(String root, String image, int disc, String cache);
    private native String poll();
    private native boolean running();
    private native void cancel();
    private native void release();
    native boolean installed(String root, int disc);
    DiscInstaller(LauncherActivity activity, File root) {
        this.activity = activity; this.root = root;
        org.libsdl.app.SDL.setContext(activity);
    }
    void stop() { cancelled = true; cancel(); }
    void install(ParcelFileDescriptor image, int disc) throws IOException {
        String failure = null; boolean done = false;
        try (ParcelFileDescriptor fd = image;
                FileOutputStream log = GameIO.output(GameIO.file(root, "logs/setup-android.log"), true)) {
            if (cancelled) throw new IOException("Installation cancelled.");
            start(root.getAbsolutePath(), "/proc/self/fd/" + fd.getFd(), disc,
                    activity.getCacheDir().getAbsolutePath());
            for (;;) {
                if (cancelled) cancel();
                String line = poll();
                if (line == null) {
                    if (!running()) { line = poll(); if (line == null) break; }
                    else { SystemClock.sleep(100); continue; }
                }
                activity.installProgress(line);
                log.write((line + "\n").getBytes(StandardCharsets.UTF_8));
                if (line.startsWith("@done ")) done = true;
                if (line.startsWith("@stopped ")) failure = line.substring(9).replace("\\n", "\n");
            }
        } finally { release(); }
        if (cancelled) throw new IOException("Installation cancelled.");
        if (!done) throw new IOException(failure == null ? "Installation did not complete." : failure);
        GameFiles.packResources(root);
    }
    // Called on the native extraction thread; preparation runs in an isolated process.
    @SuppressWarnings("unused") boolean prepareText() {
        synchronized (PREPARE_LOCK) { return prepareTextLocked(); }
    }
    private void waitForPreparationExit() {
        android.app.ActivityManager manager = (android.app.ActivityManager)activity.getSystemService(android.content.Context.ACTIVITY_SERVICE);
        long deadline = SystemClock.elapsedRealtime() + 5000;
        while (SystemClock.elapsedRealtime() < deadline) {
            boolean running = false;
            java.util.List<android.app.ActivityManager.RunningAppProcessInfo> processes = manager.getRunningAppProcesses();
            if (processes != null) for (android.app.ActivityManager.RunningAppProcessInfo process : processes)
                running |= process.processName.equals(activity.getPackageName() + ":prepare");
            if (!running) return;
            SystemClock.sleep(50);
        }
    }
    private boolean prepareTextLocked() {
        File result = GameIO.file(activity.getCacheDir(), "engine/text-result-" + java.util.UUID.randomUUID());
        try {
            Files.deleteIfExists(result.toPath());
            if (cancelled) return false;
            activity.runOnUiThread(() -> {
                if (!cancelled) activity.startActivity(new Intent(activity, TextPreparationActivity.class)
                        .putExtra("gameRoot", root.getAbsolutePath()).putExtra("prepareText", true)
                        .putExtra("prepareResult", result.getAbsolutePath()));
            });
            long deadline = SystemClock.elapsedRealtime() + 10 * 60 * 1000;
            while (SystemClock.elapsedRealtime() < deadline) {
                if (result.isFile()) {
                    String code = new String(Files.readAllBytes(result.toPath()), StandardCharsets.UTF_8).trim();
                    waitForPreparationExit();
                    Files.deleteIfExists(result.toPath());
                    returnToInstaller();
                    return !cancelled && code.equals("0");
                }
                if (cancelled) break;
                SystemClock.sleep(200);
            }
        } catch (IOException error) { activity.installProgress("@note " + error.getMessage()); }
        activity.sendBroadcast(new Intent(GameActivity.CANCEL_PREPARATION).setPackage(activity.getPackageName()));
        waitForPreparationExit();
        returnToInstaller();
        return false;
    }
    private void returnToInstaller() {
        activity.runOnUiThread(() -> activity.startActivity(new Intent(activity, LauncherActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT)));
    }
}

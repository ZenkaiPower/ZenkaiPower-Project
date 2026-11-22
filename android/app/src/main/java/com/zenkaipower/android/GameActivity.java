package com.zenkaipower.android;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;
import android.content.Context;
import android.os.Bundle;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.ViewGroup;
import android.content.Intent;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.os.Build;
import java.io.File;

/** SDL owns the window, controller input, audio focus and Android lifecycle. */
public class GameActivity extends SDLActivity {
    public byte[] matchmakingRequest(String url, String body, String[] headers, int limit) {
        return MatchmakingHttp.request(url, body, headers, limit);
    }
    static final String CANCEL_PREPARATION = "com.zenkaipower.android.CANCEL_PREPARATION";
    private BroadcastReceiver preparationCancel;
    static native boolean nativeTouchControlsActive();
    static native int nativeTouchVibration();
    static native boolean nativeBattleActive();
    static native int nativeCameraMode();
    static native void nativeCameraDrag(int mode,float dx,float dy);
    static native void nativePauseBattle();
    private VirtualPadView touchPad;
    private File storageRoot;
    @Override protected SDLSurface createSDLSurface(Context context) {
        return new TouchMouseSurface(context);
    }
    // The legacy registration is restricted to Android 10–12, before these flags exist.
    @android.annotation.SuppressLint("UnspecifiedRegisterReceiverFlag")
    @Override protected void onCreate(Bundle state) {
        storageRoot = GameStorage.selected(this);
        String requested = getIntent().getStringExtra("gameRoot");
        boolean privatePreparation = false;
        if (requested != null && (storageRoot == null || !requested.equals(storageRoot.getAbsolutePath()))) {
            storageRoot = null;
            // Installer regression fixtures prepare text inside this app's cache.
            // Shared paths must always match the selected tree.
            if (getIntent().getBooleanExtra("prepareText", false)) try {
                File candidate = new File(requested).getCanonicalFile();
                if (candidate.getPath().startsWith(getCacheDir().getCanonicalPath() + File.separator)) {
                    storageRoot = candidate;
                    privatePreparation = true;
                }
            } catch (java.io.IOException ignored) { }
        }
        if (gameRoot() == null || (!privatePreparation && !GameStorage.hasAccess(this))
                || !GameIO.file(getCacheDir(), "engine/libzenkaipower.so.rel").isFile()) {
            super.onCreate(state);
            startActivity(new Intent(this, LauncherActivity.class));
            finish();
            return;
        }
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (getIntent().getBooleanExtra("prepareText", false)) {
            preparationCancel = new BroadcastReceiver() {
                @Override public void onReceive(Context context, Intent intent) {
                    cancelPreparation();
                }
            };
            IntentFilter filter = new IntentFilter(CANCEL_PREPARATION);
            if (Build.VERSION.SDK_INT >= 33) registerReceiver(preparationCancel, filter, Context.RECEIVER_NOT_EXPORTED);
            else registerReceiver(preparationCancel, filter);
            mLayout.addView(new PreparationScreen(this, this::cancelPreparation),
                    new ViewGroup.LayoutParams(-1, -1));
            return;
        }
        touchPad = new VirtualPadView(this, (TouchMouseSurface)mSurface, gameRoot());
        mLayout.addView(touchPad, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }
    private void cancelPreparation() {
        try {
            String result = getIntent().getStringExtra("prepareResult");
            java.nio.file.Files.write((result == null ? GameIO.file(getCacheDir(), "engine/text-result") : GameIO.file(result)).toPath(), new byte[] {'1', '\n'});
        } catch (java.io.IOException ignored) { }
        finish(); android.os.Process.killProcess(android.os.Process.myPid());
    }
    @Override public void onBackPressed() {
        if (getIntent().getBooleanExtra("prepareText", false)) cancelPreparation();
        else if (touchPad != null) ((TouchMouseSurface)mSurface).sendBack();
        else super.onBackPressed();
    }
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        // SDL's surface can consume hardware Back before Activity sees it.
        if (touchPad != null && event.getKeyCode() == KeyEvent.KEYCODE_BACK &&
                !event.isFromSource(InputDevice.SOURCE_MOUSE)) {
            if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled()) onBackPressed();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
    @Override protected void onPause() {
        if (touchPad != null) touchPad.setVibrationResumed(false);
        if (touchPad != null) touchPad.release();
        super.onPause();
    }
    @Override protected void onResume() {
        super.onResume();
        if (touchPad != null) touchPad.setVibrationResumed(true);
    }
    @Override protected void onDestroy() {
        if (preparationCancel != null) unregisterReceiver(preparationCancel);
        super.onDestroy();
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        if (!focused && touchPad != null) touchPad.release();
        super.onWindowFocusChanged(focused);
        if (focused && touchPad != null) hideStatusBar();
    }
    @Override protected boolean sendCommand(int command, Object data) {
        boolean sent = super.sendCommand(command, data);
        if (sent && command == COMMAND_CHANGE_WINDOW_STYLE) {
            // Apply after SDL updates the window style on the UI thread.
            getWindow().getDecorView().post(() -> {
                if (touchPad != null) hideStatusBar();
            });
        }
        return sent;
    }
    private void hideStatusBar() {
        Window window = getWindow();
        window.clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) controller.hide(WindowInsets.Type.statusBars());
        } else {
            View decor = window.getDecorView();
            decor.setSystemUiVisibility(decor.getSystemUiVisibility() | View.SYSTEM_UI_FLAG_FULLSCREEN);
            window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }
    }
    @Override protected String[] getLibraries() {
        // libzenkaipower is loaded at a low address by libmain, after sidecars are available.
        return new String[] {"SDL3", "main"};
    }
    @Override public void setOrientationBis(int width, int height, boolean resizable, String hint) {
        // SDL's resizable desktop window otherwise enables portrait rotation.
        super.setOrientationBis(width, height, resizable,
                getIntent().getBooleanExtra("prepareText", false) ? "Portrait" : "LandscapeLeft LandscapeRight");
    }
    private File gameRoot() {
        return storageRoot;
    }
    @Override protected String[] getArguments() {
        return new String[] {gameRoot().getAbsolutePath(),
                getApplicationInfo().nativeLibraryDir, getCacheDir().getAbsolutePath(),
                GameIO.file(getCacheDir(), "engine").getAbsolutePath(),
                getIntent().getBooleanExtra("prepareText", false) ? "prepare-text" : "play",
                getIntent().getStringExtra("prepareResult") == null ? "" : getIntent().getStringExtra("prepareResult")};
    }
}

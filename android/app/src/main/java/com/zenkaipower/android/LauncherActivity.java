package com.zenkaipower.android;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Selects a desktop-layout folder and installs the owner's original discs. */
public final class LauncherActivity extends Activity {
    private static final int FOLDER = 2, DISC = 10;
    private static final String[] DISCS = {
        "Budokai Tenkaichi 3 — USA (SLUS-21678)",
        "Budokai Tenkaichi 3 — Europe (SLES-54945)",
        "Budokai Tenkaichi 3 — Wii Europe (RDSPAF)",
        "Sparking! METEOR — Japan (SLPS-25815)"
    };
    private static final String[] DETAILS = {
        "Required: installs the base game.",
        "Optional: official Spanish, German, French and Italian translations, and the opening soundtrack.",
        "Optional: Dragon Net Battle, required for LAN and online modes.",
        "Optional: Japanese title artwork, anime soundtrack and other Sparking! METEOR content."
    };
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final Button[] discs = new Button[4];
    private TextView status, folder;
    private Button play, choose, cancel;
    private boolean busy;
    private boolean autoStartPending;
    private int folderPreparationPercent = -1;
    private int selectedDisc;
    private volatile int verifiedMask;
    private SetupScreen screen;
    private PreparationScreen progressScreen;
    private final ExecutorService artworkWorker = Executors.newSingleThreadExecutor();
    private File root;
    private DiscInstaller installer;
    private final boolean[] installed = new boolean[4];

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        autoStartPending=state==null ? !getIntent().getBooleanExtra("showSetup",false) &&
                (getIntent().getBooleanExtra("startGame",false) || getSharedPreferences("setup",MODE_PRIVATE).getBoolean("auto-start",false))
                : state.getBoolean("autoStartPending",false);
        LauncherShortcuts.publishSetup(this);
        root = GameStorage.selected(this);
        if (state != null) selectedDisc = state.getInt("selectedDisc", 0);
        screen = new SetupScreen(this, artworkWorker, DETAILS);
        folder = screen.folder; status = screen.status; choose = screen.choose;
        play = screen.play; cancel = screen.cancel;
        choose.setOnClickListener(v -> chooseFolder());
        for (int i = 0; i < discs.length; i++) {
            final int disc = i; discs[i] = screen.install[i];
            discs[i].setOnClickListener(v -> chooseDisc(disc));
        }
        play.setOnClickListener(v -> {
            if (!GameStorage.hasAccess(this)) { chooseFolder(); return; }
            startActivity(new Intent(this, GameActivity.class).putExtra("gameRoot", root.getAbsolutePath()));
        });
        cancel.setOnClickListener(v -> cancelInstallation());
        progressScreen = new PreparationScreen(this, this::cancelInstallation);
        addContentView(progressScreen, new android.view.ViewGroup.LayoutParams(-1, -1));
        progressScreen.setVisibility(View.GONE);
        refresh();
        if (root != null && GameStorage.hasAccess(this)) prepareFolder(root);
    }
    private void chooseFolder() {
        if (busy) return;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        startActivityForResult(intent, FOLDER);
    }
    private void chooseDisc(int disc) {
        if (busy || root == null) return;
        selectedDisc = disc;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE); intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_TITLE, DISCS[disc]); startActivityForResult(intent, DISC);
    }
    @Override protected void onSaveInstanceState(Bundle state) {
        state.putBoolean("autoStartPending",autoStartPending);
        state.putInt("selectedDisc", selectedDisc); super.onSaveInstanceState(state);
    }
    private void prepareFolder(File selected) {
        setBusy("Preparing game folder…");
        folderPreparationPercent = 0;
        progressScreen.update("Preparing game folder…", false, 0);
        worker.execute(() -> {
            try {
                GameFiles.prepare(selected);
                folderProgress(5, "Checking game folder…");
                File stamp = GameIO.file(selected, "setup/.android-build");
                String build = Long.toString(getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime);
                boolean current = stamp.isFile() && new String(GameIO.readAllBytes(stamp), StandardCharsets.UTF_8).equals(build);
                for (String name : new String[] {"fonts.pak", "translations.pak", "game-info.pak", "setup.pak", "setup/engine.pak", "dragon-net-battle.pak"})
                    current &= GameIO.file(selected, name).isFile();
                if (!current) {
                    int total = countAssets("engine");
                    int[] copied = {0};
                    copyAssets("engine", selected, () -> folderProgress(
                            10 + (int)(55L * ++copied[0] / Math.max(1, total)), "Preparing game folder…"));
                }
                folderProgress(65, "Preparing engine…");
                File cache = GameIO.file(getCacheDir(), "engine");
                int extracted = 0;
                for (String suffix : new String[] {"dat", "rel", "mem"}) {
                    GamePak.extract(GameIO.file(selected, "setup/engine.pak"), "libzenkaipower.so." + suffix,
                            GameIO.file(cache, "libzenkaipower.so." + suffix));
                    folderProgress(65 + 5 * ++extracted, "Preparing engine…");
                }
                folderProgress(80, "Packing game resources…");
                GameFiles.packResources(selected);
                folderProgress(90, "Checking installed discs…");
                updateDiscState(selected);
                if ((verifiedMask & 1) != 0 && !current &&
                        !DiscInstaller.prepareTouchArtwork(selected.getAbsolutePath()))
                    throw new IOException("Could not prepare touch controller artwork.");
                if ((verifiedMask & 1) != 0 && !GameFiles.ready(selected)) {
                    folderProgress(95, "Restoring game text after interrupted installation...");
                    if (!new DiscInstaller(this, selected).prepareText()) throw new IOException("Could not restore game text. See logs/zenkaipower_log.txt.");
                    GameFiles.packResources(selected);
                    updateDiscState(selected);
                }
                folderProgress(100, "Game folder ready");
                GameIO.write(stamp, build.getBytes(StandardCharsets.UTF_8));
                runOnUiThread(() -> {
                    root = selected; finishWork("");
                    boolean launch=autoStartPending;autoStartPending=false;
                    if(launch && GameStorage.hasAccess(this) && GameFiles.ready(root)) play.performClick();
                });
            } catch (Exception error) { showError(error); }
        });
    }
    private void folderProgress(int percent, String text) {
        runOnUiThread(() -> {
            folderPreparationPercent = percent;
            status.setText(text);
            progressScreen.update(text, false, percent);
        });
    }
    private void setBusy(String text) {
        screen.clearError();
        folderPreparationPercent = -1;
        busy = true; refresh(); progressMessage(text);
        progressScreen.setVisibility(View.VISIBLE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
    private void progressMessage(String text) {
        status.setText(text);
        progressScreen.update(text, installer != null, folderPreparationPercent);
    }
    private void cancelInstallation() {
        if (installer == null) return;
        installer.stop();
        status.setText("Cancelling installation...");
        progressScreen.update("Cancelling installation...", false);
        cancel.setEnabled(false);
    }
    @Override public void onBackPressed() {
        if (busy) { cancelInstallation(); return; }
        super.onBackPressed();
    }
    private void refresh() {
        boolean access = GameStorage.hasAccess(this), ready = GameFiles.ready(root);
        folder.setText(root == null ? "" : root.getAbsolutePath()); choose.setEnabled(!busy);
        for (int i = 0; i < discs.length; i++) {
            discs[i].setEnabled(!busy && root != null && access && (i == 0 || ready));

        }
        screen.states(installed, verifiedMask);
        play.setEnabled(!busy && access && ready);
        cancel.setVisibility(busy && installer != null ? View.VISIBLE : View.GONE); cancel.setEnabled(busy);
    }
    private void updateDiscState(File selected) {
        // Initialize the installer JNI library and SDL context before verification.
        // Verification already checks optional-disc completeness; reuse its result.
        new DiscInstaller(this, selected);
        verifiedMask = SetupArtwork.verify(selected.getAbsolutePath());
        installed[0] = GameFiles.ready(selected);
        for (int i = 1; i < installed.length; i++) installed[i] = (verifiedMask & (1 << i)) != 0;
    }
    void installProgress(String line) {
        runOnUiThread(() -> {
            if (line.startsWith("@disc-verified ")) {
                for (int i = 0; i < 4; i++) if (line.substring(15).equals(SetupScreen.IDS[i])) verifiedMask |= 1 << i;
                screen.states(installed, verifiedMask); screen.artwork(root, verifiedMask);
            }
            if (line.startsWith("@note ") || line.startsWith("@ok ")) progressMessage(line.substring(line.indexOf(' ') + 1));
            else if (line.startsWith("@step ")) {
                String[] parts = line.split(" ", 4);
                progressMessage(parts.length == 4 ? parts[3] : line.substring(6));
            }
        });
    }
    private void finishWork(String message) {
        folderPreparationPercent = -1;
        busy = false; installer = null; refresh(); status.setText(message);
        progressScreen.setVisibility(View.GONE);
        screen.artwork(root, verifiedMask);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
    private void showError(Exception error) {
        runOnUiThread(() -> {
            autoStartPending=false;
            String message=error.getMessage() == null ? "Installation failed." : error.getMessage();
            finishWork(message);
            screen.showError(message);
        });
    }
    private void copyAsset(String asset, File target) throws IOException {
        if (target.isFile() && assetMatches(asset, target)) return;
        File temp = GameIO.file(target.getPath() + ".part"); GameFiles.mkdir(target.getParentFile());
        try (InputStream in = getAssets().open(asset); FileOutputStream out = GameIO.output(temp)) {
            byte[] buffer = new byte[65536]; int n;
            while ((n = in.read(buffer)) != -1) out.write(buffer, 0, n);
        }
        if (!temp.renameTo(target)) throw new IOException("Cannot install " + target.getName());
    }
    private boolean assetMatches(String asset, File target) throws IOException {
        try (InputStream expected = getAssets().open(asset); InputStream actual = GameIO.input(target)) {
            byte[] a = new byte[65536], b = new byte[65536]; int count;
            while ((count = expected.read(a)) != -1) {
                int used = 0;
                while (used < count) {
                    int read = actual.read(b, used, count - used);
                    if (read < 0) return false;
                    used += read;
                }
                for (int i = 0; i < count; i++) if (a[i] != b[i]) return false;
            }
            return actual.read() == -1;
        }
    }
    private int countAssets(String asset) throws IOException {
        String[] children = getAssets().list(asset);
        if (children == null || children.length == 0) return 1;
        int count = 0;
        for (String name : children) count += countAssets(asset + "/" + name);
        return count;
    }
    private void copyAssets(String asset, File target, Runnable copied) throws IOException {
        String[] children = getAssets().list(asset);
        if (children == null || children.length == 0) { copyAsset(asset, target); copied.run(); return; }
        for (String name : children) copyAssets(asset + "/" + name, GameIO.file(target, name), copied);
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == FOLDER && result == RESULT_OK && data != null && data.getData() != null) {
            try {
                root = GameStorage.grant(this, data.getData(), data.getFlags());
                prepareFolder(root);
            }
            catch (Exception error) { showError(error); }
        } else if (request == DISC && result == RESULT_OK && data != null && data.getData() != null) {
            try {
                ParcelFileDescriptor fd = getContentResolver().openFileDescriptor(data.getData(), "r");
                if (fd == null) throw new IOException("Cannot open the disc image.");
                if (fd.getStatSize() < 0) { fd.close(); throw new IOException("Choose a local ISO or RVZ file with random access."); }
                installer = new DiscInstaller(this, root);
                setBusy("Checking " + DISCS[selectedDisc] + "…");
                final DiscInstaller task = installer; final int disc = selectedDisc;
                worker.execute(() -> {
                    try {
                        task.install(fd, disc); updateDiscState(root);
                        runOnUiThread(() -> {
                            finishWork("Installation complete.");
                            // Replace the setup activity after the isolated SDL helper exits.
                            startActivity(new Intent(this, LauncherActivity.class)
                                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP));
                        });
                    }
                    catch (Exception error) { showError(error); }
                });
            } catch (Exception error) { showError(error); }
        }
    }
    @Override protected void onDestroy() {
        if (installer != null) installer.stop(); worker.shutdown(); artworkWorker.shutdownNow(); super.onDestroy();
    }
}

package com.zenkaipower.android;

import android.app.Instrumentation;
import android.content.Intent;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import java.io.File;
import java.nio.file.Files;
import java.util.Arrays;

/** Device checks use an isolated cache folder and never change the player's data. */
public final class InstallerChecks extends Instrumentation {
    private static native void extractFixture(String root, String image, int kind) throws java.io.IOException;
    private static native String verifyWiiDescriptor(String root, int descriptor);
    private Bundle arguments;
    @Override public void onCreate(Bundle args) { super.onCreate(args); arguments=args; start(); }
    private void check(boolean ok, String message) { if (!ok) throw new AssertionError(message); }
    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            LauncherActivity launcher = (LauncherActivity)startActivitySync(new Intent(getTargetContext(), LauncherActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            waitForIdleSync();
            File root = Files.createTempDirectory(getTargetContext().getCacheDir().toPath(), "installer-check-").toFile();
            GameFiles.prepare(root);
            new DiscInstaller(launcher, root); // Initialize the production JNI bridge before artwork checks.
            check(SetupArtwork.load(root, 0, 0) == null, "Cover unlocked before verification");
            check(SetupArtwork.load(root, 0, 1) == null, "Disc icon unlocked before verification");
            check(SetupArtwork.load(root, 0, 2) == null, "Edition artwork unlocked before verification");
            Files.write(new File(root, "gamedata/.installed").toPath(), new byte[] {1});
            check((SetupArtwork.verify(root.getAbsolutePath()) & 1) == 0, "A marker unlocked the cover without matching programs");
            check(SetupArtwork.load(root, 0, 0) == null, "Invalid installation decrypted the cover");
            File programs = new File(root, "gamedata/disc"), bin = new File(programs, "BIN"); bin.mkdirs();
            byte[] corruptElf = new byte[256];
            java.nio.ByteBuffer elf = java.nio.ByteBuffer.wrap(corruptElf).order(java.nio.ByteOrder.LITTLE_ENDIAN);
            elf.putInt(0, 0x464c457f); elf.putInt(0x20, 0x40); elf.putShort(0x2e, (short)24); elf.putShort(0x30, (short)1);
            elf.putInt(0x44, 1); elf.putInt(0x48, 2); elf.putInt(0x4c, 0xfffffff0); elf.putInt(0x50, 0x80); elf.putInt(0x54, 32);
            File program = new File(programs, "SLUS_216.78"), menu = new File(bin, "DBZP.BIN");
            Files.write(program.toPath(), corruptElf); Files.write(menu.toPath(), new byte[] {1});
            check((SetupArtwork.verify(root.getAbsolutePath()) & 1) == 0, "Malformed/mismatching programs unlocked artwork");
            Files.delete(program.toPath()); Files.delete(menu.toPath()); Files.delete(bin.toPath()); Files.delete(programs.toPath());
            Files.delete(new File(root, "gamedata/.installed").toPath());
            File image = new File(root, "invalid.iso"); Files.write(image.toPath(), new byte[65536]);
            for (int disc = 0; disc < 4; disc++) {
                DiscInstaller installer = new DiscInstaller(launcher, root);
                boolean rejected = false;
                try { installer.install(ParcelFileDescriptor.open(image, ParcelFileDescriptor.MODE_READ_ONLY), disc); }
                catch (java.io.IOException expected) { rejected = true; }
                check(rejected, "Invalid disc accepted: " + disc);
                check(!new File(root, "gamedata/.installed").exists(), "Invalid disc published an installation marker");
            }
            System.loadLibrary("reader_checks");
            File fixtures = new File(root, "fixtures"); copyAssets("installer", fixtures);
            File extracted = new File(root, "extracted"); GameFiles.prepare(extracted);
            boolean malformedRejected = false;
            try { extractFixture(extracted.getAbsolutePath(), new File(fixtures, "invalid-dol.iso").getAbsolutePath(), 2); }
            catch (java.io.IOException expected) { malformedRejected = true; }
            check(malformedRejected, "Malformed DOL metadata was accepted");
            extractFixture(extracted.getAbsolutePath(), new File(fixtures, "jp.iso").getAbsolutePath(), 3);
            check(Arrays.equals(Files.readAllBytes(new File(extracted, "gamedata/sparking-meteor.pak").toPath()),
                    Files.readAllBytes(new File(fixtures, "reference-jp/sparking-meteor.pak").toPath())), "Japanese extraction differs from desktop");
            try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(new File(fixtures, "jp.iso"), ParcelFileDescriptor.MODE_READ_ONLY)) {
                android.system.Os.lseek(fd.getFileDescriptor(), 17, android.system.OsConstants.SEEK_SET);
                extractFixture(extracted.getAbsolutePath(), "/proc/self/fd/" + fd.getFd(), 3);
                check(Arrays.equals(Files.readAllBytes(new File(extracted, "gamedata/sparking-meteor.pak").toPath()),
                        Files.readAllBytes(new File(fixtures, "reference-jp/sparking-meteor.pak").toPath())), "PS2 descriptor extraction differs from desktop");
                check(android.system.Os.lseek(fd.getFileDescriptor(), 0, android.system.OsConstants.SEEK_CUR) == 17,
                        "PS2 reader changed or closed the picker descriptor");
            }
            for (String format : new String[] {"iso", "rvz"}) {
                extractFixture(extracted.getAbsolutePath(), new File(fixtures, "wii." + format).getAbsolutePath(), 2);
                compare(new File(extracted, "gamedata/dragon-net-battle.pak"), new File(fixtures, "reference-wii"), "");
                // Match the launcher's document-picker route, not just a normal filesystem path.
                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(new File(fixtures, "wii." + format), ParcelFileDescriptor.MODE_READ_ONLY)) {
                    android.system.Os.lseek(fd.getFileDescriptor(), 17, android.system.OsConstants.SEEK_SET);
                    extractFixture(extracted.getAbsolutePath(), "/proc/self/fd/" + fd.getFd(), 2);
                    compare(new File(extracted, "gamedata/dragon-net-battle.pak"), new File(fixtures, "reference-wii"), "");
                    check(android.system.Os.lseek(fd.getFileDescriptor(), 0, android.system.OsConstants.SEEK_CUR) == 17,
                            "Wii reader changed or closed the picker descriptor");
                }
            }
            File buttons = new File(fixtures, "button-fixture/pzs3us1.afs");
            String ownerImage = arguments == null ? null : arguments.getString("wiiImage");
            if (ownerImage != null) {
                try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(new File(ownerImage), ParcelFileDescriptor.MODE_READ_ONLY)) {
                    String error = verifyWiiDescriptor(root.getAbsolutePath(), fd.getFd());
                    check(error.isEmpty(), "Owner's Wii image checksum verification failed through the picker descriptor: " + error);
                }
            }
            if (buttons.isFile()) {
                Files.copy(buttons.toPath(), new File(extracted, "gamedata/pzs3us1.afs").toPath());
                extractFixture(extracted.getAbsolutePath(), "", 0);
                compare(new File(extracted, "gamedata/touch-buttons.pak"), new File(fixtures, "reference-buttons"), "");
            }
            File original = GameStorage.selected(getTargetContext());
            check(GameFiles.ready(original), "A prepared installation is needed for the isolated text-generation check");
            check((SetupArtwork.verify(original.getAbsolutePath()) & 1) != 0, "Original USA program checksum did not match");
            android.graphics.Bitmap cover = SetupArtwork.load(original, 0, 0);
            check(cover != null && cover.getWidth() > 0, "Verified USA cover did not authenticate/decode");
            cover.recycle();
            for (int kind = 1; kind <= 4; kind++) {
                android.graphics.Bitmap artworkImage = SetupArtwork.load(original, 0, kind);
                check(artworkImage != null, "Verified USA artwork failed to decrypt: " + kind); artworkImage.recycle();
            }
            for (String name : new String[] {"fonts.pak", "translations.pak", "game-info.pak"})
                Files.createSymbolicLink(new File(root, name).toPath(), new File(original, name).toPath());
            File[] packages = new File(original, "gamedata").listFiles();
            check(packages != null, "Cannot read installed disc packages");
            for (File source : packages) {
                if (source.isFile() && (source.getName().endsWith(".afs") || source.getName().endsWith(".pak"))
                        && !source.getName().equals("game-text.pak"))
                    Files.createSymbolicLink(new File(root, "gamedata/" + source.getName()).toPath(), source.toPath());
            }
            DiscInstaller installer = new DiscInstaller(launcher, root);
            java.lang.reflect.Method prepare = DiscInstaller.class.getDeclaredMethod("prepareText"); prepare.setAccessible(true);
            check((Boolean)prepare.invoke(installer), "Isolated text generation failed; inspect cache engine/text-result and test logs");
            check(new File(root, "gamedata/game-text.pak").length() > 0, "Text package missing");
            check(GameFiles.ready(root), "Generated installation is not playable");
            result.putString("stream", "PASS: Wii ISO/RVZ extract through picker descriptors without closing or seeking the original; artwork gates, invalid discs, desktop extraction comparisons, button pixels and text generation pass.\n");
            finish(-1, result);
        } catch (Throwable error) {
            result.putString("stream", android.util.Log.getStackTraceString(error)); finish(0, result);
        }
    }
    private void copyAssets(String name, File target) throws Exception {
        String[] children = getContext().getAssets().list(name);
        if (children != null && children.length != 0) {
            target.mkdirs();
            for (String child : children) copyAssets(name + "/" + child, new File(target, child));
        } else {
            target.getParentFile().mkdirs();
            try (java.io.InputStream in = getContext().getAssets().open(name)) { Files.copy(in, target.toPath()); }
        }
    }
    private void compare(File pak, File expected, String prefix) throws Exception {
        File[] files = expected.listFiles(); check(files != null, "Expected test resources missing");
        for (File file : files) {
            String name = prefix + file.getName();
            if (file.isDirectory()) { compare(pak, file, name + "/"); continue; }
            if (name.equals("manifest.json") || name.equals("index.html")) continue;
            byte[] actual = GamePak.read(pak, name), original = Files.readAllBytes(file.toPath());
            check(actual != null, "Missing installed resource: " + name);
            if (name.endsWith(".png")) {
                android.graphics.Bitmap a = android.graphics.BitmapFactory.decodeByteArray(actual, 0, actual.length);
                android.graphics.Bitmap b = android.graphics.BitmapFactory.decodeByteArray(original, 0, original.length);
                check(a != null && b != null && a.sameAs(b), "Texture differs: " + name);
            } else if (!name.startsWith("ui-fragment-")) check(Arrays.equals(actual, original), "Resource differs: " + name);
        }
    }
}

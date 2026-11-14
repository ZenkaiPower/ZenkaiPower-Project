package com.zenkaipower.android;

import android.app.Instrumentation;
import android.os.Bundle;
import java.io.*;
import java.util.Arrays;
import java.util.UUID;

/** Uses an isolated child of the already granted tree. Never changes game saves. */
public final class StorageChecks extends Instrumentation {
    private static native String nativeCheck(String root);
    @Override public void onCreate(Bundle arguments) { super.onCreate(arguments); start(); }
    private static void check(boolean condition, String message) throws IOException {
        if (!condition) throw new IOException(message);
    }
    @Override public void onStart() {
        Bundle report = new Bundle(); File test = null;
        try {
            File root = GameStorage.selected(getTargetContext());
            check(root != null && GameStorage.hasAccess(getTargetContext()), "Select a game folder in the launcher first.");
            test = GameIO.file(root, ".storage-check-" + UUID.randomUUID());
            GameFiles.prepare(test);
            File save = GameIO.file(test, "savegame/example.sav"), temporary = GameIO.file(test, "savegame/example.part");
            GameIO.write(save, new byte[]{1, 2, 3});
            GameIO.write(temporary, new byte[]{4, 5, 6});
            GameIO.publish(temporary, save);
            check(Arrays.equals(GameIO.readAllBytes(save), new byte[]{4, 5, 6}), "Replacement did not reach the selected folder.");
            try (FileOutputStream out = GameIO.output(save, true)) { out.getChannel().position(0); out.write(7); }
            check(Arrays.equals(GameIO.readAllBytes(save), new byte[]{4, 5, 6, 7}), "Append lost existing save data.");
            try (GameIO.Reader in = new GameIO.Reader(save, "r")) {
                in.seek(2); check(in.readUnsignedByte() == 6 && in.length() == 4, "Granted descriptor is not seekable.");
            }
            File archive = GameIO.file(test, "test.pak");
            GameIO.write(archive, new byte[]{10, 20, 30, 40});
            try (GameIO.Reader first = new GameIO.Reader(archive, "r");
                    GameIO.Reader second = new GameIO.Reader(archive, "r")) {
                first.seek(2);
                check(second.readUnsignedByte() == 10 && first.readUnsignedByte() == 30,
                        "Cached archive readers share a seek position.");
            }
            long archiveStart = android.os.SystemClock.elapsedRealtime();
            for (int i = 0; i < 50; i++) try (GameIO.Reader reader = new GameIO.Reader(archive, "r")) {
                check(reader.readUnsignedByte() == 10, "Cached archive bytes changed.");
            }
            long archiveMillis = android.os.SystemClock.elapsedRealtime() - archiveStart;
            GameIO.write(archive, new byte[]{50, 60});
            try (GameIO.Reader reader = new GameIO.Reader(archive, "r")) {
                check(reader.length() == 2 && reader.readUnsignedByte() == 50,
                        "Archive write did not invalidate its cached descriptor.");
            }
            File missing = GameIO.file(test, "new-after-miss.bin");
            check(!missing.exists() && !missing.exists(), "Missing lookup fixture exists.");
            GameIO.write(missing, new byte[]{1});
            check(missing.isFile(), "Directory miss cache hid a newly created file.");
            // Simulate process death after preserving the original but before publication.
            File previous = GameIO.file(test, "savegame/example.sav.zenkai-previous-" + UUID.randomUUID());
            check(save.renameTo(previous), "Cannot simulate interrupted replacement.");
            check(Arrays.equals(GameIO.readAllBytes(save), new byte[]{4, 5, 6, 7}), "Interrupted save replacement was not recovered.");
            check(!save.getParentFile().delete() && save.isFile(), "Deleting a nonempty folder removed a save.");
            // The game's POSIX and SDL calls must reach the same tree too.
            System.loadLibrary("storage_checks");
            String failure = nativeCheck(test.getAbsolutePath());
            check(failure.isEmpty(), failure);
            report.putString("stream", "Folder grant: archive cursors/invalidation, miss caching, replacement, recovery, append, POSIX and SDL passed. 50 archive opens: " + archiveMillis + " ms.");
            remove(test); test = null;
            finish(-1, report);
        } catch (Throwable error) {
            report.putString("stream", error.toString() + (test == null ? "" : "\nTest files retained at " + test));
            finish(0, report);
        }
    }
    private static void remove(File folder) throws IOException {
        File[] children = folder.listFiles();
        if (children != null) for (File child : children) { if (child.isDirectory()) remove(child); else check(child.delete(), "Cannot remove test file."); }
        check(folder.delete(), "Cannot remove isolated test directory.");
    }
}

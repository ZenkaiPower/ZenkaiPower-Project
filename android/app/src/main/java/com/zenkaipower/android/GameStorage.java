package com.zenkaipower.android;

import android.content.Context;
import android.content.Intent;
import android.content.UriPermission;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.system.OsConstants;
import java.io.*;
import java.nio.file.Files;
import java.util.UUID;

/** The chosen shared folder remains the live installation and save location. */
final class GameStorage {
    static {
        System.loadLibrary("storage");
        GameIO.backend = new GameIO.Backend() {
            public File file(String path) { return GameStorage.file(path); }
            public FileInputStream input(File file) throws IOException { return GameStorage.input(file); }
            public FileOutputStream output(File file, boolean append) throws IOException { return GameStorage.output(file, append); }
            public File temp(String name, String suffix, File parent) throws IOException { return GameStorage.temp(name, suffix, parent); }
            public boolean isSymbolicLink(File file) { return GameStorage.isSymbolicLink(file); }
            public void publish(File temporary, File target) throws IOException { GameStorage.publish(temporary, target); }
        };
    }
    private static String prefix;
    private static native void nativeConfigure(String root);
    private static native int nativeOpen(String path, int flags);
    private static native long[] nativeStat(String path);
    private static native String[] nativeList(String path) throws IOException;
    private static native int nativeChange(String path, int operation);
    private static native boolean nativeRename(String from, String to);

    static File selected(Context context) {
        String value = context.getSharedPreferences("game-folder", Context.MODE_PRIVATE).getString("tree", null);
        if (value == null) return null; // Old installs must grant their folder once.
        try { return configure(context, Uri.parse(value)); }
        catch (IOException | RuntimeException error) { return null; }
    }
    private static File configure(Context context, Uri tree) throws IOException {
        File root = fromTree(tree);
        prefix = root.getAbsolutePath();
        TreeStorage.configure(context.getContentResolver(), tree, prefix);
        nativeConfigure(prefix);
        return file(prefix);
    }
    static File grant(Context context, Uri tree, int flags) throws IOException {
        int required = Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
        if ((flags & required) != required) throw new IOException("Allow read and write access to the game folder.");
        fromTree(tree); // Reject unsupported providers before retaining a grant.
        context.getContentResolver().takePersistableUriPermission(tree, required);
        File root = configure(context, tree);
        if (!context.getSharedPreferences("game-folder", Context.MODE_PRIVATE).edit()
                .putString("tree", tree.toString()).remove("path").commit())
            throw new IOException("Could not remember the game folder.");
        // Keep only this game's current tree grant.
        for (UriPermission permission : context.getContentResolver().getPersistedUriPermissions()) {
            if (!permission.getUri().equals(tree)) {
                try { context.getContentResolver().releasePersistableUriPermission(permission.getUri(),
                        (permission.isReadPermission() ? Intent.FLAG_GRANT_READ_URI_PERMISSION : 0)
                        | (permission.isWritePermission() ? Intent.FLAG_GRANT_WRITE_URI_PERMISSION : 0)); }
                catch (SecurityException ignored) { }
            }
        }
        return root;
    }
    static boolean hasAccess(Context context) {
        String value = context.getSharedPreferences("game-folder", Context.MODE_PRIVATE).getString("tree", null);
        if (value == null) return false;
        for (UriPermission grant : context.getContentResolver().getPersistedUriPermissions())
            if (grant.getUri().toString().equals(value) && grant.isReadPermission() && grant.isWritePermission()) return true;
        return false;
    }
    static File fromTree(Uri tree) throws IOException {
        if (!"com.android.externalstorage.documents".equals(tree.getAuthority()))
            throw new IOException("Choose a local folder on internal storage or an SD card.");
        String[] parts = DocumentsContract.getTreeDocumentId(tree).split(":", 2);
        if (parts.length != 2) throw new IOException("The selected storage folder is invalid.");
        File volume;
        if (parts[0].equals("primary")) volume = Environment.getExternalStorageDirectory();
        else if (parts[0].matches("[A-Za-z0-9-]+")) volume = new File("/storage", parts[0]);
        else throw new IOException("This storage provider is not supported.");
        return GameFiles.resolveFolder(volume, parts[1]);
    }
    static File file(File parent, String child) { return file(new File(parent, child).getPath()); }
    static File file(String path) {
        return prefix != null && (path.equals(prefix) || path.startsWith(prefix + "/")) ? new TreeFile(path) : new File(path);
    }
    private static final class TreeFile extends File {
        TreeFile(String path) { super(path); }
        private long[] info() { return nativeStat(getAbsolutePath()); }
        @Override public boolean exists() { return info() != null; }
        @Override public boolean isFile() { long[] i = info(); return i != null && i[0] == 0; }
        @Override public boolean isDirectory() { long[] i = info(); return i != null && i[0] == 1; }
        @Override public long length() { long[] i = info(); return i == null ? 0 : i[1]; }
        @Override public long lastModified() { long[] i = info(); return i == null ? 0 : i[2] * 1000; }
        @Override public boolean canRead() { return exists(); }
        @Override public boolean canWrite() { return exists(); }
        @Override public File getParentFile() { String p = getParent(); return p == null ? null : file(p); }
        @Override public File getAbsoluteFile() { return file(getAbsolutePath()); }
        @Override public File getCanonicalFile() throws IOException { return file(getCanonicalPath()); }
        @Override public boolean mkdir() { return nativeChange(getAbsolutePath(), 0) == 0; }
        @Override public boolean mkdirs() { return isDirectory() || (getParentFile().isDirectory() || getParentFile().mkdirs()) && mkdir(); }
        @Override public boolean delete() { return nativeChange(getAbsolutePath(), isDirectory() ? 2 : 1) == 0; }
        @Override public boolean renameTo(File target) { return nativeRename(getAbsolutePath(), target.getAbsolutePath()); }
        @Override public String[] list() {
            try { return nativeList(getAbsolutePath()); }
            catch (IOException | RuntimeException error) { return null; }
        }
        @Override public File[] listFiles() {
            String[] names = list(); if (names == null) return null;
            File[] files = new File[names.length];
            for (int i = 0; i < names.length; i++) files[i] = file(this, names[i]);
            return files;
        }
        // SAF doesn't expose volume free space. Writes report ENOSPC themselves.
        @Override public long getUsableSpace() { return Long.MAX_VALUE; }
    }
    private static ParcelFileDescriptor descriptor(File file, int flags) throws IOException {
        int fd = nativeOpen(file.getAbsolutePath(), flags | OsConstants.O_CLOEXEC);
        if (fd < 0) throw new IOException("Cannot open " + file.getName() + " (errno " + -fd + "). Check the selected folder's access and free space.");
        return ParcelFileDescriptor.adoptFd(fd);
    }
    static FileInputStream input(File file) throws IOException {
        return new ParcelFileDescriptor.AutoCloseInputStream(descriptor(file, OsConstants.O_RDONLY));
    }
    static FileOutputStream output(File file) throws IOException { return output(file, false); }
    static FileOutputStream output(File file, boolean append) throws IOException {
        return new ParcelFileDescriptor.AutoCloseOutputStream(descriptor(file, OsConstants.O_WRONLY | OsConstants.O_CREAT
                | (append ? OsConstants.O_APPEND : OsConstants.O_TRUNC)));
    }
    static File temp(String name, String suffix, File parent) throws IOException {
        File result = file(parent, name + UUID.randomUUID() + suffix);
        try (ParcelFileDescriptor fd = descriptor(result, OsConstants.O_WRONLY | OsConstants.O_CREAT | OsConstants.O_EXCL)) { }
        return result;
    }
    static boolean isSymbolicLink(File file) { return !(file instanceof TreeFile) && Files.isSymbolicLink(file.toPath()); }
    static void publish(File temporary, File target) throws IOException {
        if (temporary instanceof TreeFile || target instanceof TreeFile) {
            if (!temporary.renameTo(target)) throw new IOException("Cannot publish " + target.getName());
        } else {
            try { Files.move(temporary.toPath(), target.toPath(), java.nio.file.StandardCopyOption.ATOMIC_MOVE,
                    java.nio.file.StandardCopyOption.REPLACE_EXISTING); }
            catch (java.nio.file.AtomicMoveNotSupportedException error) {
                Files.move(temporary.toPath(), target.toPath(), java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            }
        }
    }
}

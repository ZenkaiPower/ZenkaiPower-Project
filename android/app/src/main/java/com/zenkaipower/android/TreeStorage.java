package com.zenkaipower.android;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;
import android.os.SystemClock;

/** Resolves paths only inside the granted tree. No game data is staged privately. */
final class TreeStorage {
    private static ContentResolver resolver;
    private static Uri tree;
    private static String root;
    // Directory snapshots cache misses for one second. Archive descriptors also
    // expire after one second so external replacements become visible, while
    // mutations made through this adapter invalidate both caches immediately.
    private static final Map<String, Uri> resolved = new HashMap<>();
    private static final long CACHE_MS = 1000;
    private static final int CACHE_LIMIT = 32;
    private static final Map<Uri, Directory> directories = new HashMap<>();
    private static final Map<String, Archive> archives = new HashMap<>();
    private static final class Directory {
        final long until = SystemClock.elapsedRealtime() + CACHE_MS;
        final Map<String, Uri> children = new HashMap<>();
        boolean backups;
    }
    private static final class Archive {
        final ParcelFileDescriptor descriptor;
        final long until = SystemClock.elapsedRealtime() + CACHE_MS;
        Archive(ParcelFileDescriptor descriptor) { this.descriptor = descriptor; }
    }
    private static void clearArchives() {
        for (Archive archive : archives.values()) try { archive.descriptor.close(); } catch (IOException ignored) { }
        archives.clear();
    }
    static synchronized void configure(ContentResolver content, Uri grant, String path) {
        if (!grant.equals(tree) || !path.equals(root)) {
            resolved.clear(); directories.clear(); clearArchives();
        }
        resolver = content; tree = grant; root = path;
    }
    private static Uri document(String id) {
        return DocumentsContract.buildDocumentUriUsingTree(tree, id);
    }
    private static String[] names(String path) throws IOException {
        String canonical = new File(path).getCanonicalPath();
        if (root == null || !(canonical.equals(root) || canonical.startsWith(root + "/")))
            throw new IOException("Path is outside the selected game folder.");
        String relative = canonical.substring(root.length());
        return relative.isEmpty() ? new String[0] : relative.substring(1).split("/");
    }
    private static Uri child(Uri parent, String name) throws IOException {
        Directory snapshot = directories.get(parent);
        if (snapshot != null && SystemClock.elapsedRealtime() < snapshot.until) {
            Uri found = snapshot.children.get(name);
            if (found != null || !snapshot.backups) return found;
        }
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree,
                DocumentsContract.getDocumentId(parent));
        snapshot = new Directory();
        Uri previous = null;
        try (Cursor cursor = resolver.query(children,
                new String[]{Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME}, null, null, null)) {
            if (cursor == null) throw new IOException("Cannot read the selected folder.");
            while (cursor.moveToNext()) {
                String display = cursor.getString(1);
                if (display != null) snapshot.children.put(display, document(cursor.getString(0)));
                if (display != null && display.contains(".zenkai-previous-")) snapshot.backups = true;
                if (display != null && display.startsWith(name + ".zenkai-previous-")
                        && display.substring((name + ".zenkai-previous-").length()).matches(
                                "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"))
                    previous = document(cursor.getString(0));
            }
        }
        if (directories.size() >= CACHE_LIMIT) directories.clear();
        directories.put(parent, snapshot);
        Uri found = snapshot.children.get(name);
        if (found != null) return found;
        // Recover the original after process death between the two provider renames.
        if (previous != null) {
            Uri restored = DocumentsContract.renameDocument(resolver, previous, name);
            if (restored == null) throw new IOException("Cannot restore the previous game file: " + name);
            directories.remove(parent);
            return restored;
        }
        return null;
    }
    private static Uri find(String path) throws IOException {
        Uri result = document(DocumentsContract.getTreeDocumentId(tree));
        String current = root;
        for (String name : names(path)) {
            current += "/" + name;
            Uri cached = resolved.get(current);
            result = cached == null ? child(result, name) : cached;
            if (result == null) return null;
            resolved.put(current, result);
        }
        return result;
    }
    private static void invalidate(String path) {
        resolved.keySet().removeIf(key -> key.equals(path) || key.startsWith(path + "/"));
        directories.clear();
        // Mutations can replace archive inodes; never hand out an old descriptor.
        clearArchives();
    }
    private static Uri parent(String path) throws IOException {
        if (names(path).length == 0) throw new IOException("Cannot change the selected folder itself.");
        Uri result = find(new File(path).getParent());
        if (result == null) throw new FileNotFoundException(path);
        return result;
    }
    // These methods are called by libstorage on Java and native worker threads.
    static synchronized long[] stat(String path) throws IOException {
        Uri uri = find(path);
        if (uri == null) return null;
        try (Cursor cursor = resolver.query(uri, new String[]{Document.COLUMN_MIME_TYPE,
                Document.COLUMN_SIZE, Document.COLUMN_LAST_MODIFIED}, null, null, null)) {
            if (cursor == null || !cursor.moveToFirst()) { invalidate(path); return null; }
            return new long[]{Document.MIME_TYPE_DIR.equals(cursor.getString(0)) ? 1 : 0,
                    cursor.isNull(1) ? 0 : cursor.getLong(1), cursor.isNull(2) ? 0 : cursor.getLong(2) / 1000};
        }
    }
    static synchronized String[] list(String path) throws IOException {
        Uri uri = find(path);
        if (uri == null) throw new FileNotFoundException(path);
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, DocumentsContract.getDocumentId(uri));
        ArrayList<String> result = new ArrayList<>();
        try (Cursor cursor = resolver.query(children, new String[]{Document.COLUMN_DISPLAY_NAME}, null, null, null)) {
            if (cursor == null) throw new IOException("Cannot list game folder.");
            while (cursor.moveToNext()) {
                String name = cursor.getString(0);
                if (name == null || name.isEmpty() || name.contains("/") || name.equals(".") || name.equals(".."))
                    throw new IOException("Invalid game filename.");
                result.add(name);
            }
        }
        return result.toArray(new String[0]);
    }
    static synchronized int open(String path, int flags) throws IOException {
        boolean readArchive = (flags & (OsConstants.O_ACCMODE | OsConstants.O_CREAT | OsConstants.O_TRUNC)) == OsConstants.O_RDONLY
                && (path.endsWith(".afs") || path.endsWith(".pak"));
        if (readArchive) {
            Archive cached = archives.get(path);
            if (cached != null) {
                if (SystemClock.elapsedRealtime() < cached.until) {
                    try {
                        // dup() shares its seek position. Reopening our granted fd
                        // gives every C FILE its own cursor without provider IPC.
                        return ParcelFileDescriptor.open(new File("/proc/self/fd/" + cached.descriptor.getFd()),
                                ParcelFileDescriptor.MODE_READ_ONLY).detachFd();
                    } catch (IOException ignored) { }
                }
                archives.remove(path);
                cached.descriptor.close();
            }
        }
        Uri uri = find(path);
        boolean create = (flags & OsConstants.O_CREAT) != 0;
        if (uri != null && create && (flags & OsConstants.O_EXCL) != 0) return -OsConstants.EEXIST;
        if (uri == null) {
            if (!create) return -OsConstants.ENOENT;
            uri = DocumentsContract.createDocument(resolver, parent(path), "application/octet-stream", new File(path).getName());
            if (uri == null) throw new IOException("Cannot create game file.");
            invalidate(path);
            resolved.put(path, uri);
        }
        boolean write = (flags & OsConstants.O_ACCMODE) != OsConstants.O_RDONLY;
        if (write) invalidate(path);
        String mode = write ? ((flags & OsConstants.O_TRUNC) != 0 ? "rwt" : "rw") : "r";
        ParcelFileDescriptor fd;
        try { fd = resolver.openFileDescriptor(uri, mode); }
        catch (FileNotFoundException error) { invalidate(path); throw error; }
        if (fd == null) throw new IOException("Cannot open game file.");
        try {
            Os.lseek(fd.getFileDescriptor(), 0, OsConstants.SEEK_CUR);
            if ((flags & OsConstants.O_APPEND) != 0) {
                Os.lseek(fd.getFileDescriptor(), 0, OsConstants.SEEK_END);
            }
            if (readArchive) {
                try {
                    ParcelFileDescriptor reader = ParcelFileDescriptor.open(new File("/proc/self/fd/" + fd.getFd()),
                            ParcelFileDescriptor.MODE_READ_ONLY);
                    if (archives.size() >= CACHE_LIMIT) clearArchives();
                    archives.put(path, new Archive(fd));
                    fd = null; // Owned by the archive cache until expiry or mutation.
                    return reader.detachFd();
                } catch (IOException ignored) { /* Provider descriptors may forbid reopening. */ }
            }
            return fd.detachFd();
        } catch (ErrnoException error) { throw new IOException("Game storage must support seeking.", error); }
        finally { if (fd != null) fd.close(); }
    }
    static synchronized int mkdir(String path) throws IOException {
        if (stat(path) != null) return -OsConstants.EEXIST;
        Uri created = DocumentsContract.createDocument(resolver, parent(path), Document.MIME_TYPE_DIR,
                new File(path).getName());
        if (created == null) return -OsConstants.EIO;
        invalidate(path);
        resolved.put(path, created);
        return 0;
    }
    static synchronized int delete(String path, boolean directory) throws IOException {
        long[] info = stat(path);
        if (info == null) return -OsConstants.ENOENT;
        if (directory && info[0] == 0) return -OsConstants.ENOTDIR;
        if (!directory && info[0] != 0) return -OsConstants.EISDIR;
        if (directory && list(path).length != 0) return -OsConstants.ENOTEMPTY;
        parent(path); // Never delete the tree root, even if empty.
        Uri target = find(path);
        invalidate(path);
        return DocumentsContract.deleteDocument(resolver, target) ? 0 : -OsConstants.EIO;
    }
    static synchronized int rename(String from, String to) throws IOException {
        if (from.equals(to)) return find(from) == null ? -OsConstants.ENOENT : 0;
        Uri sourceParent = parent(from), targetParent = parent(to);
        Uri source = find(from), target = find(to), backup = null;
        if (source == null) return -OsConstants.ENOENT;
        invalidate(from); invalidate(to);
        if (!new File(from).getParent().equals(new File(to).getParent())) {
            // Legacy log migration moves an unchanged filename into logs/. Do
            // not simulate unsupported moves by copying or overwriting files.
            if (target != null || !new File(from).getName().equals(new File(to).getName())) return -OsConstants.EXDEV;
            return DocumentsContract.moveDocument(resolver, source, sourceParent, targetParent) == null ? -OsConstants.EIO : 0;
        }
        long[] sourceInfo = stat(from), targetInfo = stat(to);
        if (sourceInfo == null) return -OsConstants.ENOENT;
        if (targetInfo != null) {
            if (sourceInfo[0] != targetInfo[0]) return sourceInfo[0] == 1 ? -OsConstants.ENOTDIR : -OsConstants.EISDIR;
            if (targetInfo[0] == 1 && list(to).length != 0) return -OsConstants.ENOTEMPTY;
        }
        if (target != null) {
            invalidate(to);
            backup = DocumentsContract.renameDocument(resolver, target,
                    new File(to).getName() + ".zenkai-previous-" + UUID.randomUUID());
            if (backup == null) throw new IOException("Cannot preserve the previous game file.");
        }
        try {
            invalidate(from); invalidate(to);
            if (DocumentsContract.renameDocument(resolver, source, new File(to).getName()) == null)
                throw new IOException("Cannot publish game file.");
        } catch (IOException | RuntimeException failure) {
            if (backup != null) {
                try { DocumentsContract.renameDocument(resolver, backup, new File(to).getName()); }
                catch (Exception rollback) { failure.addSuppressed(rollback); }
            }
            throw failure;
        }
        // The new file is published. A leftover backup is safe if cleanup fails.
        if (backup != null) try { DocumentsContract.deleteDocument(resolver, backup); } catch (Exception ignored) { }
        return 0;
    }
}

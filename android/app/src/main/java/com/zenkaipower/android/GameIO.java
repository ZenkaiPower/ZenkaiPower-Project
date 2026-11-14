package com.zenkaipower.android;

import java.io.*;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/** Common Java file operations; Android installs a descriptor-backed tree adapter. */
final class GameIO {
    interface Backend {
        File file(String path);
        FileInputStream input(File file) throws IOException;
        FileOutputStream output(File file, boolean append) throws IOException;
        File temp(String name, String suffix, File parent) throws IOException;
        boolean isSymbolicLink(File file);
        void publish(File temporary, File target) throws IOException;
    }
    static Backend backend = new Backend() {
        public File file(String path) { return new File(path); }
        public FileInputStream input(File file) throws IOException { return new FileInputStream(file); }
        public FileOutputStream output(File file, boolean append) throws IOException { return new FileOutputStream(file, append); }
        public File temp(String name, String suffix, File parent) throws IOException { return File.createTempFile(name, suffix, parent); }
        public boolean isSymbolicLink(File file) { return Files.isSymbolicLink(file.toPath()); }
        public void publish(File temporary, File target) throws IOException {
            try { Files.move(temporary.toPath(), target.toPath(), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING); }
            catch (java.nio.file.AtomicMoveNotSupportedException error) {
                Files.move(temporary.toPath(), target.toPath(), StandardCopyOption.REPLACE_EXISTING);
            }
        }
    };
    static File file(String path) { return backend.file(path); }
    static File file(File parent, String child) { return file(new File(parent, child).getPath()); }
    static FileInputStream input(File file) throws IOException { return backend.input(file); }
    static FileOutputStream output(File file) throws IOException { return output(file, false); }
    static FileOutputStream output(File file, boolean append) throws IOException { return backend.output(file, append); }
    static File temp(String name, String suffix, File parent) throws IOException { return backend.temp(name, suffix, parent); }
    static boolean isSymbolicLink(File file) { return backend.isSymbolicLink(file); }
    static void publish(File temporary, File target) throws IOException { backend.publish(temporary, target); }
    static byte[] readAllBytes(File file) throws IOException {
        try (FileInputStream in = input(file); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[65536]; int n;
            while ((n = in.read(buffer)) != -1) out.write(buffer, 0, n);
            return out.toByteArray();
        }
    }
    static void write(File file, byte[] bytes) throws IOException {
        try (FileOutputStream out = output(file)) { out.write(bytes); out.getFD().sync(); }
    }
    /** Seek the granted descriptor itself, never reopen /proc/self/fd. */
    static final class Reader implements Closeable {
        private final FileInputStream stream;
        private final FileChannel channel;
        Reader(File file, String mode) throws IOException {
            if (!mode.equals("r")) throw new IOException("Reader is read-only.");
            stream = input(file); channel = stream.getChannel();
        }
        int readUnsignedByte() throws IOException { int value = stream.read(); if (value < 0) throw new EOFException(); return value; }
        void readFully(byte[] bytes) throws IOException {
            int offset = 0, count;
            while (offset < bytes.length) { count = stream.read(bytes, offset, bytes.length - offset); if (count < 0) throw new EOFException(); offset += count; }
        }
        int read(byte[] bytes, int offset, int count) throws IOException { return stream.read(bytes, offset, count); }
        long length() throws IOException { return channel.size(); }
        long getFilePointer() throws IOException { return channel.position(); }
        void seek(long offset) throws IOException { channel.position(offset); }
        public void close() throws IOException { stream.close(); }
    }
}

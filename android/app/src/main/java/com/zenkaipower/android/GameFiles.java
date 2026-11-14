package com.zenkaipower.android;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

/** Files shared with the native engine and desktop installation layout. */
final class GameFiles {
    static final String[] DIRECTORIES = {"gamedata", "licenses", "logs", "net_session", "savegame", "setup", "textures", "tools"};
    interface Progress { void file(String name); }

    static boolean ready(File root) {
        if (root == null) return false;
        boolean disc = GameIO.file(root, "gamedata/disc.pak").isFile()
                || GameIO.file(root, "gamedata/disc/SLUS_216.78").isFile()
                && GameIO.file(root, "gamedata/disc/BIN/DBZP.BIN").isFile();
        boolean text = GameIO.file(root, "gamedata/game-text.pak").isFile()
                || GameIO.file(root, "gamedata/game-text/labels.json").isFile();
        return disc && text;
    }

    static File resolveFolder(File volume, String relative) throws IOException {
        File base = volume.getCanonicalFile();
        File root = GameIO.file(base, relative).getCanonicalFile();
        if (relative.isEmpty() || !root.getPath().startsWith(base.getPath() + File.separator))
            throw new IOException("Choose a game folder, not the storage root.");
        if (root.equals(GameIO.file(base, "Android")) || root.getPath().startsWith(GameIO.file(base, "Android").getPath() + File.separator))
            throw new IOException("Choose a folder outside Android's protected directories.");
        if (root.getName().equalsIgnoreCase("gamedata") || root.getName().equalsIgnoreCase("savegame"))
            throw new IOException("Choose the parent folder containing gamedata and savegame.");
        return root;
    }

    static void prepare(File root) throws IOException {
        mkdir(root);
        File probe = GameIO.temp(".zenkai-write-", ".tmp", root);
        if (!probe.delete()) throw new IOException("Cannot write to the selected folder.");
        for (String name : DIRECTORIES) mkdir(GameIO.file(root, name));
    }

    static void packResources(File root) throws IOException {
        for (String name : new String[] {"fonts", "translations", "gamedata/game-text", "gamedata/disc", "gamedata/touch-buttons"})
            GamePak.packLoose(GameIO.file(root, name));
        for (String name : new String[] {"texture", "songs", "stages", "mods"}) {
            File old = GameIO.file(root, name);
            if (old.isDirectory()) old.delete(); // Remove only mistakenly created empty folders.
        }
        for (String suffix : new String[] {"dat", "rel", "mem"}) {
            File old = GameIO.file(root, "libzenkaipower.so." + suffix);
            if (old.exists() && !old.delete()) throw new IOException("Cannot remove obsolete engine metadata");
        }
        for (String name : new String[] {"zenkaipower_log.txt", "zenkaipower_log.1.txt", "zenkaipower_log.2.txt"}) {
            File old = GameIO.file(root, name), target = GameIO.file(root, "logs/" + name);
            if (old.isFile() && !target.exists() && !old.renameTo(target)) throw new IOException("Cannot move old log");
        }
    }

    static void mkdir(File folder) throws IOException {
        if (!folder.isDirectory() && !folder.mkdirs()) throw new IOException("Cannot create " + folder);
    }

    /** Preserve existing destination files, including saves and customized settings. */
    static void copyInstallation(File source, File destination, Progress progress) throws IOException {
        File from = source.getCanonicalFile(), to = destination.getCanonicalFile();
        if (from.equals(to) || to.getPath().startsWith(from.getPath() + File.separator)
                || from.getPath().startsWith(to.getPath() + File.separator))
            throw new IOException("The source and destination folders must be separate.");
        for (String name : DIRECTORIES) copyMissing(GameIO.file(from, name), GameIO.file(to, name), progress);
    }

    private static void copyMissing(File source, File destination, Progress progress) throws IOException {
        if (!source.exists()) return;
        if (GameIO.isSymbolicLink(source) || GameIO.isSymbolicLink(destination))
            throw new IOException("Cannot copy a linked game path: " + source.getName());
        if (source.isDirectory()) {
            mkdir(destination);
            File[] children = source.listFiles();
            if (children == null) throw new IOException("Cannot read " + source);
            for (File child : children) copyMissing(child, GameIO.file(destination, child.getName()), progress);
            return;
        }
        if (destination.exists()) return;
        progress.file(source.getName());
        mkdir(destination.getParentFile());
        if (destination.getParentFile().getUsableSpace() < source.length())
            throw new IOException("Not enough free space to copy " + source.getName());
        File temp = GameIO.temp(".zenkai-copy-", ".part", destination.getParentFile());
        try {
            try (FileInputStream in = GameIO.input(source); FileOutputStream out = GameIO.output(temp)) {
                byte[] buffer = new byte[262144]; int n;
                while ((n = in.read(buffer)) != -1) out.write(buffer, 0, n);
                out.getFD().sync();
            }
            if (!temp.renameTo(destination)) throw new IOException("Cannot copy " + destination.getName());
        } finally { if (temp.exists()) temp.delete(); }
    }
}

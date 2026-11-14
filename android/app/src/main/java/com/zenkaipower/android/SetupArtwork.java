package com.zenkaipower.android;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

/** Optional artwork in the selected game folder, including document-tree storage. */
final class SetupArtwork {
    static native int verify(String root);
    private static final String[] DISCS = {"SLUS-21678", "SLES-54945", "RDSPAF", "SLPS-25815"};
    static Bitmap load(File root, int disc, int kind) {
        if (disc < 0 || disc >= DISCS.length || kind < 0 || kind > 4) return null;
        String name = kind == 0 ? "cover.jpg" : DISCS[disc] +
            (kind == 2 ? "-front.png" : kind == 3 ? "-back.png" : "-disc.png");
        File file = GameIO.file(root, "setup/" + name);
        try {
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inJustDecodeBounds = true;
            try (FileInputStream input = GameIO.input(file)) {
                long size = input.getChannel().size();
                if (size < 33 || size > 16 * 1024 * 1024) return null;
                BitmapFactory.decodeStream(input, null, options);
            }
            if (!(kind == 0 ? "image/jpeg" : "image/png").equals(options.outMimeType) || options.outWidth <= 0 ||
                options.outHeight <= 0 || options.outWidth > 4096 || options.outHeight > 4096) return null;
            int limit = kind == 1 ? 128 : kind == 0 ? 600 : 4096;
            options.inSampleSize = 1;
            while (Math.max(options.outWidth, options.outHeight) / options.inSampleSize > limit)
                options.inSampleSize *= 2;
            options.inJustDecodeBounds = false;
            try (FileInputStream input = GameIO.input(file)) {
                return BitmapFactory.decodeStream(input, null, options);
            }
        } catch (IOException | SecurityException exception) {
            return null;
        }
    }
}

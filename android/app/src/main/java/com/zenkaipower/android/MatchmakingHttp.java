package com.zenkaipower.android;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/** Blocking transport called only by the native matchmaking worker threads. */
final class MatchmakingHttp {
    static byte[] request(String address, String body, String[] headers, int limit) {
        HttpURLConnection connection = null;
        try {
            URL url = new URL(address);
            if (!url.getProtocol().equals("https")) return null;
            connection = (HttpURLConnection)url.openConnection();
            connection.setConnectTimeout(8000);
            connection.setReadTimeout(12000);
            connection.setRequestProperty("User-Agent", "ZenkaiPower-Android");
            if (headers != null) for (String header : headers) {
                int colon = header.indexOf(':');
                if (colon > 0) connection.setRequestProperty(header.substring(0, colon), header.substring(colon + 1).trim());
            }
            if (body != null) {
                byte[] bytes = body.getBytes(StandardCharsets.UTF_8);
                connection.setRequestMethod("POST");
                connection.setRequestProperty("Content-Type", "application/json");
                connection.setDoOutput(true);
                connection.setFixedLengthStreamingMode(bytes.length);
                try (OutputStream output = connection.getOutputStream()) { output.write(bytes); }
            }
            int status = connection.getResponseCode();
            android.util.Log.i("ZenkaiPower", "Matchmaking HTTPS " + url.getHost() + url.getPath() + " status=" + status);
            InputStream response = status >= 400 ? connection.getErrorStream() : connection.getInputStream();
            if (response == null) return null;
            try (InputStream input = response; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[4096];
                while (output.size() < limit) {
                    int count = input.read(buffer, 0, Math.min(buffer.length, limit - output.size()));
                    if (count < 0) break;
                    output.write(buffer, 0, count);
                }
                return output.toByteArray();
            }
        } catch (java.io.IOException error) {
            android.util.Log.w("ZenkaiPower", "Matchmaking HTTPS failed: " + error.getClass().getSimpleName());
            return null;
        } finally { if (connection != null) connection.disconnect(); }
    }
}

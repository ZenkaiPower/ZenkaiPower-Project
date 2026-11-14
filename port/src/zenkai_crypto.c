/* Unencrypted binary v3 saves with checksums and atomic replacement.
 * Legacy v2 decryption is retained only for reading/migration. Neither version
 * proves ownership or honest progression; checksums detect accidental damage. */
#include "zenkai_crypto.h"
#include "port_pack.h"
#include "../third_party/monocypher/monocypher.c"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define ZENKAI_MAX_SAVE (1024 * 1024)
#define HEADER_V2 96
#define HEADER_V3 88
#define SAVE_ID_BYTES 16
#define SAVE_HASH_BYTES 32

static const uint8_t sLegacyReadKey[32] = {
    0xa1,0xe4,0x49,0x78,0xa6,0x8f,0x23,0x59,0xf1,0x32,0xb7,0x04,0x4d,0x9e,0x57,0xc2,
    0xf0,0x36,0xb5,0x8e,0x75,0x1d,0xc7,0xa9,0x0a,0xd6,0x63,0x20,0x51,0xef,0x84,0x3b
};
/* v2: magic[8] | save_id[16] | BLAKE2b-256(plaintext)[32] |
 *     nonce[24] | tag[16] | ciphertext
 * The first 56 bytes, including BOTH metadata values, are AEAD associated
 * data. Thus callers never receive unauthenticated ID/hash values. */
static const uint8_t sMagicV2[8] = {'Z','E','N','K','A','I',0,2};
/* v3: magic[8] | save_id[16] | BLAKE2b-256(payload)[32] |
 * BLAKE2b-256(first 56 header bytes + payload)[32] | binary payload.
 * The unkeyed checksum covers metadata too; it is not authentication. */
static const uint8_t sMagicV3[8] = {'Z','E','N','K','A','I',0,3};
/* Set by Save_SetDefaults(), cleared by loading an existing main save or once
 * a new main save has been successfully flushed. A create flag alone cannot
 * distinguish overwrite of an existing game from initialization of a new one. */
static int sNewSavePending;

static int random_bytes(uint8_t *out, size_t size) {
#ifdef _WIN32
    typedef BOOLEAN (WINAPI *RandomFn)(void *, ULONG);
    HMODULE lib = LoadLibraryA("advapi32.dll");
    RandomFn fn = lib ? (RandomFn)(void *)GetProcAddress(lib, "SystemFunction036") : NULL;
    int ok = fn && fn(out, (ULONG)size);
    if (lib) FreeLibrary(lib);
    return ok;
#else
    FILE *f = fopen("/dev/urandom", "rb");
    int ok = f && fread(out, 1, size, f) == size;
    if (f) fclose(f);
    return ok;
#endif
}

static int main_save(const char *path) {
    const char *last = strrchr(path, '/');
#ifdef _WIN32
    const char *back = strrchr(path, '\\');
    if (back && (!last || back > last)) last = back;
#endif
    return strcmp(last ? last + 1 : path, "zenkaipower.sav") == 0;
}

void Port_ZenkaiNewGame(void) { sNewSavePending = 1; }
void Port_ZenkaiExistingSaveLoaded(void) { sNewSavePending = 0; }

static void envelope_checksum(uint8_t out[32], const uint8_t *header, const uint8_t *bytes, size_t length) {
    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, 32);
    crypto_blake2b_update(&ctx, header, 56);
    crypto_blake2b_update(&ctx, bytes, length);
    crypto_blake2b_final(&ctx, out);
}

/* Validate checksums (or legacy AEAD) before returning metadata or payload. */
static int load_envelope(const char *path, FILE **plain, uint8_t id[SAVE_ID_BYTES],
                         uint8_t hash[SAVE_HASH_BYTES]) {
    FILE *disk = fopen(path, "rb"), *out = NULL;
    uint8_t header[HEADER_V2], *bytes = NULL, calculated[SAVE_HASH_BYTES];
    long total, length = 0, header_size;
    int ok = 0;
    if (!disk) return 0;
    if (fseek(disk, 0, SEEK_END) || (total = ftell(disk)) < HEADER_V3 ||
        total > ZENKAI_MAX_SAVE + HEADER_V2 || fseek(disk, 0, SEEK_SET)) goto done;
    if (fread(header, 1, 8, disk) != 8) goto done;
    header_size = !memcmp(header, sMagicV3, 8) ? HEADER_V3 : !memcmp(header, sMagicV2, 8) ? HEADER_V2 : 0;
    if (!header_size || total < header_size ||
        fread(header + 8, 1, header_size - 8, disk) != (size_t)header_size - 8) goto done;
    length = total - header_size;
    if (length > ZENKAI_MAX_SAVE) goto done;
    bytes = malloc((size_t)length + 1);
    if (!bytes || fread(bytes, 1, (size_t)length, disk) != (size_t)length || ferror(disk)) goto done;
    if (header_size == HEADER_V2) {
        if (crypto_aead_unlock(bytes, header + 80, sLegacyReadKey, header + 56, header,
                               56, bytes, (size_t)length)) goto done;
    } else {
        envelope_checksum(calculated, header, bytes, (size_t)length);
        if (memcmp(calculated, header + 56, sizeof(calculated))) goto done;
    }
    crypto_blake2b(calculated, sizeof(calculated), bytes, (size_t)length);
    if (memcmp(calculated, header + 24, sizeof(calculated))) goto done;
    if (plain) {
        out = Port_PackTempFile();
        if (!out || fwrite(bytes, 1, (size_t)length, out) != (size_t)length || fseek(out, 0, SEEK_SET)) goto done;
    }
    if (id) memcpy(id, header + 8, SAVE_ID_BYTES);
    if (hash) memcpy(hash, header + 24, SAVE_HASH_BYTES);
    if (plain) { *plain = out; out = NULL; }
    ok = 1;
done:
    if (out) fclose(out);
    if (bytes) { crypto_wipe(bytes, (size_t)length); free(bytes); }
    crypto_wipe(calculated, sizeof(calculated));
    fclose(disk);
    return ok;
}

FILE *Port_ZenkaiOpen(const char *path, int mode) {
    FILE *plain = NULL;
    if (load_envelope(path, &plain, NULL, NULL)) return plain;
    /* Deliberate create/overwrite can replace a missing or damaged save. */
    return (mode & 0x200) ? Port_PackTempFile() : NULL;
}

static void to_hex(char *out, const uint8_t *bytes, size_t size) {
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < size; i++) {
        out[2 * i] = digits[bytes[i] >> 4];
        out[2 * i + 1] = digits[bytes[i] & 15];
    }
    out[2 * size] = 0;
}

int Port_ZenkaiMetadata(const char *path, char save_id[33], char save_hash[65]) {
    uint8_t id[SAVE_ID_BYTES], hash[SAVE_HASH_BYTES];
    int ok;
    if (save_id) save_id[0] = 0;
    if (save_hash) save_hash[0] = 0;
    if (!save_id || !save_hash || !main_save(path)) return 0;
    ok = load_envelope(path, NULL, id, hash);
    if (ok) { to_hex(save_id, id, sizeof(id)); to_hex(save_hash, hash, sizeof(hash)); }
    crypto_wipe(id, sizeof(id)); crypto_wipe(hash, sizeof(hash));
    return ok;
}

int Port_ZenkaiFlush(const char *path, FILE *plain) {
    uint8_t header[HEADER_V3], *bytes = NULL;
    uint8_t prior_id[SAVE_ID_BYTES];
    char temporary[640];
    FILE *disk = NULL;
    long pos = ftell(plain), length;
    int ok = 0, is_main = main_save(path);
    if (fflush(plain) || fseek(plain, 0, SEEK_END) || (length = ftell(plain)) < 0 || length > ZENKAI_MAX_SAVE) goto done;
    rewind(plain);
    bytes = malloc((size_t)length + 1);
    if (!bytes || fread(bytes, 1, (size_t)length, plain) != (size_t)length) goto done;
    memcpy(header, sMagicV3, 8);
    if (!(is_main && sNewSavePending) && load_envelope(path, NULL, prior_id, NULL))
        memcpy(header + 8, prior_id, SAVE_ID_BYTES);
    else if (!random_bytes(header + 8, SAVE_ID_BYTES)) goto done;
    crypto_blake2b(header + 24, SAVE_HASH_BYTES, bytes, (size_t)length);
    envelope_checksum(header + 56, header, bytes, (size_t)length);
    if (snprintf(temporary, sizeof(temporary), "%s.part", path) >= (int)sizeof(temporary)) goto done;
    disk = fopen(temporary, "wb");
    if (!disk) goto done;
    ok = fwrite(header, 1, HEADER_V3, disk) == HEADER_V3 &&
         fwrite(bytes, 1, (size_t)length, disk) == (size_t)length;
    if (fclose(disk)) ok = 0;
    disk = NULL;
#ifdef _WIN32
    if (ok) ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (ok) ok = rename(temporary, path) == 0;
#endif
    if (!ok) remove(temporary);
    if (ok && is_main) sNewSavePending = 0;
done:
    if (bytes) { crypto_wipe(bytes, (size_t)length); free(bytes); }
    crypto_wipe(prior_id, sizeof(prior_id));
    if (disk) fclose(disk);
    if (pos >= 0) fseek(plain, pos, SEEK_SET);
    return ok;
}

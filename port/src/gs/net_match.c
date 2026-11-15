#include "../port_save.h"
#include "../port_pack.h"
/*
 * Matchmaking with room codes: the client side of server/src/worker.js.
 *
 * The lobby (net.c) opens the UDP socket the match will use and hands it over. A thread then:
 *   1. asks a STUN server, from that socket, which address and port the rest of the internet sees it under;
 *   2. tells the Worker (host: "make a room", and then asks every two seconds whether someone has joined; the one who
 *      joins: "join room <code>");
 *   3. ends with the other player's addresses, which the lobby sends its greetings to (both sides at once: that is
 *      what lets the two routers pass each other's packets).
 * The Worker is spoken to over HTTPS with what the system has: WinHTTP on Windows, the curl library on Linux, and
 * failing those the `curl` program (ZENKAIPOWER_MATCH_CURL=1 asks for the program). No game data goes that way.
 *
 * ZENKAIPOWER_MATCH_URL=<address> names another Worker (one's own; see server/README.md).
 *
 * A program built with a build key (net_sign.c) signs every request to the Worker: three headers, X-ZENKAIPOWER-Release,
 * X-ZENKAIPOWER-Time and X-ZENKAIPOWER-Sign. One built without sends none of them. The request to GitHub (the update check) is
 * never signed.
 */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define MATCH_URL_DEFAULT "https://zenkaipower-server.zenkaipower.workers.dev"
#include "net_match.h"
#include "../port_release.h"
#include "net_relay.h"
#include "net_sign.h"

static struct {
    SDL_Mutex *lock;
    SDL_Thread *thread;
    SDL_AtomicInt cancel, gen; /* gen: which search is the current one (a thread of an earlier one writes nothing) */
    SDL_AtomicInt sockBusy;    /* the thread is talking to the relay on the match's socket: the lobby keeps off it */
    MatchStatus st;
    int role, sock, localPort, version;
    char code[8], name[20]; /* (role 3, a quick match: `code` is the queue's name until the service gives a room) */
    char queue[8];
} sM;

typedef struct StatsJob {
    struct StatsJob *next;
    char action[24], body[2048], session[40];
} StatsJob;
static SDL_Mutex *sStatsLock;
static StatsJob *sStatsHead, *sStatsTail;
static int sStatsRunning;
static struct {
    char session[40], key[40], uid[40], accountToken[65];
    unsigned round;
    int host, active, ranked, totalsReady;
    int totals[2][2][3], rankPoints[2];
    int scoringReady, startPoints[2], tolerance, finalReady, finalDelta[2];
    Uint64 refreshAt;
} sStats;

/* Reward accounting lives outside the snapshotted game heap. A rematch has a
   new battle UUID; repainting or correcting the same result replaces its gain. */
typedef struct RewardRound {
    struct RewardRound *next;
    char uid[40];
    int gain;
} RewardRound;
static SDL_SpinLock sRewardLock;
static RewardRound *sRewardRounds;
static int sRewardOpen;

void Port_NetRewardBegin(void) {
    SDL_LockSpinlock(&sRewardLock);
    while (sRewardRounds) {
        RewardRound *old = sRewardRounds;
        sRewardRounds = old->next;
        SDL_free(old);
    }
    sRewardOpen = 1;
    SDL_UnlockSpinlock(&sRewardLock);
}

void Port_NetRewardRecord(int change) {
    RewardRound *round;
    char uid[40];
    int ranked;
    if (!sStatsLock) return;
    SDL_LockMutex(sStatsLock);
    ranked = sStats.ranked;
    SDL_strlcpy(uid, sStats.uid, sizeof(uid));
    SDL_UnlockMutex(sStatsLock);
    if (!ranked || !uid[0]) return;
    SDL_LockSpinlock(&sRewardLock);
    if (sRewardOpen) {
        for (round = sRewardRounds; round && SDL_strcmp(round->uid, uid); round = round->next) {}
        if (!round) {
            round = SDL_calloc(1, sizeof(*round));
            if (round) {
                SDL_strlcpy(round->uid, uid, sizeof(round->uid));
                round->next = sRewardRounds;
                sRewardRounds = round;
            }
        }
        if (round) round->gain = change > 0 ? change : 0;
    }
    SDL_UnlockSpinlock(&sRewardLock);
}

int Port_NetRewardTake(void) {
    int points = 0;
    SDL_LockSpinlock(&sRewardLock);
    while (sRewardRounds) {
        RewardRound *old = sRewardRounds;
        /* Keep the Z award within nine displayed digits and signed 32-bit arithmetic. */
        int room = 9999999 - points;
        points += old->gain > room ? room : old->gain;
        sRewardRounds = old->next;
        SDL_free(old);
    }
    sRewardOpen = 0;
    SDL_UnlockSpinlock(&sRewardLock);
    return points;
}

/* Quick match: what this game calls itself to the service (so that a room it left behind is not given back to
   it): sixteen letters made up once per run. */
static const char *match_who(void) {
    static char who[20];
    if (who[0] == '\0') {
        int i;
        SDL_srand(SDL_GetTicksNS() ^ (Uint64)(uintptr_t)&who);
        for (i = 0; i < 16; i++) {
            who[i] = (char)('a' + SDL_rand(26));
        }
        who[16] = '\0';
    }
    return who;
}

static SDL_TLSID sMine; /* the thread's own search number */
#define STOPPED() (SDL_GetAtomicInt(&sM.cancel) || (int)(intptr_t)SDL_GetTLS(&sMine) != SDL_GetAtomicInt(&sM.gen))

static const char *match_url(void) {
    static SDL_SpinLock lock;
    static int loaded;
    static char address[1024];
    const char *e = SDL_getenv("ZENKAIPOWER_MATCH_URL");
    if (e && e[0]) return e;
    SDL_LockSpinlock(&lock);
    if (!loaded) {
        const char *base = SDL_GetBasePath();
        char path[4096], line[1026];
        FILE *file = NULL;
        if (base) {
            SDL_snprintf(path, sizeof(path), "%sdragon-net-battle/server.txt", base);
            file = Port_PackFopen(path, "rb");
        }
        /* Also support running a development build from the repository root. */
        if (!file) file = Port_PackFopen("./dragon-net-battle/server.txt", "rb");
        if (!file) file = fopen("dragon-net-battle.txt", "rb");
        if (file) {
            if (fgets(line, sizeof(line), file)) {
                char *start = line, *end;
                while (*start == ' ' || *start == '\t') start++;
                end = start + strlen(start);
                while (end > start && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
                while (end > start && end[-1] == '/') *--end = 0;
                if (strlen(start) < sizeof(address) &&
                    (!strncmp(start, "https://", 8) || !strncmp(start, "http://", 7)) &&
                    !strpbrk(start, " \t\r\n"))
                    SDL_strlcpy(address, start, sizeof(address));
                else fprintf(stderr, "zenkaipower: invalid Dragon Net server address; using default\n");
            }
            fclose(file);
        }
        loaded = 1;
    }
    SDL_UnlockSpinlock(&lock);
    return address[0] ? address : MATCH_URL_DEFAULT;
}

/* Account credentials belong to this matchmaking URL, never to the P2P peer. */
#include "../../include/port_account.h"
#include "../../include/port_username.h"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include "menu_translation.h"
typedef struct AccountLogin {
    char magic[8], url[256], name[20], token[65];
    long long expires;
} AccountLogin;
static struct {
    SDL_SpinLock lock;
    int loaded, state, validated;
    AccountLogin login;
    char pendingName[20], pendingTicket[65];
    char text[256];
} sAccount;

typedef struct AccountRequest {
    char name[20], code[8];
    int restore, confirm;
    char ticket[65];
} AccountRequest;

static void account_quote(const char *src, char *out, size_t capacity) {
    size_t n = 0;
    while (*src && n + 2 < capacity) {
        unsigned char c = (unsigned char)*src++;
        if (c < 32 || c == 127) continue;
        if (c == '"' || c == '\\') out[n++] = '\\';
        out[n++] = (char)c;
    }
    out[n] = 0;
}

static int account_token_valid(const char *token) {
    size_t i;
    if (SDL_strlen(token) != 64) return 0;
    for (i = 0; i < 64; i++) if (!((token[i] >= '0' && token[i] <= '9') || (token[i] >= 'a' && token[i] <= 'f'))) return 0;
    return 1;
}

static const char *account_path(void) {
    const char *path = SDL_getenv("ZENKAIPOWER_ACCOUNT_FILE");
    return path && path[0] ? path : Port_SaveUserFile("net_account.dat", "savegame/net_account.dat");
}

static void account_load(void) {
    FILE *fp;
    SDL_LockSpinlock(&sAccount.lock);
    if (!sAccount.loaded) {
        sAccount.loaded = 1;
        fp = fopen(account_path(), "rb");
        if (fp) {
            AccountLogin value;
            if (fread(&value, 1, sizeof(value), fp) == sizeof(value) &&
                !memcmp(value.magic, "BT3ACT1", 8) &&
                memchr(value.name, 0, sizeof(value.name)) && memchr(value.url, 0, sizeof(value.url)) &&
                memchr(value.token, 0, sizeof(value.token)) && account_token_valid(value.token) &&
                value.expires > (long long)time(NULL) * 1000)
                sAccount.login = value;
            fclose(fp);
        }
    }
    SDL_UnlockSpinlock(&sAccount.lock);
}

static int account_save(const AccountLogin *value) {
    char path[1200];
    FILE *fp;
    int ok;
    if (SDL_snprintf(path, sizeof(path), "%s.%llu.tmp", account_path(),
                     (unsigned long long)SDL_GetTicksNS()) >= (int)sizeof(path)) return 0;
#ifdef _WIN32
    {
        int fd = _open(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
        fp = fd >= 0 ? _fdopen(fd, "wb") : NULL;
        if (!fp && fd >= 0) _close(fd);
    }
#else
    {
        int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        fp = fd >= 0 ? fdopen(fd, "wb") : NULL;
        if (!fp && fd >= 0) close(fd);
    }
#endif
    if (!fp) return 0;
    ok = fwrite(value, 1, sizeof(*value), fp) == sizeof(*value) && fflush(fp) == 0;
#ifdef _WIN32
    if (ok) ok = _commit(_fileno(fp)) == 0;
#else
    if (ok) ok = fsync(fileno(fp)) == 0;
#endif
    if (fclose(fp)) ok = 0;
#ifdef _WIN32
    if (ok) ok = MoveFileExA(path, account_path(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (ok) ok = rename(path, account_path()) == 0;
#endif
    if (!ok) remove(path);
    return ok;
}

static void account_token_copy(char token[65]) {
    account_load();
    SDL_LockSpinlock(&sAccount.lock);
    token[0] = 0;
    if (sAccount.login.expires > (long long)time(NULL) * 1000 && !strcmp(sAccount.login.url, match_url()))
        SDL_strlcpy(token, sAccount.login.token, 65);
    SDL_UnlockSpinlock(&sAccount.lock);
}

static int account_body(const char *json, const char *token, char *out, size_t size) {
    size_t n = SDL_strlen(json);
    if (!token[0] || SDL_strstr(json, "\"account_token\":") || n < 2 || json[n-1] != '}') return 0;
    if (n + 85 >= size) return 0;
    SDL_snprintf(out, size, "%.*s,\"account_token\":\"%s\"}", (int)n-1, json, token);
    return 1;
}


int Port_AccountLogged(const char *name) {
    int yes;
    account_load();
    SDL_LockSpinlock(&sAccount.lock);
    yes = sAccount.validated && !SDL_strcasecmp(name, sAccount.login.name) &&
          !strcmp(sAccount.login.url, match_url()) && sAccount.login.expires > (long long)time(NULL) * 1000;
    SDL_UnlockSpinlock(&sAccount.lock);
    return yes;
}

int Port_AccountPoll(char *text, int capacity) {
    int state;
    SDL_LockSpinlock(&sAccount.lock);
    state = sAccount.state;
    if (text && capacity > 0) SDL_strlcpy(text, sAccount.text, (size_t)capacity);
    SDL_UnlockSpinlock(&sAccount.lock);
    return state;
}

static int SDLCALL account_thread(void *arg);
int Port_AccountAccept(void) {
    AccountRequest *request;
    SDL_Thread *thread;
    SDL_LockSpinlock(&sAccount.lock);
    if (sAccount.state != 3) { SDL_UnlockSpinlock(&sAccount.lock); return 0; }
    request = SDL_calloc(1, sizeof(*request));
    if (!request) { SDL_UnlockSpinlock(&sAccount.lock); return 0; }
    SDL_strlcpy(request->name, sAccount.pendingName, sizeof(request->name));
    SDL_strlcpy(request->ticket, sAccount.pendingTicket, sizeof(request->ticket));
    request->confirm = 1;
    sAccount.state = 1;
    SDL_UnlockSpinlock(&sAccount.lock);
    thread = SDL_CreateThread(account_thread, "zenkaipower-register", request);
    if (thread) { SDL_DetachThread(thread); return 1; }
    SDL_free(request);
    SDL_LockSpinlock(&sAccount.lock); sAccount.state = 3; SDL_UnlockSpinlock(&sAccount.lock);
    return 0;
}
void Port_AccountDiscard(void) {
    SDL_LockSpinlock(&sAccount.lock);
    if (sAccount.state == 3) {
        SDL_zero(sAccount.pendingName); SDL_zero(sAccount.pendingTicket);
        sAccount.text[0] = 0; sAccount.state = 0;
    }
    SDL_UnlockSpinlock(&sAccount.lock);
}

static int http_post(const char *, const char *, char *, size_t);
static int json_str(const char *, const char *, char *, size_t);
static int SDLCALL account_thread(void *arg) {
    AccountRequest request = *(AccountRequest *)arg;
    AccountLogin value;
    char body[512], answer[1024], quoted[48], token[65], message[256] = "";
    const char *expiry;
    int state = -1, valid = 0;
    SDL_free(arg);
    SDL_memset(&value, 0, sizeof(value));
    account_quote(request.name, quoted, sizeof(quoted));
    if (request.restore) {
        account_token_copy(token);
        SDL_snprintf(body, sizeof(body), "{\"account_token\":\"%s\"}", token);
        if (http_post("account/session", body, answer, sizeof(answer)) &&
            SDL_strstr(answer, "\"active\":true") && !SDL_strstr(answer, "\"error\"")) {
            char name[20];
            if (json_str(answer, "username", name, sizeof(name)) && !SDL_strcasecmp(name, request.name)) {
                state = 4; valid = 1;
            }
        } else {
            state = 0;
        }
        goto done;
    }
    if (request.confirm) {
        SDL_snprintf(body, sizeof(body), "{\"ticket\":\"%s\"}", request.ticket);
        if (!http_post("account/confirm", body, answer, sizeof(answer))) goto unavailable;
        if (SDL_strstr(answer, "\"error\"")) goto rejected;
        state = 4;
    } else if (!request.code[0]) {
        SDL_snprintf(body, sizeof(body), "{\"username\":\"%s\"}", quoted);
        if (!http_post("account/lookup", body, answer, sizeof(answer))) goto unavailable;
        if (SDL_strstr(answer, "\"error\"")) goto rejected;
        if (SDL_strstr(answer, "\"exists\":true")) {
            if (!SDL_strstr(answer, "\"active\":true")) {
                Port_AccountUiText("account/inactive", message, sizeof(message)); goto done;
            }
            state = 2; goto done;
        }
        if (!SDL_strstr(answer, "\"exists\":false")) goto unavailable;
        if (!http_post("account/prepare", body, answer, sizeof(answer))) goto unavailable;
        /* Another player may have registered this name between lookup and registration. */
        if (SDL_strstr(answer, "username already exists")) { state = 2; goto done; }
        if (SDL_strstr(answer, "\"error\"")) goto rejected;
        if (!json_str(answer, "code", message, 8) || SDL_strlen(message) != 6) goto unavailable;
        if (!json_str(answer, "ticket", value.token, sizeof(value.token)) || !account_token_valid(value.token)) goto unavailable;
        SDL_strlcpy(value.name, request.name, sizeof(value.name));
        state = 3;
        goto done;
    } else {
        SDL_snprintf(body, sizeof(body), "{\"username\":\"%s\",\"code\":\"%s\"}", quoted, request.code);
        if (!http_post("account/login", body, answer, sizeof(answer))) goto unavailable;
        if (SDL_strstr(answer, "\"error\"")) goto rejected;
        state = 4;
    }
    if (!json_str(answer, "token", value.token, sizeof(value.token)) || !account_token_valid(value.token) ||
        !json_str(answer, "username", value.name, sizeof(value.name))) goto unavailable;
    expiry = SDL_strstr(answer, "\"expires\":");
    if (!expiry) goto unavailable;
    value.expires = SDL_strtoll(expiry + 10, NULL, 10);
    if (value.expires <= (long long)time(NULL) * 1000) goto unavailable;
    SDL_memcpy(value.magic, "BT3ACT1", 8);
    SDL_strlcpy(value.url, match_url(), sizeof(value.url));
    if (state == 4 && !account_save(&value)) {
        Port_AccountUiText("account/save-error", message, sizeof(message)); state = -1;
    }
    goto done;
rejected:
    Port_AccountUiText(SDL_strstr(answer, "too many") ? "account/throttled" : "account/login-error", message, sizeof(message));
    state = -1; goto done;
unavailable:
    Port_AccountUiText("account/unavailable", message, sizeof(message)); state = -1;
done:
    SDL_LockSpinlock(&sAccount.lock);
    if (request.restore) sAccount.validated = valid;
    else if (state == 3) {
        SDL_strlcpy(sAccount.pendingName, value.name, sizeof(sAccount.pendingName));
        SDL_strlcpy(sAccount.pendingTicket, value.token, sizeof(sAccount.pendingTicket));
    }
    else if (state == 4) { sAccount.login = value; sAccount.validated = 1;
        SDL_zero(sAccount.pendingName); SDL_zero(sAccount.pendingTicket);
    }
    sAccount.state = state;
    SDL_strlcpy(sAccount.text, message, sizeof(sAccount.text));
    SDL_UnlockSpinlock(&sAccount.lock);
    SDL_memset(&value, 0, sizeof(value));
    return 0;
}

static int account_begin(const char *name, const char *code, int restore) {
    AccountRequest *request;
    SDL_Thread *thread;
    const char *error = Port_UsernameError(name);
    char message[256];
    if (error) Port_AccountUiText(error, message, sizeof(message));
    account_load();
    SDL_LockSpinlock(&sAccount.lock);
    if (sAccount.state == 1 || sAccount.state == 3) { SDL_UnlockSpinlock(&sAccount.lock); return 0; }
    if (error) {
        sAccount.state = -1;
        SDL_strlcpy(sAccount.text, message, sizeof(sAccount.text));
        SDL_UnlockSpinlock(&sAccount.lock);
        return 0;
    }
    request = SDL_calloc(1, sizeof(*request));
    if (!request) { SDL_UnlockSpinlock(&sAccount.lock); return 0; }
    SDL_strlcpy(request->name, name, sizeof(request->name));
    if (code) SDL_strlcpy(request->code, code, sizeof(request->code));
    request->restore = restore;
    sAccount.state = 1;
    sAccount.text[0] = 0;
    SDL_UnlockSpinlock(&sAccount.lock);
    thread = SDL_CreateThread(account_thread, "zenkaipower-account", request);
    if (thread) SDL_DetachThread(thread);
    else {
        SDL_free(request);
        SDL_LockSpinlock(&sAccount.lock); sAccount.state = -1;
        SDL_strlcpy(sAccount.text, "Could not start account request.", sizeof(sAccount.text));
        SDL_UnlockSpinlock(&sAccount.lock);
        return 0;
    }
    return 1;
}

int Port_AccountBegin(const char *name, const char *code) { return account_begin(name, code, 0); }
void Port_AccountRestore(const char *name) {
    int restore;
    account_load();
    SDL_LockSpinlock(&sAccount.lock);
    restore = sAccount.login.token[0] && !SDL_strcasecmp(name, sAccount.login.name) && !strcmp(sAccount.login.url, match_url());
    SDL_UnlockSpinlock(&sAccount.lock);
    if (restore) account_begin(name, NULL, 1);
}

static void set_state(int state, const char *error) {
    if ((int)(intptr_t)SDL_GetTLS(&sMine) != SDL_GetAtomicInt(&sM.gen)) {
        return;
    }
    SDL_LockMutex(sM.lock);
    sM.st.state = state;
    if (error != NULL) {
        SDL_strlcpy(sM.st.error, error, sizeof(sM.st.error));
    }
    SDL_UnlockMutex(sM.lock);
}

/* ---- the Worker, through curl ---- */
/* (http_post_native: `json` NULL makes it a GET of `url`, for the update check further down. `headers`: lines
   "Name: value" to send as well, ended by a NULL, or NULL for none.) */

#ifdef _WIN32
/* Windows: its own HTTPS (WinHTTP), so nothing has to be installed and no other program is started. */
#include <winhttp.h>

static int http_post_native(const char *url, const char *json, const char *const *headers, char *out, size_t size) {
    wchar_t wurl[512], host[256], path[256], whead[512];
    char head[512];
    URL_COMPONENTS uc;
    HINTERNET ses = NULL, con = NULL, req = NULL;
    DWORD got = 0, used = 0, status = 0, len = sizeof(status);
    int ok = 0;

    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 512) == 0) {
        return 0;
    }
    SDL_strlcpy(head, "Content-Type: application/json\r\n", sizeof(head));
    for (; headers != NULL && *headers != NULL; headers++) {
        SDL_strlcat(head, *headers, sizeof(head));
        SDL_strlcat(head, "\r\n", sizeof(head));
    }
    if (MultiByteToWideChar(CP_UTF8, 0, head, -1, whead, 512) == 0) {
        return 0;
    }
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 256;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        return 0;
    }
    ses = WinHttpOpen(L"ZENKAI Power!", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (ses != NULL) {
        WinHttpSetTimeouts(ses, 8000, 8000, 8000, 12000);
        con = WinHttpConnect(ses, host, uc.nPort, 0);
    }
    if (con != NULL) {
        req = WinHttpOpenRequest(con, json != NULL ? L"POST" : L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    }
    if (req != NULL &&
        (json != NULL ? WinHttpSendRequest(req, whead, (DWORD)-1, (LPVOID)json, (DWORD)strlen(json), (DWORD)strlen(json), 0)
                      : WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) &&
        WinHttpReceiveResponse(req, NULL)) {
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
        while (used + 1 < size && WinHttpReadData(req, out + used, (DWORD)(size - 1 - used), &got) && got != 0) {
            used += got;
        }
        out[used] = '\0';
        ok = used > 0;
    }
    if (req != NULL) { WinHttpCloseHandle(req); }
    if (con != NULL) { WinHttpCloseHandle(con); }
    if (ses != NULL) { WinHttpCloseHandle(ses); }
    return ok;
}
#elif defined(__ANDROID__)
#include <jni.h>
static int http_post_native(const char *url, const char *json, const char *const *headers, char *out, size_t size) {
    JNIEnv *env = SDL_GetAndroidJNIEnv();
    if (!env || size < 2 || (*env)->PushLocalFrame(env, 32) < 0) {
        fprintf(stderr,"zenkaipower: Android HTTPS: JNI environment unavailable\n"); return 0;
    }
    jobject activity = SDL_GetAndroidActivity();
    int ok = 0, count = 0;
    if (!activity) { fprintf(stderr,"zenkaipower: Android HTTPS: activity unavailable\n"); goto done; }
    jclass type = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetMethodID(env, type, "matchmakingRequest", "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;I)[B");
    if (!method) { fprintf(stderr,"zenkaipower: Android HTTPS: Java transport method unavailable\n"); goto done; }
    while (headers && headers[count]) count++;
    jclass stringType = (*env)->FindClass(env, "java/lang/String");
    jobjectArray extra = (*env)->NewObjectArray(env, count, stringType, NULL);
    for (int i = 0; i < count; i++) {
        jstring header = (*env)->NewStringUTF(env, headers[i]);
        (*env)->SetObjectArrayElement(env, extra, i, header);
        (*env)->DeleteLocalRef(env, header);
    }
    jstring address = (*env)->NewStringUTF(env, url);
    jstring body = json ? (*env)->NewStringUTF(env, json) : NULL;
    jbyteArray response = (*env)->CallObjectMethod(env, activity, method, address, body, extra, (jint)(size - 1));
    if (!(*env)->ExceptionCheck(env) && response) {
        jsize length = (*env)->GetArrayLength(env, response);
        if (length > 0 && (size_t)length < size) {
            (*env)->GetByteArrayRegion(env, response, 0, length, (jbyte *)out);
            out[length] = 0;
            ok = !(*env)->ExceptionCheck(env);
        }
    }
done:
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionDescribe(env); (*env)->ExceptionClear(env); }
    (*env)->PopLocalFrame(env, NULL);
    return ok;
}
#else
/* Linux: the system's curl library, if it is there (it nearly always is), loaded when first needed. */
static int http_post_native(const char *url, const char *json, const char *const *headers, char *out, size_t size) {
    typedef void *(*init_fn)(void);
    typedef void *(*append_fn)(void *, const char *);
    typedef void (*freeall_fn)(void *);
    typedef int (*setopt_fn)(void *, int, ...);
    typedef int (*perform_fn)(void *);
    typedef void (*cleanup_fn)(void *);
    static SDL_SharedObject *lib;
    static int tried;
    static SDL_SpinLock initLock;
    static init_fn init;
    static setopt_fn setopt;
    static perform_fn perform;
    static cleanup_fn cleanup;
    static append_fn append;
    static freeall_fn freeall;
    void *h, *list = NULL;
    SDL_IOStream *mem;
    FILE *fp;
    int rc;
    long n;

    SDL_LockSpinlock(&initLock);
    if (!tried) {
        static const char *const names[] = {"libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.so"};
        size_t i;
        tried = 1;
        for (i = 0; lib == NULL && i < sizeof(names) / sizeof(names[0]); i++) {
            lib = SDL_LoadObject(names[i]);
        }
        if (lib != NULL) {
            init = (init_fn)SDL_LoadFunction(lib, "curl_easy_init");
            setopt = (setopt_fn)SDL_LoadFunction(lib, "curl_easy_setopt");
            perform = (perform_fn)SDL_LoadFunction(lib, "curl_easy_perform");
            cleanup = (cleanup_fn)SDL_LoadFunction(lib, "curl_easy_cleanup");
            append = (append_fn)SDL_LoadFunction(lib, "curl_slist_append");
            freeall = (freeall_fn)SDL_LoadFunction(lib, "curl_slist_free_all");
        }
    }
    SDL_UnlockSpinlock(&initLock);
    (void)mem;
    if (init == NULL || setopt == NULL || perform == NULL || cleanup == NULL || (headers != NULL && (append == NULL || freeall == NULL)) || (h = init()) == NULL) {
        return 0;
    }
    fp = tmpfile(); /* (the library writes the answer with fwrite by default: into a file that is never on disk by name) */
    if (fp == NULL) {
        cleanup(h);
        return 0;
    }
    setopt(h, 10002 /* CURLOPT_URL */, url);
    if (json != NULL) {
        setopt(h, 10015 /* CURLOPT_POSTFIELDS */, json);
    } else { /* a GET (the update check): GitHub answers nothing without a name, and sends on to another address */
        setopt(h, 10018 /* CURLOPT_USERAGENT */, "ZENKAI Power!");
        setopt(h, 52 /* CURLOPT_FOLLOWLOCATION */, 1L);
    }
    for (; headers != NULL && *headers != NULL; headers++) {
        list = append(list, *headers);
    }
    if (list != NULL) {
        setopt(h, 10023 /* CURLOPT_HTTPHEADER */, list);
    }
    setopt(h, 10001 /* CURLOPT_WRITEDATA */, fp);
    setopt(h, 13 /* CURLOPT_TIMEOUT */, 12L);
    setopt(h, 99 /* CURLOPT_NOSIGNAL */, 1L);
    rc = perform(h);
    cleanup(h);
    if (list != NULL) {
        freeall(list);
    }
    n = 0;
    if (rc == 0) {
        rewind(fp);
        n = (long)fread(out, 1, size - 1, fp);
        out[n > 0 ? n : 0] = '\0';
    }
    fclose(fp);
    return rc == 0 && n > 0;
}
#endif

/* The build key (net_sign.c). The service checks the time a request is signed for against its own clock (five
   minutes either way), and a player's clock can be wrong by more: so the game keeps how far the service's clock is
   ahead of this computer's. It learns that from the answer to "hello" (asked when the online screen opens), and
   from a request refused for its time, which is then sent once more. */
static SDL_AtomicInt sClockAhead;

static void clock_learn(const char *answer) {
    const char *p = SDL_strstr(answer, "\"now\":");
    long long now = p != NULL ? SDL_strtoll(p + 6, NULL, 10) : 0;
    if (now > 1000000000LL) {
        SDL_SetAtomicInt(&sClockAhead, (int)(now - (long long)time(NULL)));
    }
}

/* POSTs `json` to <url>/v1/<what>; the answer's text in out. 0 = no answer. The system's own HTTPS first (above); if
   that is not to be had, the `curl` program. */
/* A new generation is required for every Dragon Net Battle visit. */
static SDL_AtomicInt sDataGeneration, sDataState;
static SDL_SpinLock sDataLock;
static char sDataSignature[65];
int Match_DataState(void) { return SDL_GetAtomicInt(&sDataState); }

static int http_post_once(const char *what, const char *json, char *out, size_t size) {
    char url[256], path[64], sig[65], head[3][96], checked[4096];
    const char *args[20], *headers[4] = {NULL, NULL, NULL, NULL};
    SDL_Process *p;
    size_t got = 0;
    int code = 0, n = 0, i;
    void *data;
    long long when = (long long)time(NULL) + SDL_GetAtomicInt(&sClockAhead);

    if (!strcmp(what, "host") || !strcmp(what, "join") || !strcmp(what, "quick")) {
        if (Match_DataState() != 1) return 0;
        size_t length = strlen(json);
        if (!length || json[length - 1] != '}' || length + 80 >= sizeof(checked)) return 0;
        SDL_LockSpinlock(&sDataLock);
        SDL_snprintf(checked, sizeof(checked), "%.*s,\"data\":\"%s\"}", (int)length - 1, json, sDataSignature);
        SDL_UnlockSpinlock(&sDataLock);
        json = checked;
    }
    SDL_snprintf(url, sizeof(url), "%s/v1/%s", match_url(), what);
    SDL_snprintf(path, sizeof(path), "/v1/%s", what);
    if (Sign_Request("POST", path, PORT_RELEASE, when, json, sig)) { /* (a program without a key: no headers) */
        SDL_snprintf(head[0], sizeof(head[0]), "X-ZENKAIPOWER-Release: %d", PORT_RELEASE);
        SDL_snprintf(head[1], sizeof(head[1]), "X-ZENKAIPOWER-Time: %lld", when);
        SDL_snprintf(head[2], sizeof(head[2]), "X-ZENKAIPOWER-Sign: %s", sig);
        for (i = 0; i < 3; i++) {
            headers[i] = head[i];
        }
    }
    if (SDL_getenv("ZENKAIPOWER_MATCH_CURL") == NULL && http_post_native(url, json, headers[0] != NULL ? headers : NULL, out, size)) {
        return 1;
    }
#ifdef _WIN32
    args[n++] = "curl.exe";
#else
    args[n++] = "curl";
#endif
    args[n++] = "-s";
    args[n++] = "-m";
    args[n++] = "12";
    args[n++] = "-X";
    args[n++] = "POST";
    args[n++] = url;
    args[n++] = "--data-binary";
    args[n++] = "@-";
    for (i = 0; headers[i] != NULL; i++) {
        args[n++] = "-H";
        args[n++] = headers[i];
    }
    args[n] = NULL;
    p = SDL_CreateProcess(args, true);
    if (p == NULL) {
        return 0;
    }
    {
        SDL_IOStream *input = SDL_GetProcessInput(p);
        if (!input || SDL_WriteIO(input, json, SDL_strlen(json)) != SDL_strlen(json)) {
            SDL_KillProcess(p, true); SDL_DestroyProcess(p); return 0;
        }
        SDL_CloseIO(input);
    }
    data = SDL_ReadProcess(p, &got, &code);
    SDL_DestroyProcess(p);
    if (data == NULL) {
        return 0;
    }
    SDL_strlcpy(out, (const char *)data, size);
    SDL_free(data);
    return got > 0;
}

static int http_post(const char *what, const char *json, char *out, size_t size) {
    char authenticated[4096], botBody[4608], token[65];
    int ok;
    if (strncmp(what, "account/", 8) && strcmp(what, "hello") && strcmp(what, "queues") && strcmp(what, "data-check")) {
        account_token_copy(token);
        if (account_body(json, token, authenticated, sizeof(authenticated))) json = authenticated;
    }
    /* Operator-only credential: inherited by the bot, never saved in account files. */
    if (SDL_getenv("ZENKAIPOWER_NET_BOT") != NULL) {
        const char *credential = SDL_getenv("ZENKAIPOWER_BOT_CREDENTIAL");
        size_t n = SDL_strlen(json), i;
        if (!credential) credential = ""; /* LAN can run without online credentials. */
        if (credential[0]) {
            if (SDL_strlen(credential) != 64) return 0;
            for (i = 0; i < 64; i++) if (!((credential[i] >= '0' && credential[i] <= '9') ||
                    (credential[i] >= 'a' && credential[i] <= 'f'))) return 0;
        }
        if (n < 2 || json[n - 1] != '}' || n + 100 >= sizeof(botBody)) return 0;
        SDL_snprintf(botBody, sizeof(botBody), "%.*s,\"bot\":true,\"bot_credential\":\"%s\"}",
                     (int)n - 1, json, credential);
        json = botBody;
    }
    ok = http_post_once(what, json, out, size);
    /* refused for the time it was signed for: the answer says the service's time; once more by that */
    if (ok && Sign_HasKey() && SDL_strstr(out, "\"build key\"") != NULL && SDL_strstr(out, "\"why\":\"time\"") != NULL && SDL_strstr(out, "\"now\":") != NULL) {
        clock_learn(out);
        ok = http_post_once(what, json, out, size);
    }
    return ok;
}

/* What the window says when the service lets only official releases in (its REQUIRE_KEY) and refused this program:
   `plain` for a program without a key, or with one the service does not know. */
static const char *key_refused(const char *answer, const char *plain) {
    return Sign_HasKey() && SDL_strstr(answer, "\"why\":\"time\"") != NULL ? "This computer's clock is wrong: set its date and time, then try again." : plain;
}
#define KEY_REFUSED(answer) (SDL_strstr(answer, "\"build key\"") != NULL)

/* The text of "key":"..." in a flat JSON answer. 0 = not there. */
static int json_str(const char *json, const char *key, char *out, size_t size) {
    char pattern[80];
    const char *p;
    size_t used = 0;
    SDL_snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    p = SDL_strstr(json, pattern);
    if (!p || size < 1) return 0;
    p += SDL_strlen(pattern);
    while (*p && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            c = (unsigned char)*p++;
            if (!c) return 0;
            if (c == 'n') c = '\n';
            else if (c == 'r') c = '\r';
            else if (c == 't') c = '\t';
            else if (c == 'b') c = '\b';
            else if (c == 'f') c = '\f';
            else if (c != '"' && c != '\\' && c != '/') return 0;
        }
        if (used + 1 >= size) return 0;
        out[used++] = (char)c;
    }
    if (*p != '"') return 0;
    out[used] = 0;
    return 1;
}

/* Retain credentials before Match_Cancel ends discovery and deletes the room.
   A Cloudflare Worker without this API returns no UUID: reporting stays off. */
static void stats_enqueue(const char *, const char *);
static int stats_number(const char *, const char *, int *);

static void stats_capture(const char *answer, const char *key, int host) {
    char uid[40] = "";
    if (STOPPED() || !json_str(answer, "uid", uid, sizeof(uid)) || SDL_strlen(uid) != 36) return;
    SDL_LockMutex(sStatsLock);
    if (!STOPPED()) {
        if (SDL_strcmp(sStats.session, uid)) {
            sStats.totalsReady = sStats.ranked = sStats.scoringReady = sStats.finalReady = 0;
            sStats.refreshAt = 0;
        }
        SDL_strlcpy(sStats.session, uid, sizeof(sStats.session));
        SDL_strlcpy(sStats.uid, uid, sizeof(sStats.uid));
        SDL_strlcpy(sStats.key, key, sizeof(sStats.key));
        sStats.ranked = !SDL_strcmp(sM.queue, "RS");
        stats_number(answer, "stats_ranked", &sStats.ranked);
        sStats.host = host;
        account_token_copy(sStats.accountToken);
    }
    SDL_UnlockMutex(sStatsLock);
}

static int stats_number(const char *json, const char *key, int *value) {
    char needle[80];
    const char *p;
    SDL_snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = SDL_strstr(json, needle);
    if (!p || !(p = SDL_strchr(p + SDL_strlen(needle), ':'))) return 0;
    *value = SDL_atoi(p + 1);
    return 1;
}

static void stats_totals(const StatsJob *job, const char *answer) {
    static const char *fields[] = {"battles", "wins", "losses"};
    int totals[2][2][3], points[2] = {0, 0}, side, ranked, field, mode;
    int start[2], tolerance, scoring, final, delta[2];
    char uid[40], winner[16];
    if (!stats_number(answer, "stats_ranked", &mode)) return;
    for (side = 0; side < 2; side++) for (ranked = 0; ranked < 2; ranked++)
        for (field = 0; field < 3; field++) {
            char key[80];
            SDL_snprintf(key, sizeof(key), "p%d_%s_%s", side + 1,
                         ranked ? "ranked" : "unranked", fields[field]);
            if (!stats_number(answer, key, &totals[side][ranked][field])) return;
        }
    stats_number(answer, "p1_rank_points", &points[0]);
    stats_number(answer, "p2_rank_points", &points[1]);
    scoring = stats_number(answer, "rank_p1_start", &start[0]) &&
              stats_number(answer, "rank_p2_start", &start[1]) &&
              stats_number(answer, "rank_tolerance", &tolerance);
    final = json_str(answer, "uid", uid, sizeof(uid)) &&
            json_str(answer, "winner", winner, sizeof(winner)) &&
            stats_number(answer, "p1_points_won", &delta[0]) &&
            stats_number(answer, "p2_points_won", &delta[1]);
    SDL_LockMutex(sStatsLock);
    if (!SDL_strcmp(job->session, sStats.session)) {
        SDL_memcpy(sStats.totals, totals, sizeof(totals));
        SDL_memcpy(sStats.rankPoints, points, sizeof(points));
        sStats.ranked = mode != 0;
        sStats.totalsReady = 1;
        if (scoring && SDL_strstr(job->body, sStats.uid)) {
            SDL_memcpy(sStats.startPoints, start, sizeof(start));
            sStats.tolerance = tolerance;
            sStats.scoringReady = 1;
        }
        if (final && !SDL_strcmp(uid, sStats.uid)) {
            SDL_memcpy(sStats.finalDelta, delta, sizeof(delta));
            sStats.finalReady = 1;
        }
    }
    SDL_UnlockMutex(sStatsLock);
}

static int SDLCALL stats_thread(void *unused) {
    (void)unused;
    for (;;) {
        StatsJob *job;
        char answer[4096];
        int attempt, success = 0;
        SDL_LockMutex(sStatsLock);
        job = sStatsHead;
        if (!job) {
            sStatsRunning = 0;
            SDL_UnlockMutex(sStatsLock);
            return 0;
        }
        sStatsHead = job->next;
        if (!sStatsHead) sStatsTail = NULL;
        SDL_UnlockMutex(sStatsLock);
        for (attempt = 0; attempt < 3; attempt++) {
            answer[0] = '\0';
            if (http_post(job->action, job->body, answer, sizeof(answer)) &&
                SDL_strstr(answer, "\"error\"") == NULL &&
                SDL_strstr(answer, "\"uid\"") != NULL) {
                stats_totals(job, answer);
                success = 1;
                break;
            }
            SDL_Delay(250u * (attempt + 1));
        }
        fprintf(stderr, "zenkaipower: stats: %s %s\n", job->action, success ? "saved" : "failed after retries");
        SDL_free(job);
    }
}

/* Caller holds sStatsLock. Each job has its own credentials and data, so a new
   search cannot overwrite an earlier battle's pending report. */
static void stats_enqueue(const char *action, const char *body) {
    StatsJob *job = SDL_calloc(1, sizeof(*job));
    if (!job) {
        fprintf(stderr, "zenkaipower: stats: could not allocate report\n");
        return;
    }
    SDL_strlcpy(job->session, sStats.session, sizeof(job->session));
    SDL_strlcpy(job->action, action, sizeof(job->action));
    if (!account_body(body, sStats.accountToken, job->body, sizeof(job->body)))
        SDL_strlcpy(job->body, body, sizeof(job->body));
    if (sStatsTail) sStatsTail->next = job;
    else sStatsHead = job;
    sStatsTail = job;
    if (!sStatsRunning) {
        SDL_Thread *thread;
        sStatsRunning = 1;
        thread = SDL_CreateThread(stats_thread, "zenkaipower-stats", NULL);
        if (thread) SDL_DetachThread(thread);
        else {
            sStatsRunning = 0;
            fprintf(stderr, "zenkaipower: stats: could not start reporting thread\n");
        }
    }
}

static SDL_AtomicInt sRankBusy, sRankPoints, sRankGeneration;
static char sRankName[20], sRankUrl[256];
static Uint64 sRankNext;
static int SDLCALL rank_points_thread(void *unused) {
    char token[65], body[128], answer[256];
    int points;
    int generation = (int)(intptr_t)unused;
    account_token_copy(token);
    SDL_snprintf(body, sizeof(body), "{\"account_token\":\"%s\"}", token);
    if (token[0] && http_post("account/stats", body, answer, sizeof(answer)) &&
        stats_number(answer, "rank_points", &points)) {
        if (generation == SDL_GetAtomicInt(&sRankGeneration)) SDL_SetAtomicInt(&sRankPoints, points + 1);
    } else if (generation == SDL_GetAtomicInt(&sRankGeneration)) SDL_SetAtomicInt(&sRankPoints, 0);
    SDL_SetAtomicInt(&sRankBusy, 0);
    return 0;
}

int Port_AccountRankPoints(const char *name) {
    if (SDL_strcmp(name, sRankName) || SDL_strcmp(match_url(), sRankUrl)) {
        SDL_strlcpy(sRankName, name, sizeof(sRankName));
        SDL_strlcpy(sRankUrl, match_url(), sizeof(sRankUrl));
        SDL_AddAtomicInt(&sRankGeneration, 1);
        SDL_SetAtomicInt(&sRankPoints, 0);
        sRankNext = 0;
    }
    if (!Port_AccountLogged(name)) return -1;
    if (SDL_GetTicks() >= sRankNext && SDL_CompareAndSwapAtomicInt(&sRankBusy, 0, 1)) {
        SDL_Thread *thread;
        sRankNext = SDL_GetTicks() + 10000;
        thread = SDL_CreateThread(rank_points_thread, "zenkaipower-rank", (void *)(intptr_t)SDL_GetAtomicInt(&sRankGeneration));
        if (thread) SDL_DetachThread(thread);
        else SDL_SetAtomicInt(&sRankBusy, 0);
    }
    return SDL_GetAtomicInt(&sRankPoints) - 1;
}

#include "../../../include/port_ranking.h"
static SDL_Mutex *sRankingLock;
static PortRankingPage sRanking;
static int sRankingBusy, sRankingGeneration, sRankingRefresh = -1;
static char sRankingUrl[256];

/* Find an object's end without treating escaped quotes or braces in a name as JSON structure. */
static const char *ranking_object_end(const char *p) {
    int quoted = 0, escaped = 0;
    for (; *p; p++) {
        if (escaped) { escaped = 0; continue; }
        if (quoted && *p == '\\') { escaped = 1; continue; }
        if (*p == '"') quoted = !quoted;
        else if (!quoted && *p == '}') return p;
    }
    return NULL;
}

static int ranking_parse(const char *answer, int offset, PortRankingPage *page) {
    const char *p = SDL_strstr(answer, "\"entries\"");
    int gotOffset;
    SDL_zero(*page);
    if (!stats_number(answer, "offset", &gotOffset) || gotOffset != offset ||
        !stats_number(answer, "total", &page->total) || page->total < 0 ||
        !p || !(p = SDL_strchr(p, '['))) return 0;
    page->offset = offset;
    p++;
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    while (*p != ']') {
        char object[512];
        const char *end;
        PortRankingEntry *row;
        size_t length;
        if (*p != '{' || page->count >= PORT_RANKING_ROWS || !(end = ranking_object_end(p))) return 0;
        length = (size_t)(end - p + 1);
        if (length >= sizeof(object)) return 0;
        SDL_memcpy(object, p, length); object[length] = 0;
        row = &page->entries[page->count];
        if (!stats_number(object, "rank", &row->rank) || row->rank != offset + page->count + 1 ||
            !stats_number(object, "points", &row->points) || row->points < 0 || row->points > 9999 ||
            !stats_number(object, "character", &row->character) ||
            !json_str(object, "display_name", row->display_name, sizeof(row->display_name))) return 0;
        page->count++;
        p = end + 1;
        while (*p == ' ' || *p == '\n' || *p == '\r') p++;
        if (*p == ',') { p++; while (*p == ' ' || *p == '\n' || *p == '\r') p++; }
        else if (*p != ']') return 0;
    }
    page->state = 1;
    return 1;
}

static int SDLCALL ranking_thread(void *unused) {
    int offset, generation;
    char body[80], answer[2048];
    PortRankingPage page;
    SDL_LockMutex(sRankingLock);
    offset = sRanking.offset; generation = sRankingGeneration;
    SDL_UnlockMutex(sRankingLock);
    SDL_snprintf(body, sizeof(body), "{\"offset\":%d,\"limit\":3}", offset);
    if (!http_post("ranking", body, answer, sizeof(answer)) || !ranking_parse(answer, offset, &page)) {
        SDL_zero(page); page.state = -1; page.offset = offset;
    }
    SDL_LockMutex(sRankingLock);
    if (generation == sRankingGeneration) sRanking = page;
    sRankingBusy = 0;
    SDL_UnlockMutex(sRankingLock);
    return 0;
}

void Port_RankingRead(int offset, int refresh, PortRankingPage *out) {
    SDL_Thread *thread;
    if (!sRankingLock) sRankingLock = SDL_CreateMutex();
    if (!sRankingLock) { SDL_zero(*out); out->state = -1; return; }
    SDL_LockMutex(sRankingLock);
    if (offset != sRanking.offset || refresh != sRankingRefresh || SDL_strcmp(match_url(), sRankingUrl)) {
        SDL_zero(sRanking); sRanking.offset = offset;
        sRankingRefresh = refresh; sRankingGeneration++;
        SDL_strlcpy(sRankingUrl, match_url(), sizeof(sRankingUrl));
    }
    if (!sRanking.state && !sRankingBusy) {
        sRankingBusy = 1;
        thread = SDL_CreateThread(ranking_thread, "zenkaipower-ranking", NULL);
        if (thread) SDL_DetachThread(thread);
        else { sRankingBusy = 0; sRanking.state = -1; }
    }
    *out = sRanking;
    SDL_UnlockMutex(sRankingLock);
}

int Port_NetStatsRanked(void) {
    int ranked = 0;
    if (!sStatsLock) return 0;
    SDL_LockMutex(sStatsLock);
    ranked = sStats.ranked;
    SDL_UnlockMutex(sStatsLock);
    return ranked;
}

int Port_NetStatsRankPoints(int side) {
    int points = 0;
    if (!sStatsLock || side < 0 || side > 1) return 0;
    SDL_LockMutex(sStatsLock);
    points = sStats.rankPoints[side];
    SDL_UnlockMutex(sStatsLock);
    return points;
}

int Port_NetStatsStartPoints(int side) {
    int points = 0;
    if (!sStatsLock || side < 0 || side > 1) return 0;
    SDL_LockMutex(sStatsLock);
    points = sStats.scoringReady ? sStats.startPoints[side] : sStats.rankPoints[side];
    SDL_UnlockMutex(sStatsLock);
    return points;
}

int Port_NetStatsAdjustPoints(int winner, int *changes) {
    int side, win, lose, gap, denominator;
    if (!sStatsLock || winner < 0 || winner > 1) {
        changes[0] = changes[1] = 0;
        return 0;
    }
    SDL_LockMutex(sStatsLock);
    if (!sStats.scoringReady) { SDL_UnlockMutex(sStatsLock); return 0; }
    win = winner; lose = 1 - win;
    gap = sStats.startPoints[win] - sStats.startPoints[lose] - sStats.tolerance;
    denominator = 1000 + (gap > 0 ? gap : 0);
    for (side = 0; side < 2; side++) {
        long long base = changes[side];
        long long magnitude;
        int limit = side == win ? 9999 - sStats.startPoints[side] : sStats.startPoints[side];
        if (base < 0) base = -base;
        magnitude = (base * 1000 + denominator / 2) / denominator;
        if (magnitude < 1) magnitude = 1;
        if (magnitude > limit) magnitude = limit;
        changes[side] = side == win ? (int)magnitude : -(int)magnitude;
    }
    SDL_UnlockMutex(sStatsLock);
    return 1;
}

int Port_NetStatsFinalPoints(int side, int *points, int *change) {
    int ready;
    if (!sStatsLock || side < 0 || side > 1) return 0;
    SDL_LockMutex(sStatsLock);
    ready = sStats.finalReady;
    if (ready) { *points = sStats.rankPoints[side]; *change = sStats.finalDelta[side]; }
    SDL_UnlockMutex(sStatsLock);
    return ready;
}

int Port_NetStatsTotals(int side, int *out) {
    int ready;
    if (!sStatsLock || side < 0 || side > 1) return 0;
    SDL_LockMutex(sStatsLock);
    if (sStats.uid[0] && !sStatsRunning && SDL_GetTicks() >= sStats.refreshAt) {
        char body[256];
        sStats.refreshAt = SDL_GetTicks() + 1000;
        SDL_snprintf(body, sizeof(body), "{\"uid\":\"%s\",\"key\":\"%s\"}", sStats.uid, sStats.key);
        stats_enqueue("battle/get", body);
    }
    ready = sStats.totalsReady;
    if (ready) SDL_memcpy(out, sStats.totals[side][sStats.ranked], 3 * sizeof(int));
    SDL_UnlockMutex(sStatsLock);
    return ready;
}

#include "bot_battle_log.h"
extern int Port_NetMe(void);
extern const char *Port_NetName(int);
static BotBattleLog sBotBattle;
static SDL_SpinLock sBotBattleLock;

void Port_NetStatsStartFull(const int *characters, const int *items, const int *costumes, int stage, int music, int picker) {
    char body[2048], roster[1536] = "";
    int type, dp, limit, i, used = 0;
    extern void Port_NetRules(int *, int *, int *);
    if (SDL_getenv("ZENKAIPOWER_NET_BOT")) {
        SDL_LockSpinlock(&sBotBattleLock);
        BotBattle_Start(&sBotBattle, stderr, Port_NetMe(), Port_NetName(0), Port_NetName(1));
        SDL_UnlockSpinlock(&sBotBattleLock);
    }
    if (!sStatsLock) return;
    SDL_LockMutex(sStatsLock);
    if (!sStats.session[0] || sStats.active) {
        SDL_UnlockMutex(sStatsLock);
        return;
    }
    SDL_strlcpy(sStats.uid, sStats.session, sizeof(sStats.uid));
    if (sStats.round) {
        /* Same deterministic rematch UUID on both peers; never changes the
           UUID version or variant. Useful if the host disappears mid-match. */
        unsigned tail = (unsigned)SDL_strtoul(sStats.session + 28, NULL, 16);
        SDL_snprintf(sStats.uid + 28, 9, "%08x", tail ^ sStats.round);
    }
    sStats.round++;
    sStats.finalReady = sStats.scoringReady = 0;
    sStats.active = 1;
    Port_NetConnectionBegin();
    if (sStats.host) {
        Port_NetRules(&type, &dp, &limit);
        for (i = 0; i < 10; i++) {
            if (characters[i] >= 0)
                used += SDL_snprintf(roster + used, sizeof(roster) - used,
                                     ",\"p%dc%d\":%d", i / 5 + 1, i % 5 + 1, characters[i]);
        }
        if (costumes) for (i = 0; i < 10; i++) if (characters[i] >= 0)
            used += SDL_snprintf(roster + used, sizeof(roster) - used,
                                 ",\"p%dc%d_costume\":%d", i / 5 + 1, i % 5 + 1, costumes[i]);
        if (stage >= 0) used += SDL_snprintf(roster + used, sizeof(roster) - used, ",\"stage\":%d", stage);
        if (music >= -1) used += SDL_snprintf(roster + used, sizeof(roster) - used, ",\"music\":%d", music);
        if (picker == 1 || picker == 2) used += SDL_snprintf(roster + used, sizeof(roster) - used, ",\"stage_music_picker\":%d", picker);
        if (items) {
            int count = 0, slot;
            used += SDL_snprintf(roster + used, sizeof(roster) - used, ",\"items\":{");
            for (i = 0; i < 10; i++) if (characters[i] >= 0) {
                used += SDL_snprintf(roster + used, sizeof(roster) - used, "%s\"p%dc%d\":[",
                                     count++ ? "," : "", i / 5 + 1, i % 5 + 1);
                for (slot = 0; slot < 7; slot++)
                    used += SDL_snprintf(roster + used, sizeof(roster) - used, "%s%d", slot ? "," : "", items[i * 7 + slot]);
                used += SDL_snprintf(roster + used, sizeof(roster) - used, "]");
            }
            SDL_snprintf(roster + used, sizeof(roster) - used, "}");
        }
        SDL_snprintf(body, sizeof(body), "{\"uid\":\"%s\",\"session_uid\":\"%s\",\"key\":\"%s\",\"battle_type\":\"%s\"%s}",
                     sStats.uid, sStats.session, sStats.key,
                     kMatchQueues[type == 0 ? 0 : type == 1 ? 1 : 2 + dp], roster);
        stats_enqueue("battle/start", body);
    }
    SDL_UnlockMutex(sStatsLock);
}

void Port_NetStatsStartDetails(const int *characters, const int *items, int stage, int music) {
    Port_NetStatsStartFull(characters, items, NULL, stage, music, 0);
}

void Port_NetStatsStartLoadouts(const int *characters, const int *items) {
    Port_NetStatsStartDetails(characters, items, -1, -2);
}

void Port_NetStatsStart(const int *characters) {
    Port_NetStatsStartDetails(characters, NULL, -1, -2);
}

static void stats_connection(void) {
    char connection[1024], body[1280];
    if (Port_NetConnectionJson(connection, sizeof(connection))) {
        SDL_snprintf(body, sizeof(body), "{\"uid\":\"%s\",\"key\":\"%s\",\"connection\":%s}",
                     sStats.uid, sStats.key, connection);
        stats_enqueue("battle/connection", body);
    }
}

void Port_NetStatsEnd(int winner, int finish, int durationMs, int p1Points, int p2Points) {
    char body[512];
    SDL_LockSpinlock(&sBotBattleLock);
    BotBattle_End(&sBotBattle, stderr, winner, finish, durationMs);
    SDL_UnlockSpinlock(&sBotBattleLock);
    if (!sStatsLock) return;
    SDL_LockMutex(sStatsLock);
    if (!sStats.ranked) p1Points = p2Points = 0;
    /* Ranked settlement needs independently authenticated reports from both peers. */
    if (sStats.active && (sStats.host || sStats.ranked)) {
        SDL_snprintf(body, sizeof(body), "{\"uid\":\"%s\",\"key\":\"%s\",\"winner\":\"%s\",\"finish_type\":\"%s\","
                     "\"battle_duration\":%d.%03d,\"p1_points_won\":%d,\"p2_points_won\":%d}",
                     sStats.uid, sStats.key, winner == 0 ? "p1" : winner == 1 ? "p2" : "tie",
                     finish == 3 ? "ringout" : finish == 2 ? "timeout" : finish == 1 ? "perfect" : "ok",
                     durationMs / 1000, durationMs % 1000, p1Points, p2Points);
        stats_enqueue("battle/end", body);
    }
    if (sStats.active) stats_connection();
    sStats.active = 0;
    SDL_UnlockMutex(sStatsLock);
}

void Port_NetStatsInterrupt(void) {
    char body[256];
    SDL_LockSpinlock(&sBotBattleLock);
    BotBattle_Interrupt(&sBotBattle, stderr);
    SDL_UnlockSpinlock(&sBotBattleLock);
    if (!sStatsLock) return;
    SDL_LockMutex(sStatsLock);
    if (sStats.session[0] && (sStats.active || sStats.round == 0)) {
        if (sStats.active) stats_connection();
        SDL_snprintf(body, sizeof(body), "{\"uid\":\"%s\",\"key\":\"%s\"}", sStats.uid, sStats.key);
        stats_enqueue("battle/interrupt", body);
    }
    SDL_memset(&sStats, 0, sizeof(sStats));
    SDL_UnlockMutex(sStatsLock);
}

static void stats_exit(void) {
    Uint64 deadline = SDL_GetTicks() + 2000;
    Port_NetStatsInterrupt();
    while (sStatsLock && SDL_GetTicks() < deadline) {
        int done;
        SDL_LockMutex(sStatsLock);
        done = !sStatsRunning && !sStatsHead;
        SDL_UnlockMutex(sStatsLock);
        if (done) break;
        SDL_Delay(10);
    }
}

/* The peer of an answer: "peer":{"name":"..","addrs":["..",".."]}. 0 = none ("peer":null, or no such part). */
static int json_peer(const char *json, MatchStatus *st) {
    const char *p = SDL_strstr(json, "\"peer\":{"), *a;
    int n = 0;

    if (p == NULL) {
        return 0;
    }
    json_str(p, "name", st->peerName, sizeof(st->peerName));
    a = SDL_strstr(p, "\"addrs\":[");
    if (a == NULL) {
        return 0;
    }
    a += 9;
    while (*a == '"' && n < MATCH_ADDRS) {
        const char *e = SDL_strchr(a + 1, '"');
        if (e == NULL || (size_t)(e - a - 1) >= sizeof(st->peerAddr[0])) {
            break;
        }
        SDL_memcpy(st->peerAddr[n], a + 1, (size_t)(e - a - 1));
        st->peerAddr[n][e - a - 1] = '\0';
        n++;
        a = e + 1;
        if (*a == ',') {
            a++;
        }
    }
    st->peerCount = n;
    return n > 0;
}

static int match_account_error(const char *answer) {
    char error[192];
    if (json_str(answer, "error", error, sizeof(error)) &&
        (SDL_strstr(error, "account") || SDL_strstr(error, "initialize") || SDL_strstr(error, "player name"))) {
        SDL_LockSpinlock(&sAccount.lock); sAccount.validated = 0; SDL_UnlockSpinlock(&sAccount.lock);
        set_state(-1, error);
        return 1;
    }
    return 0;
}

/* ---- STUN (RFC 5389): one question, "which address do you see me under?" ---- */

static int stun_ask(int sock, const char *host, int port, char *out, size_t size) {
    struct addrinfo hints, *res = NULL;
    uint8_t req[20], pkt[512];
    char portText[8];
    int tries, i;

    SDL_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    SDL_snprintf(portText, sizeof(portText), "%d", port);
    if (getaddrinfo(host, portText, &hints, &res) != 0 || res == NULL) {
        return 0;
    }
    SDL_memset(req, 0, sizeof(req));
    req[1] = 0x01; /* binding request, no attributes */
    req[4] = 0x21; req[5] = 0x12; req[6] = 0xA4; req[7] = 0x42; /* the magic cookie */
    for (i = 8; i < 20; i++) {
        req[i] = (uint8_t)SDL_rand(256);
    }
    for (tries = 0; tries < 3 && !STOPPED(); tries++) {
        int waited;
        sendto(sock, (const char *)req, sizeof(req), 0, res->ai_addr, (int)res->ai_addrlen);
        for (waited = 0; waited < 500; waited += 10) {
            struct sockaddr_in from;
            socklen_t fromLen = sizeof(from);
            int n = (int)recvfrom(sock, (char *)pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &fromLen), at;
            if (n < 20 || pkt[0] != 0x01 || pkt[1] != 0x01 || SDL_memcmp(pkt + 8, req + 8, 12) != 0) {
                SDL_Delay(10);
                continue;
            }
            for (at = 20; at + 4 <= n;) {
                int type = pkt[at] << 8 | pkt[at + 1], len = pkt[at + 2] << 8 | pkt[at + 3];
                if (at + 4 + len > n) {
                    break;
                }
                if ((type == 0x0020 || type == 0x0001) && len >= 8 && pkt[at + 5] == 0x01) {
                    const uint8_t *v = &pkt[at + 4];
                    unsigned p = (unsigned)(v[2] << 8 | v[3]);
                    uint8_t ip[4] = {v[4], v[5], v[6], v[7]};
                    if (type == 0x0020) { /* XOR-MAPPED-ADDRESS: with the cookie */
                        p ^= 0x2112;
                        ip[0] ^= 0x21; ip[1] ^= 0x12; ip[2] ^= 0xA4; ip[3] ^= 0x42;
                    }
                    SDL_snprintf(out, size, "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3], p);
                    freeaddrinfo(res);
                    return 1;
                }
                at += 4 + ((len + 3) & ~3);
            }
        }
    }
    freeaddrinfo(res);
    return 0;
}

/* This machine's address on its own network (for two players behind the same router): the address a socket would
   send from, found without sending anything. */
static int lan_address(int localPort, char *out, size_t size) {
    struct sockaddr_in to, me;
    socklen_t len = sizeof(me);
    int s = (int)socket(AF_INET, SOCK_DGRAM, 0), ok = 0;
    char text[32];

    if (s < 0) {
        return 0;
    }
    SDL_memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(9);
    inet_pton(AF_INET, "192.0.2.1", &to.sin_addr); /* (a documentation address: nothing is sent) */
    if (connect(s, (struct sockaddr *)&to, sizeof(to)) == 0 && getsockname(s, (struct sockaddr *)&me, &len) == 0 &&
        inet_ntop(AF_INET, &me.sin_addr, text, sizeof(text)) != NULL && SDL_strcmp(text, "0.0.0.0") != 0) {
        SDL_snprintf(out, size, "%s:%d", text, localPort);
        ok = 1;
    }
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
    return ok;
}

void Match_DataAsk(void);

/* ---- the thread ---- */

static int SDLCALL match_thread(void *arg) {
    char pub[64] = "", lan[64] = "", addrs[160], body[512], answer[1024], key[40] = "", you[64] = "", name[40], relayAddr[64] = "";
    int i, n = 0, hosting, searches = 0;

    SDL_SetTLS(&sMine, arg, NULL);
    /* Every online entry path, including headless bots, must verify before join/host/quick. */
    Match_DataAsk();
    while (!STOPPED() && Match_DataState() == 0) SDL_Delay(10);
    if (STOPPED()) return 0;
    if (Match_DataState() != 1) {
        set_state(-1, "Build compatibility check failed or the matchmaking service could not be reached.");
        return 0;
    }
    account_quote(sM.name, name, sizeof(name));
    if (!stun_ask(sM.sock, "stun.cloudflare.com", 3478, pub, sizeof(pub))) {
        stun_ask(sM.sock, "stun.l.google.com", 19302, pub, sizeof(pub));
    }
    lan_address(sM.localPort, lan, sizeof(lan));
    if (STOPPED()) {
        return 0;
    }
    /* (no public address found: the Worker's view of this machine's address with the local port is tried in its
       place, below, once the Worker has said what it sees) */
    SDL_snprintf(addrs, sizeof(addrs), "%s%s%s%s%s%s%s", pub[0] ? "\"" : "", pub, pub[0] ? "\"" : "", pub[0] && lan[0] ? "," : "", lan[0] ? "\"" : "", lan,
                 lan[0] ? "\"" : "");
    if (addrs[0] == '\0') {
        set_state(-1, "This computer's address could not be found (no network?).");
        return 0;
    }
again:
    hosting = sM.role == 1;
    if (sM.role == 3) {
        /* A quick match: the service puts this player with whoever waits longest in the queue (then this is the
           one who joins), or gives it a room of its own in the queue to wait in (then it is the host). A room
           lasts ten minutes; when it runs out the search simply starts again. */
        MatchStatus st;
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"rel\":%d,\"name\":\"%s\",\"addrs\":[%s],\"queue\":\"%s\",\"who\":\"%s\"%s}", sM.version, PORT_RELEASE, name, addrs, sM.queue,
                     match_who(), relayAddr[0] != '\0' ? ",\"relay\":false" : "");
        if (!http_post("quick", body, answer, sizeof(answer))) {
            set_state(-1, "Matchmaking service could not be reached.");
            return 0;
        }
        if (match_account_error(answer)) return 0;
        if (STOPPED()) {
            return 0;
        }
        searches++;
        SDL_memset(&st, 0, sizeof(st));
        if (SDL_strstr(answer, "\"role\":\"join\"") != NULL && json_peer(answer, &st)) {
            json_str(answer, "key", key, sizeof(key));
            stats_capture(answer, key, 0);
            SDL_LockMutex(sM.lock);
            SDL_strlcpy(sM.st.peerName, st.peerName, sizeof(sM.st.peerName));
            SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
            sM.st.peerCount = st.peerCount;
            sM.st.role = 2;
            sM.st.state = 3;
            SDL_UnlockMutex(sM.lock);
            json_str(answer, "code", sM.code, sizeof(sM.code));
            json_str(answer, "key", key, sizeof(key));
            goto joined;
        }
        if (SDL_strstr(answer, "\"role\":\"host\"") == NULL) {
            set_state(-1, SDL_strstr(answer, "\"old\"") != NULL ? "Update available. Online servers are unreachable until you update." :
                          KEY_REFUSED(answer) ? key_refused(answer, "Quick match needs an official release of the game.") :
                          SDL_strstr(answer, "busy") != NULL ? "Too many searches from this address; try again in a few minutes." :
                          SDL_strstr(answer, "bad request") != NULL || SDL_strstr(answer, "error") != NULL ? "The matchmaking service does not know quick match yet." :
                          "The matchmaking service gave no answer.");
            return 0;
        }
        hosting = 1;
    }
    if (hosting) {
        if (sM.role == 1) {
            SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"rel\":%d,\"name\":\"%s\",\"addrs\":[%s]}", sM.version, PORT_RELEASE, name, addrs);
            if (!http_post("host", body, answer, sizeof(answer))) {
                set_state(-1, "Matchmaking service could not be reached.");
                return 0;
            }
            if (match_account_error(answer)) return 0;
            if (STOPPED()) {
                return 0;
            }
        }
        if (!json_str(answer, "code", sM.code, sizeof(sM.code)) || !json_str(answer, "key", key, sizeof(key))) {
            set_state(-1, SDL_strstr(answer, "\"old\"") != NULL ? "Update available. Online servers are unreachable until you update." :
                          KEY_REFUSED(answer) ? key_refused(answer, "Room codes need an official release of the game. Playing by address still works.") :
                          SDL_strstr(answer, "busy") != NULL ? "Too many rooms from this address; try again in a few minutes." : "The matchmaking service gave no room.");
            return 0;
        }
        if (relayAddr[0] != '\0') { /* (a search begun again: the relay address it holds already goes with the new room) */
            SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\",\"addrs\":[%s,\"relay=%s\"]}", sM.code, key, addrs, relayAddr);
            http_post("addrs", body, answer, sizeof(answer));
        } else {
            /* A relay address, if the service gives access to one: published with the others, for a player who
               cannot reach this one directly. The code is shown only after this, so nobody joins before. */
            char server[128], user[160], pass[160], relay[64];
            if (SDL_getenv("ZENKAIPOWER_RELAY_LOG") != NULL && (SDL_strstr(answer, "\"turn\"") != NULL || SDL_strstr(answer, "noRelay") != NULL)) {
                char why[160] = ""; /* (a service that hands a login out with the room: one that is asked later says nothing here) */
                json_str(answer, "noRelay", why, sizeof(why));
                fprintf(stderr, "relay: the service %s%s\n", SDL_strstr(answer, "\"turn\"") != NULL ? "gave access" : "gave no access: ", why);
            }
            if (SDL_getenv("ZENKAIPOWER_NO_RELAY") == NULL && json_str(answer, "server", server, sizeof(server)) && json_str(answer, "username", user, sizeof(user)) &&
                json_str(answer, "credential", pass, sizeof(pass)) && Relay_Allocate(sM.sock, server, user, pass, relay, sizeof(relay)) && !STOPPED()) {
                SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\",\"addrs\":[%s,\"relay=%s\"]}", sM.code, key, addrs, relay);
                http_post("addrs", body, answer, sizeof(answer));
                SDL_strlcpy(relayAddr, relay, sizeof(relayAddr));
            }
        }
        if (STOPPED()) {
            SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", sM.code, key);
            http_post("leave", body, answer, sizeof(answer));
            return 0;
        }
        SDL_LockMutex(sM.lock);
        SDL_strlcpy(sM.st.code, sM.code, sizeof(sM.st.code)); /* (shown from here on) */
        sM.st.role = 1;
        sM.st.searches = searches;
        sM.st.state = 2;
        SDL_UnlockMutex(sM.lock);
        SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", sM.code, key);
        for (i = 0; i < 165 && !STOPPED(); i++) { /* a room lasts ten minutes */
            int k;
            /* asked every two seconds in the first minute, every four after it (the service's daily requests) */
            for (k = 0; k < (i < 30 ? 20 : 40) && !STOPPED(); k++) {
                SDL_Delay(100);
            }
            if (STOPPED()) {
                break;
            }
            if (http_post("poll", body, answer, sizeof(answer))) {
                if (match_account_error(answer)) return 0;
                MatchStatus st;
                SDL_memset(&st, 0, sizeof(st));
                if (json_peer(answer, &st)) {
                    if (STOPPED()) {
                        break;
                    }
                    stats_capture(answer, key, 1);
                    SDL_LockMutex(sM.lock);
                    SDL_strlcpy(sM.st.peerName, st.peerName, sizeof(sM.st.peerName));
                    SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
                    sM.st.peerCount = st.peerCount;
                    sM.st.state = 3;
                    SDL_UnlockMutex(sM.lock);
                    if (relayAddr[0] != '\0' || SDL_getenv("ZENKAIPOWER_NO_RELAY") != NULL) {
                        http_post("leave", body, answer, sizeof(answer)); /* the room has done its work: it is given up at once */
                        return 0;
                    }
                    /* No relay address yet (it is asked for only when it is needed). The lobby now tries the direct
                       ways; it ends this search when the two are connected. If that has not happened in a few
                       seconds, a login is asked for, the address is added to the room (the one who joined learns it
                       by its poll), and the lobby is told to let the other player through. The room is kept until
                       the lobby is done, so that the other side can still ask. */
                    {
                        int w, wait = SDL_getenv("ZENKAIPOWER_RELAY_ONLY") != NULL ? 2 : 40;
                        char server[128], user[160], pass[160], relay[64];
                        for (w = 0; w < wait && !STOPPED(); w++) {
                            SDL_Delay(100);
                        }
                        if (!STOPPED() && http_post("relay", body, answer, sizeof(answer))) {
                            if (SDL_getenv("ZENKAIPOWER_RELAY_LOG") != NULL) {
                                char why[160] = "";
                                json_str(answer, "noRelay", why, sizeof(why));
                                fprintf(stderr, "relay: asked for when the direct ways stayed silent: the service %s%s\n",
                                        SDL_strstr(answer, "\"turn\"") != NULL ? "gave access" : "gave no access: ", why);
                            }
                            if (json_str(answer, "server", server, sizeof(server)) && json_str(answer, "username", user, sizeof(user)) &&
                                json_str(answer, "credential", pass, sizeof(pass)) && !STOPPED()) {
                                int got;
                                SDL_SetAtomicInt(&sM.sockBusy, 1);
                                SDL_Delay(80); /* (the lobby is between two looks at the socket by then) */
                                got = !STOPPED() && Relay_Allocate(sM.sock, server, user, pass, relay, sizeof(relay));
                                SDL_SetAtomicInt(&sM.sockBusy, 0);
                                if (got && !STOPPED()) {
                                    char more[512];
                                    SDL_snprintf(more, sizeof(more), "{\"code\":\"%s\",\"key\":\"%s\",\"addrs\":[%s,\"relay=%s\"]}", sM.code, key, addrs, relay);
                                    http_post("addrs", more, answer, sizeof(answer));
                                    SDL_LockMutex(sM.lock);
                                    sM.st.relayGen++;
                                    SDL_UnlockMutex(sM.lock);
                                }
                            }
                        }
                        for (w = 0; w < 250 && !STOPPED(); w++) { /* (the lobby gives up after 25 seconds at the latest) */
                            SDL_Delay(100);
                        }
                    }
                    http_post("leave", body, answer, sizeof(answer));
                    return 0;
                }
                if (SDL_strstr(answer, "no room") != NULL) {
                    break;
                }
            }
        }
        if (STOPPED()) {
            http_post("leave", body, answer, sizeof(answer));
            return 0;
        }
        if (sM.role == 3) { /* (a quick match waits for as long as the player does) */
            http_post("leave", body, answer, sizeof(answer));
            goto again;
        }
        set_state(-1, "Nobody joined: the room code has run out. Host again for a new one.");
    } else {
        MatchStatus st;
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"rel\":%d,\"code\":\"%s\",\"name\":\"%s\",\"addrs\":[%s]}", sM.version, PORT_RELEASE, sM.code, name, addrs);
        if (!http_post("join", body, answer, sizeof(answer))) {
            set_state(-1, "Matchmaking service could not be reached.");
            return 0;
        }
        if (match_account_error(answer)) return 0;
        (void)you;
        SDL_memset(&st, 0, sizeof(st));
        if (!json_peer(answer, &st)) {
            char notFound[96];
            SDL_snprintf(notFound, sizeof(notFound), "Room with code %s not found.", sM.code);
            set_state(-1, SDL_strstr(answer, "\"old\"") != NULL ? "Update available. Online servers are unreachable until you update." :
                          KEY_REFUSED(answer) ? key_refused(answer, "Room codes need an official release of the game. Playing by address still works.") :
                          SDL_strstr(answer, "version") != NULL ? "The host's game is a different version: both players need the same release." :
                          SDL_strstr(answer, "full") != NULL ? "Someone has already joined that room." :
                          SDL_strstr(answer, "no room") != NULL ? notFound :
                          SDL_strstr(answer, "busy") != NULL ? "Too many wrong codes from this address; try again in an hour." : "The matchmaking service gave no answer.");
            return 0;
        }
        json_str(answer, "key", key, sizeof(key));
        stats_capture(answer, key, 0);
        if (STOPPED()) {
            return 0;
        }
        SDL_LockMutex(sM.lock);
        SDL_strlcpy(sM.st.peerName, st.peerName, sizeof(sM.st.peerName));
        SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
        sM.st.peerCount = st.peerCount;
        sM.st.state = 3;
        SDL_UnlockMutex(sM.lock);
        json_str(answer, "key", key, sizeof(key));
        goto joined;
    }
    return 0;
joined:
    /* The one who joined. If the host's direct addresses stay silent, the host asks for a relay address and adds it
       to the room: asked for here every two seconds, from the third second on, until the lobby ends this search
       (connected, or given up) or the address has come. */
    if (key[0] != '\0' && sM.code[0] != '\0') {
        MatchStatus st;
        int round, w, known;
        SDL_LockMutex(sM.lock);
        known = sM.st.peerCount;
        SDL_UnlockMutex(sM.lock);
        SDL_snprintf(body, sizeof(body), "{\"code\":\"%s\",\"key\":\"%s\"}", sM.code, key);
        for (round = 0; round < 11 && !STOPPED(); round++) {
            for (w = 0; w < (round == 0 ? 30 : 20) && !STOPPED(); w++) {
                SDL_Delay(100);
            }
            if (STOPPED() || !http_post("poll", body, answer, sizeof(answer))) {
                continue;
            }
            if (match_account_error(answer)) return 0;
            if (SDL_strstr(answer, "no room") != NULL) {
                break; /* (the host has given the room up: it is connected, or gone) */
            }
            SDL_memset(&st, 0, sizeof(st));
            if (json_peer(answer, &st) && SDL_strstr(answer, "relay=") != NULL && st.peerCount != known && !STOPPED()) {
                SDL_LockMutex(sM.lock);
                SDL_memcpy(sM.st.peerAddr, st.peerAddr, sizeof(st.peerAddr));
                sM.st.peerCount = st.peerCount;
                sM.st.addrGen++;
                SDL_UnlockMutex(sM.lock);
                break;
            }
        }
    }
    return 0;
}

/* ---- for the lobby (net.c) ---- */

/* Starts looking: role 1 makes a room, role 2 joins room `code`. `sock` is the match's own UDP socket (not blocking),
   bound to `localPort`. */
void Match_Begin(int role, int sock, int localPort, const char *code, const char *name, int version) {
    int i, n = 0;

    if (sM.lock == NULL) {
        sM.lock = SDL_CreateMutex();
    }
    if (sM.thread != NULL) {
        SDL_DetachThread(sM.thread); /* (it stops by itself: its search number is no longer the current one) */
        sM.thread = NULL;
    }
    SDL_AddAtomicInt(&sM.gen, 1);
    if (!sStatsLock) {
        sStatsLock = SDL_CreateMutex();
        atexit(stats_exit);
    }
    SDL_LockMutex(sStatsLock);
    SDL_memset(&sStats, 0, sizeof(sStats));
    SDL_UnlockMutex(sStatsLock);
    SDL_SetAtomicInt(&sM.cancel, 0);
    SDL_memset(&sM.st, 0, sizeof(sM.st));
    sM.st.state = 1;
    sM.role = role;
    sM.sock = sock;
    sM.localPort = localPort;
    sM.version = version;
    sM.code[0] = '\0';
    for (i = 0; code != NULL && code[i] != '\0' && n < 6; i++) { /* letters and digits, capitals */
        char c = code[i];
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            sM.code[n++] = c;
        }
    }
    sM.code[n] = '\0';
    SDL_strlcpy(sM.queue, role == 3 ? sM.code : "", sizeof(sM.queue));
    SDL_strlcpy(sM.name, name != NULL ? name : "Player", sizeof(sM.name));
    sM.thread = SDL_CreateThread(match_thread, "zenkaipower-match", (void *)(intptr_t)SDL_GetAtomicInt(&sM.gen));
    if (sM.thread == NULL) {
        set_state(-1, "The matchmaking could not be started.");
    }
}

/* ---- how many wait in each quick-match queue (for the window): asked in a thread of its own, now and then ---- */
static SDL_AtomicInt sQueueCount[MATCH_QUEUES], sQueueBusy;
static int sQueueVersion;
const char *const kMatchQueues[MATCH_QUEUES] = {"S", "T", "D10", "D15", "D20", "RS"};

static int SDLCALL queues_thread(void *arg) {
    char body[64], answer[512];
    int i;

    (void)arg;
    SDL_snprintf(body, sizeof(body), "{\"v\":%d}", sQueueVersion);
    if (http_post("queues", body, answer, sizeof(answer)) && SDL_strstr(answer, "\"waiting\"") != NULL) {
        for (i = 0; i < MATCH_QUEUES; i++) {
            char key[16];
            const char *p;
            SDL_snprintf(key, sizeof(key), "\"%s\":", kMatchQueues[i]);
            p = SDL_strstr(answer, key);
            SDL_SetAtomicInt(&sQueueCount[i], p != NULL ? SDL_atoi(p + SDL_strlen(key)) + 1 : 0);
        }
    }
    SDL_SetAtomicInt(&sQueueBusy, 0);
    return 0;
}

/* ---- is there a newer release? asked of GitHub, once per run, when the window is there ("Check for updates") ----
   One GET of the repository's newest published release (drafts and pre-releases are not it); nothing is sent but
   the request itself. The answer is long; the tag's name is near its start, and only the start is kept. */
static SDL_AtomicInt sUpdLatest, sUpdBusy;

static int SDLCALL update_thread(void *arg) {
    static char answer[4096];
    const char *url = "https://api.github.com/repos/" PORT_UPDATE_REPO "/releases/latest", *p;
    int ok;

    (void)arg;
    ok = SDL_getenv("ZENKAIPOWER_MATCH_CURL") == NULL && http_post_native(url, NULL, NULL, answer, sizeof(answer));
    if (!ok) { /* the `curl` program, as for the service */
#ifdef _WIN32
        const char *args[] = {"curl.exe", "-s", "-L", "-m", "12", "-A", "ZENKAI Power!", url, NULL};
#else
        const char *args[] = {"curl", "-s", "-L", "-m", "12", "-A", "ZENKAI Power!", url, NULL};
#endif
        SDL_Process *pr = SDL_CreateProcess(args, true);
        size_t got = 0;
        int code = 0;
        void *data = pr != NULL ? SDL_ReadProcess(pr, &got, &code) : NULL;
        if (pr != NULL) {
            SDL_DestroyProcess(pr);
        }
        if (data != NULL) {
            SDL_strlcpy(answer, (const char *)data, sizeof(answer));
            SDL_free(data);
            ok = got > 0;
        }
    }
    if (ok && (p = SDL_strstr(answer, "\"tag_name\"")) != NULL && (p = SDL_strchr(p + 10, 'v')) != NULL) {
        int a = 0, b = 0, c = 0;
        if (SDL_sscanf(p, "v%d.%d.%d", &a, &b, &c) == 3 && a == 0 && b >= 0 && c >= 0 && c < 100) {
            SDL_SetAtomicInt(&sUpdLatest, b * 100 + c); /* as PORT_RELEASE counts: 0.1.33 is 133 */
            fprintf(stderr, "zenkaipower: update check: the newest release is 0.%d.%d, this is 0.%d.%d\n", b, c, PORT_RELEASE / 100, PORT_RELEASE % 100);
        }
    }
    if (SDL_GetAtomicInt(&sUpdLatest) == 0) {
        fprintf(stderr, "zenkaipower: update check: no answer\n");
    }
    SDL_SetAtomicInt(&sUpdBusy, 0);
    return 0;
}

void Match_UpdateAsk(void) {
    SDL_Thread *t;
    if (SDL_GetAtomicInt(&sUpdBusy)) {
        return;
    }
    SDL_SetAtomicInt(&sUpdBusy, 1);
    t = SDL_CreateThread(update_thread, "zenkaipower-update", NULL);
    if (t == NULL) {
        SDL_SetAtomicInt(&sUpdBusy, 0);
    } else {
        SDL_DetachThread(t);
    }
}

/* The newest published release's number (0: not asked, no answer yet, or none). */
int Match_UpdateLatest(void) {
    return SDL_GetAtomicInt(&sUpdLatest);
}

/* Binary hashing, logical file count, and server comparison run off the game/render thread. */
extern int Port_DataSignature(char out[65]);
static int SDLCALL data_check_thread(void *arg) {
    int generation = (int)(intptr_t)arg, accepted = 0;
    char signature[65], expected[65], body[160], answer[512];
    const char *failure = "required game files or build binaries could not be read";
    if (generation != SDL_GetAtomicInt(&sDataGeneration)) return 0;
    if (Port_DataSignature(signature) && generation == SDL_GetAtomicInt(&sDataGeneration)) {
        SDL_snprintf(body, sizeof(body), "{\"rel\":%d,\"data\":\"%s\"}", PORT_RELEASE, signature);
        failure = "build compatibility service could not be reached";
        if (http_post("data-check", body, answer, sizeof(answer))) {
            failure = "invalid build compatibility response";
            if (json_str(answer, "expected", expected, sizeof(expected))) {
                accepted = !strcmp(signature, expected);
                failure = "this build is not listed as compatible with matchmaking";
            }
        }
    }
    SDL_LockSpinlock(&sDataLock);
    if (generation == SDL_GetAtomicInt(&sDataGeneration)) {
        SDL_strlcpy(sDataSignature, accepted ? signature : "", sizeof(sDataSignature));
        SDL_SetAtomicInt(&sDataState, accepted ? 1 : -1);
    }
    SDL_UnlockSpinlock(&sDataLock);
    if (!accepted) fprintf(stderr, "zenkaipower: online data: %s\n", failure);
    return 0;
}
void Match_DataAsk(void) {
    SDL_LockSpinlock(&sDataLock);
    int generation = SDL_AddAtomicInt(&sDataGeneration, 1) + 1;
    SDL_SetAtomicInt(&sDataState, 0);
    sDataSignature[0] = 0;
    SDL_UnlockSpinlock(&sDataLock);
    SDL_Thread *thread = SDL_CreateThread(data_check_thread, "zenkaipower-data-check", (void *)(intptr_t)generation);
    if (thread) SDL_DetachThread(thread);
    else SDL_SetAtomicInt(&sDataState, -1);
}

/* ---- is this release new enough? asked when the online screen opens ---- */
static SDL_AtomicInt sRelMin, sRelLatest, sRelBusy;

static int SDLCALL hello_thread(void *arg) {
    char body[64], answer[256];
    const char *p;

    (void)arg;
    SDL_snprintf(body, sizeof(body), "{\"rel\":%d}", PORT_RELEASE);
    if (http_post("hello", body, answer, sizeof(answer)) && (p = SDL_strstr(answer, "\"min\":")) != NULL) {
        SDL_SetAtomicInt(&sRelMin, SDL_atoi(p + 6));
        p = SDL_strstr(answer, "\"latest\":");
        SDL_SetAtomicInt(&sRelLatest, p != NULL ? SDL_atoi(p + 9) : 0);
        clock_learn(answer); /* (a service that knows the build key says its time) */
    }
    SDL_SetAtomicInt(&sRelBusy, 0);
    return 0;
}

void Match_HelloAsk(void) {
    SDL_Thread *t;
    if (SDL_GetAtomicInt(&sRelBusy)) {
        return;
    }
    SDL_SetAtomicInt(&sRelBusy, 1);
    t = SDL_CreateThread(hello_thread, "zenkaipower-hello", NULL);
    if (t == NULL) {
        SDL_SetAtomicInt(&sRelBusy, 0);
    } else {
        SDL_DetachThread(t);
    }
}

/* This release's number, and what the service last said: the oldest it lets in and the newest there is (0: not
   said, or not asked yet). */
int Match_Release(int *min, int *latest) {
    *min = SDL_GetAtomicInt(&sRelMin);
    *latest = SDL_GetAtomicInt(&sRelLatest);
    return PORT_RELEASE;
}

/* Asks the service (nothing happens if an answer is still on its way). */
void Match_QueuesAsk(int version) {
    SDL_Thread *t;
    if (SDL_GetAtomicInt(&sQueueBusy)) {
        return;
    }
    SDL_SetAtomicInt(&sQueueBusy, 1);
    sQueueVersion = version;
    t = SDL_CreateThread(queues_thread, "zenkaipower-queues", NULL);
    if (t == NULL) {
        SDL_SetAtomicInt(&sQueueBusy, 0);
    } else {
        SDL_DetachThread(t);
    }
}

/* Players waiting in queue i as last heard, -1 if not known. */
int Match_QueueWaiting(int i) {
    return i >= 0 && i < MATCH_QUEUES ? SDL_GetAtomicInt(&sQueueCount[i]) - 1 : -1;
}

int Match_SocketBusy(void) {
    return SDL_GetAtomicInt(&sM.sockBusy);
}

/* Where it stands. */
void Match_Poll(MatchStatus *out) {
    if (sM.lock == NULL) {
        SDL_memset(out, 0, sizeof(*out));
        return;
    }
    SDL_LockMutex(sM.lock);
    *out = sM.st;
    SDL_UnlockMutex(sM.lock);
}

/* Stops looking (and, for a host, gives the room up). Does not wait for the thread: a request to the Worker that is
   under way ends by itself. */
void Match_Cancel(void) {
    if (sM.thread != NULL) {
        SDL_SetAtomicInt(&sM.cancel, 1);
        SDL_DetachThread(sM.thread);
        sM.thread = NULL;
    }
    if (sM.lock != NULL) {
        SDL_LockMutex(sM.lock);
        sM.st.state = 0;
        SDL_UnlockMutex(sM.lock);
    }
}

/* ZENKAIPOWER_SIGN_TEST (called before the game starts, plat_mem.c; the program ends after it): the build key's signature
   for a fixed request, for server/test_sign.py. ZENKAIPOWER_SIGN_TEST=post also sends one "hello" the way every
   request to the service goes (so the headers can be looked at), and only to this computer: ZENKAIPOWER_MATCH_URL has to
   name http://127.0.0.1 or http://localhost. */
int Match_SignTest(void) {
    const char *mode = SDL_getenv("ZENKAIPOWER_SIGN_TEST"), *url = match_url();
    char body[64], answer[512] = "";

    Sign_SelfTest();
    if (mode != NULL && SDL_strcmp(mode, "post") == 0) {
        if (SDL_strncmp(url, "http://127.0.0.1", 16) != 0 && SDL_strncmp(url, "http://localhost", 16) != 0) {
            printf("sign: post: ZENKAIPOWER_MATCH_URL does not name this computer, nothing sent\n");
            return 1;
        }
        SDL_snprintf(body, sizeof(body), "{\"rel\":%d}", PORT_RELEASE);
        printf("sign: post %s\n", http_post("hello", body, answer, sizeof(answer)) ? answer : "no answer");
    }
    fflush(stdout);
    return 0;
}

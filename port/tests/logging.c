/* cc -std=gnu99 -Wall -Wextra -Werror port/tests/logging.c
 * $(pkg-config --cflags --libs sdl3) -pthread -o /tmp/zenkai-log-test
 * /tmp/zenkai-log-test */
#include <time.h>
static time_t sNow;
static time_t test_time(time_t *out) { if (out) *out = sNow; return sNow; }
#define time test_time
#include "../src/plat_log.c"
#undef time
#include <assert.h>
#include <sys/wait.h>

static char *read_all(const char *path) {
    FILE *file = fopen(path, "rb");
    long size;
    char *text;
    assert(file);
    assert(!fseek(file, 0, SEEK_END));
    size = ftell(file);
    assert(size >= 0);
    rewind(file);
    text = calloc((size_t)size + 1, 1);
    assert(text);
    assert(fread(text, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    return text;
}

static void seed(const char *path) {
    char date[32];
    FILE *file = fopen(path, "wb");
    assert(file);
    stamp(sNow - KEEP_SECONDS - 1, date);
    fprintf(file, "%sexpired\nold continuation\n", date);
    stamp(sNow - KEEP_SECONDS, date);
    fprintf(file, "%sboundary\n", date);
    stamp(sNow - 60, date);
    fprintf(file, "%srecent\ncrash continuation\n", date);
    fclose(file);
}

static void *writer(void *unused) {
    (void)unused;
    for (int i = 0; i < 30; i++) record("net.c", "concurrent entry\n");
    return NULL;
}

int main(void) {
    char base[] = "/tmp/zenkai-logs-XXXXXX", folder[256], paths[2][300], console[300];
    char *text;
    pthread_t threads[3];
    sNow = time(NULL);
    unsetenv("ZENKAIPOWER_NET_BOT");
    assert(mkdtemp(base));
    /* A bot cannot create log files or append duplicate match diagnostics. */
    pid_t bot = fork();
    assert(bot >= 0);
    if (!bot) {
        setenv("ZENKAIPOWER_NET_BOT", "1", 1);
        Port_LogInit(base);
        record("net.c", "bot process connection\n");
        assert(!Port_LogPath(0)[0] && !Port_LogPath(1)[0]);
        snprintf(folder, sizeof(folder), "%s/logs", base);
        assert(access(folder, F_OK) != 0);
        _exit(0);
    }
    int botStatus;
    assert(waitpid(bot, &botStatus, 0) == bot && WIFEXITED(botStatus) && !WEXITSTATUS(botStatus));
    snprintf(folder, sizeof(folder), "%s/logs", base);
    assert(!mkdir(folder, 0700));
    snprintf(paths[0], sizeof(paths[0]), "%s/zenkaipower.log", folder);
    snprintf(paths[1], sizeof(paths[1]), "%s/dragonnetbattle.log", folder);
    for (int i = 0; i < 2; i++) seed(paths[i]);
    Port_LogInit(base);
    assert(is_online("C:\\game\\port\\src\\gs\\net_match.c", "stats"));
    assert(is_online("/game/port/src/gs/state.c", "rollback"));
    assert(!is_online("C:\\game\\port\\src\\gs\\gs_draw.c", "renderer"));
    for (int i = 0; i < 2; i++) {
        assert(!strcmp(Port_LogPath(i), paths[i]));
        text = read_all(paths[i]);
        assert(!strstr(text, "expired") && !strstr(text, "old continuation"));
        assert(strstr(text, "boundary") && strstr(text, "recent"));
        assert(strstr(text, "crash continuation"));
        free(text);
    }

    /* The bot must still receive the original, untimestamped console protocol. */
    snprintf(console, sizeof(console), "%s/console", base);
    int saved = dup(STDERR_FILENO);
    FILE *capture = freopen(console, "w", stderr);
    assert(capture);
    assert(Port_LogPrintf("gs/net.c", stderr, "zenkaipower: lobby: state %d\n", 1) > 0);
    assert(Port_LogPuts("gs/gs_draw.c", "renderer ready\n", stderr) >= 0);
    Port_LogPrintf("net.c", stderr, "zenkaipower: net: CPU opponent accepted at level 5/5\n");
    fflush(stderr);
    assert(dup2(saved, STDERR_FILENO) >= 0);
    close(saved);
    text = read_all(console);
    assert(!strcmp(text, "zenkaipower: lobby: state 1\nrenderer ready\nzenkaipower: net: CPU opponent accepted at level 5/5\n"));
    free(text);
    text = read_all(paths[0]);
    assert(strstr(text, "renderer ready") && !strstr(text, "zenkaipower: lobby:"));
    free(text);
    text = read_all(paths[1]);
    assert(strstr(text, "zenkaipower: lobby: state 1") && !strstr(text, "renderer ready"));
    assert(!strstr(text, "CPU opponent"));
    free(text);

    for (int i = 0; i < 30; i++) {
        record("net_match.c", "zenkaipower: stats: battle/get saved\n");
        record("net.c", "zenkaipower: net: CPU opponent accepted at level 5/5\n");
        record("net.c", "zenkaipower: net: playing as CPU at level 5/5\n");
        record("plat_verify.c", "zenkaipower: online data: 68605 required files present; binary signature abc\n");
    }
    record("plat_verify.c", "zenkaipower: online data: 68605 required files present; binary signature def\n");
    record("net_match.c", "zenkaipower: stats: battle/get failed after retries\n");
    record("net.c", "zenkaipower: net: the two games no longer agree (desync): the match has ended.\n");
    text = read_all(paths[1]);
    assert(!strstr(text, "battle/get saved") && !strstr(text, "CPU"));
    char *first = strstr(text, "binary signature abc");
    assert(first && !strstr(first + 1, "binary signature abc"));
    assert(strstr(text, "binary signature def"));
    assert(strstr(text, "battle/get failed") && strstr(text, "desync"));
    free(text);

    FILE *ordinary = tmpfile();
    assert(ordinary);
    Port_LogPrintf("net.c", ordinary, "not a diagnostic\n");
    fclose(ordinary);
    text = read_all(paths[1]);
    assert(!strstr(text, "not a diagnostic"));
    free(text);

    /* Concurrent processes and threads share files without losing entries. */
    pid_t child = fork();
    assert(child >= 0);
    if (!child) { writer(NULL); _exit(0); }
    for (int i = 0; i < 3; i++) assert(!pthread_create(&threads[i], NULL, writer, NULL));
    for (int i = 0; i < 3; i++) assert(!pthread_join(threads[i], NULL));
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    text = read_all(paths[1]);
    int count = 0;
    for (char *at = text; (at = strstr(at, "concurrent entry")); at++) count++;
    assert(count == 120);
    free(text);

    /* A running process cleans both categories, even an idle online log. */
    sNow += KEEP_SECONDS + 3600;
    record("gs_draw.c", "new day\nsecond line\n");
    for (int i = 0; i < 2; i++) {
        text = read_all(paths[i]);
        assert(!strstr(text, "recent") && !strstr(text, "boundary"));
        assert(!strstr(text, "concurrent entry") && !strstr(text, "crash continuation"));
        if (!i) assert(strstr(text, "new day") && strstr(text, "second line"));
        else assert(!text[0]);
        free(text);
    }
    /* Long log lines survive periodic cleanup. */
    char large[12000];
    memset(large, 'x', sizeof(large) - 1);
    large[sizeof(large) - 1] = 0;
    record("net.c", large);
    sNow += 3601;
    record("net.c", "after cleanup\n");
    text = read_all(paths[1]);
    assert(strstr(text, large) && strstr(text, "after cleanup"));
    free(text);

    for (int i = 0; i < 2; i++) assert(!unlink(paths[i]));
    assert(!unlink(console));
    assert(!rmdir(folder) && !rmdir(base));
    puts("Release logging checks passed");
    return 0;
}

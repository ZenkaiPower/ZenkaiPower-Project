"""Exercise the menu gate against the current build-verification states."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/menu/port_tbo.c').read_text(encoding='utf-8')


def function(name):
    start = source.index(f'static s32 {name}(void) {{')
    # Tb_DataState is a single-line helper; the gate is a multi-line function.
    end = source.index('}', start) + 1 if name == 'Tb_DataState' else source.index('\n}', start) + 2
    return source[start:end]


code = r'''
#include <assert.h>
#include <string.h>
typedef int s32;
static int sTbLan, remoteState, notes, errors, sounds;
static const char *message;
static int Match_DataState(void) { return remoteState; }
static void Tb_Note(const char *text) { notes++; message = text; }
static void Tb_ShowMessage(const char *text) { errors++; message = text; }
static void Snd_PlaySe(int a, int b) { sounds++; }
''' + function('Tb_DataState') + '\n' + function('Tb_Verified') + r'''
int main(void) {
    remoteState = 0;
    assert(!Tb_Verified() && notes == 1 && errors == 0 && sounds == 0);
    assert(strstr(message, "CHECKING BUILD COMPATIBILITY"));
    remoteState = -1;
    assert(!Tb_Verified() && errors == 1 && sounds == 1);
    assert(strstr(message, "compatibility check failed") && !strstr(message, "modification detected"));
    remoteState = 1;
    assert(Tb_Verified() && errors == 1);
    /* LAN keeps its existing independent path when the internet check fails. */
    remoteState = -1; sTbLan = 1;
    assert(Tb_Verified() && errors == 1);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    test = work / 'gate.c'
    test.write_text(code, encoding='utf-8')
    binary = work / 'gate'
    subprocess.run([os.environ.get('CC', 'cc'), str(test), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('Online menu: pending/approved/rejected builds and independent LAN gating passed')

network = (root / 'port/src/gs/net_match.c').read_text(encoding='utf-8')
start = network.index('static int SDLCALL data_check_thread(void *arg) {')
thread = network[start:network.index('\n}', start) + 2]
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SDLCALL
#define PORT_RELEASE 1000001
#define SDL_snprintf snprintf
static int sDataGeneration = 1, sDataState, sDataLock;
static char sDataSignature[65];
static int readable = 1, reachable = 1, valid = 1, approved = 1;
static int SDL_GetAtomicInt(int *p) { return *p; }
static void SDL_SetAtomicInt(int *p, int value) { *p = value; }
static void SDL_LockSpinlock(int *p) { }
static void SDL_UnlockSpinlock(int *p) { }
static void SDL_strlcpy(char *out, const char *in, size_t count) { snprintf(out, count, "%s", in); }
static int Port_DataSignature(char *out) { strcpy(out, "fixture"); return readable; }
static int http_post(const char *path, const char *body, char *answer, size_t size) {
    assert(!strcmp(path, "data-check")); return reachable;
}
static int json_str(const char *answer, const char *key, char *out, size_t size) {
    assert(!strcmp(key, "expected")); snprintf(out, size, "%s", approved ? "fixture" : ""); return valid;
}
''' + thread + r'''
int main(void) {
    readable = 0; data_check_thread((void *)(intptr_t)1);
    assert(sDataState == -1 && !sDataSignature[0]);
    readable = 1; reachable = 0; data_check_thread((void *)(intptr_t)1);
    assert(sDataState == -1 && !sDataSignature[0]);
    reachable = 1; valid = 0; data_check_thread((void *)(intptr_t)1);
    assert(sDataState == -1 && !sDataSignature[0]);
    valid = 1; approved = 0; data_check_thread((void *)(intptr_t)1);
    assert(sDataState == -1 && !sDataSignature[0]);
    approved = 1; data_check_thread((void *)(intptr_t)1);
    assert(sDataState == 1 && !strcmp(sDataSignature, "fixture"));
    /* A superseded request cannot overwrite the current accepted result. */
    approved = 0; data_check_thread((void *)(intptr_t)0);
    assert(sDataState == 1 && !strcmp(sDataSignature, "fixture"));
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    test = work / 'verification.c'
    test.write_text(code, encoding='utf-8')
    binary = work / 'verification'
    subprocess.run([os.environ.get('CC', 'cc'), str(test), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
    for reason in ('could not be read', 'could not be reached', 'invalid build compatibility response', 'not listed as compatible'):
        assert reason in result.stderr, result.stderr
print('Verification: unreadable files, unavailable service, malformed replies and unapproved builds remain blocked with distinct diagnostics')

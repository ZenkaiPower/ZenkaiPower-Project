#include <jni.h>
#include <string>
#include <cstdlib>
#include "../../../../../port/setup/native.h"
#include "../../../../../port/setup/text.h"

static JavaVM *vm;
static jobject owner;
static std::string utf(JNIEnv *env,jstring value) {
    if (!value) return "";
    const char *p=env->GetStringUTFChars(value,nullptr);
    std::string out(p); env->ReleaseStringUTFChars(value,p); return out;
}
bool Android_PrepareText(const std::string &) {
    JNIEnv *env=nullptr;
    if (vm->AttachCurrentThread(&env,nullptr)!=JNI_OK) return false;
    jclass type=env->GetObjectClass(owner);
    jmethodID method=env->GetMethodID(type,"prepareText","()Z");
    bool ok=method && env->CallBooleanMethod(owner,method);
    if (env->ExceptionCheck()) { env->ExceptionClear(); ok=false; }
    env->DeleteLocalRef(type); vm->DetachCurrentThread(); return ok;
}
extern "C" JNIEXPORT void JNICALL
Java_com_zenkaipower_android_DiscInstaller_start(JNIEnv *env,jobject self,jstring folder,jstring image,jint disc,jstring cache) {
    Native_Join();
    if (owner) env->DeleteGlobalRef(owner);
    owner=env->NewGlobalRef(self); env->GetJavaVM(&vm);
    std::string root=utf(env,folder)+"/", path=utf(env,image), temporary=utf(env,cache);
    setenv("TMPDIR",temporary.c_str(),1);
    setenv("ZENKAIPOWER_TMPDIR",temporary.c_str(),1);
    SetupText_Load(root);
    Native_Start(root,disc==0 ? path : "",disc==1 ? path : "",disc==2 ? path : "",disc==3 ? path : "");
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_zenkaipower_android_DiscInstaller_poll(JNIEnv *env,jobject) {
    std::string line; return Native_Poll(line) ? env->NewStringUTF(line.c_str()) : nullptr;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_zenkaipower_android_DiscInstaller_running(JNIEnv *,jobject) { return Native_Running(); }
extern "C" JNIEXPORT void JNICALL
Java_com_zenkaipower_android_DiscInstaller_cancel(JNIEnv *,jobject) { Native_Cancel(); }
extern "C" JNIEXPORT void JNICALL
Java_com_zenkaipower_android_DiscInstaller_release(JNIEnv *env,jobject) {
    Native_Join();
    if (owner) { env->DeleteGlobalRef(owner); owner=nullptr; }
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_zenkaipower_android_DiscInstaller_installed(JNIEnv *env,jobject,jstring folder,jint disc) {
    return Native_DiscInstalled(utf(env,folder)+"/",disc);
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_zenkaipower_android_DiscInstaller_prepareTouchArtwork(JNIEnv *env,jclass,jstring folder) {
    return Native_PrepareTouchArtwork(utf(env,folder)+"/");
}

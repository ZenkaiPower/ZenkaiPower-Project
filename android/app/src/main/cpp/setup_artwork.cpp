#include <jni.h>
#include <array>
#include <string>
#include "../../../../../port/setup/native.h"
namespace {
std::string utf(JNIEnv *env,jstring text) {
    const char *p=env->GetStringUTFChars(text,nullptr); std::string out(p);
    env->ReleaseStringUTFChars(text,p); return out;
}
}
extern "C" JNIEXPORT jint JNICALL
Java_com_zenkaipower_android_SetupArtwork_verify(JNIEnv *env,jclass,jstring folder) {
    std::string root=utf(env,folder)+"/"; std::array<bool,4> state{}; int mask=0;
    state[0]=Native_VerifyInstalledUs(root);
    for (int i=1;i<4;i++) state[i]=Native_DiscInstalled(root,i);
    for (int i=0;i<4;i++) if (state[i]) mask|=1<<i;
    return mask;
}

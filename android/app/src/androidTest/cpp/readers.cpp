#include <jni.h>
#include "../../../../../port/setup/native.cpp"
bool Android_PrepareText(const std::string &) { return false; }
extern "C" JNIEXPORT jstring JNICALL
Java_com_zenkaipower_android_InstallerChecks_verifyWiiDescriptor(JNIEnv *env,jclass,jstring folder,jint descriptor) {
    const char *r=env->GetStringUTFChars(folder,nullptr); std::string root=std::string(r)+"/";
    env->ReleaseStringUTFChars(folder,r);
    std::string error=Native_VerifyDisc(root,"/proc/self/fd/"+std::to_string(descriptor),2);
    return env->NewStringUTF(error.c_str());
}
extern "C" JNIEXPORT void JNICALL
Java_com_zenkaipower_android_InstallerChecks_extractFixture(JNIEnv *env,jclass,jstring folder,jstring image,jint kind) {
    const char *r=env->GetStringUTFChars(folder,nullptr),*p=env->GetStringUTFChars(image,nullptr);
    std::string root=std::string(r)+"/",path=p;
    env->ReleaseStringUTFChars(folder,r); env->ReleaseStringUTFChars(image,p);
    try {
        setenv("TMPDIR",root.c_str(),1);
        if (kind==0) android_buttons(root);
        else step_optional(root,path,kind==2);
    } catch (const Stop &error) { env->ThrowNew(env->FindClass("java/io/IOException"),error.why.c_str()); }
      catch (const std::exception &error) { env->ThrowNew(env->FindClass("java/io/IOException"),error.what()); }
}

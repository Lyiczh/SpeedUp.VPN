#include "jni.h"

#include <algorithm>
#include <cerrno>

#include <arpa/inet.h>
#include <unistd.h>
#include <sys/un.h>
#include <ancillary.h>

using namespace std;

// Based on: https://android.googlesource.com/platform/libcore/+/564c7e8/luni/src/main/native/libcore_io_Linux.cpp#256
static void throwException(JNIEnv* env, jclass exceptionClass, jmethodID ctor2, const char* functionName, int error) {
    jstring detailMessage = env->NewStringUTF(functionName);
    if (detailMessage == nullptr) {
        // Not really much we can do here. We're probably dead in the water,
        // but let's try to stumble on...
        env->ExceptionClear();
    }
    env->Throw(reinterpret_cast<jthrowable>(env->NewObject(exceptionClass, ctor2, detailMessage, error)));
    env->DeleteLocalRef(detailMessage);
}

static void throwErrnoException(JNIEnv* env, const char* functionName) {
    int error = errno;
    static auto ErrnoException = reinterpret_cast<jclass>(env->NewGlobalRef(
            env->FindClass("android/system/ErrnoException")));
    static jmethodID ctor2 = env->GetMethodID(ErrnoException, "<init>", "(Ljava/lang/String;I)V");
    throwException(env, ErrnoException, ctor2, functionName, error);
}

#pragma clang diagnostic ignored "-Wunused-parameter"
extern "C" {
JNIEXPORT void JNICALL
        Java_com_github_shadowsocks_JniHelper_sendFd(JNIEnv *env, jclass type, jint tun_fd, jstring path) {
    const char *sock_str = env->GetStringUTFChars(path, nullptr);
    if (int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); fd == -1) {
        throwErrnoException(env, "socket");
    } else {
        sockaddr_un addr { .sun_family = AF_UNIX };
        strncpy(addr.sun_path, sock_str, sizeof(addr.sun_path) - 1);
        if (connect(fd, (sockaddr*) &addr, sizeof(addr)) == -1) throwErrnoException(env, "connect");
        else if (ancil_send_fd(fd, tun_fd)) throwErrnoException(env, "ancil_send_fd");
        close(fd);
    }
    env->ReleaseStringUTFChars(path, sock_str);
}

JNIEXPORT jbyteArray JNICALL
Java_com_github_shadowsocks_JniHelper_parseNumericAddress(JNIEnv *env, jclass type, jstring str) {
    const char *src = env->GetStringUTFChars(str, nullptr);
    jbyte dst[max(sizeof(in_addr), sizeof(in6_addr))];
    jbyteArray arr = nullptr;
    if (inet_pton(AF_INET, src, dst) == 1) {
        arr = env->NewByteArray(sizeof(in_addr));
        env->SetByteArrayRegion(arr, 0, sizeof(in_addr), dst);
    } else if (inet_pton(AF_INET6, src, dst) == 1) {
        arr = env->NewByteArray(sizeof(in6_addr));
        env->SetByteArrayRegion(arr, 0, sizeof(in6_addr), dst);
    }
    env->ReleaseStringUTFChars(str, src);
    return arr;
}
}

/*
 * This is called by the VM when the shared library is first loaded.
 */
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    return JNI_VERSION_1_6;
}

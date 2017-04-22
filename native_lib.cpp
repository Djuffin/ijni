#include <jni.h>
#include <jvmti.h>
#include "HelloWorld.h"

JNIEXPORT jint JNICALL Java_HelloWorld_sub (JNIEnv *env, jclass cls, jint a, jint b) {
  return sizeof(jobject);
}

JNIEXPORT jint JNICALL Java_HelloWorld_add (JNIEnv *env, jclass cls, jint a, jint b) {
  static JNINativeMethod m;
  m.name = (char*)"add";
  m.signature = (char*)"(II)I";
  m.fnPtr = (void*)Java_HelloWorld_sub;
  env->RegisterNatives(cls, &m, 1);
  return a + b;
}

JNIEXPORT jint JNICALL secret_callback (JNIEnv *env, jclass cls, jint a, jint b) {
  return 42;
}

jvmtiEnv* CreateJvmtiEnv(JavaVM* vm) {
  jvmtiEnv* jvmti_env;
  jint result = vm->GetEnv((void**)&jvmti_env, JVMTI_VERSION_1_2);
  if (result != JNI_OK) {
    printf("error1\n");
    return nullptr;
  }
  return jvmti_env;
}

void JNICALL
NativeMethodBind(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread,
            jmethodID method,
            void* address,
            void** new_address_ptr) {
  if (address == (void*)Java_HelloWorld_add) {
    *new_address_ptr = (void*)secret_callback;
  }
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
  jvmtiError error;

  jvmtiEnv* ti = CreateJvmtiEnv(vm);

  // Hook up event callbacks
  jvmtiEventCallbacks callbacks = {};
  callbacks.NativeMethodBind = &NativeMethodBind;
  error = ti->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (error != JNI_OK) {
    printf("error2\n");
  }
  jvmtiCapabilities caps;
  error = ti->GetPotentialCapabilities(&caps);
  error = ti->AddCapabilities(&caps);
  error = ti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_NATIVE_METHOD_BIND, nullptr);
  if (error != JNI_OK) {
    printf("error3\n");
  }

  return 0;
}

#include <jni.h>
#include "HelloWorld.h"

JNIEXPORT jint JNICALL Java_HelloWorld_sub (JNIEnv *env, jclass class, jint a, jint b) {
  return sizeof(jobject);
}

JNIEXPORT jint JNICALL Java_HelloWorld_add (JNIEnv *env, jclass class, jint a, jint b) {
  static JNINativeMethod m;
  m.name = "add";
  m.signature = "(II)I";
  m.fnPtr = Java_HelloWorld_sub;
  (*env)->RegisterNatives(env, class, &m, 1);
  return a + b;
}
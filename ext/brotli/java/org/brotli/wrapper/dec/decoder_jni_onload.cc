/* Copyright 2017 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#include <jni.h>

#include "decoder_jni.h"   // NOLINT: build/include

#ifdef __cplusplus
extern "C" {
#endif

static const JNINativeMethod kDecoderMethods[] = {
    {"nativeCreate", "([J)Ljava/nio/ByteBuffer;",
     reinterpret_cast<void*>(
         Java_org_brotli_wrapper_dec_DecoderJNI_nativeCreate)},
    {"nativePush", "([JI)V",
     reinterpret_cast<void*>(
         Java_org_brotli_wrapper_dec_DecoderJNI_nativePush)},
    {"nativePull", "([J)Ljava/nio/ByteBuffer;",
     reinterpret_cast<void*>(
         Java_org_brotli_wrapper_dec_DecoderJNI_nativePull)},
    {"nativeDestroy", "([J)V",
     reinterpret_cast<void*>(
         Java_org_brotli_wrapper_dec_DecoderJNI_nativeDestroy)},
    {"nativeAttachDictionary", "([JLjava/nio/ByteBuffer;)Z",
     reinterpret_cast<void*>(
         Java_org_brotli_wrapper_dec_DecoderJNI_nativeAttachDictionary)}};

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }

  jclass clazz =
      env->FindClass("org/brotli/wrapper/dec/DecoderJNI");
  if (clazz == nullptr) {
    return -1;
  }

  if (env->RegisterNatives(
          clazz, kDecoderMethods,
          sizeof(kDecoderMethods) / sizeof(kDecoderMethods[0])) < 0) {
    return -1;
  }

  return JNI_VERSION_1_6;
}

#ifdef __cplusplus
}
#endif

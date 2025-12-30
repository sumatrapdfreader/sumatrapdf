/* Copyright 2017 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#ifndef BROTLI_WRAPPER_DEC_DECODER_JNI_H_
#define BROTLI_WRAPPER_DEC_DECODER_JNI_H_

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a new Decoder.
 *
 * Cookie to address created decoder is stored in out_cookie. In case of failure
 * cookie is 0.
 *
 * @param ctx {out_cookie, in_directBufferSize} tuple
 * @returns direct ByteBuffer if directBufferSize is not 0; otherwise null
 */
JNIEXPORT jobject JNICALL
Java_org_brotli_wrapper_dec_DecoderJNI_nativeCreate(
    JNIEnv* env, jobject /*jobj*/, jlongArray ctx);

/**
 * Push data to decoder.
 *
 * status codes:
 *  - 0 error happened
 *  - 1 stream is finished, no more input / output expected
 *  - 2 needs more input to process further
 *  - 3 needs more output to process further
 *  - 4 ok, can proceed further without additional input
 *
 * @param ctx {in_cookie, out_status} tuple
 * @param input_length number of bytes provided in input or direct input;
 *                     0 to process further previous input
 */
JNIEXPORT void JNICALL
Java_org_brotli_wrapper_dec_DecoderJNI_nativePush(
    JNIEnv* env, jobject /*jobj*/, jlongArray ctx, jint input_length);

/**
 * Pull decompressed data from decoder.
 *
 * @param ctx {in_cookie, out_status} tuple
 * @returns direct ByteBuffer; all the produced data MUST be consumed before
 *          any further invocation; null in case of error
 */
JNIEXPORT jobject JNICALL
Java_org_brotli_wrapper_dec_DecoderJNI_nativePull(
    JNIEnv* env, jobject /*jobj*/, jlongArray ctx);

/**
 * Releases all used resources.
 *
 * @param ctx {in_cookie} tuple
 */
JNIEXPORT void JNICALL
Java_org_brotli_wrapper_dec_DecoderJNI_nativeDestroy(
    JNIEnv* env, jobject /*jobj*/, jlongArray ctx);

JNIEXPORT jboolean JNICALL
Java_org_brotli_wrapper_dec_DecoderJNI_nativeAttachDictionary(
    JNIEnv* env, jobject /*jobj*/, jlongArray ctx, jobject dictionary);

#ifdef __cplusplus
}
#endif

#endif  // BROTLI_WRAPPER_DEC_DECODER_JNI_H_

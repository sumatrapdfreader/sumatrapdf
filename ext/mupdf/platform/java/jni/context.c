// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

/* Context interface */

/* Put the fz_context in thread-local storage */

#ifdef _WIN32
static CRITICAL_SECTION mutexes[FZ_LOCK_MAX];
#else
static pthread_mutex_t mutexes[FZ_LOCK_MAX];
#endif

static void lock(void *user, int lock)
{
#ifdef _WIN32
	EnterCriticalSection(&mutexes[lock]);
#else
	(void)pthread_mutex_lock(&mutexes[lock]);
#endif
}

static void unlock(void *user, int lock)
{
#ifdef _WIN32
	LeaveCriticalSection(&mutexes[lock]);
#else
	(void)pthread_mutex_unlock(&mutexes[lock]);
#endif
}


static const fz_locks_context locks =
{
	NULL, /* user */
	lock,
	unlock
};

static void fin_base_context(JNIEnv *env)
{
	int i;

	fz_drop_context(base_context);
	base_context = NULL;

	for (i = 0; i < FZ_LOCK_MAX; i++)
#ifdef _WIN32
		DeleteCriticalSection(&mutexes[i]);
#else
		(void)pthread_mutex_destroy(&mutexes[i]);
#endif
}

#ifndef _WIN32
static void drop_tls_context(void *arg)
{
	fz_context *ctx = (fz_context *)arg;

	fz_drop_context(ctx);
}
#endif

static void log_callback(void *user, const char *message)
{
	jboolean detach = JNI_FALSE;
	JNIEnv *env = NULL;
	jobject jcallback;
	jstring jmessage;
	jobject jlock;
	jmethodID mid;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		return;

	if (user != NULL)
		mid = mid_Context_Log_error;
	else
		mid = mid_Context_Log_warning;

	jcallback = (*env)->GetStaticObjectField(env, cls_Context, fid_Context_log);
	if (jcallback)
	{
		jlock = (*env)->GetStaticObjectField(env, cls_Context, fid_Context_lock);
		(*env)->MonitorEnter(env, jlock);
		jmessage = (*env)->NewStringUTF(env, message);
		(*env)->CallVoidMethod(env, jcallback, mid, jmessage);
		(*env)->DeleteLocalRef(env, jmessage);
		(*env)->MonitorExit(env, jlock);
		(*env)->DeleteLocalRef(env, jcallback);
		(*env)->DeleteLocalRef(env, jlock);
	}

	jni_detach_thread(detach);
}

static int init_base_context(JNIEnv *env)
{
	int i;
#ifdef FZ_JAVA_STORE_SIZE
	size_t fz_store_size = FZ_JAVA_STORE_SIZE;
#else
	size_t fz_store_size = FZ_STORE_DEFAULT;
#endif
	char *env_fz_store_size;

#ifdef _WIN32
	/* No destructor on windows. We will leak contexts.
	 * There is no easy way around this, but this page:
	 * http://stackoverflow.com/questions/3241732/is-there-anyway-to-dynamically-free-thread-local-storage-in-the-win32-apis/3245082#3245082
	 * suggests a workaround that we can implement if we
	 * need to. */
	context_key = TlsAlloc();
	if (context_key == TLS_OUT_OF_INDEXES)
	{
		LOGE("cannot get thread local storage for storing base context");
		return -1;
	}
#else
	int ret = pthread_key_create(&context_key, drop_tls_context);
	if (ret < 0)
	{
		LOGE("cannot get thread local storage for storing base context");
		return -1;
	}
#endif

	for (i = 0; i < FZ_LOCK_MAX; i++)
#ifdef _WIN32
		InitializeCriticalSection(&mutexes[i]);
#else
		(void)pthread_mutex_init(&mutexes[i], NULL);
#endif

	env_fz_store_size = getenv("FZ_JAVA_STORE_SIZE");
	if (env_fz_store_size)
		fz_store_size = fz_atoz(env_fz_store_size);
	base_context = fz_new_context(NULL, &locks, fz_store_size);

	if (!base_context)
	{
		LOGE("cannot create base context");
		fin_base_context(env);
		return -1;
	}

	fz_set_error_callback(base_context, log_callback, (void *) 1);
	fz_set_warning_callback(base_context, log_callback, (void *) 0);

	fz_try(base_context)
		fz_register_document_handlers(base_context);
	fz_catch(base_context)
	{
		fz_report_error(base_context);
		LOGE("cannot register document handlers");
		fin_base_context(env);
		return -1;
	}

#ifdef HAVE_ANDROID
	fz_install_load_system_font_funcs(base_context,
		load_droid_font,
		load_droid_cjk_font,
		load_droid_fallback_font);
#endif

	return 0;
}

static fz_context *get_context(JNIEnv *env)
{
	fz_context *ctx = (fz_context *)
#ifdef _WIN32
		TlsGetValue(context_key);
	if (ctx == NULL && GetLastError() != ERROR_SUCCESS) jni_throw_run(env, "cannot get context");
#else
		pthread_getspecific(context_key);
#endif

	if (ctx)
		return ctx;

	ctx = fz_clone_context(base_context);
	if (!ctx) jni_throw_oom(env, "failed to clone fz_context");

#ifdef _WIN32
	if (TlsSetValue(context_key, ctx) == 0) jni_throw_run(env, "cannot store context");
#else
	if (pthread_setspecific(context_key, ctx) != 0) jni_throw_run(env, "cannot store context");
#endif
	return ctx;
}


JNIEXPORT jint JNICALL
FUN(Context_initNative)(JNIEnv *env, jclass cls)
{
	if (!check_enums())
		return -1;

	/* Must init the context before find_finds, because the act of
	 * finding the fids can cause classes to load. This causes
	 * statics to be setup, which can in turn call JNI code, which
	 * requires the context. (For example see ColorSpace) */
	if (init_base_context(env) < 0)
		return -1;

	if (find_fids(env) < 0)
	{
		fin_base_context(env);
		return -1;
	}

	return 0;
}

JNIEXPORT void JNICALL
FUN(Context_emptyStore)(JNIEnv *env, jclass cls)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return;

	fz_empty_store(ctx);
}

JNIEXPORT void JNICALL
FUN(Context_enableICC)(JNIEnv *env, jclass cls)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return;

	fz_enable_icc(ctx);
}

JNIEXPORT void JNICALL
FUN(Context_disableICC)(JNIEnv *env, jclass cls)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return;

	fz_disable_icc(ctx);
}

JNIEXPORT void JNICALL
FUN(Context_setAntiAliasLevel)(JNIEnv *env, jclass cls, jint level)
{
	fz_context *ctx = get_context(env);
	if (!ctx) return;

	fz_set_aa_level(ctx, level);
}

JNIEXPORT jobject JNICALL
FUN(Context_getVersion)(JNIEnv *env, jclass cls)
{
	fz_context *ctx = get_context(env);
	jobject jversion = NULL;
	jobject jvs = NULL;

	if (!ctx) return NULL;

	jvs = (*env)->NewStringUTF(env, FZ_VERSION);
	if (!jvs || (*env)->ExceptionCheck(env))
		return NULL;

	jversion = (*env)->NewObject(env, cls_Context_Version, mid_Context_Version_init);
	if (!jversion || (*env)->ExceptionCheck(env))
		return NULL;

	(*env)->SetIntField(env, jversion, fid_Context_Version_major, FZ_VERSION_MAJOR);
	(*env)->SetIntField(env, jversion, fid_Context_Version_minor, FZ_VERSION_MINOR);
	(*env)->SetIntField(env, jversion, fid_Context_Version_patch, FZ_VERSION_PATCH);
	(*env)->SetObjectField(env, jversion, fid_Context_Version_version, jvs);

	return jversion;
}

JNIEXPORT void JNICALL
FUN(Context_setUserCSS)(JNIEnv *env, jclass cls, jstring jcss)
{
	fz_context *ctx = get_context(env);
	const char *css = NULL;

	if (jcss)
		css = (*env)->GetStringUTFChars(env, jcss, NULL);

	fz_try(ctx)
		fz_set_user_css(ctx, css);
	fz_always(ctx)
		if (jcss)
			(*env)->ReleaseStringUTFChars(env, jcss, css);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Context_useDocumentCSS)(JNIEnv *env, jclass cls, jboolean state)
{
	fz_context *ctx = get_context(env);

	fz_try(ctx)
		fz_set_use_document_css(ctx, state);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(Context_shrinkStore)(JNIEnv *env, jclass cls, jint percent)
{
	fz_context *ctx = get_context(env);
	int success = 0;

	if (!ctx) return JNI_FALSE;
	if (percent < 0) jni_throw_arg(env, "percent must not be negative");
	if (percent > 100) jni_throw_arg(env, "percent must not be more than 100");

	fz_try(ctx)
		success = fz_shrink_store(ctx, percent);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return success != 0;
}

static fz_font *load_java_font_file(fz_context *ctx, const char *name, const char *script, int bold, int italic)
{
	jboolean detach = JNI_FALSE;
	JNIEnv *env = NULL;
	fz_font *font = NULL;
	jobject jfont = NULL;
	jstring jname = NULL;
	jstring jscript = NULL;
	jobject jfontloader = NULL;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in load_java_font_file");

	jname = (*env)->NewStringUTF(env, name);
	if (!jname || (*env)->ExceptionCheck(env))
		fz_throw_and_detach_thread(ctx, detach, FZ_ERROR_GENERIC, "cannot convert font name to java string");

	jscript = (*env)->NewStringUTF(env, script);
	if (!jscript || (*env)->ExceptionCheck(env))
	{
		(*env)->DeleteLocalRef(env, jname);
		fz_throw_and_detach_thread(ctx, detach, FZ_ERROR_GENERIC, "cannot convert script name to java string");
	}

	jfontloader = (*env)->GetStaticObjectField(env, cls_Font, fid_Font_fontLoader);
	if (jfontloader)
	{
		jfont = (*env)->CallObjectMethod(env, jfontloader, mid_FontLoader_loadFont, jname, jscript, bold, italic);
		(*env)->DeleteLocalRef(env, jfontloader);
		(*env)->DeleteLocalRef(env, jscript);
		(*env)->DeleteLocalRef(env, jname);
		if ((*env)->ExceptionCheck(env))
			fz_throw_and_detach_thread(ctx, detach, FZ_ERROR_GENERIC, "cannot load font");

		if (jfont)
		{
			font = CAST(fz_font *, (*env)->GetLongField(env, jfont, fid_Font_pointer));
			font = fz_keep_font(ctx, font);
		}
	}

	jni_detach_thread(detach);

	return font;
}

static fz_font *load_java_font(fz_context *ctx, const char *name, int bold, int italic, int needs_exact_metrics)
{
	return load_java_font_file(ctx, name, "undefined", bold, italic);
}

static fz_font *load_java_cjk_font(fz_context *ctx, const char *name, int ordering, int serif)
{
	switch (ordering)
	{
	case FZ_ADOBE_CNS: return load_java_font_file(ctx, name, "TC", 0, 0);
	case FZ_ADOBE_GB: return load_java_font_file(ctx, name, "SC", 0, 0);
	case FZ_ADOBE_JAPAN: return load_java_font_file(ctx, name, "JP", 0, 0);
	case FZ_ADOBE_KOREA: return load_java_font_file(ctx, name, "KR", 0, 0);
	}
	return NULL;
}

static fz_font *load_java_fallback_font(fz_context *ctx, int script, int language, int serif, int bold, int italic)
{
	return load_java_font_file(ctx, "undefined", fz_lookup_script_name(ctx, script, language), bold, italic);
}

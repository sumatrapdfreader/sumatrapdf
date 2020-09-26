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

static int init_base_context(JNIEnv *env)
{
	int i;

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

	base_context = fz_new_context(NULL, &locks, FZ_STORE_DEFAULT);
	if (!base_context)
	{
		LOGE("cannot create base context");
		fin_base_context(env);
		return -1;
	}

	fz_try(base_context)
		fz_register_document_handlers(base_context);
	fz_catch(base_context)
	{
		LOGE("cannot register document handlers (%s)", fz_caught_message(base_context));
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

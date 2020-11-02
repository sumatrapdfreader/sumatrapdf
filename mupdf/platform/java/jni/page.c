/* Page interface */

JNIEXPORT void JNICALL
FUN(Page_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page_safe(env, self);
	if (!ctx || !page) return;
	(*env)->SetLongField(env, self, fid_Page_pointer, 0);
	fz_drop_page(ctx, page);
}

JNIEXPORT jobject JNICALL
FUN(Page_toPixmap)(JNIEnv *env, jobject self, jobject jctm, jobject jcs, jboolean alpha, jboolean showExtra)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_pixmap *pixmap = NULL;

	if (!ctx || !page) return NULL;

	fz_try(ctx)
	{
		if (showExtra)
			pixmap = fz_new_pixmap_from_page(ctx, page, ctm, cs, alpha);
		else
			pixmap = fz_new_pixmap_from_page_contents(ctx, page, ctm, cs, alpha);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Pixmap_safe_own(ctx, env, pixmap);
}

JNIEXPORT jobject JNICALL
FUN(Page_getBounds)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_rect rect;

	if (!ctx || !page) return NULL;

	fz_try(ctx)
		rect = fz_bound_page(ctx, page);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Rect_safe(ctx, env, rect);
}

JNIEXPORT void JNICALL
FUN(Page_run)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie = from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !page) return;
	if (!dev) jni_throw_arg_void(env, "device must not be null");

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_run_page(ctx, page, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Page_runPageContents)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie = from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !page) return;
	if (!dev) jni_throw_arg_void(env, "device must not be null");

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_run_page_contents(ctx, page, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Page_runPageAnnots)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie = from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !page) return;
	if (!dev) jni_throw_arg_void(env, "device must not be null");

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_run_page_annots(ctx, page, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(Page_runPageWidgets)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie = from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !page) return;
	if (!dev) jni_throw_arg_void(env, "device must not be null");

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_run_page_widgets(ctx, page, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(Page_getLinks)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_link *link = NULL;
	fz_link *links = NULL;
	jobject jlinks = NULL;
	int link_count;
	int i;

	if (!ctx || !page) return NULL;

	fz_var(links);

	fz_try(ctx)
		links = fz_load_links(ctx, page);
	fz_catch(ctx)
	{
		fz_drop_link(ctx, links);
		jni_rethrow(env, ctx);
	}

	/* count the links */
	link = links;
	for (link_count = 0; link; link_count++)
		link = link->next;

	/* no links, return NULL instead of empty array */
	if (link_count == 0)
	{
		fz_drop_link(ctx, links);
		return NULL;
	}

	/* now run through actually creating the link objects */
	jlinks = (*env)->NewObjectArray(env, link_count, cls_Link, NULL);
	if (!jlinks || (*env)->ExceptionCheck(env))
	{
		fz_drop_link(ctx, links);
		return NULL;
	}

	link = links;
	for (i = 0; link && i < link_count; i++)
	{
		jobject jbounds = NULL;
		jobject jlink = NULL;
		jobject juri = NULL;

		jbounds = to_Rect_safe(ctx, env, link->rect);
		if (!jbounds || (*env)->ExceptionCheck(env))
		{
			fz_drop_link(ctx, links);
			return NULL;
		}

		juri = (*env)->NewStringUTF(env, link->uri);
		if (!juri || (*env)->ExceptionCheck(env))
		{
			fz_drop_link(ctx, links);
			return NULL;
		}

		jlink = (*env)->NewObject(env, cls_Link, mid_Link_init, jbounds, juri);
		if (!jlink || (*env)->ExceptionCheck(env))
		{
			fz_drop_link(ctx, links);
			return NULL;
		}
		(*env)->DeleteLocalRef(env, juri);
		(*env)->DeleteLocalRef(env, jbounds);

		(*env)->SetObjectArrayElement(env, jlinks, i, jlink);
		if ((*env)->ExceptionCheck(env))
		{
			fz_drop_link(ctx, links);
			return NULL;
		}

		(*env)->DeleteLocalRef(env, jlink);
		link = link->next;
	}

	fz_drop_link(ctx, links);

	return jlinks;
}

JNIEXPORT jobject JNICALL
FUN(Page_search)(JNIEnv *env, jobject self, jstring jneedle)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_quad hits[256];
	const char *needle = NULL;
	int n = 0;

	if (!ctx || !page) return NULL;
	if (!jneedle) jni_throw_arg(env, "needle must not be null");

	needle = (*env)->GetStringUTFChars(env, jneedle, NULL);
	if (!needle) return 0;

	fz_try(ctx)
		n = fz_search_page(ctx, page, needle, hits, nelem(hits));
	fz_always(ctx)
		(*env)->ReleaseStringUTFChars(env, jneedle, needle);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_QuadArray_safe(ctx, env, hits, n);
}

JNIEXPORT jobject JNICALL
FUN(Page_toDisplayList)(JNIEnv *env, jobject self, jboolean showExtra)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_display_list *list = NULL;

	if (!ctx || !page) return NULL;

	fz_try(ctx)
		if (showExtra)
			list = fz_new_display_list_from_page(ctx, page);
		else
			list = fz_new_display_list_from_page_contents(ctx, page);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DisplayList_safe_own(ctx, env, list);
}

JNIEXPORT jobject JNICALL
FUN(Page_toStructuredText)(JNIEnv *env, jobject self, jstring joptions)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_stext_page *text = NULL;
	const char *options= NULL;
	fz_stext_options opts;

	if (!ctx || !page) return NULL;

	if (joptions)
	{
		options = (*env)->GetStringUTFChars(env, joptions, NULL);
		if (!options) return NULL;
	}

	fz_try(ctx)
	{
		fz_parse_stext_options(ctx, &opts, options);
		text = fz_new_stext_page_from_page(ctx, page, &opts);
	}
	fz_always(ctx)
	{
		if (options)
			(*env)->ReleaseStringUTFChars(env, joptions, options);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_StructuredText_safe_own(ctx, env, text);
}

JNIEXPORT jbyteArray JNICALL
FUN(Page_textAsHtml)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_stext_page *text = NULL;
	fz_device *dev = NULL;
	fz_matrix ctm;
	jbyteArray arr = NULL;
	fz_buffer *buf = NULL;
	fz_output *out = NULL;
	unsigned char *data;
	size_t len;

	if (!ctx || !page) return NULL;

	fz_var(text);
	fz_var(dev);
	fz_var(buf);
	fz_var(out);

	fz_try(ctx)
	{
		ctm = fz_identity;
		text = fz_new_stext_page(ctx, fz_bound_page(ctx, page));
		dev = fz_new_stext_device(ctx, text, NULL);
		fz_run_page(ctx, page, dev, ctm, NULL);
		fz_close_device(ctx, dev);

		buf = fz_new_buffer(ctx, 256);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_print_stext_header_as_html(ctx, out);
		fz_print_stext_page_as_html(ctx, out, text, page->number);
		fz_print_stext_trailer_as_html(ctx, out);
		fz_close_output(ctx, out);

		len = fz_buffer_storage(ctx, buf, &data);
		arr = (*env)->NewByteArray(env, (jsize)len);
		if ((*env)->ExceptionCheck(env))
			fz_throw_java(ctx, env);
		if (!arr)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot create byte array");

		(*env)->SetByteArrayRegion(env, arr, 0, (jsize)len, (jbyte *)data);
		if ((*env)->ExceptionCheck(env))
			fz_throw_java(ctx, env);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
		fz_drop_buffer(ctx, buf);
		fz_drop_device(ctx, dev);
		fz_drop_stext_page(ctx, text);
	}
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return arr;
}

JNIEXPORT jobject JNICALL
FUN(Page_createLink)(JNIEnv *env, jobject self, jobject jrect, jstring juri)
{
	fz_context *ctx = get_context(env);
	fz_page *page = from_Page(env, self);
	fz_rect rect = from_Rect(env, jrect);
	fz_link *link = NULL;
	const char *uri = NULL;

	if (!ctx || !page)
		return NULL;

	fz_try(ctx)
	{
		if (juri != NULL)
		{
			uri = (*env)->GetStringUTFChars(env, juri, NULL);
			if (!uri)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot not get UTF string");
		}

		link = fz_create_link(ctx, page, rect, uri);
	}
	fz_always(ctx)
	{
		if (uri)
			(*env)->ReleaseStringUTFChars(env, juri, uri);
	}
	fz_catch(ctx)
	{
		jni_rethrow(env, ctx);
	}

	return to_Link_safe_own(ctx, env, link);
}

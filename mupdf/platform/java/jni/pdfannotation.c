/* Annotation interface */

JNIEXPORT void JNICALL
FUN(PDFAnnotation_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation_safe(env, self);
	if (!ctx || !annot) return;
	(*env)->SetLongField(env, self, fid_PDFAnnotation_pointer, 0);
	pdf_drop_annot(ctx, annot);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_run)(JNIEnv *env, jobject self, jobject jdev, jobject jctm, jobject jcookie)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_device *dev = from_Device(env, jdev);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_cookie *cookie= from_Cookie(env, jcookie);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !annot) return;
	if (!dev) jni_throw_arg_void(env, "device must not be null");

	info = lockNativeDevice(env, jdev, &err);
	if (err)
		return;
	fz_try(ctx)
		pdf_run_annot(ctx, annot, dev, ctm, cookie);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_toPixmap)(JNIEnv *env, jobject self, jobject jctm, jobject jcs, jboolean alpha)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_pixmap *pixmap = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pixmap = pdf_new_pixmap_from_annot(ctx, annot, ctm, cs, NULL, alpha);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Pixmap_safe_own(ctx, env, pixmap);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getBounds)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		rect = pdf_bound_annot(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Rect_safe(ctx, env, rect);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_toDisplayList)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_display_list *list = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		list = pdf_new_display_list_from_annot(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_DisplayList_safe_own(ctx, env, list);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getType)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jint subtype = 0;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		subtype = pdf_annot_type(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return subtype;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getFlags)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jint flags = 0;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		flags = pdf_annot_flags(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return flags;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setFlags)(JNIEnv *env, jobject self, jint flags)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_flags(ctx, annot, flags);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getContents)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *contents = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		contents = pdf_annot_contents(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, contents);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setContents)(JNIEnv *env, jobject self, jstring jcontents)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *contents = "";

	if (!ctx || !annot) return;
	if (jcontents)
	{
		contents = (*env)->GetStringUTFChars(env, jcontents, NULL);
		if (!contents) return;
	}

	fz_try(ctx)
		pdf_set_annot_contents(ctx, annot, contents);
	fz_always(ctx)
		if (contents)
			(*env)->ReleaseStringUTFChars(env, jcontents, contents);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getAuthor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *author = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		author = pdf_annot_author(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, author);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setAuthor)(JNIEnv *env, jobject self, jstring jauthor)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *author = "";

	if (!ctx || !annot) return;
	if (jauthor)
	{
		author = (*env)->GetStringUTFChars(env, jauthor, NULL);
		if (!author) return;
	}

	fz_try(ctx)
		pdf_set_annot_author(ctx, annot, author);
	fz_always(ctx)
		if (author)
			(*env)->ReleaseStringUTFChars(env, jauthor, author);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jlong JNICALL
FUN(PDFAnnotation_getModificationDateNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jlong t;

	if (!ctx || !annot) return -1;

	fz_try(ctx)
		t = pdf_annot_modification_date(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return t * 1000;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setModificationDate)(JNIEnv *env, jobject self, jlong time)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_set_annot_modification_date(ctx, annot, time / 1000);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jlong JNICALL
FUN(PDFAnnotation_getCreationDateNative)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jlong t;

	if (!ctx || !annot) return -1;

	fz_try(ctx)
		t = pdf_annot_creation_date(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return t * 1000;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setCreationDate)(JNIEnv *env, jobject self, jlong time)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_set_annot_creation_date(ctx, annot, time / 1000);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getRect)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		rect = pdf_annot_rect(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Rect_safe(ctx, env, rect);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setRect)(JNIEnv *env, jobject self, jobject jrect)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_rect rect = from_Rect(env, jrect);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_rect(ctx, annot, rect);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jfloat JNICALL
FUN(PDFAnnotation_getBorder)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jfloat border;

	if (!ctx || !annot) return 0;

	fz_try(ctx)
		border = pdf_annot_border(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return border;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setBorder)(JNIEnv *env, jobject self, jfloat border)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_border(ctx, annot, border);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getColor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n;
	float color[4];
	jfloatArray arr;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_color(ctx, annot, &n, color);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	arr = (*env)->NewFloatArray(env, n);
	if (!arr || (*env)->ExceptionCheck(env)) return NULL;

	(*env)->SetFloatArrayRegion(env, arr, 0, n, &color[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setColor)(JNIEnv *env, jobject self, jobject jcolor)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	float color[4];
	int n = 0;

	if (!ctx || !annot) return;
	if (!from_jfloatArray(env, color, nelem(color), jcolor)) return;
	if (jcolor)
		n = (*env)->GetArrayLength(env, jcolor);

	fz_try(ctx)
		pdf_set_annot_color(ctx, annot, n, color);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getInteriorColor)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n;
	float color[4];
	jfloatArray arr;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_interior_color(ctx, annot, &n, color);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	arr = (*env)->NewFloatArray(env, n);
	if (!arr || (*env)->ExceptionCheck(env)) return NULL;

	(*env)->SetFloatArrayRegion(env, arr, 0, n, &color[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return arr;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setInteriorColor)(JNIEnv *env, jobject self, jobject jcolor)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	float color[4];
	int n = 0;

	if (!ctx || !annot) return;
	if (!from_jfloatArray(env, color, nelem(color), jcolor)) return;
	if (jcolor)
		n = (*env)->GetArrayLength(env, jcolor);

	fz_try(ctx)
		pdf_set_annot_interior_color(ctx, annot, n, color);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getQuadPointCount)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_quad_point_count(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return n;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getQuadPoint)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_quad q;

	fz_try(ctx)
		q = pdf_annot_quad_point(ctx, annot, i);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Quad_safe(ctx, env, q);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_clearQuadPoints)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_clear_annot_quad_points(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addQuadPoint)(JNIEnv *env, jobject self, jobject qobj)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_quad q = from_Quad(env, qobj);

	fz_try(ctx)
		pdf_add_annot_quad_point(ctx, annot, q);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getVertexCount)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_vertex_count(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return n;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getVertex)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point v;

	fz_try(ctx)
		v = pdf_annot_vertex(ctx, annot, i);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Point_safe(ctx, env, v);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_clearVertices)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_clear_annot_vertices(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addVertex)(JNIEnv *env, jobject self, float x, float y)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_add_annot_vertex(ctx, annot, fz_make_point(x, y));
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getInkListCount)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_ink_list_count(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return n;
}

JNIEXPORT jint JNICALL
FUN(PDFAnnotation_getInkListStrokeCount)(JNIEnv *env, jobject self, jint i)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	int n = 0;

	fz_try(ctx)
		n = pdf_annot_ink_list_stroke_count(ctx, annot, i);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return n;
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getInkListStrokeVertex)(JNIEnv *env, jobject self, jint i, jint k)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	fz_point v;

	fz_try(ctx)
		v = pdf_annot_ink_list_stroke_vertex(ctx, annot, i, k);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return to_Point_safe(ctx, env, v);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_clearInkList)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_clear_annot_ink_list(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addInkListStroke)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_add_annot_ink_list_stroke(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_addInkListStrokeVertex)(JNIEnv *env, jobject self, float x, float y)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	fz_try(ctx)
		pdf_add_annot_ink_list_stroke_vertex(ctx, annot, fz_make_point(x, y));
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventEnter)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		pdf_annot_event_enter(ctx, annot);
		annot->is_hot = 1;
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventExit)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		pdf_annot_event_exit(ctx, annot);
		annot->is_hot = 0;
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventDown)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		pdf_annot_event_down(ctx, annot);
		pdf_annot_set_active(ctx, annot, 1);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventUp)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
	{
		if (pdf_annot_hot(ctx, annot) && pdf_annot_active(ctx, annot))
			pdf_annot_event_up(ctx, annot);
		pdf_annot_set_active(ctx, annot, 0);
	}
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventFocus)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_annot_event_focus(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_eventBlur)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_annot_event_blur(ctx, annot);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_update)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean changed = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		changed = pdf_update_annot(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return changed;
}

JNIEXPORT jboolean JNICALL
FUN(PDFAnnotation_isOpen)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	jboolean open = JNI_FALSE;

	if (!ctx || !annot) return JNI_FALSE;

	fz_try(ctx)
		open = pdf_annot_is_open(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return open;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setIsOpen)(JNIEnv *env, jobject self, jboolean open)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_is_open(ctx, annot, open);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jstring JNICALL
FUN(PDFAnnotation_getIcon)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *name = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		name = pdf_annot_icon_name(ctx, annot);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	return (*env)->NewStringUTF(env, name);
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setIcon)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	const char *name = NULL;

	if (!ctx || !annot) return;
	if (!jname) jni_throw_arg_void(env, "icon name must not be null");

	name = (*env)->GetStringUTFChars(env, jname, NULL);
	if (!name) return;

	fz_try(ctx)
		pdf_set_annot_icon_name(ctx, annot, name);
	fz_always(ctx)
		if (name)
			(*env)->ReleaseStringUTFChars(env, jname, name);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jintArray JNICALL
FUN(PDFAnnotation_getLineEndingStyles)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);
	enum pdf_line_ending s = 0, e = 0;
	int line_endings[2];
	jintArray jline_endings = NULL;

	if (!ctx || !annot) return NULL;

	fz_try(ctx)
		pdf_annot_line_ending_styles(ctx, annot, &s, &e);
	fz_catch(ctx)
		jni_rethrow(env, ctx);

	line_endings[0] = s;
	line_endings[1] = e;

	jline_endings = (*env)->NewIntArray(env, 2);
	if (!jline_endings || (*env)->ExceptionCheck(env)) return NULL;

	(*env)->SetIntArrayRegion(env, jline_endings, 0, 2, &line_endings[0]);
	if ((*env)->ExceptionCheck(env)) return NULL;

	return jline_endings;
}

JNIEXPORT void JNICALL
FUN(PDFAnnotation_setLineEndingStyles)(JNIEnv *env, jobject self, jint start_style, jint end_style)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return;

	fz_try(ctx)
		pdf_set_annot_line_ending_styles(ctx, annot, start_style, end_style);
	fz_catch(ctx)
		jni_rethrow_void(env, ctx);
}

JNIEXPORT jobject JNICALL
FUN(PDFAnnotation_getObject)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	pdf_annot *annot = from_PDFAnnotation(env, self);

	if (!ctx || !annot) return NULL;

	return to_PDFObject_safe(ctx, env, annot->obj);
}

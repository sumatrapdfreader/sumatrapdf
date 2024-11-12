// Copyright (C) 2024 Artifex Software, Inc.
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

typedef struct {
	JNIEnv *env;
	jobject hits;
	int error;
} search_state;

static int hit_callback(fz_context *ctx, void *opaque, int quads, fz_quad *quad)
{
	search_state *state = (search_state *) opaque;
	JNIEnv *env = state->env;
	jobjectArray arr;
	int i;

	arr = (*env)->NewObjectArray(env, quads, cls_Quad, NULL);
	if (!arr || (*env)->ExceptionCheck(env))
	{
		state->error = 1;
		return 1;
	}

	(*env)->CallVoidMethod(env, state->hits, mid_ArrayList_add, arr);
	if ((*env)->ExceptionCheck(env))
	{
		state->error = 1;
		return 1;
	}

	for (i = 0; i < quads; i++)
	{
		jobject jquad = to_Quad_safe(ctx, env, quad[i]);
		if (!jquad || (*env)->ExceptionCheck(env))
		{
			state->error = 1;
			return 1;
		}
		(*env)->SetObjectArrayElement(env, arr, i, jquad);
		if ((*env)->ExceptionCheck(env))
		{
			state->error = 1;
			return 1;
		}
		(*env)->DeleteLocalRef(env, jquad);
	}

	(*env)->DeleteLocalRef(env, arr);

	return 0;
}

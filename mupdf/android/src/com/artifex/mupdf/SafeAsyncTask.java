package com.artifex.mupdf;

import android.os.AsyncTask;
import java.util.concurrent.RejectedExecutionException;


public abstract class SafeAsyncTask<Params, Progress, Result> extends AsyncTask<Params, Progress, Result> {
	public void safeExecute(Params... params) {
		try {
			execute(params);
		} catch(RejectedExecutionException e) {
			// Failed to start in the background, so do it in the foreground
			onPreExecute();
			if (isCancelled()) {
				onCancelled();
			} else {
				onPostExecute(doInBackground(params));
			}
		}
	}
}

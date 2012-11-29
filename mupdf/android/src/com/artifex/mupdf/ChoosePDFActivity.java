package com.artifex.mupdf;

import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.List;

import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.FileObserver;
import android.os.Handler;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;

public class ChoosePDFActivity extends ListActivity {
	private File         mDirectory;
	private File []      mFiles;
	private Handler	     mHandler;
	private Runnable     mUpdateFiles;
	private ArrayAdapter<String> adapter;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		Resources res = getResources();
		String appName = res.getString(R.string.app_name);
		String version = res.getString(R.string.version);
		String title = res.getString(R.string.picker_title);
		setTitle(String.format(title, appName, version));

		String storageState = Environment.getExternalStorageState();

		if (!Environment.MEDIA_MOUNTED.equals(storageState)
				&& !Environment.MEDIA_MOUNTED_READ_ONLY.equals(storageState))
		{
			AlertDialog.Builder builder = new AlertDialog.Builder(this);
			builder.setTitle(R.string.no_media_warning);
			builder.setMessage(R.string.no_media_hint);
			AlertDialog alert = builder.create();
			alert.setButton(AlertDialog.BUTTON_POSITIVE,"Dismiss",
					new OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							finish();
						}
					});
			alert.show();
			return;
		}

	}

	@Override
	protected void onResume() {
		super.onResume();

		mDirectory = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);

		// Create a list adapter...
		adapter = new ArrayAdapter<String>(this, R.layout.picker_entry);
		setListAdapter(adapter);

		// ...that is updated dynamically when files are scanned
		mHandler = new Handler();
		mUpdateFiles = new Runnable() {
			public void run() {
				mFiles = mDirectory.listFiles(new FilenameFilter() {
					public boolean accept(File file, String name) {
						if (name.toLowerCase().endsWith(".pdf"))
							return true;
						if (name.toLowerCase().endsWith(".xps"))
							return true;
						if (name.toLowerCase().endsWith(".cbz"))
							return true;
						return false;
					}
				});
				adapter.clear();
				if (mFiles != null)
					for (File f : mFiles)
						adapter.add(f.getName());
				adapter.sort(String.CASE_INSENSITIVE_ORDER);
			}
		};

		// Start initial file scan...
		mHandler.post(mUpdateFiles);

		// ...and observe the directory and scan files upon changes.
		FileObserver observer = new FileObserver(mDirectory.getPath(), FileObserver.CREATE | FileObserver.DELETE) {
			public void onEvent(int event, String path) {
				mHandler.post(mUpdateFiles);
			}
		};
		observer.startWatching();
	}

	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		super.onListItemClick(l, v, position, id);
		Uri uri = Uri.parse(mFiles[position].getAbsolutePath());
		Intent intent = new Intent(this,MuPDFActivity.class);
		intent.setAction(Intent.ACTION_VIEW);
		intent.setData(uri);
		startActivity(intent);
	}
}

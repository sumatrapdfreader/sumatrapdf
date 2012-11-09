package com.artifex.mupdf;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.RectF;
import android.view.LayoutInflater;
import android.view.WindowManager;
import android.widget.EditText;

class PassClickResult {
	public final boolean changed;
	public final WidgetType type;
	public final String text;

	public PassClickResult(boolean _changed, WidgetType _type, String _text) {
		changed = _changed;
		type = _type;
		text = _text;
	}
}

public class MuPDFPageView extends PageView {
	private final MuPDFCore mCore;
	private SafeAsyncTask<Void,Void,PassClickResult> mPassClick;
	private RectF mWidgetAreas[];
	private SafeAsyncTask<Void,Void,RectF[]> mLoadWidgetAreas;
	private AlertDialog.Builder mTextEntryBuilder;
	private AlertDialog mTextEntry;
	private EditText mEditText;
	private SafeAsyncTask<String,Void,Boolean> mSetWidgetText;
	private Runnable changeReporter;

	public MuPDFPageView(Context c, MuPDFCore core, Point parentSize) {
		super(c, parentSize);
		mCore = core;
		mTextEntryBuilder = new AlertDialog.Builder(c);
		mTextEntryBuilder.setTitle("MuPDF: fill out text field");
		LayoutInflater inflater = (LayoutInflater)c.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
		mEditText = (EditText)inflater.inflate(R.layout.textentry, null);
		mTextEntryBuilder.setView(mEditText);
		mTextEntryBuilder.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which) {
				dialog.dismiss();
			}
		});
		mTextEntryBuilder.setPositiveButton("Okay", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which) {
				mSetWidgetText = new SafeAsyncTask<String,Void,Boolean> () {
					@Override
					protected Boolean doInBackground(String... arg0) {
						return mCore.setFocusedWidgetText(mPageNumber, arg0[0]);
					}
					@Override
					protected void onPostExecute(Boolean result) {
						changeReporter.run();
						if (!result)
							invokeTextDialog(mEditText.getText().toString());
					}
				};

				mSetWidgetText.safeExecute(mEditText.getText().toString());
			}
		});
		mTextEntry = mTextEntryBuilder.create();
	}

	public int hitLinkPage(float x, float y) {
		// Since link highlighting was implemented, the super class
		// PageView has had sufficient information to be able to
		// perform this method directly. Making that change would
		// make MuPDFCore.hitLinkPage superfluous.
		float scale = mSourceScale*(float)getWidth()/(float)mSize.x;
		float docRelX = (x - getLeft())/scale;
		float docRelY = (y - getTop())/scale;

		return mCore.hitLinkPage(mPageNumber, docRelX, docRelY);
	}

	private void invokeTextDialog(String text) {
		mEditText.setText(text);
		mTextEntry.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
		mTextEntry.show();
	}

	public void setChangeReporter(Runnable reporter) {
		changeReporter = reporter;
	}

	public boolean passClickEvent(float x, float y) {
		float scale = mSourceScale*(float)getWidth()/(float)mSize.x;
		final float docRelX = (x - getLeft())/scale;
		final float docRelY = (y - getTop())/scale;
		boolean hitWidget = false;

		if (mWidgetAreas != null) {
			for (int i = 0; i < mWidgetAreas.length && !hitWidget; i++)
				if (mWidgetAreas[i].contains(docRelX, docRelY))
					hitWidget = true;
		}

		if (hitWidget) {
			mPassClick = new SafeAsyncTask<Void,Void,PassClickResult>() {
				@Override
				protected PassClickResult doInBackground(Void... arg0) {
					return mCore.passClickEvent(mPageNumber, docRelX, docRelY);
				}

				@Override
				protected void onPostExecute(PassClickResult result) {
					if (result.changed) {
						changeReporter.run();
					}

					switch(result.type) {
					case TEXT:
						invokeTextDialog(result.text);
						break;
					}
				}
			};

			mPassClick.safeExecute();
		}

		return hitWidget;
	}

	@Override
	protected void drawPage(BitmapHolder h, int sizeX, int sizeY,
			int patchX, int patchY, int patchWidth, int patchHeight) {
		mCore.drawPage(h, mPageNumber, sizeX, sizeY, patchX, patchY, patchWidth, patchHeight);
	}

	@Override
	protected void updatePage(BitmapHolder h, int sizeX, int sizeY,
			int patchX, int patchY, int patchWidth, int patchHeight) {
		mCore.updatePage(h, mPageNumber, sizeX, sizeY, patchX, patchY, patchWidth, patchHeight);
	}

	@Override
	protected LinkInfo[] getLinkInfo() {
		return mCore.getPageLinks(mPageNumber);
	}

	@Override
	public void setPage(final int page, PointF size) {
		mLoadWidgetAreas = new SafeAsyncTask<Void,Void,RectF[]> () {
			@Override
			protected RectF[] doInBackground(Void... arg0) {
				return mCore.getWidgetAreas(page);
			}

			@Override
			protected void onPostExecute(RectF[] result) {
				mWidgetAreas = result;
			}
		};

		mLoadWidgetAreas.safeExecute();

		super.setPage(page, size);
	}
}

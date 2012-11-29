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

abstract class PassClickResultVisitor {
	public abstract void visitText(PassClickResultText result);
	public abstract void visitChoice(PassClickResultChoice result);
}

class PassClickResult {
	public final boolean changed;

	public PassClickResult(boolean _changed) {
		changed = _changed;
	}

	public void acceptVisitor(PassClickResultVisitor visitor) {
	}
}

class PassClickResultText extends PassClickResult {
	public final String text;

	public PassClickResultText(boolean _changed, String _text) {
		super(_changed);
		text = _text;
	}

	public void acceptVisitor(PassClickResultVisitor visitor) {
		visitor.visitText(this);
	}
}

class PassClickResultChoice extends PassClickResult {
	public final String [] options;
	public final String [] selected;

	public PassClickResultChoice(boolean _changed, String [] _options, String [] _selected) {
		super(_changed);
		options = _options;
		selected = _selected;
	}

	public void acceptVisitor(PassClickResultVisitor visitor) {
		visitor.visitChoice(this);
	}
}

public class MuPDFPageView extends PageView {
	private final MuPDFCore mCore;
	private AsyncTask<Void,Void,PassClickResult> mPassClick;
	private RectF mWidgetAreas[];
	private AsyncTask<Void,Void,RectF[]> mLoadWidgetAreas;
	private AlertDialog.Builder mTextEntryBuilder;
	private AlertDialog.Builder mChoiceEntryBuilder;
	private AlertDialog mTextEntry;
	private EditText mEditText;
	private AsyncTask<String,Void,Boolean> mSetWidgetText;
	private AsyncTask<String,Void,Void> mSetWidgetChoice;
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
				mSetWidgetText = new AsyncTask<String,Void,Boolean> () {
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

				mSetWidgetText.execute(mEditText.getText().toString());
			}
		});
		mTextEntry = mTextEntryBuilder.create();

		mChoiceEntryBuilder = new AlertDialog.Builder(c);
		mChoiceEntryBuilder.setTitle("MuPDF: choose value");
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

	private void invokeChoiceDialog(final String [] options) {
		mChoiceEntryBuilder.setItems(options, new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which) {
				mSetWidgetChoice = new AsyncTask<String,Void,Void>() {
					@Override
					protected Void doInBackground(String... params) {
						String [] sel = {params[0]};
						mCore.setFocusedWidgetChoiceSelected(sel);
						return null;
					}

					@Override
					protected void onPostExecute(Void result) {
						changeReporter.run();
					}
				};

				mSetWidgetChoice.execute(options[which]);
			}
		});
		AlertDialog dialog = mChoiceEntryBuilder.create();
		dialog.show();
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
			mPassClick = new AsyncTask<Void,Void,PassClickResult>() {
				@Override
				protected PassClickResult doInBackground(Void... arg0) {
					return mCore.passClickEvent(mPageNumber, docRelX, docRelY);
				}

				@Override
				protected void onPostExecute(PassClickResult result) {
					if (result.changed) {
						changeReporter.run();
					}

					result.acceptVisitor(new PassClickResultVisitor() {
						@Override
						public void visitText(PassClickResultText result) {
							invokeTextDialog(result.text);
						}

						@Override
						public void visitChoice(PassClickResultChoice result) {
							invokeChoiceDialog(result.options);
						}
					});
				}
			};

			mPassClick.execute();
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
		mLoadWidgetAreas = new AsyncTask<Void,Void,RectF[]> () {
			@Override
			protected RectF[] doInBackground(Void... arg0) {
				return mCore.getWidgetAreas(page);
			}

			@Override
			protected void onPostExecute(RectF[] result) {
				mWidgetAreas = result;
			}
		};

		mLoadWidgetAreas.execute();

		super.setPage(page, size);
	}
}

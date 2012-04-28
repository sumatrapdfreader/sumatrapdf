package com.artifex.mupdf;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.graphics.RectF;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.text.Editable;
import android.text.TextWatcher;
import android.text.method.PasswordTransformationMethod;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.TranslateAnimation;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.RelativeLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.ViewSwitcher;

class SearchTaskResult {
	public final String txt;
	public final int   pageNumber;
	public final RectF searchBoxes[];
	static private SearchTaskResult singleton;

	SearchTaskResult(String _txt, int _pageNumber, RectF _searchBoxes[]) {
		txt = _txt;
		pageNumber = _pageNumber;
		searchBoxes = _searchBoxes;
	}

	static public SearchTaskResult get() {
		return singleton;
	}

	static public void set(SearchTaskResult r) {
		singleton = r;
	}
}

class ProgressDialogX extends ProgressDialog {
	public ProgressDialogX(Context context) {
		super(context);
	}

	private boolean mCancelled = false;

	public boolean isCancelled() {
		return mCancelled;
	}

	@Override
	public void cancel() {
		mCancelled = true;
		super.cancel();
	}
}
public class MuPDFActivity extends Activity
{
	/* The core rendering instance */
	private enum LinkState {DEFAULT, HIGHLIGHT, INHIBIT};
	private final int    TAP_PAGE_MARGIN = 5;
	private static final int    SEARCH_PROGRESS_DELAY = 200;
	private MuPDFCore    core;
	private String       mFileName;
	private ReaderView   mDocView;
	private View         mButtonsView;
	private boolean      mButtonsVisible;
	private EditText     mPasswordView;
	private TextView     mFilenameView;
	private SeekBar      mPageSlider;
	private TextView     mPageNumberView;
	private ImageButton  mSearchButton;
	private ImageButton  mCancelButton;
	private ImageButton  mOutlineButton;
	private ViewSwitcher mTopBarSwitcher;
// XXX	private ImageButton  mLinkButton;
	private boolean      mTopBarIsSearch;
	private ImageButton  mSearchBack;
	private ImageButton  mSearchFwd;
	private EditText     mSearchText;
	private AsyncTask<Integer,Integer,SearchTaskResult> mSearchTask;
	//private SearchTaskResult mSearchTaskResult;
	private AlertDialog.Builder mAlertBuilder;
	private LinkState    mLinkState = LinkState.DEFAULT;
	private final Handler mHandler = new Handler();

	private MuPDFCore openFile(String path)
	{
		int lastSlashPos = path.lastIndexOf('/');
		mFileName = new String(lastSlashPos == -1
					? path
					: path.substring(lastSlashPos+1));
		System.out.println("Trying to open "+path);
		try
		{
			core = new MuPDFCore(path);
			// New file: drop the old outline data
			OutlineActivityData.set(null);
		}
		catch (Exception e)
		{
			System.out.println(e);
			return null;
		}
		return core;
	}

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		mAlertBuilder = new AlertDialog.Builder(this);

		if (core == null) {
			core = (MuPDFCore)getLastNonConfigurationInstance();

			if (savedInstanceState != null && savedInstanceState.containsKey("FileName")) {
				mFileName = savedInstanceState.getString("FileName");
			}
		}
		if (core == null) {
			Intent intent = getIntent();
			if (Intent.ACTION_VIEW.equals(intent.getAction())) {
				Uri uri = intent.getData();
				if (uri.toString().startsWith("content://media/external/file")) {
					// Handle view requests from the Transformer Prime's file manager
					// Hopefully other file managers will use this same scheme, if not
					// using explicit paths.
					Cursor cursor = getContentResolver().query(uri, new String[]{"_data"}, null, null, null);
					if (cursor.moveToFirst()) {
						uri = Uri.parse(cursor.getString(0));
					}
				}
				core = openFile(Uri.decode(uri.getEncodedPath()));
			}
			if (core != null && core.needsPassword()) {
				requestPassword(savedInstanceState);
				return;
			}
		}
		if (core == null)
		{
			AlertDialog alert = mAlertBuilder.create();
			alert.setTitle(R.string.open_failed);
			alert.setButton(AlertDialog.BUTTON_POSITIVE, "Dismiss",
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							finish();
						}
					});
			alert.show();
			return;
		}

		createUI(savedInstanceState);
	}

	public void requestPassword(final Bundle savedInstanceState) {
		mPasswordView = new EditText(this);
		mPasswordView.setInputType(EditorInfo.TYPE_TEXT_VARIATION_PASSWORD);
		mPasswordView.setTransformationMethod(new PasswordTransformationMethod());

		AlertDialog alert = mAlertBuilder.create();
		alert.setTitle(R.string.enter_password);
		alert.setView(mPasswordView);
		alert.setButton(AlertDialog.BUTTON_POSITIVE, "Ok",
				new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which) {
				if (core.authenticatePassword(mPasswordView.getText().toString())) {
					createUI(savedInstanceState);
				} else {
					requestPassword(savedInstanceState);
				}
			}
		});
		alert.setButton(AlertDialog.BUTTON_NEGATIVE, "Cancel",
				new DialogInterface.OnClickListener() {

			public void onClick(DialogInterface dialog, int which) {
				finish();
			}
		});
		alert.show();
	}

	public void createUI(Bundle savedInstanceState) {
		if (core == null)
			return;
		// Now create the UI.
		// First create the document view making use of the ReaderView's internal
		// gesture recognition
		mDocView = new ReaderView(this) {
			private boolean showButtonsDisabled;

			public boolean onSingleTapUp(MotionEvent e) {
				if (e.getX() < super.getWidth()/TAP_PAGE_MARGIN) {
					super.moveToPrevious();
				} else if (e.getX() > super.getWidth()*(TAP_PAGE_MARGIN-1)/TAP_PAGE_MARGIN) {
					super.moveToNext();
				} else if (!showButtonsDisabled) {
					int linkPage = -1;
					if (mLinkState != LinkState.INHIBIT) {
						MuPDFPageView pageView = (MuPDFPageView) mDocView.getDisplayedView();
						if (pageView != null) {
// XXX							linkPage = pageView.hitLinkPage(e.getX(), e.getY());
						}
					}

					if (linkPage != -1) {
						mDocView.setDisplayedViewIndex(linkPage);
					} else {
						if (!mButtonsVisible) {
							showButtons();
						} else {
							hideButtons();
						}
					}
				}
				return super.onSingleTapUp(e);
			}

			public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
				if (!showButtonsDisabled)
					hideButtons();

				return super.onScroll(e1, e2, distanceX, distanceY);
			}

			public boolean onScaleBegin(ScaleGestureDetector d) {
				// Disabled showing the buttons until next touch.
				// Not sure why this is needed, but without it
				// pinch zoom can make the buttons appear
				showButtonsDisabled = true;
				return super.onScaleBegin(d);
			}

			public boolean onTouchEvent(MotionEvent event) {
				if (event.getActionMasked() == MotionEvent.ACTION_DOWN)
					showButtonsDisabled = false;

				return super.onTouchEvent(event);
			}

			protected void onChildSetup(int i, View v) {
				if (SearchTaskResult.get() != null && SearchTaskResult.get().pageNumber == i)
					((PageView)v).setSearchBoxes(SearchTaskResult.get().searchBoxes);
				else
					((PageView)v).setSearchBoxes(null);

				((PageView)v).setLinkHighlighting(mLinkState == LinkState.HIGHLIGHT);
			}

			protected void onMoveToChild(int i) {
				if (core == null)
					return;
				mPageNumberView.setText(String.format("%d/%d", i+1, core.countPages()));
				mPageSlider.setMax(core.countPages()-1);
				mPageSlider.setProgress(i);
				if (SearchTaskResult.get() != null && SearchTaskResult.get().pageNumber != i) {
					SearchTaskResult.set(null);
					mDocView.resetupChildren();
				}
			}

			protected void onSettle(View v) {
				// When the layout has settled ask the page to render
				// in HQ
				((PageView)v).addHq();
			}

			protected void onUnsettle(View v) {
				// When something changes making the previous settled view
				// no longer appropriate, tell the page to remove HQ
				((PageView)v).removeHq();
			}
		};
		mDocView.setAdapter(new MuPDFPageAdapter(this, core));

		// Make the buttons overlay, and store all its
		// controls in variables
		makeButtonsView();

		// Set the file-name text
		mFilenameView.setText(mFileName);

		// Activate the seekbar
		mPageSlider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
			public void onStopTrackingTouch(SeekBar seekBar) {
				mDocView.setDisplayedViewIndex(seekBar.getProgress());
			}

			public void onStartTrackingTouch(SeekBar seekBar) {}

			public void onProgressChanged(SeekBar seekBar, int progress,
					boolean fromUser) {
				updatePageNumView(progress);
			}
		});

		// Activate the search-preparing button
		mSearchButton.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				searchModeOn();
			}
		});

		mCancelButton.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				searchModeOff();
			}
		});

		// Search invoking buttons are disabled while there is no text specified
		mSearchBack.setEnabled(false);
		mSearchFwd.setEnabled(false);

		// React to interaction with the text widget
		mSearchText.addTextChangedListener(new TextWatcher() {

			public void afterTextChanged(Editable s) {
				boolean haveText = s.toString().length() > 0;
				mSearchBack.setEnabled(haveText);
				mSearchFwd.setEnabled(haveText);

				// Remove any previous search results
				if (SearchTaskResult.get() != null && !mSearchText.getText().toString().equals(SearchTaskResult.get().txt)) {
					SearchTaskResult.set(null);
					mDocView.resetupChildren();
				}
			}
			public void beforeTextChanged(CharSequence s, int start, int count,
					int after) {}
			public void onTextChanged(CharSequence s, int start, int before,
					int count) {}
		});

		//React to Done button on keyboard
		mSearchText.setOnEditorActionListener(new TextView.OnEditorActionListener() {
			public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
				if (actionId == EditorInfo.IME_ACTION_DONE)
					search(1);
				return false;
			}
		});

		mSearchText.setOnKeyListener(new View.OnKeyListener() {
			public boolean onKey(View v, int keyCode, KeyEvent event) {
				if (event.getAction() == KeyEvent.ACTION_DOWN && keyCode == KeyEvent.KEYCODE_ENTER)
					search(1);
				return false;
			}
		});

		// Activate search invoking buttons
		mSearchBack.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				search(-1);
			}
		});
		mSearchFwd.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				search(1);
			}
		});

/* XXX
		mLinkButton.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				switch(mLinkState) {
				case DEFAULT:
					mLinkState = LinkState.HIGHLIGHT;
					mLinkButton.setImageResource(R.drawable.ic_hl_link);
					//Inform pages of the change.
					mDocView.resetupChildren();
					break;
				case HIGHLIGHT:
					mLinkState = LinkState.INHIBIT;
					mLinkButton.setImageResource(R.drawable.ic_nolink);
					//Inform pages of the change.
					mDocView.resetupChildren();
					break;
				case INHIBIT:
					mLinkState = LinkState.DEFAULT;
					mLinkButton.setImageResource(R.drawable.ic_link);
					break;
				}
			}
		});
*/

		if (core.hasOutline()) {
			mOutlineButton.setOnClickListener(new View.OnClickListener() {
				public void onClick(View v) {
					OutlineItem outline[] = core.getOutline();
					if (outline != null) {
						OutlineActivityData.get().items = outline;
						Intent intent = new Intent(MuPDFActivity.this, OutlineActivity.class);
						startActivityForResult(intent, 0);
					}
				}
			});
		} else {
			mOutlineButton.setVisibility(View.GONE);
		}

		// Reenstate last state if it was recorded
		SharedPreferences prefs = getPreferences(Context.MODE_PRIVATE);
		mDocView.setDisplayedViewIndex(prefs.getInt("page"+mFileName, 0));

		if (savedInstanceState == null || !savedInstanceState.getBoolean("ButtonsHidden", false))
			showButtons();

		if(savedInstanceState != null && savedInstanceState.getBoolean("SearchMode", false))
			searchModeOn();

		// Stick the document view and the buttons overlay into a parent view
		RelativeLayout layout = new RelativeLayout(this);
		layout.addView(mDocView);
		layout.addView(mButtonsView);
		layout.setBackgroundResource(R.drawable.tiled_background);
		//layout.setBackgroundResource(R.color.canvas);
		setContentView(layout);
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		if (resultCode >= 0)
			mDocView.setDisplayedViewIndex(resultCode);
		super.onActivityResult(requestCode, resultCode, data);
	}

	public Object onRetainNonConfigurationInstance()
	{
		MuPDFCore mycore = core;
		core = null;
		return mycore;
	}

	@Override
	protected void onSaveInstanceState(Bundle outState) {
		super.onSaveInstanceState(outState);

		if (mFileName != null && mDocView != null) {
			outState.putString("FileName", mFileName);

			// Store current page in the prefs against the file name,
			// so that we can pick it up each time the file is loaded
			// Other info is needed only for screen-orientation change,
			// so it can go in the bundle
			SharedPreferences prefs = getPreferences(Context.MODE_PRIVATE);
			SharedPreferences.Editor edit = prefs.edit();
			edit.putInt("page"+mFileName, mDocView.getDisplayedViewIndex());
			edit.commit();
		}

		if (!mButtonsVisible)
			outState.putBoolean("ButtonsHidden", true);

		if (mTopBarIsSearch)
			outState.putBoolean("SearchMode", true);
	}

	@Override
	protected void onPause() {
		super.onPause();

		killSearch();

		if (mFileName != null && mDocView != null) {
			SharedPreferences prefs = getPreferences(Context.MODE_PRIVATE);
			SharedPreferences.Editor edit = prefs.edit();
			edit.putInt("page"+mFileName, mDocView.getDisplayedViewIndex());
			edit.commit();
		}
	}

	public void onDestroy()
	{
		if (core != null)
			core.onDestroy();
		core = null;
		super.onDestroy();
	}

	void showButtons() {
		if (core == null)
			return;
		if (!mButtonsVisible) {
			mButtonsVisible = true;
			// Update page number text and slider
			int index = mDocView.getDisplayedViewIndex();
			updatePageNumView(index);
			mPageSlider.setMax(core.countPages()-1);
			mPageSlider.setProgress(index);
			if (mTopBarIsSearch) {
				mSearchText.requestFocus();
				showKeyboard();
			}

			Animation anim = new TranslateAnimation(0, 0, -mTopBarSwitcher.getHeight(), 0);
			anim.setDuration(200);
			anim.setAnimationListener(new Animation.AnimationListener() {
				public void onAnimationStart(Animation animation) {
					mTopBarSwitcher.setVisibility(View.VISIBLE);
				}
				public void onAnimationRepeat(Animation animation) {}
				public void onAnimationEnd(Animation animation) {}
			});
			mTopBarSwitcher.startAnimation(anim);

			anim = new TranslateAnimation(0, 0, mPageSlider.getHeight(), 0);
			anim.setDuration(200);
			anim.setAnimationListener(new Animation.AnimationListener() {
				public void onAnimationStart(Animation animation) {
					mPageSlider.setVisibility(View.VISIBLE);
				}
				public void onAnimationRepeat(Animation animation) {}
				public void onAnimationEnd(Animation animation) {
					mPageNumberView.setVisibility(View.VISIBLE);
				}
			});
			mPageSlider.startAnimation(anim);
		}
	}

	void hideButtons() {
		if (mButtonsVisible) {
			mButtonsVisible = false;
			hideKeyboard();

			Animation anim = new TranslateAnimation(0, 0, 0, -mTopBarSwitcher.getHeight());
			anim.setDuration(200);
			anim.setAnimationListener(new Animation.AnimationListener() {
				public void onAnimationStart(Animation animation) {}
				public void onAnimationRepeat(Animation animation) {}
				public void onAnimationEnd(Animation animation) {
					mTopBarSwitcher.setVisibility(View.INVISIBLE);
				}
			});
			mTopBarSwitcher.startAnimation(anim);

			anim = new TranslateAnimation(0, 0, 0, mPageSlider.getHeight());
			anim.setDuration(200);
			anim.setAnimationListener(new Animation.AnimationListener() {
				public void onAnimationStart(Animation animation) {
					mPageNumberView.setVisibility(View.INVISIBLE);
				}
				public void onAnimationRepeat(Animation animation) {}
				public void onAnimationEnd(Animation animation) {
					mPageSlider.setVisibility(View.INVISIBLE);
				}
			});
			mPageSlider.startAnimation(anim);
		}
	}

	void searchModeOn() {
		mTopBarIsSearch = true;
		//Focus on EditTextWidget
		mSearchText.requestFocus();
		showKeyboard();
		mTopBarSwitcher.showNext();
	}

	void searchModeOff() {
		mTopBarIsSearch = false;
		hideKeyboard();
		mTopBarSwitcher.showPrevious();
		SearchTaskResult.set(null);
		// Make the ReaderView act on the change to mSearchTaskResult
		// via overridden onChildSetup method.
		mDocView.resetupChildren();
	}

	void updatePageNumView(int index) {
		if (core == null)
			return;
		mPageNumberView.setText(String.format("%d/%d", index+1, core.countPages()));
	}

	void makeButtonsView() {
		mButtonsView = getLayoutInflater().inflate(R.layout.buttons,null);
		mFilenameView = (TextView)mButtonsView.findViewById(R.id.docNameText);
		mPageSlider = (SeekBar)mButtonsView.findViewById(R.id.pageSlider);
		mPageNumberView = (TextView)mButtonsView.findViewById(R.id.pageNumber);
		mSearchButton = (ImageButton)mButtonsView.findViewById(R.id.searchButton);
		mCancelButton = (ImageButton)mButtonsView.findViewById(R.id.cancel);
		mOutlineButton = (ImageButton)mButtonsView.findViewById(R.id.outlineButton);
		mTopBarSwitcher = (ViewSwitcher)mButtonsView.findViewById(R.id.switcher);
		mSearchBack = (ImageButton)mButtonsView.findViewById(R.id.searchBack);
		mSearchFwd = (ImageButton)mButtonsView.findViewById(R.id.searchForward);
		mSearchText = (EditText)mButtonsView.findViewById(R.id.searchText);
// XXX		mLinkButton = (ImageButton)mButtonsView.findViewById(R.id.linkButton);
		mTopBarSwitcher.setVisibility(View.INVISIBLE);
		mPageNumberView.setVisibility(View.INVISIBLE);
		mPageSlider.setVisibility(View.INVISIBLE);
	}

	void showKeyboard() {
		InputMethodManager imm = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
		if (imm != null)
			imm.showSoftInput(mSearchText, 0);
	}

	void hideKeyboard() {
		InputMethodManager imm = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
		if (imm != null)
			imm.hideSoftInputFromWindow(mSearchText.getWindowToken(), 0);
	}

	void killSearch() {
		if (mSearchTask != null) {
			mSearchTask.cancel(true);
			mSearchTask = null;
		}
	}

	void search(int direction) {
		hideKeyboard();
		if (core == null)
			return;
		killSearch();

		final ProgressDialogX progressDialog = new ProgressDialogX(this);
		progressDialog.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
		progressDialog.setTitle(getString(R.string.searching_));
		progressDialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
			public void onCancel(DialogInterface dialog) {
				killSearch();
			}
		});
		progressDialog.setMax(core.countPages());

		mSearchTask = new AsyncTask<Integer,Integer,SearchTaskResult>() {
			@Override
			protected SearchTaskResult doInBackground(Integer... params) {
				int index;
				if (SearchTaskResult.get() == null)
					index = mDocView.getDisplayedViewIndex();
				else
					index = SearchTaskResult.get().pageNumber + params[0].intValue();

				while (0 <= index && index < core.countPages() && !isCancelled()) {
					publishProgress(index);
					RectF searchHits[] = core.searchPage(index, mSearchText.getText().toString());

					if (searchHits != null && searchHits.length > 0)
						return new SearchTaskResult(mSearchText.getText().toString(), index, searchHits);

					index += params[0].intValue();
				}
				return null;
			}

			@Override
			protected void onPostExecute(SearchTaskResult result) {
				progressDialog.cancel();
				if (result != null) {
					// Ask the ReaderView to move to the resulting page
					mDocView.setDisplayedViewIndex(result.pageNumber);
				    SearchTaskResult.set(result);
					// Make the ReaderView act on the change to mSearchTaskResult
					// via overridden onChildSetup method.
				    mDocView.resetupChildren();
				} else {
					mAlertBuilder.setTitle(R.string.text_not_found);
					AlertDialog alert = mAlertBuilder.create();
					alert.setButton(AlertDialog.BUTTON_POSITIVE, "Dismiss",
							(DialogInterface.OnClickListener)null);
					alert.show();
				}
			}

			@Override
			protected void onCancelled() {
				super.onCancelled();
				progressDialog.cancel();
			}

			@Override
			protected void onProgressUpdate(Integer... values) {
				super.onProgressUpdate(values);
				progressDialog.setProgress(values[0].intValue());
			}

			@Override
			protected void onPreExecute() {
				super.onPreExecute();
				mHandler.postDelayed(new Runnable() {
					public void run() {
						if (!progressDialog.isCancelled())
							progressDialog.show();
					}
				}, SEARCH_PROGRESS_DELAY);
			}
		};

		mSearchTask.execute(new Integer(direction));
	}
}

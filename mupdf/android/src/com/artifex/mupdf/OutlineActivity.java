package com.artifex.mupdf;

import android.app.ListActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.ListView;

public class OutlineActivity extends ListActivity {
	OutlineItem mItems[];
	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
	    super.onCreate(savedInstanceState);

	    mItems = OutlineActivityData.get().items;
	    setListAdapter(new OutlineAdapter(getLayoutInflater(),mItems));
	    // Restore the position within the list from last viewing
	    getListView().setSelection(OutlineActivityData.get().position);
	    setResult(-1);
	}

	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		super.onListItemClick(l, v, position, id);
		OutlineActivityData.get().position = getListView().getFirstVisiblePosition();
		setResult(mItems[position].page);
		finish();
	}
}

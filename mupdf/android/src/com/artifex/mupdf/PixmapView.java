package com.artifex.mupdf;

import android.app.*;
import android.os.*;
import android.content.*;
import android.content.res.*;
import android.graphics.*;
import android.util.*;
import android.view.*;
import android.widget.*;
import java.net.*;
import java.io.*;

public class PixmapView extends SurfaceView implements SurfaceHolder.Callback
{
	private SurfaceHolder holder;
	private MuPDFThread thread = null;
	private boolean threadStarted = false;
	private MuPDFCore core;

	/* Constructor */
	public PixmapView(Context context, MuPDFCore core)
	{
		super(context);
		System.out.println("PixmapView construct");
		this.core = core;
		holder = getHolder();
		holder.addCallback(this);
		thread = new MuPDFThread(holder, core);
		setFocusable(true); // need to get the key events
	}

	/* load our native library */
	static {
		System.loadLibrary("mupdf");
	}

	/* Handlers for keys - so we can actually do stuff */
	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event)
	{
		if (thread.onKeyDown(keyCode, event))
			return true;
		return super.onKeyDown(keyCode, event);
	}

	@Override
	public boolean onKeyUp(int keyCode, KeyEvent event)
	{
		if (thread.onKeyUp(keyCode, event))
			return true;
		return super.onKeyUp(keyCode, event);
	}

	@Override
	public boolean onTouchEvent(MotionEvent event)
	{
		if (thread.onTouchEvent(event))
			return true;
		return super.onTouchEvent(event);
	}

	public void changePage(int delta)
	{
		thread.changePage(delta);
	}

	/* Handlers for SurfaceHolder callbacks; these are called when the
	 * surface is created/destroyed/changed. We need to ensure that we only
	 * draw into the surface between the created and destroyed calls.
	 * Therefore, we start/stop the thread that actually runs MuPDF on
	 * creation/destruction. */
	public void surfaceCreated(SurfaceHolder holder)
	{
	}

	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
	{
		thread.newScreenSize(width, height);
		if (!threadStarted)
		{
			threadStarted = true;
			thread.setRunning(true);
			thread.start();
		}
	}

	public void surfaceDestroyed(SurfaceHolder holder)
	{
		boolean retry = true;
		System.out.println("Surface destroyed 1 this="+this);
		thread.setRunning(false);
		System.out.println("Surface destroyed 2");
		while (retry)
		{
			try
			{
				thread.join();
				retry = false;
			}
			catch (InterruptedException e)
			{
			}
		}
		threadStarted = false;
		System.out.println("Surface destroyed 3");
	}

	class MuPDFThread extends Thread
	{
		private SurfaceHolder holder;
		private boolean running = false;
		private int keycode = -1;
		private int screenWidth;
		private int screenHeight;
		private int screenGeneration;
		private Bitmap bitmap;
		private MuPDFCore core;

		/* The following variables deal with the size of the current page;
		 * specifically, its position on the screen, its raw size, its
		 * current scale, and its current scaled size (in terms of whole
		 * pixels).
		 */
		private int pageOriginX;
		private int pageOriginY;
		private float pageScale;
		private int pageWidth;
		private int pageHeight;

		/* The following variables deal with the multitouch handling */
		private final int NONE = 0;
		private final int DRAG = 1;
		private final int ZOOM = 2;
		private int touchMode = NONE;
		private float touchInitialSpacing;
		private float touchDragStartX;
		private float touchDragStartY;
		private float touchInitialOriginX;
		private float touchInitialOriginY;
		private float touchInitialScale;
		private PointF touchZoomMidpoint;

		/* The following control the inner loop; other events etc cause
		 * action to be set. The inner loop runs around a tight loop
		 * performing the action requested of it.
		 */
		private boolean wakeMe = false;
		private int action;
		private final int SLEEP = 0;
		private final int REDRAW = 1;
		private final int DIE = 2;
		private final int GOTOPAGE = 3;
		private int actionPageNum;

		/* Members for blitting, declared here to avoid causing gcs */
		private Rect srcRect;
		private RectF dstRect;

		public MuPDFThread(SurfaceHolder holder, MuPDFCore core)
		{
			this.holder = holder;
			this.core = core;
			touchZoomMidpoint = new PointF(0,0);
			srcRect = new Rect(0,0,0,0);
			dstRect = new RectF(0,0,0,0);
		}

		public void setRunning(boolean running)
		{
			this.running = running;
			if (!running)
			{
				System.out.println("killing 1");
				synchronized(this)
				{
					System.out.println("killing 2");
					action = DIE;
					if (wakeMe)
					{
						wakeMe = false;
						System.out.println("killing 3");
						this.notify();
						System.out.println("killing 4");
					}
				}
			}
		}

		public void newScreenSize(int width, int height)
		{
			this.screenWidth = width;
			this.screenHeight = height;
			this.screenGeneration++;
		}

		public boolean onKeyDown(int keyCode, KeyEvent msg)
		{
			keycode = keyCode;
			return false;
		}

		public boolean onKeyUp(int keyCode, KeyEvent msg)
		{
			return false;
		}

		public synchronized void changePage(int delta)
		{
			action = GOTOPAGE;
			if (delta == Integer.MIN_VALUE)
				actionPageNum = 0;
			else if (delta == Integer.MAX_VALUE)
				actionPageNum = core.numPages-1;
			else
			{
				actionPageNum += delta;
				if (actionPageNum < 0)
					actionPageNum = 0;
				if (actionPageNum > core.numPages-1)
					actionPageNum = core.numPages-1;
			}
			if (wakeMe)
			{
				wakeMe = false;
				this.notify();
			}
		}

		private float spacing(MotionEvent event)
		{
			float x = event.getX(0) - event.getX(1);
			float y = event.getY(0) - event.getY(1);
			return FloatMath.sqrt(x*x+y*y);
		}

		private void midpoint(PointF point, MotionEvent event)
		{
			float x = event.getX(0) + event.getX(1);
			float y = event.getY(0) + event.getY(1);
			point.set(x/2, y/2);
		}

		private synchronized void forceRedraw()
		{
			if (wakeMe)
			{
				wakeMe = false;
				this.notify();
			}
			action = REDRAW;
		}

		public synchronized void setPageOriginTo(int x, int y)
		{
			/* Adjust the coordinates so that the page always covers the
			 * centre of the screen. */
			if (x + pageWidth < screenWidth/2)
			{
				x = screenWidth/2 - pageWidth;
			}
			else if (x > screenWidth/2)
			{
				x = screenWidth/2;
			}
			if (y + pageHeight < screenHeight/2)
			{
				y = screenHeight/2 - pageHeight;
			}
			else if (y > screenHeight/2)
			{
				y = screenHeight/2;
			}
			if ((x != pageOriginX) || (y != pageOriginY))
			{
				pageOriginX = x;
				pageOriginY = y;
			}
			forceRedraw();
		}

		public void setPageScaleTo(float scale, PointF midpoint)
		{
			float x, y;
			/* Convert midpoint (in screen coords) to page coords */
			x = (midpoint.x - pageOriginX)/pageScale;
			y = (midpoint.y - pageOriginY)/pageScale;
			/* Find new scaled page sizes */
			synchronized(this)
			{
				pageWidth = (int)(core.pageWidth*scale+0.5);
				if (pageWidth < screenWidth/2)
				{
					scale = screenWidth/2/core.pageWidth;
					pageWidth = (int)(core.pageWidth*scale+0.5);
				}
				pageHeight = (int)(core.pageHeight*scale+0.5);
				if (pageHeight < screenHeight/2)
				{
					scale = screenHeight/2/core.pageHeight;
					pageWidth = (int)(core.pageWidth *scale+0.5);
					pageHeight = (int)(core.pageHeight*scale+0.5);
				}
				pageScale = scale;
				/* Now given this new scale, calculate page origins so that
				 * x and y are at midpoint */
				float xscale = (float)pageWidth /core.pageWidth;
				float yscale = (float)pageHeight/core.pageHeight;
				setPageOriginTo((int)(midpoint.x - x*xscale + 0.5),
						(int)(midpoint.y - y*yscale + 0.5));
			}
		}

		public void scalePageToScreen()
		{
			float scaleX, scaleY;
			scaleX = (float)screenWidth /core.pageWidth;
			scaleY = (float)screenHeight/core.pageHeight;
			synchronized(this)
			{
				if (scaleX < scaleY)
					pageScale = scaleX;
				else
					pageScale = scaleY;
				pageWidth = (int)(core.pageWidth * pageScale + 0.5);
				pageHeight = (int)(core.pageHeight * pageScale + 0.5);
				pageOriginX = (screenWidth - pageWidth)/2;
				pageOriginY = (screenHeight - pageHeight)/2;
				forceRedraw();
			}
			System.out.println("scalePageToScreen: Raw="+
					core.pageWidth+"x"+core.pageHeight+" scaled="+
					pageWidth+","+pageHeight+" pageScale="+
					pageScale);
		}

		public boolean onTouchEvent(MotionEvent event)
		{
			int action = event.getAction();
			boolean done = false;
			switch (action & MotionEvent.ACTION_MASK)
			{
				case MotionEvent.ACTION_DOWN:
					touchMode = DRAG;
					touchDragStartX = event.getX();
					touchDragStartY = event.getY();
					touchInitialOriginX = pageOriginX;
					touchInitialOriginY = pageOriginY;
					System.out.println("Starting dragging from: "+touchDragStartX+","+touchDragStartY+" ("+pageOriginX+","+pageOriginY+")");
					done = true;
					break;
				case MotionEvent.ACTION_POINTER_DOWN:
					touchInitialSpacing = spacing(event);
					if (touchInitialSpacing > 10f)
					{
						System.out.println("Started zooming: spacing="+touchInitialSpacing);
						touchInitialScale = pageScale;
						touchMode = ZOOM;
						done = true;
					}
					break;
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP:
					if (touchMode != NONE)
					{
						System.out.println("Released!");
						touchMode = NONE;
						done = true;
					}
					break;
				case MotionEvent.ACTION_MOVE:
					if (touchMode == DRAG)
					{
						float x = touchInitialOriginX+event.getX()-touchDragStartX;
						float y = touchInitialOriginY+event.getY()-touchDragStartY;
						System.out.println("Dragged to "+x+","+y);
						setPageOriginTo((int)(x+0.5),(int)(y+0.5));
						done = true;
					}
					else if (touchMode == ZOOM)
					{
						float newSpacing = spacing(event);
						if (newSpacing > 10f)
						{
							float newScale = touchInitialScale*newSpacing/touchInitialSpacing;
							System.out.println("Zoomed to "+newSpacing);
							midpoint(touchZoomMidpoint,event);
							setPageScaleTo(newScale,touchZoomMidpoint);
							done = true;
						}
					}
			}
			return done;
		}

		public void run()
		{
			boolean redraw = false;
			int patchW = 0;
			int patchH = 0;
			int patchX = 0;
			int patchY = 0;
			int localPageW = 0;
			int localPageH = 0;
			int localScreenGeneration = screenGeneration;
			int localAction;
			int localActionPageNum = core.pageNum;

			/* Set up our default action */
			action = GOTOPAGE;
			actionPageNum = core.pageNum;
			while (action != DIE)
			{
				synchronized(this)
				{
					while (action == SLEEP)
					{
						wakeMe = true;
						try
						{
							System.out.println("Render thread sleeping");
							this.wait();
							System.out.println("Render thread woken");
						}
						catch (java.lang.InterruptedException e)
						{
							System.out.println("Render thread exception:"+e);
						}
					}

					/* Now we do as little as we can get away with while
					 * synchronised. In general this means copying any action
					 * or global variables into local ones so that when we
					 * unsynchronoise, other people can alter them again.
					 */
					switch (action)
					{
						case DIE:
							System.out.println("Woken to die!");
							break;
						case GOTOPAGE:
							localActionPageNum = actionPageNum;
							break;
						case REDRAW:
							/* Figure out what area of the page we want to
							 * redraw (in local variables, in docspace).
							 * We'll always draw a screensized lump, unless
							 * that's too big. */
							System.out.println("page="+pageWidth+","+pageHeight+" ("+core.pageWidth+","+core.pageHeight+"@"+pageScale+") @ "+pageOriginX+","+pageOriginY);
							localPageW = pageWidth;
							localPageH = pageHeight;
							patchW = pageWidth;
							patchH = pageHeight;
							patchX = -pageOriginX;
							patchY = -pageOriginY;
							if (patchX < 0)
								patchX = 0;
							if (patchW > screenWidth)
								patchW = screenWidth;
							srcRect.left = 0;
							if (patchX+patchW > pageWidth)
							{
								srcRect.left += patchX+patchW-pageWidth;
								patchX = pageWidth-patchW;
							}
							if (patchY < 0)
								patchY = 0;
							if (patchH > screenHeight)
								patchH = screenHeight;
							srcRect.top = 0;
							if (patchY+patchH > pageHeight)
							{
								srcRect.top += patchY+patchH-pageHeight;
								patchY = pageHeight-patchH;
							}
							dstRect.left = pageOriginX;
							if (dstRect.left < 0)
								dstRect.left = 0;
							dstRect.top = pageOriginY;
							if (dstRect.top < 0)
								dstRect.top = 0;
							dstRect.right = dstRect.left + patchW;
							srcRect.right = srcRect.left + patchW;
							if (srcRect.right > screenWidth)
							{
								dstRect.right -= srcRect.right-screenWidth;
								srcRect.right = screenWidth;
							}
							if (dstRect.right > screenWidth)
							{
								srcRect.right -= dstRect.right-screenWidth;
								dstRect.right = screenWidth;
							}
							dstRect.bottom = dstRect.top + patchH;
							srcRect.bottom = srcRect.top + patchH;
							if (srcRect.bottom > screenHeight)
							{
								dstRect.bottom -=srcRect.bottom-screenHeight;
								srcRect.bottom = screenHeight;
							}
							if (dstRect.bottom > screenHeight)
							{
								srcRect.bottom -=dstRect.bottom-screenHeight;
								dstRect.bottom = screenHeight;
							}
							System.out.println("patch=["+patchX+","+patchY+","+patchW+","+patchH+"]");
							break;
					}
					localAction = action;
					if (action != DIE)
						action = SLEEP;
				}

				/* In the redraw case:
				 * pW, pH, pX, pY, localPageW, localPageH are now all set
				 * in local variables, and we are safe from the global vars
				 * being altered in calls from other threads. This is all
				 * the information we need to actually do our render.
				 */
				switch (localAction)
				{
					case GOTOPAGE:
						core.gotoPage(localActionPageNum);
						scalePageToScreen();
						action = REDRAW;
						break;
					case REDRAW:
						if ((bitmap == null) ||
							(bitmap.getWidth() != patchW) ||
							(bitmap.getHeight() != patchH))
						{
							/* make bitmap of required size */
							bitmap = Bitmap.createBitmap(patchW, patchH, Bitmap.Config.ARGB_8888);
						}
						System.out.println("Calling redraw native method");
						core.drawPage(bitmap, localPageW, localPageH, patchX, patchY, patchW, patchH);
						System.out.println("Called native method");
						{
							Canvas c = null;
							try
							{
								c = holder.lockCanvas(null);
								synchronized(holder)
								{
									if (localScreenGeneration == screenGeneration)
									{
										doDraw(c);
									}
									else
									{
										/* Someone has changed the screen
										 * under us! Better redraw again...
										 */
										action = REDRAW;
									}
								}
							}
							finally
							{
								if (c != null)
									holder.unlockCanvasAndPost(c);
							}
						}
				}
			}
		}

		protected void doDraw(Canvas canvas)
		{
			if ((canvas == null) || (bitmap == null))
				return;
			/* Clear the screen */
			canvas.drawRGB(128,128,128);
			/* Draw our bitmap on top */
			System.out.println("Blitting bitmap from "+srcRect.left+","+srcRect.top+","+srcRect.right+","+srcRect.bottom+" to "+dstRect.left+","+dstRect.top+","+dstRect.right+","+dstRect.bottom);
			canvas.drawBitmap(bitmap, srcRect, dstRect, (Paint)null);
		}
	}
}

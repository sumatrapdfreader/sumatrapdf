#import <UIKit/UIKit.h>

#undef ABS
#undef MIN
#undef MAX

#include "fitz/fitz.h"
#include "pdf/mupdf.h"
#include "xps/muxps.h"
#include "cbz/mucbz.h"

#include "document.h"

#define GAP 20
#define INDICATOR_Y -44-24
#define SLIDER_W (width - GAP - 24)
#define SEARCH_W (width - GAP - 170)

static dispatch_queue_t queue;
static float screenScale = 1;
static fz_context *ctx = NULL;

@interface MuLibraryController : UITableViewController <UIActionSheetDelegate>
{
	NSArray *files;
	NSTimer *timer;
	struct document *_doc; // temporaries for juggling password dialog
	NSString *_filename;
}
- (void) openDocument: (NSString*)filename;
- (void) askForPassword: (NSString*)prompt;
- (void) onPasswordOkay;
- (void) onPasswordCancel;
- (void) reload;
@end

@interface MuOutlineController : UITableViewController
{
	id target;
	NSMutableArray *titles;
	NSMutableArray *pages;
}
- (id) initWithTarget: (id)aTarget titles: (NSMutableArray*)aTitles pages: (NSMutableArray*)aPages;
@end

@interface MuHitView : UIView
{
	CGSize pageSize;
	int hitCount;
	CGRect hitRects[500];
}
- (id) initWithSearchResults: (int)n forDocument: (struct document *)doc;
- (void) setPageSize: (CGSize)s;
@end

@interface MuPageView : UIScrollView <UIScrollViewDelegate>
{
	struct document *doc;
	int number;
	UIActivityIndicatorView *loadingView;
	UIImageView *imageView;
	UIImageView *tileView;
	MuHitView *hitView;
	CGSize pageSize;
	CGRect tileFrame;
	float tileScale;
	BOOL cancel;
}
- (id) initWithFrame: (CGRect)frame document: (struct document*)aDoc page: (int)aNumber;
- (void) displayImage: (UIImage*)image;
- (void) resizeImage;
- (void) loadPage;
- (void) loadTile;
- (void) willRotate;
- (void) resetZoomAnimated: (BOOL)animated;
- (void) showSearchResults: (int)count;
- (void) clearSearchResults;
- (int) number;
@end

@interface MuDocumentController : UIViewController <UIScrollViewDelegate, UISearchBarDelegate>
{
	struct document *doc;
	NSString *key;
	MuOutlineController *outline;
	UIScrollView *canvas;
	UILabel *indicator;
	UISlider *slider;
	UISearchBar *searchBar;
	UIBarButtonItem *nextButton, *prevButton, *cancelButton, *searchButton, *outlineButton;
	UIBarButtonItem *sliderWrapper;
	int searchPage;
	int cancelSearch;
	int width; // current screen size
	int height;
	int current; // currently visible page
	int scroll_animating; // stop view updates during scrolling animations
}
- (id) initWithFilename: (NSString*)nsfilename document: (struct document *)aDoc;
- (void) createPageView: (int)number;
- (void) gotoPage: (int)number animated: (BOOL)animated;
- (void) onShowOutline: (id)sender;
- (void) onShowSearch: (id)sender;
- (void) onCancelSearch: (id)sender;
- (void) resetSearch;
- (void) showSearchResults: (int)count forPage: (int)number;
- (void) onSlide: (id)sender;
- (void) onTap: (UITapGestureRecognizer*)sender;
- (void) showNavigationBar;
- (void) hideNavigationBar;
@end

@interface MuAppDelegate : NSObject <UIApplicationDelegate, UINavigationControllerDelegate>
{
	UIWindow *window;
	UINavigationController *navigator;
	MuLibraryController *library;
}
@end

#pragma mark -

static void showAlert(NSString *msg, NSString *filename)
{
	UIAlertView *alert = [[UIAlertView alloc]
		initWithTitle: msg
		message: filename
		delegate: nil
		cancelButtonTitle: @"Okay"
		otherButtonTitles: nil];
	[alert show];
	[alert release];
}

static void flattenOutline(NSMutableArray *titles, NSMutableArray *pages, fz_outline *outline, int level)
{
	char indent[8*4+1];
	if (level > 8)
		level = 8;
	memset(indent, ' ', level * 4);
	indent[level * 4] = 0;
	while (outline)
	{
		if (outline->dest.kind == FZ_LINK_GOTO)
		{
			int page = outline->dest.ld.gotor.page;
			if (page >= 0 && outline->title)
			{
				NSString *title = [NSString stringWithUTF8String: outline->title];
				[titles addObject: [NSString stringWithFormat: @"%s%@", indent, title]];
				[pages addObject: [NSNumber numberWithInt: page]];
			}
		}
		flattenOutline(titles, pages, outline->down, level + 1);
		outline = outline->next;
	}
}

static void loadOutline(NSMutableArray *titles, NSMutableArray *pages, struct document *doc)
{
}

static void releasePixmap(void *info, const void *data, size_t size)
{
	fz_drop_pixmap(ctx, info);
}

static UIImage *newImageWithPixmap(fz_pixmap *pix)
{
	unsigned char *samples = fz_pixmap_samples(ctx, pix);
	int w = fz_pixmap_width(ctx, pix);
	int h = fz_pixmap_height(ctx, pix);
	CGDataProviderRef cgdata = CGDataProviderCreateWithData(pix, samples, w * 4 * h, releasePixmap);
	CGColorSpaceRef cgcolor = CGColorSpaceCreateDeviceRGB();
	CGImageRef cgimage = CGImageCreate(w, h, 8, 32, 4 * w,
			cgcolor, kCGBitmapByteOrderDefault,
			cgdata, NULL, NO, kCGRenderingIntentDefault);
	UIImage *image = [[UIImage alloc]
		initWithCGImage: cgimage
		scale: screenScale
		orientation: UIImageOrientationUp];
	CGDataProviderRelease(cgdata);
	CGColorSpaceRelease(cgcolor);
	CGImageRelease(cgimage);
	return image;
}

static CGSize fitPageToScreen(CGSize page, CGSize screen)
{
	float hscale = screen.width / page.width;
	float vscale = screen.height / page.height;
	float scale = fz_min(hscale, vscale);
	hscale = floorf(page.width * scale) / page.width;
	vscale = floorf(page.height * scale) / page.height;
	return CGSizeMake(hscale, vscale);
}

static CGSize measurePage(struct document *doc, int number)
{
	CGSize pageSize;
	measure_page(doc, number, &pageSize.width, &pageSize.height);
	return pageSize;
}

static UIImage *renderPage(struct document *doc, int number, CGSize screenSize)
{
	CGSize pageSize;
	fz_bbox bbox;
	fz_matrix ctm;
	fz_device *dev;
	fz_pixmap *pix;
	CGSize scale;

	screenSize.width *= screenScale;
	screenSize.height *= screenScale;

	measure_page(doc, number, &pageSize.width, &pageSize.height);
	scale = fitPageToScreen(pageSize, screenSize);
	ctm = fz_scale(scale.width, scale.height);
	bbox = (fz_bbox){0, 0, pageSize.width * scale.width, pageSize.height * scale.height};

	pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb, bbox);
	fz_clear_pixmap_with_value(ctx, pix, 255);

	dev = fz_new_draw_device(ctx, pix);
	draw_page(doc, number, dev, ctm, NULL);
	fz_free_device(dev);

	return newImageWithPixmap(pix);
}

static UIImage *renderTile(struct document *doc, int number, CGSize screenSize, CGRect tileRect, float zoom)
{
	CGSize pageSize;
	fz_rect rect;
	fz_bbox bbox;
	fz_matrix ctm;
	fz_device *dev;
	fz_pixmap *pix;
	CGSize scale;

	screenSize.width *= screenScale;
	screenSize.height *= screenScale;
	tileRect.origin.x *= screenScale;
	tileRect.origin.y *= screenScale;
	tileRect.size.width *= screenScale;
	tileRect.size.height *= screenScale;

	measure_page(doc, number, &pageSize.width, &pageSize.height);
	scale = fitPageToScreen(pageSize, screenSize);
	ctm = fz_scale(scale.width * zoom, scale.height * zoom);

	rect.x0 = tileRect.origin.x;
	rect.y0 = tileRect.origin.y;
	rect.x1 = tileRect.origin.x + tileRect.size.width;
	rect.y1 = tileRect.origin.y + tileRect.size.height;
	bbox = fz_round_rect(rect);

	pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb, bbox);
	fz_clear_pixmap_with_value(ctx, pix, 255);

	dev = fz_new_draw_device(ctx, pix);
	draw_page(doc, number, dev, ctm, NULL);
	fz_free_device(dev);

	return newImageWithPixmap(pix);
}

#pragma mark -

@implementation MuLibraryController

- (void) viewWillAppear: (BOOL)animated
{
	[self setTitle: @"PDF, XPS and CBZ Documents"];
	[self reload];
	printf("library viewWillAppear (starting reload timer)\n");
	timer = [NSTimer timerWithTimeInterval: 3
		target: self selector: @selector(reload) userInfo: nil
		repeats: YES];
	[[NSRunLoop currentRunLoop] addTimer: timer forMode: NSDefaultRunLoopMode];
}

- (void) viewWillDisappear: (BOOL)animated
{
	printf("library viewWillDisappear (stopping reload timer)\n");
	[timer invalidate];
	timer = nil;
}

- (void) reload
{
	if (files) {
		[files release];
		files = nil;
	}

	NSFileManager *fileman = [NSFileManager defaultManager];
	NSString *docdir = [NSString stringWithFormat: @"%@/Documents", NSHomeDirectory()];
	NSMutableArray *outfiles = [[NSMutableArray alloc] init];
	NSDirectoryEnumerator *direnum = [fileman enumeratorAtPath:docdir];
	NSString *file;
	BOOL isdir;
	while (file = [direnum nextObject]) {
		NSString *filepath = [docdir stringByAppendingPathComponent:file];
		NSLog(@"file %@\n", file);
		if ([fileman fileExistsAtPath:filepath isDirectory:&isdir] && !isdir) {
			[outfiles addObject:file];
		}
	}

	files = outfiles;

	[[self tableView] reloadData];
}

- (void) dealloc
{
	[files release];
	[super dealloc];
}

- (BOOL) shouldAutorotateToInterfaceOrientation: (UIInterfaceOrientation)o
{
	return YES;
}

- (NSInteger) numberOfSectionsInTableView: (UITableView*)tableView
{
	return 1;
}

- (NSInteger) tableView: (UITableView*)tableView numberOfRowsInSection: (NSInteger)section
{
	return [files count] + 1;
}

- (void) actionSheet:(UIActionSheet *)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex
{
	if (buttonIndex == [actionSheet destructiveButtonIndex])
	{
		char filename[PATH_MAX];
		int row = [actionSheet tag];

		dispatch_sync(queue, ^{});

		strcpy(filename, [NSHomeDirectory() UTF8String]);
		strcat(filename, "/Documents/");
		strcat(filename, [[files objectAtIndex: row - 1] UTF8String]);

		printf("delete document '%s'\n", filename);

		unlink(filename);

		[self reload];
	}
}

- (void) onTapDelete: (UIControl*)sender
{
	int row = [sender tag];
	NSString *title = [NSString stringWithFormat: @"Delete %@?", [files objectAtIndex: row - 1]];
	UIActionSheet *sheet = [[UIActionSheet alloc]
							initWithTitle: title
							delegate: self
							cancelButtonTitle: @"Cancel"
							destructiveButtonTitle: @"Delete"
							otherButtonTitles: nil];
	[sheet setTag: row];
	[sheet showInView: [self tableView]];
	[sheet release];
}

- (UITableViewCell*) tableView: (UITableView*)tableView cellForRowAtIndexPath: (NSIndexPath*)indexPath
{
	static NSString *cellid = @"MuCellIdent";
	UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier: cellid];
	if (!cell)
		cell = [[[UITableViewCell alloc] initWithStyle: UITableViewCellStyleDefault reuseIdentifier: cellid] autorelease];
	int row = [indexPath row];
	if (row == 0) {
		[[cell textLabel] setText: @"About MuPDF"];
		[[cell textLabel] setFont: [UIFont systemFontOfSize: 20]];
	} else {
		[[cell textLabel] setText: [files objectAtIndex: row - 1]];
		[[cell textLabel] setFont: [UIFont systemFontOfSize: 20]];
	}

	if (row > 0)
	{
		UIButton *deleteButton = [UIButton buttonWithType:UIButtonTypeCustom];
		[deleteButton setImage: [UIImage imageNamed: @"x_alt_blue.png"] forState: UIControlStateNormal];
		[deleteButton setFrame: CGRectMake(0, 0, 35, 35)];
		[deleteButton addTarget: self action: @selector(onTapDelete:) forControlEvents: UIControlEventTouchUpInside];
		[deleteButton setTag: row];
		[cell setAccessoryView: deleteButton];
	}
	else
	{
		[cell setAccessoryView: nil];
	}

	return cell;
}

- (void) tableView: (UITableView*)tableView didSelectRowAtIndexPath: (NSIndexPath*)indexPath
{
	int row = [indexPath row];
	if (row == 0)
		[self openDocument: @"../MuPDF.app/About.xps"];
	else
		[self openDocument: [files objectAtIndex: row - 1]];
}

- (void) openDocument: (NSString*)nsfilename
{
	char filename[PATH_MAX];

	dispatch_sync(queue, ^{});

	strcpy(filename, [NSHomeDirectory() UTF8String]);
	strcat(filename, "/Documents/");
	strcat(filename, [nsfilename UTF8String]);

	printf("open document '%s'\n", filename);

	_filename = [nsfilename retain];
	_doc = open_document(ctx, filename);
	if (!_doc) {
		showAlert(@"Cannot open document", nsfilename);
		return;
	}

	if (needs_password(_doc))
		[self askForPassword: @"'%@' needs a password:"];
	else
		[self onPasswordOkay];
}

- (void) askForPassword: (NSString*)prompt
{
	UIAlertView *passwordAlertView = [[UIAlertView alloc]
		initWithTitle: @"Password Protected"
		message: [NSString stringWithFormat: prompt, [_filename lastPathComponent]]
		delegate: self
		cancelButtonTitle: @"Cancel"
		otherButtonTitles: @"Done", nil];
	[passwordAlertView setAlertViewStyle: UIAlertViewStyleSecureTextInput];
	[passwordAlertView show];
	[passwordAlertView release];
}

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
	char *password = (char*) [[[alertView textFieldAtIndex: 0] text] UTF8String];
	[alertView dismissWithClickedButtonIndex: buttonIndex animated: TRUE];
	if (buttonIndex == 1) {
		if (authenticate_password(_doc, password))
			[self onPasswordOkay];
		else
			[self askForPassword: @"Wrong password for '%@'. Try again:"];
	} else {
		[self onPasswordCancel];
	}
}

- (void) onPasswordOkay
{
	MuDocumentController *document = [[MuDocumentController alloc] initWithFilename: _filename document: _doc];
	if (document) {
		[self setTitle: @"Library"];
		[[self navigationController] pushViewController: document animated: YES];
		[document release];
	}
	[_filename release];
	_doc = NULL;
}

- (void) onPasswordCancel
{
	[_filename release];
	printf("close document (password cancel)\n");
	close_document(_doc);
	_doc = NULL;
}

@end

#pragma mark -

@implementation MuOutlineController

- (id) initWithTarget: (id)aTarget titles: (NSMutableArray*)aTitles pages: (NSMutableArray*)aPages
{
	self = [super initWithStyle: UITableViewStylePlain];
	if (self) {
		[self setTitle: @"Table of Contents"];
		target = aTarget; // only keep a weak reference, to avoid retain cycles
		titles = [aTitles retain];
		pages = [aPages retain];
		[[self tableView] setSeparatorStyle: UITableViewCellSeparatorStyleNone];
	}
	return self;
}

- (void) dealloc
{
	[titles release];
	[pages release];
	[super dealloc];
}

- (BOOL) shouldAutorotateToInterfaceOrientation: (UIInterfaceOrientation)o
{
	return YES;
}

- (NSInteger) numberOfSectionsInTableView: (UITableView*)tableView
{
	return 1;
}

- (NSInteger) tableView: (UITableView*)tableView numberOfRowsInSection: (NSInteger)section
{
	return [titles count];
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
	return 28;
}

- (UITableViewCell*) tableView: (UITableView*)tableView cellForRowAtIndexPath: (NSIndexPath*)indexPath
{
	static NSString *cellid = @"MuCellIdent";
	UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier: cellid];
	if (!cell)
	{
		cell = [[[UITableViewCell alloc] initWithStyle: UITableViewCellStyleValue1 reuseIdentifier: cellid] autorelease];
		[[cell textLabel] setFont: [UIFont systemFontOfSize: 16]];
		[[cell detailTextLabel] setFont: [UIFont systemFontOfSize: 16]];
	}
	NSString *title = [titles objectAtIndex: [indexPath row]];
	NSString *page = [pages objectAtIndex: [indexPath row]];
	[[cell textLabel] setText: title];
	[[cell detailTextLabel] setText: [NSString stringWithFormat: @"%d", [page intValue]+1]];
	return cell;
}

- (void) tableView: (UITableView*)tableView didSelectRowAtIndexPath: (NSIndexPath*)indexPath
{
	NSNumber *page = [pages objectAtIndex: [indexPath row]];
	[target gotoPage: [page intValue] animated: NO];
	[[self navigationController] popViewControllerAnimated: YES];
}

@end

#pragma mark -

@implementation MuHitView

- (id) initWithSearchResults: (int)n forDocument: (struct document *)doc
{
	self = [super initWithFrame: CGRectMake(0,0,100,100)];
	if (self) {
		[self setOpaque: NO];

		pageSize = CGSizeMake(100,100);

		for (int i = 0; i < n && i < nelem(hitRects); i++) {
			fz_bbox bbox = search_result_bbox(doc, i); // this is thread-safe enough
			hitRects[i].origin.x = bbox.x0;
			hitRects[i].origin.y = bbox.y0;
			hitRects[i].size.width = bbox.x1 - bbox.x0;
			hitRects[i].size.height = bbox.y1 - bbox.y0;
		}
		hitCount = n;
	}
	return self;
}

- (void) setPageSize: (CGSize)s
{
	pageSize = s;
	// if page takes a long time to load we may have drawn at the initial (wrong) size
	[self setNeedsDisplay];
}

- (void) drawRect: (CGRect)r
{
	CGSize scale = fitPageToScreen(pageSize, self.bounds.size);

	[[UIColor colorWithRed: 0.3 green: 0.3 blue: 1 alpha: 0.5] set];

	for (int i = 0; i < hitCount; i++) {
		CGRect rect = hitRects[i];
		rect.origin.x *= scale.width;
		rect.origin.y *= scale.height;
		rect.size.width *= scale.width;
		rect.size.height *= scale.height;
		UIRectFill(rect);
	}
}

@end

@implementation MuPageView

- (id) initWithFrame: (CGRect)frame document: (struct document*)aDoc page: (int)aNumber
{
	self = [super initWithFrame: frame];
	if (self) {
		doc = aDoc;
		number = aNumber;
		cancel = NO;

		[self setShowsVerticalScrollIndicator: NO];
		[self setShowsHorizontalScrollIndicator: NO];
		[self setDecelerationRate: UIScrollViewDecelerationRateFast];
		[self setDelegate: self];

		// zoomDidFinish/Begin events fire before bounce animation completes,
		// making a mess when we rearrange views during the animation.
		[self setBouncesZoom: NO];

		[self resetZoomAnimated: NO];

		// TODO: use a one shot timer to delay the display of this?
		loadingView = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhiteLarge];
		[loadingView startAnimating];
		[self addSubview: loadingView];

		[self loadPage];
	}
	return self;
}

- (void) dealloc
{
	// dealloc can trigger in background thread when the queued block is
	// our last owner, and releases us on completion.
	// Send the dealloc back to the main thread so we don't mess up UIKit.
	if (dispatch_get_current_queue() != dispatch_get_main_queue()) {
		__block id block_self = self; // don't auto-retain self!
		dispatch_async(dispatch_get_main_queue(), ^{ [block_self dealloc]; });
	} else {
		[hitView release];
		[tileView release];
		[loadingView release];
		[imageView release];
		[super dealloc];
	}
}

- (int) number
{
	return number;
}

- (void) showSearchResults: (int)count
{
	if (hitView) {
		[hitView removeFromSuperview];
		[hitView release];
		hitView = nil;
	}
	hitView = [[MuHitView alloc] initWithSearchResults: count forDocument: doc];
	if (imageView) {
		[hitView setFrame: [imageView frame]];
		[hitView setPageSize: pageSize];
	}
	[self addSubview: hitView];
}

- (void) clearSearchResults
{
	if (hitView) {
		[hitView removeFromSuperview];
		[hitView release];
		hitView = nil;
	}
}

- (void) resetZoomAnimated: (BOOL)animated
{
	// discard tile and any pending tile jobs
	tileFrame = CGRectZero;
	tileScale = 1;
	if (tileView) {
		[tileView removeFromSuperview];
		[tileView release];
		tileView = nil;
	}

	[self setMinimumZoomScale: 1];
	[self setMaximumZoomScale: 5];
	[self setZoomScale: 1 animated: animated];
}

- (void) removeFromSuperview
{
	cancel = YES;
	[super removeFromSuperview];
}

- (void) loadPage
{
	if (number < 0 || number >= count_pages(doc))
		return;
	dispatch_async(queue, ^{
		if (!cancel) {
			printf("render page %d\n", number);
			CGSize size = measurePage(doc, number);
			UIImage *image = renderPage(doc, number, self.bounds.size);
			dispatch_async(dispatch_get_main_queue(), ^{
				pageSize = size;
				[self displayImage: image];
				[image release];
			});
		} else {
			printf("cancel page %d\n", number);
		}
	});
}

- (void) displayImage: (UIImage*)image
{
	if (loadingView) {
		[loadingView removeFromSuperview];
		[loadingView release];
		loadingView = nil;
	}

	if (hitView)
		[hitView setPageSize: pageSize];

	if (!imageView) {
		imageView = [[UIImageView alloc] initWithImage: image];
		imageView.opaque = YES;
		[self addSubview: imageView];
		if (hitView)
			[self bringSubviewToFront: hitView];
	} else {
		[imageView setImage: image];
	}

	[self resizeImage];
}

- (void) resizeImage
{
	if (imageView) {
		CGSize imageSize = imageView.image.size;
		CGSize scale = fitPageToScreen(imageSize, self.bounds.size);
		if (fabs(scale.width - 1) > 0.1) {
			CGRect frame = [imageView frame];
			frame.size.width = imageSize.width * scale.width;
			frame.size.height = imageSize.height * scale.height;
			[imageView setFrame: frame];

			printf("resized view; queuing up a reload (%d)\n", number);
			dispatch_async(queue, ^{
				dispatch_async(dispatch_get_main_queue(), ^{
					CGSize scale = fitPageToScreen(imageView.image.size, self.bounds.size);
					if (fabs(scale.width - 1) > 0.01)
						[self loadPage];
				});
			});
		} else {
			[imageView sizeToFit];
		}

		[self setContentSize: imageView.frame.size];

		[self layoutIfNeeded];
	}

}

- (void) willRotate
{
	if (imageView) {
		[self resetZoomAnimated: NO];
		[self resizeImage];
	}
}

- (void) layoutSubviews
{
	[super layoutSubviews];

	// center the image as it becomes smaller than the size of the screen

	CGSize boundsSize = self.bounds.size;
	CGRect frameToCenter = loadingView ? loadingView.frame : imageView.frame;

	// center horizontally
	if (frameToCenter.size.width < boundsSize.width)
		frameToCenter.origin.x = floor((boundsSize.width - frameToCenter.size.width) / 2);
	else
		frameToCenter.origin.x = 0;

	// center vertically
	if (frameToCenter.size.height < boundsSize.height)
		frameToCenter.origin.y = floor((boundsSize.height - frameToCenter.size.height) / 2);
	else
		frameToCenter.origin.y = 0;

	if (loadingView)
		loadingView.frame = frameToCenter;
	else
		imageView.frame = frameToCenter;

	if (hitView && imageView)
		[hitView setFrame: [imageView frame]];
}

- (UIView*) viewForZoomingInScrollView: (UIScrollView*)scrollView
{
	return imageView;
}

- (void) loadTile
{
	CGSize screenSize = self.bounds.size;

	tileFrame.origin = self.contentOffset;
	tileFrame.size = self.bounds.size;
	tileFrame = CGRectIntersection(tileFrame, imageView.frame);
	tileScale = self.zoomScale;

	CGRect frame = tileFrame;
	float scale = tileScale;

	CGRect viewFrame = frame;
	if (self.contentOffset.x < imageView.frame.origin.x)
		viewFrame.origin.x = 0;
	if (self.contentOffset.y < imageView.frame.origin.y)
		viewFrame.origin.y = 0;

	if (scale < 1.01)
		return;

	dispatch_async(queue, ^{
		__block BOOL isValid;
		dispatch_sync(dispatch_get_main_queue(), ^{
			isValid = CGRectEqualToRect(frame, tileFrame) && scale == tileScale;
		});
		if (!isValid) {
			printf("cancel tile\n");
			return;
		}

		printf("render tile\n");
		UIImage *image = renderTile(doc, number, screenSize, viewFrame, scale);

		dispatch_async(dispatch_get_main_queue(), ^{
			isValid = CGRectEqualToRect(frame, tileFrame) && scale == tileScale;
			if (isValid) {
				tileFrame = CGRectZero;
				tileScale = 1;
				if (tileView) {
					[tileView removeFromSuperview];
					[tileView release];
					tileView = nil;
				}

				tileView = [[UIImageView alloc] initWithFrame: frame];
				[tileView setImage: image];
				[self addSubview: tileView];
				if (hitView)
					[self bringSubviewToFront: hitView];
			} else {
				printf("discard tile\n");
			}
			[image release];
		});
	});
}

- (void) scrollViewDidScrollToTop:(UIScrollView *)scrollView { [self loadTile]; }
- (void) scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView { [self loadTile]; }
- (void) scrollViewDidEndDecelerating:(UIScrollView *)scrollView { [self loadTile]; }
- (void) scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
	if (!decelerate)
		[self loadTile];
}

- (void) scrollViewWillBeginZooming: (UIScrollView*)scrollView withView: (UIView*)view
{
	// discard tile and any pending tile jobs
	tileFrame = CGRectZero;
	tileScale = 1;
	if (tileView) {
		[tileView removeFromSuperview];
		[tileView release];
		tileView = nil;
	}
}

- (void) scrollViewDidEndZooming: (UIScrollView*)scrollView withView: (UIView*)view atScale: (float)scale
{
	[self loadTile];
}

- (void) scrollViewDidZoom: (UIScrollView*)scrollView
{
	if (hitView && imageView)
		[hitView setFrame: [imageView frame]];
}

@end

#pragma mark -

@implementation MuDocumentController

- (id) initWithFilename: (NSString*)filename document: (struct document *)aDoc
{
	self = [super init];
	if (!self)
		return nil;

	key = [filename retain];
	doc = aDoc;

	dispatch_sync(queue, ^{});

	fz_outline *root = load_outline(doc);
	if (root) {
		NSMutableArray *titles = [[NSMutableArray alloc] init];
		NSMutableArray *pages = [[NSMutableArray alloc] init];
		flattenOutline(titles, pages, root, 0);
		if ([titles count])
			outline = [[MuOutlineController alloc] initWithTarget: self titles: titles pages: pages];
		[titles release];
		[pages release];
		fz_free_outline(ctx, root);
	}

	return self;
}

- (void) loadView
{
	[[NSUserDefaults standardUserDefaults] setObject: key forKey: @"OpenDocumentKey"];

	current = [[NSUserDefaults standardUserDefaults] integerForKey: key];
	if (current < 0 || current >= count_pages(doc))
		current = 0;

	UIView *view = [[UIView alloc] initWithFrame: CGRectZero];
	[view setAutoresizingMask: UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
	[view setAutoresizesSubviews: YES];

	canvas = [[UIScrollView alloc] initWithFrame: CGRectMake(0,0,GAP,0)];
	[canvas setAutoresizingMask: UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
	[canvas setPagingEnabled: YES];
	[canvas setShowsHorizontalScrollIndicator: NO];
	[canvas setShowsVerticalScrollIndicator: NO];
	[canvas setDelegate: self];

	[canvas addGestureRecognizer: [[[UITapGestureRecognizer alloc] initWithTarget: self action: @selector(onTap:)] autorelease]];

	scroll_animating = NO;

	indicator = [[UILabel alloc] initWithFrame: CGRectZero];
	[indicator setAutoresizingMask: UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleTopMargin];
	[indicator setText: @"0000 of 9999"];
	[indicator sizeToFit];
	[indicator setCenter: CGPointMake(0, INDICATOR_Y)];
	[indicator setTextAlignment: UITextAlignmentCenter];
	[indicator setBackgroundColor: [[UIColor blackColor] colorWithAlphaComponent: 0.5]];
	[indicator setTextColor: [UIColor whiteColor]];

	[view addSubview: canvas];
	[view addSubview: indicator];

	slider = [[UISlider alloc] initWithFrame: CGRectZero];
	[slider setMinimumValue: 0];
	[slider setMaximumValue: count_pages(doc) - 1];
	[slider addTarget: self action: @selector(onSlide:) forControlEvents: UIControlEventValueChanged];

	sliderWrapper = [[UIBarButtonItem alloc] initWithCustomView: slider];

	[self setToolbarItems: [NSArray arrayWithObjects: sliderWrapper, nil]];

	// Set up the buttons on the navigation and search bar

	if (outline) {
		outlineButton = [[UIBarButtonItem alloc]
			initWithBarButtonSystemItem: UIBarButtonSystemItemBookmarks
			target:self action:@selector(onShowOutline:)];
	}
	cancelButton = [[UIBarButtonItem alloc]
		initWithTitle: @"Cancel" style: UIBarButtonItemStyleBordered
		target:self action:@selector(onCancelSearch:)];
	searchButton = [[UIBarButtonItem alloc]
		initWithBarButtonSystemItem: UIBarButtonSystemItemSearch
		target:self action:@selector(onShowSearch:)];
	prevButton = [[UIBarButtonItem alloc]
		initWithBarButtonSystemItem: UIBarButtonSystemItemRewind
		target:self action:@selector(onSearchPrev:)];
	nextButton = [[UIBarButtonItem alloc]
		initWithBarButtonSystemItem: UIBarButtonSystemItemFastForward
		target:self action:@selector(onSearchNext:)];

	searchBar = [[UISearchBar alloc] initWithFrame: CGRectMake(0,0,50,32)];
	[searchBar setPlaceholder: @"Search"];
	[searchBar setDelegate: self];
	// HACK to make transparent background
	[[searchBar.subviews objectAtIndex:0] removeFromSuperview];

	[prevButton setEnabled: NO];
	[nextButton setEnabled: NO];

	[[self navigationItem] setRightBarButtonItems:
		[NSArray arrayWithObjects: searchButton, outlineButton, nil]];

	// TODO: add activityindicator to search bar

	[self setView: view];
	[view release];
}

- (void) dealloc
{
	if (doc) {
		struct document *self_doc = doc; // don't auto-retain self here!
		dispatch_async(queue, ^{
			printf("close document\n");
			close_document(self_doc);
		});
	}

	[indicator release]; indicator = nil;
	[slider release]; slider = nil;
	[sliderWrapper release]; sliderWrapper = nil;
	[searchBar release]; searchBar = nil;
	[outlineButton release]; outlineButton = nil;
	[searchButton release]; searchButton = nil;
	[cancelButton release]; cancelButton = nil;
	[prevButton release]; prevButton = nil;
	[nextButton release]; nextButton = nil;
	[canvas release]; canvas = nil;

	[outline release];
	[key release];
	[super dealloc];
}

- (void) viewWillAppear: (BOOL)animated
{
	[self setTitle: [key lastPathComponent]];

	[slider setValue: current];

	[indicator setText: [NSString stringWithFormat: @" %d of %d ", current+1, count_pages(doc)]];

	[[self navigationController] setToolbarHidden: NO animated: animated];
}

- (void) viewWillLayoutSubviews
{
	CGSize size = [canvas frame].size;
	int max_width = fz_max(width, size.width);

	width = size.width;
	height = size.height;

	[canvas setContentInset: UIEdgeInsetsZero];
	[canvas setContentSize: CGSizeMake(count_pages(doc) * width, height)];
	[canvas setContentOffset: CGPointMake(current * width, 0)];

	[sliderWrapper setWidth: SLIDER_W];
	[searchBar setFrame: CGRectMake(0,0,SEARCH_W,32)];

	[[[self navigationController] toolbar] setNeedsLayout]; // force layout!

	// use max_width so we don't clamp the content offset too early during animation
	[canvas setContentSize: CGSizeMake(count_pages(doc) * max_width, height)];
	[canvas setContentOffset: CGPointMake(current * width, 0)];

	for (MuPageView *view in [canvas subviews]) {
		if ([view number] == current) {
			[view setFrame: CGRectMake([view number] * width, 0, width-GAP, height)];
			[view willRotate];
		}
	}
	for (MuPageView *view in [canvas subviews]) {
		if ([view number] != current) {
			[view setFrame: CGRectMake([view number] * width, 0, width-GAP, height)];
			[view willRotate];
		}
	}
}

- (void) viewDidAppear: (BOOL)animated
{
	[self scrollViewDidScroll: canvas];
}

- (void) viewWillDisappear: (BOOL)animated
{
	[self setTitle: @"Resume"];
	[[NSUserDefaults standardUserDefaults] removeObjectForKey: @"OpenDocumentKey"];
	[[self navigationController] setToolbarHidden: YES animated: animated];
}

- (void) showNavigationBar
{
	if ([[self navigationController] isNavigationBarHidden]) {
		[[self navigationController] setNavigationBarHidden: NO];
		[[self navigationController] setToolbarHidden: NO];
		[indicator setHidden: NO];

		[UIView beginAnimations: @"MuNavBar" context: NULL];

		[[[self navigationController] navigationBar] setAlpha: 1];
		[[[self navigationController] toolbar] setAlpha: 1];
		[indicator setAlpha: 1];

		[UIView commitAnimations];
	}
}

- (void) hideNavigationBar
{
	if (![[self navigationController] isNavigationBarHidden]) {
		[searchBar resignFirstResponder];

		[UIView beginAnimations: @"MuNavBar" context: NULL];
		[UIView setAnimationDelegate: self];
		[UIView setAnimationDidStopSelector: @selector(onHideNavigationBarFinished)];

		[[[self navigationController] navigationBar] setAlpha: 0];
		[[[self navigationController] toolbar] setAlpha: 0];
		[indicator setAlpha: 0];

		[UIView commitAnimations];
	}
}

- (void) onHideNavigationBarFinished
{
	[[self navigationController] setNavigationBarHidden: YES];
	[[self navigationController] setToolbarHidden: YES];
	[indicator setHidden: YES];
}

- (void) onShowOutline: (id)sender
{
	[[self navigationController] pushViewController: outline animated: YES];
}

- (void) onShowSearch: (id)sender
{
	[[self navigationItem] setTitleView: searchBar];
	[[self navigationItem] setRightBarButtonItems:
		[NSArray arrayWithObjects: nextButton, prevButton, nil]];
	[[self navigationItem] setLeftBarButtonItem: cancelButton];
	[searchBar becomeFirstResponder];
}

- (void) onCancelSearch: (id)sender
{
	cancelSearch = YES;
	[searchBar resignFirstResponder];
	[[self navigationItem] setTitleView: nil];
	[[self navigationItem] setRightBarButtonItems:
		[NSArray arrayWithObjects: searchButton, outlineButton, nil]];
	[[self navigationItem] setLeftBarButtonItem: nil];
	[self resetSearch];
}

- (void) resetSearch
{
	searchPage = -1;
	for (MuPageView *view in [canvas subviews])
		[view clearSearchResults];
}

- (void) showSearchResults: (int)count forPage: (int)number
{
	printf("search found match on page %d\n", number);
	searchPage = number;
	[self gotoPage: number animated: NO];
	for (MuPageView *view in [canvas subviews])
		if ([view number] == number)
			[view showSearchResults: count];
		else
			[view clearSearchResults];
}

- (void) searchInDirection: (int)dir
{
	UITextField *searchField;
	char *needle;
	int start;

	[searchBar resignFirstResponder];

	if (searchPage == current)
		start = current + dir;
	else
		start = current;

	needle = strdup([[searchBar text] UTF8String]);

	searchField = nil;
	for (id view in [searchBar subviews])
		if ([view isKindOfClass: [UITextField class]])
			searchField = view;

	[prevButton setEnabled: NO];
	[nextButton setEnabled: NO];
	[searchField setEnabled: NO];

	cancelSearch = NO;

	dispatch_async(queue, ^{
		for (int i = start; i >= 0 && i < count_pages(doc); i += dir) {
			int n = search_page(doc, i, needle, NULL);
			if (n) {
				dispatch_async(dispatch_get_main_queue(), ^{
					[prevButton setEnabled: YES];
					[nextButton setEnabled: YES];
					[searchField setEnabled: YES];
					[self showSearchResults: n forPage: i];
					free(needle);
				});
				return;
			}
			if (cancelSearch) {
				dispatch_async(dispatch_get_main_queue(), ^{
					[prevButton setEnabled: YES];
					[nextButton setEnabled: YES];
					[searchField setEnabled: YES];
					free(needle);
				});
				return;
			}
		}
		dispatch_async(dispatch_get_main_queue(), ^{
			printf("no search results found\n");
			[prevButton setEnabled: YES];
			[nextButton setEnabled: YES];
			[searchField setEnabled: YES];
			UIAlertView *alert = [[UIAlertView alloc]
				initWithTitle: @"No matches found for:"
				message: [NSString stringWithUTF8String: needle]
				delegate: nil
				cancelButtonTitle: @"Close"
				otherButtonTitles: nil];
			[alert show];
			[alert release];
			free(needle);
		});
	});
}

- (void) onSearchPrev: (id)sender
{
	[self searchInDirection: -1];
}

- (void) onSearchNext: (id)sender
{
	[self searchInDirection: 1];
}

- (void) searchBarSearchButtonClicked: (UISearchBar*)sender
{
	[self onSearchNext: sender];
}

- (void) searchBar: (UISearchBar*)sender textDidChange: (NSString*)searchText
{
	[self resetSearch];
	if ([[searchBar text] length] > 0) {
		[prevButton setEnabled: YES];
		[nextButton setEnabled: YES];
	} else {
		[prevButton setEnabled: NO];
		[nextButton setEnabled: NO];
	}
}

- (void) onSlide: (id)sender
{
	int number = [slider value];
	if ([slider isTracking])
		[indicator setText: [NSString stringWithFormat: @" %d of %d ", number+1, count_pages(doc)]];
	else
		[self gotoPage: number animated: NO];
}

- (void) onTap: (UITapGestureRecognizer*)sender
{
	CGPoint p = [sender locationInView: canvas];
	CGPoint ofs = [canvas contentOffset];
	float x0 = (width - GAP) / 5;
	float x1 = (width - GAP) - x0;
	p.x -= ofs.x;
	p.y -= ofs.y;
	if (p.x < x0) {
		[self gotoPage: current-1 animated: YES];
	} else if (p.x > x1) {
		[self gotoPage: current+1 animated: YES];
	} else {
		if ([[self navigationController] isNavigationBarHidden])
			[self showNavigationBar];
		else
			[self hideNavigationBar];
	}
}

- (void) scrollViewWillBeginDragging: (UIScrollView *)scrollView
{
	[self hideNavigationBar];
}

- (void) scrollViewDidScroll: (UIScrollView*)scrollview
{
	if (width == 0)
		return; // not visible yet

	if (scroll_animating)
		return; // don't mess with layout during animations

	float x = [canvas contentOffset].x + width * 0.5f;
	current = x / width;

	[[NSUserDefaults standardUserDefaults] setInteger: current forKey: key];

	[indicator setText: [NSString stringWithFormat: @" %d of %d ", current+1, count_pages(doc)]];
	[slider setValue: current];

	// swap the distant page views out

	NSMutableSet *invisiblePages = [[NSMutableSet alloc] init];
	for (MuPageView *view in [canvas subviews]) {
		if ([view number] != current)
			[view resetZoomAnimated: YES];
		if ([view number] < current - 2 || [view number] > current + 2)
			[invisiblePages addObject: view];
	}
	for (MuPageView *view in invisiblePages)
		[view removeFromSuperview];
	[invisiblePages release]; // don't bother recycling them...

	[self createPageView: current];
	[self createPageView: current - 1];
	[self createPageView: current + 1];

	// reset search results when page has flipped
	if (current != searchPage)
		[self resetSearch];
}

- (void) createPageView: (int)number
{
	if (number < 0 || number >= count_pages(doc))
		return;
	int found = 0;
	for (MuPageView *view in [canvas subviews])
		if ([view number] == number)
			found = 1;
	if (!found) {
		MuPageView *view = [[MuPageView alloc] initWithFrame: CGRectMake(number * width, 0, width-GAP, height) document: doc page: number];
		[canvas addSubview: view];
		[view release];
	}
}

- (void) gotoPage: (int)number animated: (BOOL)animated
{
	if (number < 0)
		number = 0;
	if (number >= count_pages(doc))
		number = count_pages(doc) - 1;
	if (current == number)
		return;
	if (animated) {
		// setContentOffset:animated: does not use the normal animation
		// framework. It also doesn't play nice with the tap gesture
		// recognizer. So we do our own page flipping animation here.
		// We must set the scroll_animating flag so that we don't create
		// or remove subviews until after the animation, or they'll
		// swoop in from origo during the animation.

		scroll_animating = YES;
		[UIView beginAnimations: @"MuScroll" context: NULL];
		[UIView setAnimationDuration: 0.4];
		[UIView setAnimationBeginsFromCurrentState: YES];
		[UIView setAnimationDelegate: self];
		[UIView setAnimationDidStopSelector: @selector(onGotoPageFinished)];

		for (MuPageView *view in [canvas subviews])
			[view resetZoomAnimated: NO];

		[canvas setContentOffset: CGPointMake(number * width, 0)];
		[slider setValue: number];
		[indicator setText: [NSString stringWithFormat: @" %d of %d ", number+1, count_pages(doc)]];

		[UIView commitAnimations];
	} else {
		for (MuPageView *view in [canvas subviews])
			[view resetZoomAnimated: NO];
		[canvas setContentOffset: CGPointMake(number * width, 0)];
	}
	current = number;
}

- (void) onGotoPageFinished
{
	scroll_animating = NO;
	[self scrollViewDidScroll: canvas];
}

- (BOOL) shouldAutorotateToInterfaceOrientation: (UIInterfaceOrientation)o
{
	return YES;
}

- (void) didRotateFromInterfaceOrientation: (UIInterfaceOrientation)o
{
	[canvas setContentSize: CGSizeMake(count_pages(doc) * width, height)];
	[canvas setContentOffset: CGPointMake(current * width, 0)];
}

@end

#pragma mark -

@implementation MuAppDelegate

- (BOOL) application: (UIApplication*)application didFinishLaunchingWithOptions: (NSDictionary*)launchOptions
{
	NSString *filename;

	queue = dispatch_queue_create("com.artifex.mupdf.queue", NULL);

	// use at most 128M for resource cache
	ctx = fz_new_context(NULL, NULL, 128<<20);

	screenScale = [[UIScreen mainScreen] scale];

	library = [[MuLibraryController alloc] initWithStyle: UITableViewStylePlain];

	navigator = [[UINavigationController alloc] initWithRootViewController: library];
	[[navigator navigationBar] setTranslucent: YES];
	[[navigator toolbar] setTranslucent: YES];
	[navigator setDelegate: self];

	window = [[UIWindow alloc] initWithFrame: [[UIScreen mainScreen] bounds]];
	[window setBackgroundColor: [UIColor scrollViewTexturedBackgroundColor]];
	[window addSubview: [navigator view]];
	[window makeKeyAndVisible];

	filename = [[NSUserDefaults standardUserDefaults] objectForKey: @"OpenDocumentKey"];
	if (filename)
		[library openDocument: filename];

	filename = [launchOptions objectForKey: UIApplicationLaunchOptionsURLKey];
	NSLog(@"urlkey = %@\n", filename);

	return YES;
}

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
	NSLog(@"openURL: %@\n", url);
	if ([url isFileURL]) {
		NSString *path = [url path];
		NSString *dir = [NSString stringWithFormat: @"%@/Documents/", NSHomeDirectory()];
		path = [path stringByReplacingOccurrencesOfString:@"/private" withString:@""];
		path = [path stringByReplacingOccurrencesOfString:dir withString:@""];
		NSLog(@"file relative path: %@\n", path);
		[library openDocument:path];
		return YES;
	}
	return NO;
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
	printf("applicationDidEnterBackground!\n");
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
	printf("applicationWillEnterForeground!\n");
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
	printf("applicationDidBecomeActive!\n");
}

- (void)applicationWillTerminate:(UIApplication *)application
{
	printf("applicationWillTerminate!\n");
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
	printf("applicationDidReceiveMemoryWarning\n");
}

- (void) dealloc
{
	dispatch_release(queue);
	[library release];
	[navigator release];
	[window release];
	[super dealloc];
}

@end

#pragma mark -

int main(int argc, char *argv[])
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	int retVal = UIApplicationMain(argc, argv, nil, @"MuAppDelegate");
	[pool release];
	return retVal;
}

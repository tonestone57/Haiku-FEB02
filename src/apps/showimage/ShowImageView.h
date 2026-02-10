/*
 * Copyright 2003-2011, Haiku, Inc. All Rights Reserved.
 * Copyright 2004-2005 yellowTAB GmbH. All Rights Reserverd.
 * Copyright 2006 Bernd Korz. All Rights Reserved
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Fernando Francisco de Oliveira
 *		Michael Wilber
 *		Michael Pfeiffer
 *		Ryan Leavengood
 *		yellowTAB GmbH
 *		Bernd Korz
 *		Stephan Aßmus <superstippi@gmx.de>
 *		Axel Dörfler, axeld@pinc-software.de
 */
#ifndef SHOW_IMAGE_VIEW_H
#define SHOW_IMAGE_VIEW_H


#include <Bitmap.h>
#include <Entry.h>
#include <Mime.h>
#include <Messenger.h>
#include <Point.h>
#include <TranslationDefs.h>
#include <View.h>

#include "Filter.h"
#include "SelectionBox.h"


class BitmapOwner;

class ShowImageView : public BView {
public:
	enum image_orientation {
		k0 = 0,
		k90,
		k180,
		k270,
		k0V,
		k90V,
		k0H,
		k270V
	};

	static const int kNumberOfOrientations = 8;

	ShowImageView(const char* name, uint32 flags);
	virtual ~ShowImageView();

	virtual void AttachedToWindow();
	virtual void FrameResized(float width, float height);
	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint point);
	virtual void MouseUp(BPoint point);
	virtual void MouseMoved(BPoint point, uint32 state, const BMessage* message);
	virtual void KeyDown(const char* bytes, int32 numBytes);
	virtual void MessageReceived(BMessage* message);
	virtual void WindowActivated(bool active);
	virtual void Pulse();

	virtual void GetPreferredSize(float* width, float* height);

	status_t SetImage(const BMessage* message);
	status_t SetImage(const entry_ref* ref, BBitmap* bitmap, BitmapOwner* owner);

	BBitmap* Bitmap();
	const entry_ref* Image() const
	{
		return &fCurrentRef;
	}

	void SetZoom(float zoom, BPoint where = BPoint(-1, -1));
	float Zoom() const
	{
		return fZoom;
	}
	void ZoomIn(BPoint where = BPoint(-1, -1));
	void ZoomOut(BPoint where = BPoint(-1, -1));

	void SetScaleBilinear(bool enabled);
	bool ScaleBilinear() const
	{
		return fScaleBilinear;
	}

	void SetStretchToBounds(bool enable);
	bool StretchesToBounds() const
	{
		return fStretchToBounds;
	}

	void ForceOriginalSize(bool enable)
	{
		fForceOriginalSize = enable;
		FitToBounds();
	}

	void SetHideIdlingCursor(bool hide);

	void FitToBounds();
	void Rotate(int degree);
	void Flip(bool vertical);
	void ResizeImage(int w, int h);
	void SetIcon(bool clear);

	void SetShowCaption(bool show);

	void FixupScrollBars();
	void FixupScrollBar(orientation o, float bitmapLength, float viewLength);

	// Coordinate conversions
	BPoint ImageToView(BPoint p) const;
	BPoint ViewToImage(BPoint p) const;
	BRect ImageToView(BRect r) const;
	void ConstrainToImage(BPoint& point) const;
	void ConstrainToImage(BRect& rect) const;

	void SetSelectionMode(bool selectionMode);
	bool IsSelectionModeEnabled() const
	{
		return fSelectionMode;
	}
	void SelectAll();
	void ClearSelection();
	void CopySelectionToClipboard();
	void SaveToFile(BDirectory* dir, const char* name, BBitmap* bitmap,
		const translation_format* format);
	void SetScale(float scale)
	{
		fZoom = scale;
	}

private:
	void _SendMessageToWindow(BMessage* message);
	void _SendMessageToWindow(uint32 code);
	void _Notify();
	void _UpdateStatusText();
	void _DeleteBitmap();
	void _DeleteSelectionBitmap();
	float _FitToBoundsZoom() const;
	BRect _AlignBitmap();
	void _DrawBackground(BRect border);
	void _LayoutCaption(BFont& font, BPoint& pos, BRect& rect);
	void _DrawCaption();
	void _UpdateCaption();
	void _DrawImage(BRect rect);
	BBitmap* _CopySelection(uchar alpha = 255, bool imageSize = true);
	bool _AddSupportedTypes(BMessage* msg, BBitmap* bitmap);
	void _BeginDrag(BPoint sourcePoint);
	bool _OutputFormatForType(BBitmap* bitmap, const char* type,
		translation_format* format);
	void _SendInMessage(BMessage* msg, BBitmap* bitmap,
		translation_format* format);
	void _HandleDrop(BMessage* msg);
	void _ScrollBitmap(BPoint point);
	void _GetMergeRects(BBitmap* merge, BRect selection, BRect& srcRect,
		BRect& dstRect);
	void _GetSelectionMergeRects(BRect& srcRect, BRect& dstRect);
	void _MergeWithBitmap(BBitmap* merge, BRect selection);
	void _UpdateSelectionRect(BPoint point, bool final);
	float _LimitToRange(float v, orientation o, bool absolute);
	void _ScrollRestricted(float x, float y, bool absolute);
	void _ScrollRestrictedTo(float x, float y);
	void _ScrollRestrictedBy(float x, float y);
	void _MouseWheelChanged(BMessage* message);
	void _ShowPopUpMenu(BPoint screen);
	void _SettingsSetBool(const char* name, bool value);
	void _SetHasSelection(bool hasSelection);
	void _DoImageOperation(ImageProcessor::operation op, bool quiet);
	void _UserDoImageOperation(ImageProcessor::operation op, bool quiet = false);
	void _SetIcon(bool clear, icon_size which);
	void _ToggleSlideShow();
	void _StopSlideShow();
	void _ExitFullScreen();
	void _ShowToolBarIfEnabled(bool show);
	void _AnimateSelection(bool enabled);

	BitmapOwner*		fBitmapOwner;
	BBitmap*			fBitmap;
	BBitmap*			fDisplayBitmap;
	BBitmap*			fSelectionBitmap;

	float				fZoom;

	bool				fScaleBilinear;

	BPoint				fBitmapLocationInView;

	bool				fStretchToBounds;
	bool				fForceOriginalSize;
	bool				fHideCursor;
	bool				fScrollingBitmap;
	bool				fCreatingSelection;
	bool				fResizingSelection;
	BPoint				fFirstPoint;
	BRect				fCopyFromRect;
	bool				fSelectionMode;
	bool				fAnimateSelection;
	bool				fHasSelection;
	bool				fShowCaption;
	bool				fShowingPopUpMenu;
	int32				fHideCursorCountDown;
	int32				fStickyZoomCountDown;
	bool				fIsActiveWin;
	BCursor*			fDefaultCursor;
	BCursor*			fGrabCursor;

	entry_ref			fCurrentRef;
	BString				fCaption;
	BString				fFormatDescription;
	BString				fMimeType;
	SelectionBox		fSelectionBox;
	int32				fImageOrientation;

	static image_orientation fTransformation[ImageProcessor::kNumberOfAffineTransformations]
		[kNumberOfOrientations];
};


#endif	// SHOW_IMAGE_VIEW_H

/*
 * Copyright 2003-2010, Haiku, Inc. All Rights Reserved.
 * Copyright 2004-2005 yellowTAB GmbH. All Rights Reserverd.
 * Copyright 2006 Bernd Korz. All Rights Reserved
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Fernando Francisco de Oliveira
 *		Michael Wilber
 *		Michael Pfeiffer
 *		yellowTAB GmbH
 *		Bernd Korz
 *		Stephan Aßmus <superstippi@gmx.de>
 */
#ifndef SELECTION_BOX_H
#define SELECTION_BOX_H


#include <View.h>

class ShowImageView;


class SelectionBox {
public:
								SelectionBox();
								~SelectionBox();

			void				SetBounds(ShowImageView* view, BRect bounds);
			BRect				Bounds() const;

			bool				MouseDown(ShowImageView* view, BPoint where);
			void				MouseMoved(ShowImageView* view, BPoint where);
			void				MouseUp(ShowImageView* view, BPoint where);

			void				Animate();
			void				Draw(ShowImageView* view,
									const BRect& updateRect) const;

private:
			void				_InitPatterns();

			BRect				_RectInView(ShowImageView* view) const;
			uint32				_GetDragHandle(ShowImageView* view,
									BPoint point) const;

private:
			BRect				fBounds;

			// Use patterns to simulate marching ants for selection.
			pattern				fPatternUp;
			pattern				fPatternDown;
			pattern				fPatternLeft;
			pattern				fPatternRight;

			uint32				fDragMode;
};

#endif	// SELECTION_BOX_H

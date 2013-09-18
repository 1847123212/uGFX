/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#include "gfx.h"

#if GFX_USE_GWIN

#include "gwin/class_gwin.h"

// Needed if there is no window manager
#define MIN_WIN_WIDTH	1
#define MIN_WIN_HEIGHT	1

/*-----------------------------------------------
 * Data
 *-----------------------------------------------*/

static const gwinVMT basegwinVMT = {
		"GWIN",					// The classname
		sizeof(GWindowObject),	// The object size
		0,						// The destroy routine
		0,						// The redraw routine
		0,						// The after-clear routine
};

static color_t	defaultFgColor = White;
static color_t	defaultBgColor = Black;
#if GDISP_NEED_TEXT
	static font_t	defaultFont;
#endif

/*-----------------------------------------------
 * Helper Routines
 *-----------------------------------------------*/

#if !GWIN_NEED_WINDOWMANAGER
	static void _gwm_vis(GHandle gh) {
		if (gh->vmt->Redraw) {
			#if GDISP_NEED_CLIP
				gdispSetClip(gh->x, gh->y, gh->width, gh->height);
			#endif
			gh->vmt->Redraw(gh);
		} else
			gwinClear(gh);
	}
	static void _gwm_redim(GHandle gh, coord_t x, coord_t y, coord_t width, coord_t height) {
		gh->x = x; gh->y = y;
		gh->width = width; gh->height = height;
		if (gh->x < 0) { gh->width += gh->x; gh->x = 0; }
		if (gh->y < 0) { gh->height += gh->y; gh->y = 0; }
		if (gh->x > gdispGetWidth()-MIN_WIN_WIDTH)		gh->x = gdispGetWidth()-MIN_WIN_WIDTH;
		if (gh->y > gdispGetHeight()-MIN_WIN_HEIGHT)	gh->y = gdispGetHeight()-MIN_WIN_HEIGHT;
		if (gh->width < MIN_WIN_WIDTH) { gh->width = MIN_WIN_WIDTH; }
		if (gh->height < MIN_WIN_HEIGHT) { gh->height = MIN_WIN_HEIGHT; }
		if (gh->x+gh->width > gdispGetWidth()) gh->width = gdispGetWidth() - gh->x;
		if (gh->y+gh->height > gdispGetHeight()) gh->height = gdispGetHeight() - gh->y;

		// Redraw the window
		if ((gh->flags & GWIN_FLG_VISIBLE)) {
			if (gh->vmt->Redraw) {
				#if GDISP_NEED_CLIP
					gdispSetClip(gh->x, gh->y, gh->width, gh->height);
				#endif
				gh->vmt->Redraw(gh);
			}
		}
	}
#endif

/*-----------------------------------------------
 * Class Routines
 *-----------------------------------------------*/

void _gwinInit(void) {
	#if GWIN_NEED_WIDGET
		extern void _gwidgetInit(void);

		_gwidgetInit();
	#endif
	#if GWIN_NEED_WINDOWMANAGER
		extern void _gwmInit(void);

		_gwmInit();
	#endif
}

// Internal routine for use by GWIN components only
// Initialise a window creating it dynamically if required.
GHandle _gwindowCreate(GWindowObject *pgw, const GWindowInit *pInit, const gwinVMT *vmt, uint16_t flags) {
	// Allocate the structure if necessary
	if (!pgw) {
		if (!(pgw = (GWindowObject *)gfxAlloc(vmt->size)))
			return 0;
		pgw->flags = flags|GWIN_FLG_DYNAMIC;
	} else
		pgw->flags = flags;
	
	// Initialise all basic fields
	pgw->vmt = vmt;
	pgw->color = defaultFgColor;
	pgw->bgcolor = defaultBgColor;
	#if GDISP_NEED_TEXT
		pgw->font = defaultFont;
	#endif

#if GWIN_NEED_WINDOWMANAGER
	if (!_GWINwm->vmt->Add(pgw, pInit)) {
		if ((pgw->flags & GWIN_FLG_DYNAMIC))
			gfxFree(pgw);
		return 0;
	}
#else
	_gwm_redim(pgw, pInit->x, pInit->y, pInit->width, pInit->height);
#endif

	return (GHandle)pgw;
}

/*-----------------------------------------------
 * Routines that affect all windows
 *-----------------------------------------------*/

void gwinSetDefaultColor(color_t clr) {
	defaultFgColor = clr;
}

color_t gwinGetDefaultColor(void) {
	return defaultFgColor;
}

void gwinSetDefaultBgColor(color_t bgclr) {
	defaultBgColor = bgclr;
}

color_t gwinGetDefaultBgColor(void) {
	return defaultBgColor;
}

#if GDISP_NEED_TEXT
	void gwinSetDefaultFont(font_t font) {
		defaultFont = font;
	}

	font_t gwinGetDefaultFont(void) {
		return defaultFont;
	}
#endif

/*-----------------------------------------------
 * The GWindow Routines
 *-----------------------------------------------*/

GHandle gwinWindowCreate(GWindowObject *pgw, const GWindowInit *pInit) {
	if (!(pgw = _gwindowCreate(pgw, pInit, &basegwinVMT, 0)))
		return 0;
	gwinSetVisible(pgw, pInit->show);
	return pgw;
}

void gwinDestroy(GHandle gh) {
	// Remove from the window manager
	#if GWIN_NEED_WINDOWMANAGER
		_GWINwm->vmt->Delete(gh);
	#endif

	// Class destroy routine
	if (gh->vmt->Destroy)
		gh->vmt->Destroy(gh);

	// Clean up the structure
	if (gh->flags & GWIN_FLG_DYNAMIC) {
		gh->flags = 0;							// To be sure, to be sure
		gfxFree((void *)gh);
	} else
		gh->flags = 0;							// To be sure, to be sure
}

const char *gwinGetClassName(GHandle gh) {
	return gh->vmt->classname;
}

void gwinSetVisible(GHandle gh, bool_t visible) {
	if (visible) {
		if (!(gh->flags & GWIN_FLG_VISIBLE)) {
			gh->flags |= GWIN_FLG_VISIBLE;
			#if GWIN_NEED_WINDOWMANAGER
				_GWINwm->vmt->Visible(gh);
			#else
				_gwm_vis(gh);
			#endif
		}
	} else {
		if ((gh->flags & GWIN_FLG_VISIBLE)) {
			gh->flags &= ~GWIN_FLG_VISIBLE;
			#if GWIN_NEED_WINDOWMANAGER
				_GWINwm->vmt->Visible(gh);
			#endif
		}
	}
}

bool_t gwinGetVisible(GHandle gh) {
	return (gh->flags & GWIN_FLG_VISIBLE) ? TRUE : FALSE;
}

void gwinSetEnabled(GHandle gh, bool_t enabled) {
	if (enabled) {
		if (!(gh->flags & GWIN_FLG_ENABLED)) {
			gh->flags |= GWIN_FLG_ENABLED;
			if ((gh->flags & GWIN_FLG_VISIBLE) && gh->vmt->Redraw) {
				#if GDISP_NEED_CLIP
					gdispSetClip(gh->x, gh->y, gh->width, gh->height);
				#endif
				gh->vmt->Redraw(gh);
			}
		}
	} else {
		if ((gh->flags & GWIN_FLG_ENABLED)) {
			gh->flags &= ~GWIN_FLG_ENABLED;
			if ((gh->flags & GWIN_FLG_VISIBLE) && gh->vmt->Redraw) {
				#if GDISP_NEED_CLIP
					gdispSetClip(gh->x, gh->y, gh->width, gh->height);
				#endif
				gh->vmt->Redraw(gh);
			}
		}
	}
}

bool_t gwinGetEnabled(GHandle gh) {
	return (gh->flags & GWIN_FLG_ENABLED) ? TRUE : FALSE;
}

void gwinMove(GHandle gh, coord_t x, coord_t y) {
	#if GWIN_NEED_WINDOWMANAGER
		_GWINwm->vmt->Redim(gh, x, y, gh->width, gh->height);
	#else
		_gwm_redim(gh, x, y, gh->width, gh->height);
	#endif
}

void gwinResize(GHandle gh, coord_t width, coord_t height) {
	#if GWIN_NEED_WINDOWMANAGER
		_GWINwm->vmt->Redim(gh, gh->x, gh->y, width, height);
	#else
		_gwm_redim(gh, gh->x, gh->y, width, height);
	#endif
}

void gwinRedraw(GHandle gh) {
	#if GWIN_NEED_WINDOWMANAGER
		gwinRaise(gh);
	#else
		if ((gh->flags & GWIN_FLG_VISIBLE))
			_gwm_vis(gh);
	#endif
}

#if GDISP_NEED_TEXT
	void gwinSetFont(GHandle gh, font_t font) {
		gh->font = font;
	}
#endif

void gwinClear(GHandle gh) {
	if (!((gh->flags & GWIN_FLG_VISIBLE)))
		return;

	#if GDISP_NEED_CLIP
		gdispSetClip(gh->x, gh->y, gh->width, gh->height);
	#endif
	gdispFillArea(gh->x, gh->y, gh->width, gh->height, gh->bgcolor);
	if (gh->vmt->AfterClear)
		gh->vmt->AfterClear(gh);
}

void gwinDrawPixel(GHandle gh, coord_t x, coord_t y) {
	if (!((gh->flags & GWIN_FLG_VISIBLE)))
		return;

	#if GDISP_NEED_CLIP
		gdispSetClip(gh->x, gh->y, gh->width, gh->height);
	#endif
	gdispDrawPixel(gh->x+x, gh->y+y, gh->color);
}

void gwinDrawLine(GHandle gh, coord_t x0, coord_t y0, coord_t x1, coord_t y1) {
	if (!((gh->flags & GWIN_FLG_VISIBLE)))
		return;

	#if GDISP_NEED_CLIP
		gdispSetClip(gh->x, gh->y, gh->width, gh->height);
	#endif
	gdispDrawLine(gh->x+x0, gh->y+y0, gh->x+x1, gh->y+y1, gh->color);
}

void gwinDrawBox(GHandle gh, coord_t x, coord_t y, coord_t cx, coord_t cy) {
	if (!((gh->flags & GWIN_FLG_VISIBLE)))
		return;

	#if GDISP_NEED_CLIP
		gdispSetClip(gh->x, gh->y, gh->width, gh->height);
	#endif
	gdispDrawBox(gh->x+x, gh->y+y, cx, cy, gh->color);
}

void gwinFillArea(GHandle gh, coord_t x, coord_t y, coord_t cx, coord_t cy) {
	if (!((gh->flags & GWIN_FLG_VISIBLE)))
		return;

	#if GDISP_NEED_CLIP
		gdispSetClip(gh->x, gh->y, gh->width, gh->height);
	#endif
	gdispFillArea(gh->x+x, gh->y+y, cx, cy, gh->color);
}

void gwinBlitArea(GHandle gh, coord_t x, coord_t y, coord_t cx, coord_t cy, coord_t srcx, coord_t srcy, coord_t srccx, const pixel_t *buffer) {
	if (!((gh->flags & GWIN_FLG_VISIBLE)))
		return;

	#if GDISP_NEED_CLIP
		gdispSetClip(gh->x, gh->y, gh->width, gh->height);
	#endif
	gdispBlitAreaEx(gh->x+x, gh->y+y, cx, cy, srcx, srcy, srccx, buffer);
}

#if GDISP_NEED_CIRCLE
	void gwinDrawCircle(GHandle gh, coord_t x, coord_t y, coord_t radius) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)))
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispDrawCircle(gh->x+x, gh->y+y, radius, gh->color);
	}

	void gwinFillCircle(GHandle gh, coord_t x, coord_t y, coord_t radius) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)))
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispFillCircle(gh->x+x, gh->y+y, radius, gh->color);
	}
#endif

#if GDISP_NEED_ELLIPSE
	void gwinDrawEllipse(GHandle gh, coord_t x, coord_t y, coord_t a, coord_t b) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)))
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispDrawEllipse(gh->x+x, gh->y+y, a, b, gh->color);
	}

	void gwinFillEllipse(GHandle gh, coord_t x, coord_t y, coord_t a, coord_t b) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)))
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispFillEllipse(gh->x+x, gh->y+y, a, b, gh->color);
	}
#endif

#if GDISP_NEED_ARC
	void gwinDrawArc(GHandle gh, coord_t x, coord_t y, coord_t radius, coord_t startangle, coord_t endangle) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)))
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispDrawArc(gh->x+x, gh->y+y, radius, startangle, endangle, gh->color);
	}

	void gwinFillArc(GHandle gh, coord_t x, coord_t y, coord_t radius, coord_t startangle, coord_t endangle) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)))
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispFillArc(gh->x+x, gh->y+y, radius, startangle, endangle, gh->color);
	}
#endif

#if GDISP_NEED_PIXELREAD
	color_t gwinGetPixelColor(GHandle gh, coord_t x, coord_t y) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)))
			return defaultBgColor;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		return gdispGetPixelColor(gh->x+x, gh->y+y);
	}
#endif

#if GDISP_NEED_TEXT
	void gwinDrawChar(GHandle gh, coord_t x, coord_t y, char c) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)) || !gh->font)
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispDrawChar(gh->x+x, gh->y+y, c, gh->font, gh->color);
	}

	void gwinFillChar(GHandle gh, coord_t x, coord_t y, char c) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)) || !gh->font)
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispFillChar(gh->x+x, gh->y+y, c, gh->font, gh->color, gh->bgcolor);
	}

	void gwinDrawString(GHandle gh, coord_t x, coord_t y, const char *str) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)) || !gh->font)
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispDrawString(gh->x+x, gh->y+y, str, gh->font, gh->color);
	}

	void gwinFillString(GHandle gh, coord_t x, coord_t y, const char *str) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)) || !gh->font)
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispFillString(gh->x+x, gh->y+y, str, gh->font, gh->color, gh->bgcolor);
	}

	void gwinDrawStringBox(GHandle gh, coord_t x, coord_t y, coord_t cx, coord_t cy, const char* str, justify_t justify) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)) || !gh->font)
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispDrawStringBox(gh->x+x, gh->y+y, cx, cy, str, gh->font, gh->color, justify);
	}

	void gwinFillStringBox(GHandle gh, coord_t x, coord_t y, coord_t cx, coord_t cy, const char* str, justify_t justify) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)) || !gh->font)
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispFillStringBox(gh->x+x, gh->y+y, cx, cy, str, gh->font, gh->color, gh->bgcolor, justify);
	}
#endif

#if GDISP_NEED_CONVEX_POLYGON
	void gwinDrawPoly(GHandle gh, coord_t tx, coord_t ty, const point *pntarray, unsigned cnt) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)))
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispDrawPoly(tx+gh->x, ty+gh->y, pntarray, cnt, gh->color);
	}

	void gwinFillConvexPoly(GHandle gh, coord_t tx, coord_t ty, const point *pntarray, unsigned cnt) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)))
			return;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		gdispFillConvexPoly(tx+gh->x, ty+gh->y, pntarray, cnt, gh->color);
	}
#endif

#if GDISP_NEED_IMAGE
	gdispImageError gwinDrawImage(GHandle gh, gdispImage *img, coord_t x, coord_t y, coord_t cx, coord_t cy, coord_t sx, coord_t sy) {
		if (!((gh->flags & GWIN_FLG_VISIBLE)))
			return GDISP_IMAGE_ERR_OK;

		#if GDISP_NEED_CLIP
			gdispSetClip(gh->x, gh->y, gh->width, gh->height);
		#endif
		return gdispImageDraw(img, gh->x+x, gh->y+y, cx, cy, sx, sy);
	}
#endif

#endif /* GFX_USE_GWIN */
/** @} */


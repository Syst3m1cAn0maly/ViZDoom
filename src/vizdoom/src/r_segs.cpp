// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		All the clipping: columns, horizontal spans, sky columns.
//
// This file contains some code from the Build Engine.
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stddef.h>

#include "templates.h"
#include "i_system.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomdata.h"
#include "p_lnspec.h"

#include "r_local.h"
#include "r_sky.h"
#include "v_video.h"

#include "m_swap.h"
#include "w_wad.h"
#include "stats.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "g_level.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_3dfloors.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "viz_depth.h"
#include "viz_labels.h"

#define WALLYREPEAT 8


CVAR(Bool, r_np2, true, 0)

//CVAR (Int, ty, 8, 0)
//CVAR (Int, tx, 8, 0)

#define HEIGHTBITS 12
#define HEIGHTSHIFT (FRACBITS-HEIGHTBITS)

extern fixed_t globaluclip, globaldclip;


// OPTIMIZE: closed two sided lines as single sided

// killough 1/6/98: replaced globals with statics where appropriate

static bool		segtextured;	// True if any of the segs textures might be visible.
bool		markfloor;		// False if the back side is the same plane.
bool		markceiling;
FTexture *toptexture;
FTexture *bottomtexture;
FTexture *midtexture;
fixed_t rw_offset_top;
fixed_t rw_offset_mid;
fixed_t rw_offset_bottom;


int		wallshade;

short	walltop[MAXWIDTH];	// [RH] record max extents of wall
short	wallbottom[MAXWIDTH];
short	wallupper[MAXWIDTH];
short	walllower[MAXWIDTH];
fixed_t	swall[MAXWIDTH];
fixed_t	lwall[MAXWIDTH];
fixed_t	lwallscale;

//
// regular wall
//
extern fixed_t	rw_backcz1, rw_backcz2;
extern fixed_t	rw_backfz1, rw_backfz2;
extern fixed_t	rw_frontcz1, rw_frontcz2;
extern fixed_t	rw_frontfz1, rw_frontfz2;

int				rw_ceilstat, rw_floorstat;
bool			rw_mustmarkfloor, rw_mustmarkceiling;
bool			rw_prepped;
bool			rw_markmirror;
bool			rw_havehigh;
bool			rw_havelow;

fixed_t			rw_light;		// [RH] Scale lights with viewsize adjustments
fixed_t			rw_lightstep;
fixed_t			rw_lightleft;

static fixed_t	rw_frontlowertop;

static int		rw_x;
static int		rw_stopx;
fixed_t			rw_offset;
static fixed_t	rw_scalestep;
static fixed_t	rw_midtexturemid;
static fixed_t	rw_toptexturemid;
static fixed_t	rw_bottomtexturemid;
static fixed_t	rw_midtexturescalex;
static fixed_t	rw_midtexturescaley;
static fixed_t	rw_toptexturescalex;
static fixed_t	rw_toptexturescaley;
static fixed_t	rw_bottomtexturescalex;
static fixed_t	rw_bottomtexturescaley;

FTexture		*rw_pic;

static fixed_t	*maskedtexturecol;

static void R_RenderDecal (side_t *wall, DBaseDecal *first, drawseg_t *clipper, int pass);
static void WallSpriteColumn (void (*drawfunc)(const BYTE *column, const FTexture::Span *spans));
void wallscan_np2(int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal, fixed_t yrepeat, fixed_t top, fixed_t bot, bool mask);
static void wallscan_np2_ds(drawseg_t *ds, int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal, fixed_t yrepeat);
static void call_wallscan(int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal, fixed_t yrepeat, bool mask);

//=============================================================================
//
// CVAR r_fogboundary
//
// If true, makes fog look more "real" by shading the walls separating two
// sectors with different fog.
//=============================================================================

CVAR(Bool, r_fogboundary, true, 0)

inline bool IsFogBoundary (sector_t *front, sector_t *back)
{
	return r_fogboundary && fixedcolormap == NULL && front->ColorMap->Fade &&
		front->ColorMap->Fade != back->ColorMap->Fade &&
		(front->GetTexture(sector_t::ceiling) != skyflatnum || back->GetTexture(sector_t::ceiling) != skyflatnum);
}

//=============================================================================
//
// CVAR r_drawmirrors
//
// Set to false to disable rendering of mirrors
//=============================================================================

CVAR(Bool, r_drawmirrors, true, 0)

//
// R_RenderMaskedSegRange
//
fixed_t *MaskedSWall;
fixed_t MaskedScaleY;

static void BlastMaskedColumn (bool (*blastfunc)(const BYTE *pixels, const FTexture::Span *spans), FTexture *tex)
{
	// calculate lighting
	if (fixedcolormap == NULL && fixedlightlev < 0)
	{
		dc_colormap = basecolormap->Maps + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
	}

	dc_iscale = MulScale18 (MaskedSWall[dc_x], MaskedScaleY);
 	if (sprflipvert)
		sprtopscreen = centeryfrac + FixedMul(dc_texturemid, spryscale);
	else
		sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
	
	// killough 1/25/98: here's where Medusa came in, because
	// it implicitly assumed that the column was all one patch.
	// Originally, Doom did not construct complete columns for
	// multipatched textures, so there were no header or trailer
	// bytes in the column referred to below, which explains
	// the Medusa effect. The fix is to construct true columns
	// when forming multipatched textures (see r_data.c).

	// draw the texture
	const FTexture::Span *spans;
	const BYTE *pixels = tex->GetColumn (maskedtexturecol[dc_x] >> FRACBITS, &spans);
	blastfunc (pixels, spans);
	rw_light += rw_lightstep;
	spryscale += rw_scalestep;
}

// Clip a midtexture to the floor and ceiling of the sector in front of it.
void ClipMidtex(int x1, int x2)
{
	short most[MAXWIDTH];

	WallMost(most, curline->frontsector->ceilingplane, &WallC);
	for (int i = x1; i < x2; ++i)
	{
		if (wallupper[i] < most[i])
			wallupper[i] = most[i];
	}
	WallMost(most, curline->frontsector->floorplane, &WallC);
	for (int i = x1; i < x2; ++i)
	{
		if (walllower[i] > most[i])
			walllower[i] = most[i];
	}
}

void R_RenderFakeWallRange(drawseg_t *ds, int x1, int x2);
//VIZDOOM_CODE
void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2)
{
	FTexture	*tex;
	int			i;
	sector_t	tempsec;		// killough 4/13/98
	fixed_t		texheight, textop, texheightscale;
	bool		notrelevant = false;
	fixed_t		rowoffset;

	const sector_t *sec;

	sprflipvert = false;

	curline = ds->curline;

	// killough 4/11/98: draw translucent 2s normal textures
	// [RH] modified because we don't use user-definable translucency maps
	ESPSResult drawmode;

	drawmode = R_SetPatchStyle (LegacyRenderStyles[curline->linedef->flags & ML_ADDTRANS ? STYLE_Add : STYLE_Translucent],
		MIN(curline->linedef->Alpha, FRACUNIT),	0, 0);

	if ((drawmode == DontDraw && !ds->bFogBoundary && !ds->bFakeBoundary))
	{
		return;
	}

	NetUpdate ();

	frontsector = curline->frontsector;
	backsector = curline->backsector;

	tex = TexMan(curline->sidedef->GetTexture(side_t::mid), true);
	if (i_compatflags & COMPATF_MASKEDMIDTEX)
	{
		tex = tex->GetRawTexture();
	}

	// killough 4/13/98: get correct lightlevel for 2s normal textures
	sec = R_FakeFlat (frontsector, &tempsec, NULL, NULL, false);

	basecolormap = sec->ColorMap;	// [RH] Set basecolormap

	wallshade = ds->shade;
	rw_lightstep = ds->lightstep;
	rw_light = ds->light + (x1 - ds->x1) * rw_lightstep;

	if (fixedlightlev < 0)
	{
		for (i = frontsector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
		{
			if (!(fake3D & FAKE3D_CLIPTOP))
			{
				sclipTop = sec->ceilingplane.ZatPoint(viewx, viewy);
			}
			if (sclipTop <= frontsector->e->XFloor.lightlist[i].plane.ZatPoint(viewx, viewy))
			{
				lightlist_t *lit = &frontsector->e->XFloor.lightlist[i];
				basecolormap = lit->extra_colormap;
				wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource == NULL) + r_actualextralight);
				break;
			}
		}
	}

	mfloorclip = openings + ds->sprbottomclip - ds->x1;
	mceilingclip = openings + ds->sprtopclip - ds->x1;

	// [RH] Draw fog partition
	if (ds->bFogBoundary)
	{
		R_DrawFogBoundary (x1, x2, mceilingclip, mfloorclip);
		if (ds->maskedtexturecol == -1)
		{
			goto clearfog;
		}
	}
	if ((ds->bFakeBoundary && !(ds->bFakeBoundary & 4)) || drawmode == DontDraw)
	{
		goto clearfog;
	}

	MaskedSWall = (fixed_t *)(openings + ds->swall) - ds->x1;
	MaskedScaleY = ds->yrepeat;
	maskedtexturecol = (fixed_t *)(openings + ds->maskedtexturecol) - ds->x1;
	spryscale = ds->iscale + ds->iscalestep * (x1 - ds->x1);
	rw_scalestep = ds->iscalestep;

	if (fixedlightlev >= 0)
		dc_colormap = basecolormap->Maps + fixedlightlev;
	else if (fixedcolormap != NULL)
		dc_colormap = fixedcolormap;

	// find positioning
	texheight = tex->GetScaledHeight() << FRACBITS;
	texheightscale = abs(curline->sidedef->GetTextureYScale(side_t::mid));
	if (texheightscale != FRACUNIT)
	{
		texheight = FixedDiv(texheight, texheightscale);
	}
	if (curline->linedef->flags & ML_DONTPEGBOTTOM)
	{
		dc_texturemid = MAX (frontsector->GetPlaneTexZ(sector_t::floor), backsector->GetPlaneTexZ(sector_t::floor)) + texheight;
	}
	else
	{
		dc_texturemid = MIN (frontsector->GetPlaneTexZ(sector_t::ceiling), backsector->GetPlaneTexZ(sector_t::ceiling));
	}

	rowoffset = curline->sidedef->GetTextureYOffset(side_t::mid);

	if (!(curline->linedef->flags & ML_WRAP_MIDTEX) &&
		!(curline->sidedef->Flags & WALLF_WRAP_MIDTEX))
	{ // Texture does not wrap vertically.
		fixed_t textop;

		if (MaskedScaleY < 0)
		{
			MaskedScaleY = -MaskedScaleY;
			sprflipvert = true;
		}
		if (tex->bWorldPanning)
		{
			// rowoffset is added before the MulScale3 so that the masked texture will
			// still be positioned in world units rather than texels.
			dc_texturemid += rowoffset - viewz;
			textop = dc_texturemid;
			dc_texturemid = MulScale16 (dc_texturemid, MaskedScaleY);
		}
		else
		{
			// rowoffset is added outside the multiply so that it positions the texture
			// by texels instead of world units.
			textop = dc_texturemid - viewz + SafeDivScale16 (rowoffset, MaskedScaleY);
			dc_texturemid = MulScale16 (dc_texturemid - viewz, MaskedScaleY) + rowoffset;
		}
		if (sprflipvert)
		{
			MaskedScaleY = -MaskedScaleY;
			dc_texturemid -= tex->GetHeight() << FRACBITS;
		}

		// [RH] Don't bother drawing segs that are completely offscreen
		if (MulScale12 (globaldclip, ds->sz1) < -textop &&
			MulScale12 (globaldclip, ds->sz2) < -textop)
		{ // Texture top is below the bottom of the screen
			goto clearfog;
		}

		if (MulScale12 (globaluclip, ds->sz1) > texheight - textop &&
			MulScale12 (globaluclip, ds->sz2) > texheight - textop)
		{ // Texture bottom is above the top of the screen
			goto clearfog;
		}

		if ((fake3D & FAKE3D_CLIPBOTTOM) && textop < sclipBottom - viewz)
		{
			notrelevant = true;
			goto clearfog;
		}
		if ((fake3D & FAKE3D_CLIPTOP) && textop - texheight > sclipTop - viewz)
		{
			notrelevant = true;
			goto clearfog;
		}

		WallC.sz1 = ds->sz1;
		WallC.sz2 = ds->sz2;
		WallC.sx1 = ds->sx1;
		WallC.sx2 = ds->sx2;

		if (fake3D & FAKE3D_CLIPTOP)
		{
			OWallMost(wallupper, textop < sclipTop - viewz ? textop : sclipTop - viewz, &WallC);
		}
		else
		{
			OWallMost(wallupper, textop, &WallC);
		}
		if (fake3D & FAKE3D_CLIPBOTTOM)
		{
			OWallMost(walllower, textop - texheight > sclipBottom - viewz ? textop - texheight : sclipBottom - viewz, &WallC);
		}
		else
		{
			OWallMost(walllower, textop - texheight, &WallC);
		}

		for (i = x1; i < x2; i++)
		{
			if (wallupper[i] < mceilingclip[i])
				wallupper[i] = mceilingclip[i];
		}
		for (i = x1; i < x2; i++)
		{
			if (walllower[i] > mfloorclip[i])
				walllower[i] = mfloorclip[i];
		}

		if (CurrentSkybox)
		{ // Midtex clipping doesn't work properly with skyboxes, since you're normally below the floor
		  // or above the ceiling, so the appropriate end won't be clipped automatically when adding
		  // this drawseg.
			if ((curline->linedef->flags & ML_CLIP_MIDTEX) ||
				(curline->sidedef->Flags & WALLF_CLIP_MIDTEX))
			{
				ClipMidtex(x1, x2);
			}
		}

		mfloorclip = walllower;
		mceilingclip = wallupper;

		// draw the columns one at a time
		if (drawmode == DoDraw0)
		{
			for (dc_x = x1; dc_x < x2; ++dc_x)
			{
				BlastMaskedColumn (R_DrawMaskedColumn, tex);
			}
		}
		else
		{
			// [RH] Draw up to four columns at once
			int stop = x2 & ~3;

			if (x1 >= x2)
				goto clearfog;

			dc_x = x1;

			while ((dc_x < stop) && (dc_x & 3))
			{
				if(vizDepthMap!=NULL) {
					vizDepthMap->setActualDepth((unsigned int)  ((MaskedSWall[dc_x] - 7500) * 255) / (1250000 - 7500));
					if (MaskedSWall[dc_x] > 1250000)
						vizDepthMap->setActualDepth(255);
					if (MaskedSWall[dc_x] < 7500)
						vizDepthMap->setActualDepth(0);
				}
				BlastMaskedColumn (R_DrawMaskedColumn, tex);
				dc_x++;
			}

			while (dc_x < stop)
			{
				for(int pcf=0;pcf<4;pcf++) {
					if(vizDepthMap!=NULL) {
						vizDepthMap->helperBuffer[pcf]=(unsigned int)  ((MaskedSWall[dc_x+pcf] - 7500) * 255) / (1250000 - 7500);
						if (MaskedSWall[dc_x+pcf] > 1250000)
							vizDepthMap->helperBuffer[pcf]=(255);
						if (MaskedSWall[dc_x+pcf] < 7500)
							vizDepthMap->helperBuffer[pcf]=(0);
					}
					/*static long max, min;
					if (min == 0) min = max;
					if (MaskedSWall[dc_x] > max || MaskedSWall[dc_x] < min) {
						if (MaskedSWall[dc_x] > max)
							max = MaskedSWall[dc_x];
						else
							min = MaskedSWall[dc_x];
						printf("MAX: %ld MIN: %ld\n", max, min);
					}*/
				}
				rt_initcols();
				BlastMaskedColumn (R_DrawMaskedColumnHoriz, tex); dc_x++;
				BlastMaskedColumn (R_DrawMaskedColumnHoriz, tex); dc_x++;
				BlastMaskedColumn (R_DrawMaskedColumnHoriz, tex); dc_x++;
				BlastMaskedColumn (R_DrawMaskedColumnHoriz, tex);
				rt_draw4cols (dc_x - 3);
				dc_x++;
			}

			while (dc_x < x2)
			{
				if(vizDepthMap!=NULL) {
					vizDepthMap->setActualDepth((unsigned int)  ((MaskedSWall[dc_x] - 7500) * 255) / (1250000 - 7500));
					if (MaskedSWall[dc_x] > 1250000)
						vizDepthMap->setActualDepth(255);
					if (MaskedSWall[dc_x] < 7500)
						vizDepthMap->setActualDepth(0);
				}
				BlastMaskedColumn (R_DrawMaskedColumn, tex);
				dc_x++;
			}
		}
	}
	else
	{ // Texture does wrap vertically.
		if (tex->bWorldPanning)
		{
			// rowoffset is added before the MulScale3 so that the masked texture will
			// still be positioned in world units rather than texels.
			dc_texturemid += rowoffset - viewz;
			dc_texturemid = MulScale16 (dc_texturemid, MaskedScaleY);
		}
		else
		{
			// rowoffset is added outside the multiply so that it positions the texture
			// by texels instead of world units.
			dc_texturemid = MulScale16 (dc_texturemid - viewz, MaskedScaleY) + rowoffset;
		}

		WallC.sz1 = ds->sz1;
		WallC.sz2 = ds->sz2;
		WallC.sx1 = ds->sx1;
		WallC.sx2 = ds->sx2;

		if (CurrentSkybox)
		{ // Midtex clipping doesn't work properly with skyboxes, since you're normally below the floor
		  // or above the ceiling, so the appropriate end won't be clipped automatically when adding
		  // this drawseg.
			if ((curline->linedef->flags & ML_CLIP_MIDTEX) ||
				(curline->sidedef->Flags & WALLF_CLIP_MIDTEX))
			{
				ClipMidtex(x1, x2);
			}
		}

		if (fake3D & FAKE3D_CLIPTOP)
		{
			OWallMost(wallupper, sclipTop - viewz, &WallC);
			for (i = x1; i < x2; i++)
			{
				if (wallupper[i] < mceilingclip[i])
					wallupper[i] = mceilingclip[i];
			}
			mceilingclip = wallupper;
		}			
		if (fake3D & FAKE3D_CLIPBOTTOM)
		{
			OWallMost(walllower, sclipBottom - viewz, &WallC);
			for (i = x1; i < x2; i++)
			{
				if (walllower[i] > mfloorclip[i])
					walllower[i] = mfloorclip[i];
			}
			mfloorclip = walllower;
		}

		rw_offset = 0;
		rw_pic = tex;
		wallscan_np2_ds(ds, x1, x2, mceilingclip, mfloorclip, MaskedSWall, maskedtexturecol, ds->yrepeat);
	}

clearfog:
	R_FinishSetPatchStyle ();
	if (ds->bFakeBoundary & 3)
	{
		R_RenderFakeWallRange(ds, x1, x2);
	}
	if (!notrelevant)
	{
		if (fake3D & FAKE3D_REFRESHCLIP)
		{
			assert(ds->bkup >= 0);
			memcpy(openings + ds->sprtopclip, openings + ds->bkup, (ds->x2 - ds->x1) * 2);
		}
		else
		{
			clearbufshort(openings + ds->sprtopclip - ds->x1 + x1, x2 - x1, viewheight);
		}
	}
	return;
}

// kg3D - render one fake wall
void R_RenderFakeWall(drawseg_t *ds, int x1, int x2, F3DFloor *rover)
{
	int i;
	fixed_t xscale, yscale;

	fixed_t Alpha = Scale(rover->alpha, OPAQUE, 255);
	ESPSResult drawmode;
	drawmode = R_SetPatchStyle (LegacyRenderStyles[rover->flags & FF_ADDITIVETRANS ? STYLE_Add : STYLE_Translucent],
		Alpha, 0, 0);

	if(drawmode == DontDraw) {
		R_FinishSetPatchStyle();
		return;
	}

	rw_lightstep = ds->lightstep;
	rw_light = ds->light + (x1 - ds->x1) * rw_lightstep;

	mfloorclip = openings + ds->sprbottomclip - ds->x1;
	mceilingclip = openings + ds->sprtopclip - ds->x1;

	spryscale = ds->iscale + ds->iscalestep * (x1 - ds->x1);
	rw_scalestep = ds->iscalestep;
	MaskedSWall = (fixed_t *)(openings + ds->swall) - ds->x1;

	// find positioning
	side_t *scaledside;
	side_t::ETexpart scaledpart;
	if (rover->flags & FF_UPPERTEXTURE)
	{
		scaledside = curline->sidedef;
		scaledpart = side_t::top;
	}
	else if (rover->flags & FF_LOWERTEXTURE)
	{
		scaledside = curline->sidedef;
		scaledpart = side_t::bottom;
	}
	else
	{
		scaledside = rover->master->sidedef[0];
		scaledpart = side_t::mid;
	}
	xscale = FixedMul(rw_pic->xScale, scaledside->GetTextureXScale(scaledpart));
	yscale = FixedMul(rw_pic->yScale, scaledside->GetTextureYScale(scaledpart));
	// encapsulate the lifetime of rowoffset
	fixed_t rowoffset = curline->sidedef->GetTextureYOffset(side_t::mid) + rover->master->sidedef[0]->GetTextureYOffset(side_t::mid);
	dc_texturemid = rover->model->GetPlaneTexZ(sector_t::ceiling);
	rw_offset = curline->sidedef->GetTextureXOffset(side_t::mid) + rover->master->sidedef[0]->GetTextureXOffset(side_t::mid);
	if (rowoffset < 0)
	{
		rowoffset += rw_pic->GetHeight() << FRACBITS;
	}
	if (rw_pic->bWorldPanning)
	{
		// rowoffset is added before the MulScale3 so that the masked texture will
		// still be positioned in world units rather than texels.

		dc_texturemid = MulScale16(dc_texturemid - viewz + rowoffset, yscale);
		rw_offset = MulScale16 (rw_offset, xscale);
	}
	else
	{
		// rowoffset is added outside the multiply so that it positions the texture
		// by texels instead of world units.
		dc_texturemid = MulScale16(dc_texturemid - viewz, yscale) + rowoffset;
	}

	if (fixedlightlev >= 0)
		dc_colormap = basecolormap->Maps + fixedlightlev;
	else if (fixedcolormap != NULL)
		dc_colormap = fixedcolormap;

	WallC.sz1 = ds->sz1;
	WallC.sz2 = ds->sz2;
	WallC.sx1 = ds->sx1;
	WallC.sx2 = ds->sx2;
	WallC.tx1 = ds->cx;
	WallC.ty1 = ds->cy;
	WallC.tx2 = ds->cx + ds->cdx;
	WallC.ty2 = ds->cy + ds->cdy;
	WallT = ds->tmapvals;

	OWallMost(wallupper, sclipTop - viewz, &WallC);
	OWallMost(walllower, sclipBottom - viewz, &WallC);

	for (i = x1; i < x2; i++)
	{
		if (wallupper[i] < mceilingclip[i])
			wallupper[i] = mceilingclip[i];
	}
	for (i = x1; i < x2; i++)
	{
		if (walllower[i] > mfloorclip[i])
			walllower[i] = mfloorclip[i];
	}

	PrepLWall (lwall, curline->sidedef->TexelLength*xscale, ds->sx1, ds->sx2);
	wallscan_np2_ds(ds, x1, x2, wallupper, walllower, MaskedSWall, lwall, yscale);
	R_FinishSetPatchStyle();
}

// kg3D - walls of fake floors
void R_RenderFakeWallRange (drawseg_t *ds, int x1, int x2)
{
	FTexture *const DONT_DRAW = ((FTexture*)(intptr_t)-1);
	int i,j;
	F3DFloor *rover, *fover = NULL;
	int passed, last;
	fixed_t floorheight;
	fixed_t ceilingheight;

	sprflipvert = false;
	curline = ds->curline;

	frontsector = curline->frontsector;
	backsector = curline->backsector;

	if (backsector == NULL)
	{
		return;
	}
	if ((ds->bFakeBoundary & 3) == 2)
	{
		sector_t *sec = backsector;
		backsector = frontsector;
		frontsector = sec;
	}

	floorheight = backsector->CenterFloor();
	ceilingheight = backsector->CenterCeiling();

	// maybe fix clipheights
	if (!(fake3D & FAKE3D_CLIPBOTTOM)) sclipBottom = floorheight;
	if (!(fake3D & FAKE3D_CLIPTOP))    sclipTop = ceilingheight;

	// maybe not visible
	if (sclipBottom >= frontsector->CenterCeiling()) return;
	if (sclipTop <= frontsector->CenterFloor()) return;

	if (fake3D & FAKE3D_DOWN2UP)
	{ // bottom to viewz
		last = 0;
		for (i = backsector->e->XFloor.ffloors.Size() - 1; i >= 0; i--) 
		{
			rover = backsector->e->XFloor.ffloors[i];
			if (!(rover->flags & FF_EXISTS)) continue;

			// visible?
			passed = 0;
			if (!(rover->flags & FF_RENDERSIDES) ||
				rover->top.plane->a || rover->top.plane->b ||
				rover->bottom.plane->a || rover->bottom.plane->b ||
				rover->top.plane->Zat0() <= sclipBottom ||
				rover->bottom.plane->Zat0() >= ceilingheight ||
				rover->top.plane->Zat0() <= floorheight)
			{
				if (!i)
				{
					passed = 1;
				}
				else
				{
					continue;
				}
			}

			rw_pic = NULL;
			if (rover->bottom.plane->Zat0() >= sclipTop || passed) 
			{
				if (last)
				{
					break;
				}
				// maybe wall from inside rendering?
				fover = NULL;
				for (j = frontsector->e->XFloor.ffloors.Size() - 1; j >= 0; j--)
				{
					fover = frontsector->e->XFloor.ffloors[j];
					if (fover->model == rover->model)
					{ // never
						fover = NULL;
						break;
					}
					if (!(fover->flags & FF_EXISTS)) continue;
					if (!(fover->flags & FF_RENDERSIDES)) continue;
					// no sloped walls, it's bugged
					if (fover->top.plane->a || fover->top.plane->b || fover->bottom.plane->a || fover->bottom.plane->b) continue;

					// visible?
					if (fover->top.plane->Zat0() <= sclipBottom) continue; // no
					if (fover->bottom.plane->Zat0() >= sclipTop)
					{ // no, last possible
 						fover = NULL;
						break;
					}
					// it is, render inside?
					if (!(fover->flags & (FF_BOTHPLANES|FF_INVERTPLANES)))
					{ // no
						fover = NULL;
					}
					break;
				}
				// nothing
				if (!fover || j == -1)
				{
					break;
				}
				// correct texture
				if (fover->flags & rover->flags & FF_SWIMMABLE)
				{	// don't ever draw (but treat as something has been found)
					rw_pic = DONT_DRAW;
				}
				else if(fover->flags & FF_UPPERTEXTURE)
				{
					rw_pic = TexMan(curline->sidedef->GetTexture(side_t::top), true);
				}
				else if(fover->flags & FF_LOWERTEXTURE)
				{
					rw_pic = TexMan(curline->sidedef->GetTexture(side_t::bottom), true);
				}
				else
				{
					rw_pic = TexMan(fover->master->sidedef[0]->GetTexture(side_t::mid), true);
				}
			} 
			else if (frontsector->e->XFloor.ffloors.Size()) 
			{
				// maybe not visible?
				fover = NULL;
				for (j = frontsector->e->XFloor.ffloors.Size() - 1; j >= 0; j--)
				{
					fover = frontsector->e->XFloor.ffloors[j];
					if (fover->model == rover->model) // never
					{
						break;
					}
					if (!(fover->flags & FF_EXISTS)) continue;
					if (!(fover->flags & FF_RENDERSIDES)) continue;
					// no sloped walls, it's bugged
					if (fover->top.plane->a || fover->top.plane->b || fover->bottom.plane->a || fover->bottom.plane->b) continue;

					// visible?
					if (fover->top.plane->Zat0() <= sclipBottom) continue; // no
					if (fover->bottom.plane->Zat0() >= sclipTop)
					{ // visible, last possible
 						fover = NULL;
						break;
					}
					if ((fover->flags & FF_SOLID) == (rover->flags & FF_SOLID) &&
						!(!(fover->flags & FF_SOLID) && (fover->alpha == 255 || rover->alpha == 255))
					)
					{
						break;
					}
					if (fover->flags & rover->flags & FF_SWIMMABLE)
					{ // don't ever draw (but treat as something has been found)
						rw_pic = DONT_DRAW;
					}
					fover = NULL; // visible
					break;
				}
				if (fover && j != -1)
				{
					fover = NULL;
					last = 1;
					continue; // not visible
				}
			}
			if (!rw_pic) 
			{
				fover = NULL;
				if (rover->flags & FF_UPPERTEXTURE)
				{
					rw_pic = TexMan(curline->sidedef->GetTexture(side_t::top), true);
				}
				else if(rover->flags & FF_LOWERTEXTURE)
				{
					rw_pic = TexMan(curline->sidedef->GetTexture(side_t::bottom), true);
				}
				else
				{
					rw_pic = TexMan(rover->master->sidedef[0]->GetTexture(side_t::mid), true);
				}
			}
			// correct colors now
			basecolormap = frontsector->ColorMap;
			wallshade = ds->shade;
			if (fixedlightlev < 0)
			{
				if ((ds->bFakeBoundary & 3) == 2)
				{
					for (j = backsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
					{
						if (sclipTop <= backsector->e->XFloor.lightlist[j].plane.Zat0())
						{
							lightlist_t *lit = &backsector->e->XFloor.lightlist[j];
							basecolormap = lit->extra_colormap;
							wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != NULL) + r_actualextralight);
							break;
						}
					}
				}
				else
				{
					for (j = frontsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
					{
						if (sclipTop <= frontsector->e->XFloor.lightlist[j].plane.Zat0())
						{
							lightlist_t *lit = &frontsector->e->XFloor.lightlist[j];
							basecolormap = lit->extra_colormap;
							wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != NULL) + r_actualextralight);
							break;
						}
					}
				}
			}
			if (rw_pic != DONT_DRAW)
			{
				R_RenderFakeWall(ds, x1, x2, fover ? fover : rover);
			}
			else rw_pic = NULL;
			break;
		}
	}
	else
	{ // top to viewz
		for (i = 0; i < (int)backsector->e->XFloor.ffloors.Size(); i++)
		{
			rover = backsector->e->XFloor.ffloors[i];
			if (!(rover->flags & FF_EXISTS)) continue;

			// visible?
			passed = 0;
			if (!(rover->flags & FF_RENDERSIDES) ||
				rover->top.plane->a || rover->top.plane->b ||
				rover->bottom.plane->a || rover->bottom.plane->b ||
				rover->bottom.plane->Zat0() >= sclipTop ||
				rover->top.plane->Zat0() <= floorheight ||
				rover->bottom.plane->Zat0() >= ceilingheight)
			{
				if ((unsigned)i == backsector->e->XFloor.ffloors.Size() - 1)
				{
					passed = 1;
				}
				else
				{
					continue;
				}
			}
			rw_pic = NULL;
			if (rover->top.plane->Zat0() <= sclipBottom || passed)
			{ // maybe wall from inside rendering?
				fover = NULL;
				for (j = 0; j < (int)frontsector->e->XFloor.ffloors.Size(); j++)
				{
					fover = frontsector->e->XFloor.ffloors[j];
					if (fover->model == rover->model)
					{ // never
						fover = NULL;
						break;
					}
					if (!(fover->flags & FF_EXISTS)) continue;
					if (!(fover->flags & FF_RENDERSIDES)) continue;
					// no sloped walls, it's bugged
					if (fover->top.plane->a || fover->top.plane->b || fover->bottom.plane->a || fover->bottom.plane->b) continue;

					// visible?
					if (fover->bottom.plane->Zat0() >= sclipTop) continue; // no
					if (fover->top.plane->Zat0() <= sclipBottom)
					{ // no, last possible
 						fover = NULL;
						break;
					}
					// it is, render inside?
					if (!(fover->flags & (FF_BOTHPLANES|FF_INVERTPLANES)))
					{ // no
						fover = NULL;
					}
					break;
				}
				// nothing
				if (!fover || (unsigned)j == frontsector->e->XFloor.ffloors.Size())
				{
					break;
				}
				// correct texture
				if (fover->flags & rover->flags & FF_SWIMMABLE)
				{
					rw_pic = DONT_DRAW;	// don't ever draw (but treat as something has been found)
				}
				else if (fover->flags & FF_UPPERTEXTURE)
				{
					rw_pic = TexMan(curline->sidedef->GetTexture(side_t::top), true);
				}
				else if (fover->flags & FF_LOWERTEXTURE)
				{
					rw_pic = TexMan(curline->sidedef->GetTexture(side_t::bottom), true);
				}
				else
				{
					rw_pic = TexMan(fover->master->sidedef[0]->GetTexture(side_t::mid), true);
				}
			}
			else if (frontsector->e->XFloor.ffloors.Size())
			{ // maybe not visible?
				fover = NULL;
				for (j = 0; j < (int)frontsector->e->XFloor.ffloors.Size(); j++)
				{
					fover = frontsector->e->XFloor.ffloors[j];
					if (fover->model == rover->model)
					{ // never
						break;
					}
					if (!(fover->flags & FF_EXISTS)) continue;
					if (!(fover->flags & FF_RENDERSIDES)) continue;
					// no sloped walls, its bugged
					if(fover->top.plane->a || fover->top.plane->b || fover->bottom.plane->a || fover->bottom.plane->b) continue;

					// visible?
					if (fover->bottom.plane->Zat0() >= sclipTop) continue; // no
					if (fover->top.plane->Zat0() <= sclipBottom)
					{ // visible, last possible
 						fover = NULL;
						break;
					}
					if ((fover->flags & FF_SOLID) == (rover->flags & FF_SOLID) &&
						!(!(rover->flags & FF_SOLID) && (fover->alpha == 255 || rover->alpha == 255))
					)
					{
						break;
					}
					if (fover->flags & rover->flags & FF_SWIMMABLE)
					{ // don't ever draw (but treat as something has been found)
						rw_pic = DONT_DRAW;
					}
					fover = NULL; // visible
					break;
				}
				if (fover && (unsigned)j != frontsector->e->XFloor.ffloors.Size())
				{ // not visible
					break;
				}
			}
			if (rw_pic == NULL)
			{
				fover = NULL;
				if (rover->flags & FF_UPPERTEXTURE)
				{
					rw_pic = TexMan(curline->sidedef->GetTexture(side_t::top), true);
				}
				else if (rover->flags & FF_LOWERTEXTURE)
				{
					rw_pic = TexMan(curline->sidedef->GetTexture(side_t::bottom), true);
				}
				else
				{
					rw_pic = TexMan(rover->master->sidedef[0]->GetTexture(side_t::mid), true);
				}
			}
			// correct colors now
			basecolormap = frontsector->ColorMap;
			wallshade = ds->shade;
			if (fixedlightlev < 0)
			{
				if ((ds->bFakeBoundary & 3) == 2)
				{
					for (j = backsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
					{
						if (sclipTop <= backsector->e->XFloor.lightlist[j].plane.Zat0())
						{
							lightlist_t *lit = &backsector->e->XFloor.lightlist[j];
							basecolormap = lit->extra_colormap;
							wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != NULL) + r_actualextralight);
							break;
						}
					}
				}
				else
				{
					for (j = frontsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
					{
						if(sclipTop <= frontsector->e->XFloor.lightlist[j].plane.Zat0())
						{
							lightlist_t *lit = &frontsector->e->XFloor.lightlist[j];
							basecolormap = lit->extra_colormap;
							wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != NULL) + r_actualextralight);
							break;
						}
					}
				}
			}

			if (rw_pic != DONT_DRAW)
			{
				R_RenderFakeWall(ds, x1, x2, fover ? fover : rover);
			}
			else
			{
				rw_pic = NULL;
			}
			break;
		}
	}
	return;
}

// prevlineasm1 is like vlineasm1 but skips the loop if only drawing one pixel
inline fixed_t prevline1 (fixed_t vince, BYTE *colormap, int count, fixed_t vplce, const BYTE *bufplce, BYTE *dest)
{
	dc_iscale = vince;
	dc_colormap = colormap;
	dc_count = count;
	dc_texturefrac = vplce;
	dc_source = bufplce;
	dc_dest = dest;
	return doprevline1 ();
}
//VIZDOOM_CODE
void wallscan (int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal,
			   fixed_t yrepeat, const BYTE *(*getcol)(FTexture *tex, int x))
{
	int x, shiftval;
	int y1ve[4], y2ve[4], u4, d4, z;
	char bad;
	fixed_t light = rw_light - rw_lightstep;
	SDWORD texturemid, xoffset;
	BYTE *basecolormapdata;

	// This function also gets used to draw skies. Unlike BUILD, skies are
	// drawn by visplane instead of by bunch, so these checks are invalid.
	//if ((uwal[x1] > viewheight) && (uwal[x2] > viewheight)) return;
	//if ((dwal[x1] < 0) && (dwal[x2] < 0)) return;

	if (rw_pic->UseType == FTexture::TEX_Null)
	{
		return;
	}

//extern cycle_t WallScanCycles;
//clock (WallScanCycles);

	rw_pic->GetHeight();	// Make sure texture size is loaded
	shiftval = rw_pic->HeightBits;
	setupvline (32-shiftval);
	yrepeat >>= 2 + shiftval;
	texturemid = dc_texturemid << (16 - shiftval);
	xoffset = rw_offset;
	basecolormapdata = basecolormap->Maps;

	x = x1;
	//while ((umost[x] > dmost[x]) && (x < x2)) x++;

	bool fixed = (fixedcolormap != NULL || fixedlightlev >= 0);
	if (fixed)
	{
		palookupoffse[0] = dc_colormap;
		palookupoffse[1] = dc_colormap;
		palookupoffse[2] = dc_colormap;
		palookupoffse[3] = dc_colormap;
	}

	for(; (x < x2) && (x & 3); ++x)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];//max(uwal[x],umost[x]);
		y2ve[0] = dwal[x];//min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;
		assert (y1ve[0] < viewheight);
		assert (y2ve[0] <= viewheight);

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + x + dc_destorg;
		dc_iscale = swal[x] * yrepeat;
		int a_dc_iscale = dc_iscale<0 ? -dc_iscale : dc_iscale;
		if(yrepeat==1024) a_dc_iscale/=8;
		if(yrepeat==512) a_dc_iscale/=4;
		if(yrepeat==256) a_dc_iscale/=2;
		//if(yrepeat!=256) printf("YREPEAT: %d\n", yrepeat);
		if(vizDepthMap!=NULL) {
			vizDepthMap->setActualDepth((unsigned int)  ((a_dc_iscale / 100 - 9500) * 255) / (1600000 - 9500));
			if (a_dc_iscale > 160000000)
				vizDepthMap->setActualDepth(255);
			if (a_dc_iscale < 950000)
				vizDepthMap->setActualDepth(0);
		}
		/*static long long max, min;
		if(min==0) min=max;
		if(a_dc_iscale>max||a_dc_iscale<min)
		{
			if(a_dc_iscale>max)
				max=a_dc_iscale;
			else
				min=a_dc_iscale;
			printf("MAX: %ld MIN: %ld ACT: %d\n", max, min, 255 - ((a_dc_iscale / 100 - 10000) * 255) / (13000000 - 10000));
		}*/
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);
		if(vizDepthMap!=NULL) {
			vizDepthMap->storeX(x);
			vizDepthMap->storeY(y1ve[0]);
		}
		dovline1();
	}

	for(; x < x2-3; x += 4)
	{
		bad = 0;
		for (z = 3; z>= 0; --z)
		{
			y1ve[z] = uwal[x+z];//max(uwal[x+z],umost[x+z]);
			y2ve[z] = dwal[x+z];//min(dwal[x+z],dmost[x+z])-1;
			if (y2ve[z] <= y1ve[z]) { bad += 1<<z; continue; }
			assert (y1ve[z] < viewheight);
			assert (y2ve[z] <= viewheight);

			bufplce[z] = getcol (rw_pic, (lwal[x+z] + xoffset) >> FRACBITS);
			vince[z] = swal[x+z] * yrepeat;
			if(vizDepthMap!=NULL) {
				long tmp = swal[x + z]*yrepeat;
				if(yrepeat==1024)
					tmp/=8;
				if(yrepeat==512)
					tmp/=4;
				if(yrepeat==256)
					tmp/=2;
				vizDepthMap->helperBuffer[z] = tmp<0 ? (unsigned int)-tmp : (unsigned int)tmp;
			}
			vplce[z] = texturemid + FixedMul (vince[z], (y1ve[z]<<FRACBITS)-centeryfrac+FRACUNIT);
		}
		if (bad == 15)
		{
			light += rw_lightstep << 2;
			continue;
		}

		if (!fixed)
		{
			for (z = 0; z < 4; ++z)
			{
				light += rw_lightstep;
				palookupoffse[z] = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
			}
		}

		u4 = MAX(MAX(y1ve[0],y1ve[1]),MAX(y1ve[2],y1ve[3]));
		d4 = MIN(MIN(y2ve[0],y2ve[1]),MIN(y2ve[2],y2ve[3]));

		if ((bad != 0) || (u4 >= d4))
		{
			for (z = 0; z < 4; ++z)
			{
				if (!(bad & 1))
				{
					if(vizDepthMap!=NULL) {
						vizDepthMap->storeX(x + z);
						vizDepthMap->storeY(y1ve[z]);
						unsigned int a_dc_iscale = vizDepthMap->helperBuffer[z];
						vizDepthMap->setActualDepth((unsigned int)  ((a_dc_iscale / 100 - 9500) * 255) / (1600000 - 9500));
						if (a_dc_iscale > 160000000)
							vizDepthMap->setActualDepth(255);
						if (a_dc_iscale < 950000)
							vizDepthMap->setActualDepth(0);
					}
					prevline1(vince[z],palookupoffse[z],y2ve[z]-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+x+z+dc_destorg);
				}
				bad >>= 1;
			}
			continue;
		}

		for (z = 0; z < 4; ++z)
		{
			if (u4 > y1ve[z])
			{
				if(vizDepthMap!=NULL) {
					vizDepthMap->storeX(x + z);
					vizDepthMap->storeY(y1ve[z]);
				}

				if(vizDepthMap!=NULL) {
					unsigned int a_dc_iscale = vizDepthMap->helperBuffer[z];
					vizDepthMap->setActualDepth((unsigned int)  ((a_dc_iscale / 100 - 9500) * 255) / (1600000 - 9500));
					if (a_dc_iscale > 160000000)
						vizDepthMap->setActualDepth(255);
					if (a_dc_iscale < 950000)
						vizDepthMap->setActualDepth(0);
				}
				vplce[z] = prevline1(vince[z],palookupoffse[z],u4-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+x+z+dc_destorg);
			}
		}

		if (d4 > u4)
		{
			dc_count = d4-u4;
			dc_dest = ylookup[u4]+x+dc_destorg;

			if(vizDepthMap!=NULL) {
				for (unsigned int pcf = 0; pcf < 4; pcf++) {
					unsigned int a_dc_iscale = vizDepthMap->helperBuffer[pcf];//vince[pcf]<0 ? -vince[pcf] : vince[pcf];
					vizDepthMap->setActualDepth((unsigned int)  ((a_dc_iscale / 100 - 9500) * 255) / (1600000 - 9500));
					if (a_dc_iscale > 160000000)
						vizDepthMap->setActualDepth(255);
					if (a_dc_iscale < 950000)
						vizDepthMap->setActualDepth(0);
					for (int c = 0; c <= dc_count; c++)
						vizDepthMap->setPoint(x + pcf, u4 + c);
					/*static long long max, min;
					if(min==0) min=max;
					if(a_dc_iscale>max||a_dc_iscale<min)
					{
						if(a_dc_iscale>max)
							max=a_dc_iscale;
						else
							min=a_dc_iscale;
						printf("MAX: %lld MIN: %lld ACT: %d H: %d W: %d\n", max, min, ((a_dc_iscale / 100 - 9500) * 255) / (1600000 - 9500), screen->GetHeight(), screen->GetWidth());
					}*/
				}
			}
            if(vizLabels!=NULL) {
                vizLabels->setLabel(0);
                for (unsigned int pcf = 0; pcf < 4; pcf++) {
                    for (int c = 0; c <= dc_count; c++)
                        vizLabels->setPoint(x + pcf, u4 + c);
                }
                vizLabels->setLabel(0);
            }
			dovline4();
		}

		BYTE *i = x+ylookup[d4]+dc_destorg;
		for (z = 0; z < 4; ++z)
		{
			if (y2ve[z] > d4)
			{
				if(vizDepthMap!=NULL) {
					vizDepthMap->storeX(x + z);
					vizDepthMap->storeY(d4);
				}

				if(vizDepthMap!=NULL) {
					unsigned int a_dc_iscale = vizDepthMap->helperBuffer[z];
					vizDepthMap->setActualDepth((unsigned int)  ((a_dc_iscale / 100 - 9500) * 255) / (1600000 - 9500));
					if (a_dc_iscale > 160000000)
						vizDepthMap->setActualDepth(255);
					if (a_dc_iscale < 950000)
						vizDepthMap->setActualDepth(0);
				}
				prevline1(vince[z],palookupoffse[0],y2ve[z]-d4,vplce[z],bufplce[z],i+z);
			}
		}
	}
	for(;x<x2;x++)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];//max(uwal[x],umost[x]);
		y2ve[0] = dwal[x];//min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;
		assert (y1ve[0] < viewheight);
		assert (y2ve[0] <= viewheight);

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + x + dc_destorg;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		if(vizDepthMap!=NULL) {
			vizDepthMap->storeX(x);
			vizDepthMap->storeY(y1ve[0]);
			unsigned int a_dc_iscale = dc_iscale<0 ? -dc_iscale : dc_iscale;
			if(yrepeat==1024)
				a_dc_iscale/=8;
			if(yrepeat==512)
				a_dc_iscale/=4;
			if(yrepeat==256)
				a_dc_iscale/=2;
			vizDepthMap->setActualDepth((unsigned int)  ((a_dc_iscale / 100 - 9500) * 255) / (1600000 - 9500));
			if (a_dc_iscale > 160000000)
				vizDepthMap->setActualDepth(255);
			if (a_dc_iscale < 950000)
				vizDepthMap->setActualDepth(0);
		}
		dovline1();
	}

//unclock (WallScanCycles);

	NetUpdate ();
}

void wallscan_striped (int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal, fixed_t yrepeat)
{
	FDynamicColormap *startcolormap = basecolormap;
	int startshade = wallshade;
	bool fogginess = foggy;

	short most1[MAXWIDTH], most2[MAXWIDTH], most3[MAXWIDTH];
	short *up, *down;

	up = uwal;
	down = most1;

	assert(WallC.sx1 <= x1);
	assert(WallC.sx2 >= x2);

	// kg3D - fake floors instead of zdoom light list
	for (unsigned int i = 0; i < frontsector->e->XFloor.lightlist.Size(); i++)
	{
		int j = WallMost (most3, frontsector->e->XFloor.lightlist[i].plane, &WallC);
		if (j != 3)
		{
			for (int j = x1; j < x2; ++j)
			{
				down[j] = clamp (most3[j], up[j], dwal[j]);
			}
			wallscan (x1, x2, up, down, swal, lwal, yrepeat);
			up = down;
			down = (down == most1) ? most2 : most1;
		}

		lightlist_t *lit = &frontsector->e->XFloor.lightlist[i];
		basecolormap = lit->extra_colormap;
		wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(fogginess,
			*lit->p_lightlevel, lit->lightsource != NULL) + r_actualextralight);
 	}

	wallscan (x1, x2, up, dwal, swal, lwal, yrepeat);
	basecolormap = startcolormap;
	wallshade = startshade;
}

static void call_wallscan(int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal, fixed_t yrepeat, bool mask)
{
	if (mask)
	{
		if (colfunc == basecolfunc)
		{
			maskwallscan(x1, x2, uwal, dwal, swal, lwal, yrepeat);
		}
		else
		{
			transmaskwallscan(x1, x2, uwal, dwal, swal, lwal, yrepeat);
		}
	}
	else
	{
		if (fixedcolormap != NULL || fixedlightlev >= 0 || !(frontsector->e && frontsector->e->XFloor.lightlist.Size()))
		{
			wallscan(x1, x2, uwal, dwal, swal, lwal, yrepeat);
		}
		else
		{
			wallscan_striped(x1, x2, uwal, dwal, swal, lwal, yrepeat);
		}
	}
}

//=============================================================================
//
// wallscan_np2
//
// This is a wrapper around wallscan that helps it tile textures whose heights
// are not powers of 2. It divides the wall into texture-sized strips and calls
// wallscan for each of those. Since only one repetition of the texture fits
// in each strip, wallscan will not tile.
//
//=============================================================================

void wallscan_np2(int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal, fixed_t yrepeat, fixed_t top, fixed_t bot, bool mask)
{
	if (!r_np2)
	{
		call_wallscan(x1, x2, uwal, dwal, swal, lwal, yrepeat, mask);
	}
	else
	{
		short most1[MAXWIDTH], most2[MAXWIDTH], most3[MAXWIDTH];
		short *up, *down;
		fixed_t texheight = rw_pic->GetHeight() << FRACBITS;
		fixed_t scaledtexheight = FixedDiv(texheight, yrepeat);
		fixed_t partition;

		if (yrepeat >= 0)
		{ // normal orientation: draw strips from top to bottom
			partition = top - (top - FixedDiv(dc_texturemid, yrepeat) - viewz) % scaledtexheight;
			up = uwal;
			down = most1;
			dc_texturemid = FixedMul(partition - viewz, yrepeat) + texheight;
			while (partition > bot)
			{
				int j = OWallMost(most3, partition - viewz, &WallC);
				if (j != 3)
				{
					for (int j = x1; j < x2; ++j)
					{
						down[j] = clamp(most3[j], up[j], dwal[j]);
					}
					call_wallscan(x1, x2, up, down, swal, lwal, yrepeat, mask);
					up = down;
					down = (down == most1) ? most2 : most1;
				}
				partition -= scaledtexheight;
				dc_texturemid -= texheight;
 			}
			call_wallscan(x1, x2, up, dwal, swal, lwal, yrepeat, mask);
		}
		else
		{ // upside down: draw strips from bottom to top
			partition = bot - (bot - FixedDiv(dc_texturemid, yrepeat) - viewz) % scaledtexheight;
			up = most1;
			down = dwal;
			dc_texturemid = FixedMul(partition - viewz, yrepeat) + texheight;
			while (partition < top)
			{
				int j = OWallMost(most3, partition - viewz, &WallC);
				if (j != 12)
				{
					for (int j = x1; j < x2; ++j)
					{
						up[j] = clamp(most3[j], uwal[j], down[j]);
					}
					call_wallscan(x1, x2, up, down, swal, lwal, yrepeat, mask);
					down = up;
					up = (up == most1) ? most2 : most1;
				}
				partition -= scaledtexheight;
				dc_texturemid -= texheight;
 			}
			call_wallscan(x1, x2, uwal, down, swal, lwal, yrepeat, mask);
		}
	}
}

static void wallscan_np2_ds(drawseg_t *ds, int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal, fixed_t yrepeat)
{
	if (rw_pic->GetHeight() != 1 << rw_pic->HeightBits)
	{
		fixed_t frontcz1 = ds->curline->frontsector->ceilingplane.ZatPoint(ds->curline->v1->x, ds->curline->v1->y);
		fixed_t frontfz1 = ds->curline->frontsector->floorplane.ZatPoint(ds->curline->v1->x, ds->curline->v1->y);
		fixed_t frontcz2 = ds->curline->frontsector->ceilingplane.ZatPoint(ds->curline->v2->x, ds->curline->v2->y);
		fixed_t frontfz2 = ds->curline->frontsector->floorplane.ZatPoint(ds->curline->v2->x, ds->curline->v2->y);
		fixed_t top = MAX(frontcz1, frontcz2);
		fixed_t bot = MIN(frontfz1, frontfz2);
		if (fake3D & FAKE3D_CLIPTOP)
		{
			top = MIN(top, sclipTop);
		}
		if (fake3D & FAKE3D_CLIPBOTTOM)
		{
			bot = MAX(bot, sclipBottom);
		}
		wallscan_np2(x1, x2, uwal, dwal, swal, lwal, yrepeat, top, bot, true);
	}
	else
	{
		call_wallscan(x1, x2, uwal, dwal, swal, lwal, yrepeat, true);
	}
}

inline fixed_t mvline1 (fixed_t vince, BYTE *colormap, int count, fixed_t vplce, const BYTE *bufplce, BYTE *dest)
{
	dc_iscale = vince;
	dc_colormap = colormap;
	dc_count = count;
	dc_texturefrac = vplce;
	dc_source = bufplce;
	dc_dest = dest;
	return domvline1 ();
}

void maskwallscan (int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal,
	fixed_t yrepeat, const BYTE *(*getcol)(FTexture *tex, int x))
{
	int x, shiftval;
	BYTE *p;
	int y1ve[4], y2ve[4], u4, d4, startx, dax, z;
	char bad;
	fixed_t light = rw_light - rw_lightstep;
	SDWORD texturemid, xoffset;
	BYTE *basecolormapdata;

	if (rw_pic->UseType == FTexture::TEX_Null)
	{
		return;
	}

	if (!rw_pic->bMasked)
	{ // Textures that aren't masked can use the faster wallscan.
		wallscan (x1, x2, uwal, dwal, swal, lwal, yrepeat, getcol);
		return;
	}

//extern cycle_t WallScanCycles;
//clock (WallScanCycles);

	rw_pic->GetHeight();	// Make sure texture size is loaded
	shiftval = rw_pic->HeightBits;
	setupmvline (32-shiftval);
	yrepeat >>= 2 + shiftval;
	texturemid = dc_texturemid << (16 - shiftval);
	xoffset = rw_offset;
	basecolormapdata = basecolormap->Maps;

	x = startx = x1;
	p = x + dc_destorg;

	bool fixed = (fixedcolormap != NULL || fixedlightlev >= 0);
	if (fixed)
	{
		palookupoffse[0] = dc_colormap;
		palookupoffse[1] = dc_colormap;
		palookupoffse[2] = dc_colormap;
		palookupoffse[3] = dc_colormap;
	}

	for(; (x < x2) && ((size_t)p & 3); ++x, ++p)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];//max(uwal[x],umost[x]);
		y2ve[0] = dwal[x];//min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + p;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		domvline1();
	}

	for(; x < x2-3; x += 4, p+= 4)
	{
		bad = 0;
		for (z = 3, dax = x+3; z >= 0; --z, --dax)
		{
			y1ve[z] = uwal[dax];
			y2ve[z] = dwal[dax];
			if (y2ve[z] <= y1ve[z]) { bad += 1<<z; continue; }

			bufplce[z] = getcol (rw_pic, (lwal[dax] + xoffset) >> FRACBITS);
			vince[z] = swal[dax] * yrepeat;
			vplce[z] = texturemid + FixedMul (vince[z], (y1ve[z]<<FRACBITS)-centeryfrac+FRACUNIT);
		}
		if (bad == 15)
		{
			light += rw_lightstep << 2;
			continue;
		}

		if (!fixed)
		{
			for (z = 0; z < 4; ++z)
			{
				light += rw_lightstep;
				palookupoffse[z] = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
			}
		}

		u4 = MAX(MAX(y1ve[0],y1ve[1]),MAX(y1ve[2],y1ve[3]));
		d4 = MIN(MIN(y2ve[0],y2ve[1]),MIN(y2ve[2],y2ve[3]));

		if ((bad != 0) || (u4 >= d4))
		{
			for (z = 0; z < 4; ++z)
			{
				if (!(bad & 1))
				{
					mvline1(vince[z],palookupoffse[z],y2ve[z]-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+p+z);
				}
				bad >>= 1;
			}
			continue;
		}

		for (z = 0; z < 4; ++z)
		{
			if (u4 > y1ve[z])
			{
				vplce[z] = mvline1(vince[z],palookupoffse[z],u4-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+p+z);
			}
		}

		if (d4 > u4)
		{
			dc_count = d4-u4;
			dc_dest = ylookup[u4]+p;
			domvline4();
		}

		BYTE *i = p+ylookup[d4];
		for (z = 0; z < 4; ++z)
		{
			if (y2ve[z] > d4)
			{
				mvline1(vince[z],palookupoffse[0],y2ve[z]-d4,vplce[z],bufplce[z],i+z);
			}
		}
	}
	for(; x < x2; ++x, ++p)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];
		y2ve[0] = dwal[x];
		if (y2ve[0] <= y1ve[0]) continue;

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + p;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		domvline1();
	}

//unclock(WallScanCycles);

	NetUpdate ();
}

inline void preptmvline1 (fixed_t vince, BYTE *colormap, int count, fixed_t vplce, const BYTE *bufplce, BYTE *dest)
{
	dc_iscale = vince;
	dc_colormap = colormap;
	dc_count = count;
	dc_texturefrac = vplce;
	dc_source = bufplce;
	dc_dest = dest;
}

void transmaskwallscan (int x1, int x2, short *uwal, short *dwal, fixed_t *swal, fixed_t *lwal,
	fixed_t yrepeat, const BYTE *(*getcol)(FTexture *tex, int x))
{
	fixed_t (*tmvline1)();
	void (*tmvline4)();
	int x, shiftval;
	BYTE *p;
	int y1ve[4], y2ve[4], u4, d4, startx, dax, z;
	char bad;
	fixed_t light = rw_light - rw_lightstep;
	SDWORD texturemid, xoffset;
	BYTE *basecolormapdata;

	if (rw_pic->UseType == FTexture::TEX_Null)
	{
		return;
	}

	if (!R_GetTransMaskDrawers (&tmvline1, &tmvline4))
	{
		// The current translucency is unsupported, so draw with regular maskwallscan instead.
		maskwallscan (x1, x2, uwal, dwal, swal, lwal, yrepeat, getcol);
		return;
	}

//extern cycle_t WallScanCycles;
//clock (WallScanCycles);

	rw_pic->GetHeight();	// Make sure texture size is loaded
	shiftval = rw_pic->HeightBits;
	setuptmvline (32-shiftval);
	yrepeat >>= 2 + shiftval;
	texturemid = dc_texturemid << (16 - shiftval);
	xoffset = rw_offset;
	basecolormapdata = basecolormap->Maps;

	x = startx = x1;
	p = x + dc_destorg;

	bool fixed = (fixedcolormap != NULL || fixedlightlev >= 0);
	if (fixed)
	{
		palookupoffse[0] = dc_colormap;
		palookupoffse[1] = dc_colormap;
		palookupoffse[2] = dc_colormap;
		palookupoffse[3] = dc_colormap;
	}

	for(; (x < x2) && ((size_t)p & 3); ++x, ++p)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];//max(uwal[x],umost[x]);
		y2ve[0] = dwal[x];//min(dwal[x],dmost[x]);
		if (y2ve[0] <= y1ve[0]) continue;

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + p;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		tmvline1();
	}

	for(; x < x2-3; x += 4, p+= 4)
	{
		bad = 0;
		for (z = 3, dax = x+3; z >= 0; --z, --dax)
		{
			y1ve[z] = uwal[dax];
			y2ve[z] = dwal[dax];
			if (y2ve[z] <= y1ve[z]) { bad += 1<<z; continue; }

			bufplce[z] = getcol (rw_pic, (lwal[dax] + xoffset) >> FRACBITS);
			vince[z] = swal[dax] * yrepeat;
			vplce[z] = texturemid + FixedMul (vince[z], (y1ve[z]<<FRACBITS)-centeryfrac+FRACUNIT);
		}
		if (bad == 15)
		{
			light += rw_lightstep << 2;
			continue;
		}

		if (!fixed)
		{
			for (z = 0; z < 4; ++z)
			{
				light += rw_lightstep;
				palookupoffse[z] = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
			}
		}

		u4 = MAX(MAX(y1ve[0],y1ve[1]),MAX(y1ve[2],y1ve[3]));
		d4 = MIN(MIN(y2ve[0],y2ve[1]),MIN(y2ve[2],y2ve[3]));

		if ((bad != 0) || (u4 >= d4))
		{
			for (z = 0; z < 4; ++z)
			{
				if (!(bad & 1))
				{
					preptmvline1(vince[z],palookupoffse[z],y2ve[z]-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+p+z);
					tmvline1();
				}
				bad >>= 1;
			}
			continue;
		}

		for (z = 0; z < 4; ++z)
		{
			if (u4 > y1ve[z])
			{
				preptmvline1(vince[z],palookupoffse[z],u4-y1ve[z],vplce[z],bufplce[z],ylookup[y1ve[z]]+p+z);
				vplce[z] = tmvline1();
			}
		}

		if (d4 > u4)
		{
			dc_count = d4-u4;
			dc_dest = ylookup[u4]+p;
			tmvline4();
		}

		BYTE *i = p+ylookup[d4];
		for (z = 0; z < 4; ++z)
		{
			if (y2ve[z] > d4)
			{
				preptmvline1(vince[z],palookupoffse[0],y2ve[z]-d4,vplce[z],bufplce[z],i+z);
				tmvline1();
			}
		}
	}
	for(; x < x2; ++x, ++p)
	{
		light += rw_lightstep;
		y1ve[0] = uwal[x];
		y2ve[0] = dwal[x];
		if (y2ve[0] <= y1ve[0]) continue;

		if (!fixed)
		{ // calculate lighting
			dc_colormap = basecolormapdata + (GETPALOOKUP (light, wallshade) << COLORMAPSHIFT);
		}

		dc_source = getcol (rw_pic, (lwal[x] + xoffset) >> FRACBITS);
		dc_dest = ylookup[y1ve[0]] + p;
		dc_iscale = swal[x] * yrepeat;
		dc_count = y2ve[0] - y1ve[0];
		dc_texturefrac = texturemid + FixedMul (dc_iscale, (y1ve[0]<<FRACBITS)-centeryfrac+FRACUNIT);

		tmvline1();
	}

//unclock(WallScanCycles);

	NetUpdate ();
}

//
// R_RenderSegLoop
// Draws zero, one, or two textures for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// CALLED: CORE LOOPING ROUTINE.
//
// [RH] Rewrote this to use Build's wallscan, so it's quite far
// removed from the original Doom routine.
//

void R_RenderSegLoop ()
{
	int x1 = rw_x;
	int x2 = rw_stopx;
	int x;
	fixed_t xscale, yscale;
	fixed_t xoffset = rw_offset;

	if (fixedlightlev >= 0)
		dc_colormap = basecolormap->Maps + fixedlightlev;
	else if (fixedcolormap != NULL)
		dc_colormap = fixedcolormap;

	// clip wall to the floor and ceiling
	for (x = x1; x < x2; ++x)
	{
		if (walltop[x] < ceilingclip[x])
		{
			walltop[x] = ceilingclip[x];
		}
		if (wallbottom[x] > floorclip[x])
		{
			wallbottom[x] = floorclip[x];
		}
	}

	// mark ceiling areas
	if (markceiling)
	{
		for (x = x1; x < x2; ++x)
		{
			short top = (fakeFloor && fake3D & 2) ? fakeFloor->ceilingclip[x] : ceilingclip[x];
			short bottom = MIN (walltop[x], floorclip[x]);
			if (top < bottom)
			{
				ceilingplane->top[x] = top;
				ceilingplane->bottom[x] = bottom;
			}
		}
	}

	// mark floor areas
	if (markfloor)
	{
		for (x = x1; x < x2; ++x)
		{
			short top = MAX (wallbottom[x], ceilingclip[x]);
			short bottom = (fakeFloor && fake3D & 1) ? fakeFloor->floorclip[x] : floorclip[x];
			if (top < bottom)
			{
				assert (bottom <= viewheight);
				floorplane->top[x] = top;
				floorplane->bottom[x] = bottom;
			}
		}
	}

	// kg3D - fake planes clipping
	if (fake3D & FAKE3D_REFRESHCLIP)
	{
		if (fake3D & FAKE3D_CLIPBOTFRONT)
		{
			memcpy (fakeFloor->floorclip+x1, wallbottom+x1, (x2-x1)*sizeof(short));
		}
		else
		{
			for (x = x1; x < x2; ++x)
			{
				walllower[x] = MIN (MAX (walllower[x], ceilingclip[x]), wallbottom[x]);
			}
			memcpy (fakeFloor->floorclip+x1, walllower+x1, (x2-x1)*sizeof(short));
		}
		if (fake3D & FAKE3D_CLIPTOPFRONT)
		{
			memcpy (fakeFloor->ceilingclip+x1, walltop+x1, (x2-x1)*sizeof(short));
		}
		else
		{
			for (x = x1; x < x2; ++x)
			{
				wallupper[x] = MAX (MIN (wallupper[x], floorclip[x]), walltop[x]);
			}
			memcpy (fakeFloor->ceilingclip+x1, wallupper+x1, (x2-x1)*sizeof(short));
		}
	}
	if(fake3D & 7) return;

	// draw the wall tiers
	if (midtexture)
	{ // one sided line
		if (midtexture->UseType != FTexture::TEX_Null && viewactive)
		{
			dc_texturemid = rw_midtexturemid;
			rw_pic = midtexture;
			xscale = FixedMul(rw_pic->xScale, rw_midtexturescalex);
			yscale = FixedMul(rw_pic->yScale, rw_midtexturescaley);
			if (xscale != lwallscale)
			{
				PrepLWall (lwall, curline->sidedef->TexelLength*xscale, WallC.sx1, WallC.sx2);
				lwallscale = xscale;
			}
			if (midtexture->bWorldPanning)
			{
				rw_offset = MulScale16 (rw_offset_mid, xscale);
			}
			else
			{
				rw_offset = rw_offset_mid;
			}
			if (xscale < 0)
			{
				rw_offset = -rw_offset;
			}
			if (rw_pic->GetHeight() != 1 << rw_pic->HeightBits)
			{
				wallscan_np2(x1, x2, walltop, wallbottom, swall, lwall, yscale, MAX(rw_frontcz1, rw_frontcz2), MIN(rw_frontfz1, rw_frontfz2), false);
			}
			else
			{
				call_wallscan(x1, x2, walltop, wallbottom, swall, lwall, yscale, false);
			}
		}
		clearbufshort (ceilingclip+x1, x2-x1, viewheight);
		clearbufshort (floorclip+x1, x2-x1, 0xffff);
	}
	else
	{ // two sided line
		if (toptexture != NULL && toptexture->UseType != FTexture::TEX_Null)
		{ // top wall
			for (x = x1; x < x2; ++x)
			{
				wallupper[x] = MAX (MIN (wallupper[x], floorclip[x]), walltop[x]);
			}
			if (viewactive)
			{
				dc_texturemid = rw_toptexturemid;
				rw_pic = toptexture;
				xscale = FixedMul(rw_pic->xScale, rw_toptexturescalex);
				yscale = FixedMul(rw_pic->yScale, rw_toptexturescaley);
				if (xscale != lwallscale)
				{
					PrepLWall (lwall, curline->sidedef->TexelLength*xscale, WallC.sx1, WallC.sx2);
					lwallscale = xscale;
				}
				if (toptexture->bWorldPanning)
				{
					rw_offset = MulScale16 (rw_offset_top, xscale);
				}
				else
				{
					rw_offset = rw_offset_top;
				}
				if (xscale < 0)
				{
					rw_offset = -rw_offset;
				}
				if (rw_pic->GetHeight() != 1 << rw_pic->HeightBits)
				{
					wallscan_np2(x1, x2, walltop, wallupper, swall, lwall, yscale, MAX(rw_frontcz1, rw_frontcz2), MIN(rw_backcz1, rw_backcz2), false);
				}
				else
				{
					call_wallscan(x1, x2, walltop, wallupper, swall, lwall, yscale, false);
				}
			}
			memcpy (ceilingclip+x1, wallupper+x1, (x2-x1)*sizeof(short));
		}
		else if (markceiling)
		{ // no top wall
			memcpy (ceilingclip+x1, walltop+x1, (x2-x1)*sizeof(short));
		}

		
		if (bottomtexture != NULL && bottomtexture->UseType != FTexture::TEX_Null)
		{ // bottom wall
			for (x = x1; x < x2; ++x)
			{
				walllower[x] = MIN (MAX (walllower[x], ceilingclip[x]), wallbottom[x]);
			}
			if (viewactive)
			{
				dc_texturemid = rw_bottomtexturemid;
				rw_pic = bottomtexture;
				xscale = FixedMul(rw_pic->xScale, rw_bottomtexturescalex);
				yscale = FixedMul(rw_pic->yScale, rw_bottomtexturescaley);
				if (xscale != lwallscale)
				{
					PrepLWall (lwall, curline->sidedef->TexelLength*xscale, WallC.sx1, WallC.sx2);
					lwallscale = xscale;
				}
				if (bottomtexture->bWorldPanning)
				{
					rw_offset = MulScale16 (rw_offset_bottom, xscale);
				}
				else
				{
					rw_offset = rw_offset_bottom;
				}
				if (xscale < 0)
				{
					rw_offset = -rw_offset;
				}
				if (rw_pic->GetHeight() != 1 << rw_pic->HeightBits)
				{
					wallscan_np2(x1, x2, walllower, wallbottom, swall, lwall, yscale, MAX(rw_backfz1, rw_backfz2), MIN(rw_frontfz1, rw_frontfz2), false);
				}
				else
				{
					call_wallscan(x1, x2, walllower, wallbottom, swall, lwall, yscale, false);
				}
			}
			memcpy (floorclip+x1, walllower+x1, (x2-x1)*sizeof(short));
		}
		else if (markfloor)
		{ // no bottom wall
			memcpy (floorclip+x1, wallbottom+x1, (x2-x1)*sizeof(short));
		}
	}
	rw_offset = xoffset;
}

void R_NewWall (bool needlights)
{
	fixed_t rowoffset, yrepeat;

	rw_markmirror = false;

	sidedef = curline->sidedef;
	linedef = curline->linedef;

	// mark the segment as visible for auto map
	if (!r_dontmaplines) linedef->flags |= ML_MAPPED;

	midtexture = toptexture = bottomtexture = 0;

	if (backsector == NULL)
	{
		// single sided line
		// a single sided line is terminal, so it must mark ends
		markfloor = markceiling = true;
		// [RH] Render mirrors later, but mark them now.
		if (linedef->special != Line_Mirror || !r_drawmirrors)
		{
			// [RH] Horizon lines do not need to be textured
			if (linedef->special != Line_Horizon)
			{
				midtexture = TexMan(sidedef->GetTexture(side_t::mid), true);
				rw_offset_mid = sidedef->GetTextureXOffset(side_t::mid);
				rowoffset = sidedef->GetTextureYOffset(side_t::mid);
				rw_midtexturescalex = sidedef->GetTextureXScale(side_t::mid);
				rw_midtexturescaley = sidedef->GetTextureYScale(side_t::mid);
				yrepeat = FixedMul(midtexture->yScale, rw_midtexturescaley);
				if (yrepeat >= 0)
				{ // normal orientation
					if (linedef->flags & ML_DONTPEGBOTTOM)
					{ // bottom of texture at bottom
						rw_midtexturemid = MulScale16(frontsector->GetPlaneTexZ(sector_t::floor) - viewz, yrepeat) + (midtexture->GetHeight() << FRACBITS);
					}
					else
					{ // top of texture at top
						rw_midtexturemid = MulScale16(frontsector->GetPlaneTexZ(sector_t::ceiling) - viewz, yrepeat);
						if (rowoffset < 0 && midtexture != NULL)
						{
							rowoffset += midtexture->GetHeight() << FRACBITS;
						}
					}
				}
				else
				{ // upside down
					rowoffset = -rowoffset;
					if (linedef->flags & ML_DONTPEGBOTTOM)
					{ // top of texture at bottom
						rw_midtexturemid = MulScale16(frontsector->GetPlaneTexZ(sector_t::floor) - viewz, yrepeat);
					}
					else
					{ // bottom of texture at top
						rw_midtexturemid = MulScale16(frontsector->GetPlaneTexZ(sector_t::ceiling) - viewz, yrepeat) + (midtexture->GetHeight() << FRACBITS);
					}
				}
				if (midtexture->bWorldPanning)
				{
					rw_midtexturemid += MulScale16(rowoffset, yrepeat);
				}
				else
				{
					// rowoffset is added outside the multiply so that it positions the texture
					// by texels instead of world units.
					rw_midtexturemid += rowoffset;
				}
			}
		}
		else
		{
			rw_markmirror = true;
		}
	}
	else
	{ // two-sided line
		// hack to allow height changes in outdoor areas

		rw_frontlowertop = frontsector->GetPlaneTexZ(sector_t::ceiling);

		if (frontsector->GetTexture(sector_t::ceiling) == skyflatnum &&
			backsector->GetTexture(sector_t::ceiling) == skyflatnum)
		{
			if (rw_havehigh)
			{ // front ceiling is above back ceiling
				memcpy (&walltop[WallC.sx1], &wallupper[WallC.sx1], (WallC.sx2 - WallC.sx1)*sizeof(walltop[0]));
				rw_havehigh = false;
			}
			else if (rw_havelow && frontsector->ceilingplane != backsector->ceilingplane)
			{ // back ceiling is above front ceiling
				// The check for rw_havelow is not Doom-compliant, but it avoids HoM that
				// would otherwise occur because there is space made available for this
				// wall but nothing to draw for it.
				// Recalculate walltop so that the wall is clipped by the back sector's
				// ceiling instead of the front sector's ceiling.
				WallMost (walltop, backsector->ceilingplane, &WallC);
			}
			// Putting sky ceilings on the front and back of a line alters the way unpegged
			// positioning works.
			rw_frontlowertop = backsector->GetPlaneTexZ(sector_t::ceiling);
		}

		if ((rw_backcz1 <= rw_frontfz1 && rw_backcz2 <= rw_frontfz2) ||
			(rw_backfz1 >= rw_frontcz1 && rw_backfz2 >= rw_frontcz2))
		{
			// closed door
			markceiling = markfloor = true;
		}
		else
		{
			markfloor = rw_mustmarkfloor
				|| backsector->floorplane != frontsector->floorplane
				|| backsector->lightlevel != frontsector->lightlevel
				|| backsector->GetTexture(sector_t::floor) != frontsector->GetTexture(sector_t::floor)

				// killough 3/7/98: Add checks for (x,y) offsets
				|| backsector->GetXOffset(sector_t::floor) != frontsector->GetXOffset(sector_t::floor)
				|| backsector->GetYOffset(sector_t::floor) != frontsector->GetYOffset(sector_t::floor)
				|| backsector->GetAlpha(sector_t::floor) != frontsector->GetAlpha(sector_t::floor)

				// killough 4/15/98: prevent 2s normals
				// from bleeding through deep water
				|| frontsector->heightsec

				|| backsector->GetPlaneLight(sector_t::floor) != frontsector->GetPlaneLight(sector_t::floor)
				|| backsector->GetFlags(sector_t::floor) != frontsector->GetFlags(sector_t::floor)

				// [RH] Add checks for colormaps
				|| backsector->ColorMap != frontsector->ColorMap

				|| backsector->GetXScale(sector_t::floor) != frontsector->GetXScale(sector_t::floor)
				|| backsector->GetYScale(sector_t::floor) != frontsector->GetYScale(sector_t::floor)

				|| backsector->GetAngle(sector_t::floor) != frontsector->GetAngle(sector_t::floor)

				// kg3D - add fake lights
				|| (frontsector->e && frontsector->e->XFloor.lightlist.Size())
				|| (backsector->e && backsector->e->XFloor.lightlist.Size())

				|| (sidedef->GetTexture(side_t::mid).isValid() &&
					((linedef->flags & (ML_CLIP_MIDTEX|ML_WRAP_MIDTEX)) ||
					 (sidedef->Flags & (WALLF_CLIP_MIDTEX|WALLF_WRAP_MIDTEX))))
				;

			markceiling = (frontsector->GetTexture(sector_t::ceiling) != skyflatnum ||
				backsector->GetTexture(sector_t::ceiling) != skyflatnum) &&
				(rw_mustmarkceiling
				|| backsector->ceilingplane != frontsector->ceilingplane
				|| backsector->lightlevel != frontsector->lightlevel
				|| backsector->GetTexture(sector_t::ceiling) != frontsector->GetTexture(sector_t::ceiling)

				// killough 3/7/98: Add checks for (x,y) offsets
				|| backsector->GetXOffset(sector_t::ceiling) != frontsector->GetXOffset(sector_t::ceiling)
				|| backsector->GetYOffset(sector_t::ceiling) != frontsector->GetYOffset(sector_t::ceiling)
				|| backsector->GetAlpha(sector_t::ceiling) != frontsector->GetAlpha(sector_t::ceiling)

				// killough 4/15/98: prevent 2s normals
				// from bleeding through fake ceilings
				|| (frontsector->heightsec && frontsector->GetTexture(sector_t::ceiling) != skyflatnum)

				|| backsector->GetPlaneLight(sector_t::ceiling) != frontsector->GetPlaneLight(sector_t::ceiling)
				|| backsector->GetFlags(sector_t::ceiling) != frontsector->GetFlags(sector_t::ceiling)

				// [RH] Add check for colormaps
				|| backsector->ColorMap != frontsector->ColorMap

				|| backsector->GetXScale(sector_t::ceiling) != frontsector->GetXScale(sector_t::ceiling)
				|| backsector->GetYScale(sector_t::ceiling) != frontsector->GetYScale(sector_t::ceiling)

				|| backsector->GetAngle(sector_t::ceiling) != frontsector->GetAngle(sector_t::ceiling)

				// kg3D - add fake lights
				|| (frontsector->e && frontsector->e->XFloor.lightlist.Size())
				|| (backsector->e && backsector->e->XFloor.lightlist.Size())

				|| (sidedef->GetTexture(side_t::mid).isValid() &&
					((linedef->flags & (ML_CLIP_MIDTEX|ML_WRAP_MIDTEX)) ||
					(sidedef->Flags & (WALLF_CLIP_MIDTEX|WALLF_WRAP_MIDTEX))))
				);
		}

		if (rw_havehigh)
		{ // top texture
			toptexture = TexMan(sidedef->GetTexture(side_t::top), true);

			rw_offset_top = sidedef->GetTextureXOffset(side_t::top);
			rowoffset = sidedef->GetTextureYOffset(side_t::top);
			rw_toptexturescalex = sidedef->GetTextureXScale(side_t::top);
			rw_toptexturescaley = sidedef->GetTextureYScale(side_t::top);
			yrepeat = FixedMul(toptexture->yScale, rw_toptexturescaley);
			if (yrepeat >= 0)
			{ // normal orientation
				if (linedef->flags & ML_DONTPEGTOP)
				{ // top of texture at top
					rw_toptexturemid = MulScale16(frontsector->GetPlaneTexZ(sector_t::ceiling) - viewz, yrepeat);
					if (rowoffset < 0 && toptexture != NULL)
					{
						rowoffset += toptexture->GetHeight() << FRACBITS;
					}
				}
				else
				{ // bottom of texture at bottom
					rw_toptexturemid = MulScale16(backsector->GetPlaneTexZ(sector_t::ceiling) - viewz, yrepeat) + (toptexture->GetHeight() << FRACBITS);
				}
			}
			else
			{ // upside down
				rowoffset = -rowoffset;
				if (linedef->flags & ML_DONTPEGTOP)
				{ // bottom of texture at top
					rw_toptexturemid = MulScale16(frontsector->GetPlaneTexZ(sector_t::ceiling) - viewz, yrepeat) + (toptexture->GetHeight() << FRACBITS);
				}
				else
				{ // top of texture at bottom
					rw_toptexturemid = MulScale16(backsector->GetPlaneTexZ(sector_t::ceiling) - viewz, yrepeat);
				}
			}
			if (toptexture->bWorldPanning)
			{
				rw_toptexturemid += MulScale16(rowoffset, yrepeat);
			}
			else
			{
				rw_toptexturemid += rowoffset;
			}
		}
		if (rw_havelow)
		{ // bottom texture
			bottomtexture = TexMan(sidedef->GetTexture(side_t::bottom), true);

			rw_offset_bottom = sidedef->GetTextureXOffset(side_t::bottom);
			rowoffset = sidedef->GetTextureYOffset(side_t::bottom);
			rw_bottomtexturescalex = sidedef->GetTextureXScale(side_t::bottom);
			rw_bottomtexturescaley = sidedef->GetTextureYScale(side_t::bottom);
			yrepeat = FixedMul(bottomtexture->yScale, rw_bottomtexturescaley);
			if (yrepeat >= 0)
			{ // normal orientation
				if (linedef->flags & ML_DONTPEGBOTTOM)
				{ // bottom of texture at bottom
					rw_bottomtexturemid = MulScale16(rw_frontlowertop - viewz, yrepeat);
				}
				else
				{ // top of texture at top
					rw_bottomtexturemid = MulScale16(backsector->GetPlaneTexZ(sector_t::floor) - viewz, yrepeat);
					if (rowoffset < 0 && bottomtexture != NULL)
					{
						rowoffset += bottomtexture->GetHeight() << FRACBITS;
					}
				}
			}
			else
			{ // upside down
				rowoffset = -rowoffset;
				if (linedef->flags & ML_DONTPEGBOTTOM)
				{ // top of texture at bottom
					rw_bottomtexturemid = MulScale16(rw_frontlowertop - viewz, yrepeat);
				}
				else
				{ // bottom of texture at top
					rw_bottomtexturemid = MulScale16(backsector->GetPlaneTexZ(sector_t::floor) - viewz, yrepeat) + (bottomtexture->GetHeight() << FRACBITS);
				}
			}
			if (bottomtexture->bWorldPanning)
			{
				rw_bottomtexturemid += MulScale16(rowoffset, yrepeat);
			}
			else
			{
				rw_bottomtexturemid += rowoffset;
			}
		}
	}

	// if a floor / ceiling plane is on the wrong side of the view plane,
	// it is definitely invisible and doesn't need to be marked.

	// killough 3/7/98: add deep water check
	if (frontsector->GetHeightSec() == NULL)
	{
		int planeside;

		planeside = frontsector->floorplane.PointOnSide(viewx, viewy, viewz);
		if (frontsector->floorplane.c < 0)	// 3D floors have the floor backwards
			planeside = -planeside;
		if (planeside <= 0)		// above view plane
			markfloor = false;

		if (frontsector->GetTexture(sector_t::ceiling) != skyflatnum)
		{
			planeside = frontsector->ceilingplane.PointOnSide(viewx, viewy, viewz);
			if (frontsector->ceilingplane.c > 0)	// 3D floors have the ceiling backwards
				planeside = -planeside;
			if (planeside <= 0)		// below view plane
				markceiling = false;
		}
	}

	FTexture *midtex = TexMan(sidedef->GetTexture(side_t::mid), true);

	segtextured = midtex != NULL || toptexture != NULL || bottomtexture != NULL;

	// calculate light table
	if (needlights && (segtextured || (backsector && IsFogBoundary(frontsector, backsector))))
	{
		lwallscale =
			midtex ? FixedMul(midtex->xScale, sidedef->GetTextureXScale(side_t::mid)) :
			toptexture ? FixedMul(toptexture->xScale, sidedef->GetTextureXScale(side_t::top)) :
			bottomtexture ? FixedMul(bottomtexture->xScale, sidedef->GetTextureXScale(side_t::bottom)) :
			FRACUNIT;

		PrepWall (swall, lwall, sidedef->TexelLength * lwallscale, WallC.sx1, WallC.sx2);

		if (fixedcolormap == NULL && fixedlightlev < 0)
		{
			wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, frontsector->lightlevel)
				+ r_actualextralight);
			GlobVis = r_WallVisibility;
			rw_lightleft = SafeDivScale12 (GlobVis, WallC.sz1);
			rw_lightstep = (SafeDivScale12 (GlobVis, WallC.sz2) - rw_lightleft) / (WallC.sx2 - WallC.sx1);
		}
		else
		{
			rw_lightleft = FRACUNIT;
			rw_lightstep = 0;
		}
	}
}


//
// R_CheckDrawSegs
//

void R_CheckDrawSegs ()
{
	if (ds_p == &drawsegs[MaxDrawSegs])
	{ // [RH] Grab some more drawsegs
		size_t newdrawsegs = MaxDrawSegs ? MaxDrawSegs*2 : 32;
		ptrdiff_t firstofs = firstdrawseg - drawsegs;
		drawsegs = (drawseg_t *)M_Realloc (drawsegs, newdrawsegs * sizeof(drawseg_t));
		firstdrawseg = drawsegs + firstofs;
		ds_p = drawsegs + MaxDrawSegs;
		MaxDrawSegs = newdrawsegs;
		DPrintf ("MaxDrawSegs increased to %zu\n", MaxDrawSegs);
	}
}

//
// R_CheckOpenings
//

ptrdiff_t R_NewOpening (ptrdiff_t len)
{
	ptrdiff_t res = lastopening;
	lastopening += len;
	if ((size_t)lastopening > maxopenings)
	{
		do
			maxopenings = maxopenings ? maxopenings*2 : 16384;
		while ((size_t)lastopening > maxopenings);
		openings = (short *)M_Realloc (openings, maxopenings * sizeof(*openings));
		DPrintf ("MaxOpenings increased to %zu\n", maxopenings);
	}
	return res;
}


//
// R_StoreWallRange
// A wall segment will be drawn between start and stop pixels (inclusive).
//

void R_StoreWallRange (int start, int stop)
{
	int i;
	bool maskedtexture = false;

#ifdef RANGECHECK
	if (start >= viewwidth || start >= stop)
		I_FatalError ("Bad R_StoreWallRange: %i to %i", start , stop);
#endif

	// don't overflow and crash
	R_CheckDrawSegs ();
	
	if (!rw_prepped)
	{
		rw_prepped = true;
		R_NewWall (true);
	}

	rw_offset = sidedef->GetTextureXOffset(side_t::mid);
	rw_light = rw_lightleft + rw_lightstep * (start - WallC.sx1);

	ds_p->sx1 = WallC.sx1;
	ds_p->sx2 = WallC.sx2;
	ds_p->sz1 = WallC.sz1;
	ds_p->sz2 = WallC.sz2;
	ds_p->cx = WallC.tx1;
	ds_p->cy = WallC.ty1;
	ds_p->cdx = WallC.tx2 - WallC.tx1;
	ds_p->cdy = WallC.ty2 - WallC.ty1;
	ds_p->tmapvals = WallT;
	ds_p->siz1 = (DWORD)DivScale32 (1, WallC.sz1) >> 1;
	ds_p->siz2 = (DWORD)DivScale32 (1, WallC.sz2) >> 1;
	ds_p->x1 = rw_x = start;
	ds_p->x2 = stop;
	ds_p->curline = curline;
	rw_stopx = stop;
	ds_p->bFogBoundary = false;
	ds_p->bFakeBoundary = false;
	if(fake3D & 7) ds_p->fake = 1;
	else ds_p->fake = 0;

	// killough 1/6/98, 2/1/98: remove limit on openings
	ds_p->sprtopclip = ds_p->sprbottomclip = ds_p->maskedtexturecol = ds_p->bkup = ds_p->swall = -1;

	if (rw_markmirror)
	{
		size_t drawsegnum = ds_p - drawsegs;
		WallMirrors.Push (drawsegnum);
		ds_p->silhouette = SIL_BOTH;
	}
	else if (backsector == NULL)
	{
		ds_p->sprtopclip = R_NewOpening (stop - start);
		ds_p->sprbottomclip = R_NewOpening (stop - start);
		clearbufshort (openings + ds_p->sprtopclip, stop-start, viewheight);
		memset (openings + ds_p->sprbottomclip, -1, (stop-start)*sizeof(short));
		ds_p->silhouette = SIL_BOTH;
	}
	else
	{
		// two sided line
		ds_p->silhouette = 0;

		if (rw_frontfz1 > rw_backfz1 || rw_frontfz2 > rw_backfz2 ||
			backsector->floorplane.PointOnSide(viewx, viewy, viewz) < 0)
		{
			ds_p->silhouette = SIL_BOTTOM;
		}

		if (rw_frontcz1 < rw_backcz1 || rw_frontcz2 < rw_backcz2 ||
			backsector->ceilingplane.PointOnSide(viewx, viewy, viewz) < 0)
		{
			ds_p->silhouette |= SIL_TOP;
		}

		// killough 1/17/98: this test is required if the fix
		// for the automap bug (r_bsp.c) is used, or else some
		// sprites will be displayed behind closed doors. That
		// fix prevents lines behind closed doors with dropoffs
		// from being displayed on the automap.
		//
		// killough 4/7/98: make doorclosed external variable

		{
			extern int doorclosed;	// killough 1/17/98, 2/8/98, 4/7/98
			if (doorclosed || (rw_backcz1 <= rw_frontfz1 && rw_backcz2 <= rw_frontfz2))
			{
				ds_p->sprbottomclip = R_NewOpening (stop - start);
				memset (openings + ds_p->sprbottomclip, -1, (stop-start)*sizeof(short));
				ds_p->silhouette |= SIL_BOTTOM;
			}
			if (doorclosed || (rw_backfz1 >= rw_frontcz1 && rw_backfz2 >= rw_frontcz2))
			{						// killough 1/17/98, 2/8/98
				ds_p->sprtopclip = R_NewOpening (stop - start);
				clearbufshort (openings + ds_p->sprtopclip, stop - start, viewheight);
				ds_p->silhouette |= SIL_TOP;
			}
		}

		if(!ds_p->fake && r_3dfloors && backsector->e && backsector->e->XFloor.ffloors.Size()) {
			for(i = 0; i < (int)backsector->e->XFloor.ffloors.Size(); i++) {
				F3DFloor *rover = backsector->e->XFloor.ffloors[i];
				if(rover->flags & FF_RENDERSIDES && (!(rover->flags & FF_INVERTSIDES) || rover->flags & FF_ALLSIDES)) {
					ds_p->bFakeBoundary |= 1;
					break;
				}
			}
		}
		if(!ds_p->fake && r_3dfloors && frontsector->e && frontsector->e->XFloor.ffloors.Size()) {
			for(i = 0; i < (int)frontsector->e->XFloor.ffloors.Size(); i++) {
				F3DFloor *rover = frontsector->e->XFloor.ffloors[i];
				if(rover->flags & FF_RENDERSIDES && (rover->flags & FF_ALLSIDES || rover->flags & FF_INVERTSIDES)) {
					ds_p->bFakeBoundary |= 2;
					break;
				}
			}
		}
		// kg3D - no for fakes
		if(!ds_p->fake)
		// allocate space for masked texture tables, if needed
		// [RH] Don't just allocate the space; fill it in too.
		if ((TexMan(sidedef->GetTexture(side_t::mid), true)->UseType != FTexture::TEX_Null || ds_p->bFakeBoundary || IsFogBoundary (frontsector, backsector)) &&
			(rw_ceilstat != 12 || !sidedef->GetTexture(side_t::top).isValid()) &&
			(rw_floorstat != 3 || !sidedef->GetTexture(side_t::bottom).isValid()) &&
			(WallC.sz1 >= TOO_CLOSE_Z && WallC.sz2 >= TOO_CLOSE_Z))
		{
			fixed_t *swal;
			fixed_t *lwal;
			int i;

			maskedtexture = true;

			// kg3D - backup for mid and fake walls
			ds_p->bkup = R_NewOpening(stop - start);
			memcpy(openings + ds_p->bkup, &ceilingclip[start], sizeof(short)*(stop - start));

			ds_p->bFogBoundary = IsFogBoundary (frontsector, backsector);
			if (sidedef->GetTexture(side_t::mid).isValid() || ds_p->bFakeBoundary)
			{
				if(sidedef->GetTexture(side_t::mid).isValid())
					ds_p->bFakeBoundary |= 4; // it is also mid texture

				ds_p->maskedtexturecol = R_NewOpening ((stop - start) * 2);
				ds_p->swall = R_NewOpening ((stop - start) * 2);

				lwal = (fixed_t *)(openings + ds_p->maskedtexturecol);
				swal = (fixed_t *)(openings + ds_p->swall);
				FTexture *pic = TexMan(sidedef->GetTexture(side_t::mid), true);
				fixed_t yrepeat = FixedMul(pic->yScale, sidedef->GetTextureYScale(side_t::mid));
				fixed_t xoffset = sidedef->GetTextureXOffset(side_t::mid);

				if (pic->bWorldPanning)
				{
					xoffset = MulScale16 (xoffset, lwallscale);
				}

				for (i = start; i < stop; i++)
				{
					*lwal++ = lwall[i] + xoffset;
					*swal++ = swall[i];
				}

				fixed_t istart = MulScale18 (*((fixed_t *)(openings + ds_p->swall)), yrepeat);
				fixed_t iend = MulScale18 (*(swal - 1), yrepeat);

				if (istart < 3 && istart >= 0) istart = 3;
				if (istart > -3 && istart < 0) istart = -3;
				if (iend < 3 && iend >= 0) iend = 3;
				if (iend > -3 && iend < 0) iend = -3;
				istart = DivScale32 (1, istart);
				iend = DivScale32 (1, iend);
				ds_p->yrepeat = yrepeat;
				ds_p->iscale = istart;
				if (stop - start > 0)
				{
					ds_p->iscalestep = (iend - istart) / (stop - start);
				}
				else
				{
					ds_p->iscalestep = 0;
				}
			}
			ds_p->light = rw_light;
			ds_p->lightstep = rw_lightstep;

			// Masked midtextures should get the light level from the sector they reference,
			// not from the current subsector, which is what the current wallshade value
			// comes from. We make an exeption for polyobjects, however, since their "home"
			// sector should be whichever one they move into.
			if (curline->sidedef->Flags & WALLF_POLYOBJ)
			{
				ds_p->shade = wallshade;
			}
			else
			{
				ds_p->shade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, curline->frontsector->lightlevel)
					+ r_actualextralight);
			}

			if (ds_p->bFogBoundary || ds_p->maskedtexturecol != -1)
			{
				size_t drawsegnum = ds_p - drawsegs;
				InterestingDrawsegs.Push (drawsegnum);
			}
		}
	}
	
	// render it
	if (markceiling)
	{
		if (ceilingplane)
		{	// killough 4/11/98: add NULL ptr checks
			ceilingplane = R_CheckPlane (ceilingplane, start, stop);
		}
		else
		{
			markceiling = false;
		}
	}
	
	if (markfloor)
	{
		if (floorplane)
		{	// killough 4/11/98: add NULL ptr checks
			floorplane = R_CheckPlane (floorplane, start, stop);
		}
		else
		{
			markfloor = false;
		}
	}

	R_RenderSegLoop ();

	if(fake3D & 7) {
		ds_p++;
		return;
	}

	// save sprite clipping info
	if ( ((ds_p->silhouette & SIL_TOP) || maskedtexture) && ds_p->sprtopclip == -1)
	{
		ds_p->sprtopclip = R_NewOpening (stop - start);
		memcpy (openings + ds_p->sprtopclip, &ceilingclip[start], sizeof(short)*(stop-start));
	}

	if ( ((ds_p->silhouette & SIL_BOTTOM) || maskedtexture) && ds_p->sprbottomclip == -1)
	{
		ds_p->sprbottomclip = R_NewOpening (stop - start);
		memcpy (openings + ds_p->sprbottomclip, &floorclip[start], sizeof(short)*(stop-start));
	}

	if (maskedtexture && curline->sidedef->GetTexture(side_t::mid).isValid())
	{
		ds_p->silhouette |= SIL_TOP | SIL_BOTTOM;
	}

	// [RH] Draw any decals bound to the seg
	for (DBaseDecal *decal = curline->sidedef->AttachedDecals; decal != NULL; decal = decal->WallNext)
	{
		R_RenderDecal (curline->sidedef, decal, ds_p, 0);
	}

	ds_p++;
}

int OWallMost (short *mostbuf, fixed_t z, const FWallCoords *wallc)
{
	int bad, y, ix1, ix2, iy1, iy2;
	fixed_t s1, s2, s3, s4;

	z = -(z >> 4);
	s1 = MulScale16 (globaluclip, wallc->sz1); s2 = MulScale16 (globaluclip, wallc->sz2);
	s3 = MulScale16 (globaldclip, wallc->sz1); s4 = MulScale16 (globaldclip, wallc->sz2);
	bad = (z<s1)+((z<s2)<<1)+((z>s3)<<2)+((z>s4)<<3);

#if 1
	if ((bad&3) == 3)
	{
		memset (&mostbuf[wallc->sx1], 0, (wallc->sx2 - wallc->sx1)*sizeof(mostbuf[0]));
		return bad;
	}

	if ((bad&12) == 12)
	{
		clearbufshort (&mostbuf[wallc->sx1], wallc->sx2 - wallc->sx1, viewheight);
		return bad;
	}
#endif
	ix1 = wallc->sx1; iy1 = wallc->sz1;
	ix2 = wallc->sx2; iy2 = wallc->sz2;
#if 1
	if (bad & 3)
	{
		int t = DivScale30 (z-s1, s2-s1);
		int inty = wallc->sz1 + MulScale30 (wallc->sz2 - wallc->sz1, t);
		int xcross = wallc->sx1 + Scale (MulScale30 (wallc->sz2, t), wallc->sx2 - wallc->sx1, inty);

		if ((bad & 3) == 2)
		{
			if (wallc->sx1 <= xcross) { iy2 = inty; ix2 = xcross; }
			if (wallc->sx2 > xcross) memset (&mostbuf[xcross], 0, (wallc->sx2-xcross)*sizeof(mostbuf[0]));
		}
		else
		{
			if (xcross <= wallc->sx2) { iy1 = inty; ix1 = xcross; }
			if (xcross > wallc->sx1) memset (&mostbuf[wallc->sx1], 0, (xcross-wallc->sx1)*sizeof(mostbuf[0]));
		}
	}

	if (bad & 12)
	{
		int t = DivScale30 (z-s3, s4-s3);
		int inty = wallc->sz1 + MulScale30 (wallc->sz2 - wallc->sz1, t);
		int xcross = wallc->sx1 + Scale (MulScale30 (wallc->sz2, t), wallc->sx2 - wallc->sx1, inty);

		if ((bad & 12) == 8)
		{
			if (wallc->sx1 <= xcross) { iy2 = inty; ix2 = xcross; }
			if (wallc->sx2 > xcross) clearbufshort (&mostbuf[xcross], wallc->sx2 - xcross, viewheight);
		}
		else
		{
			if (xcross <= wallc->sx2) { iy1 = inty; ix1 = xcross; }
			if (xcross > wallc->sx1) clearbufshort (&mostbuf[wallc->sx1], xcross - wallc->sx1, viewheight);
		}
	}

	y = Scale (z, InvZtoScale, iy1);
	if (ix2 == ix1)
	{
		mostbuf[ix1] = (short)((y + centeryfrac) >> FRACBITS);
	}
	else
	{
		fixed_t yinc  = (Scale (z, InvZtoScale, iy2) - y) / (ix2 - ix1);
		qinterpolatedown16short (&mostbuf[ix1], ix2-ix1, y + centeryfrac, yinc);
	}
#else
	double max = viewheight;
	double zz = z / 65536.0;
#if 0
	double z1 = zz * InvZtoScale / wallc->sz1;
	double z2 = zz * InvZtoScale / wallc->sz2 - z1;
	z2 /= (wallc->sx2 - wallc->sx1);
	z1 += centeryfrac / 65536.0;

	for (int x = wallc->sx1; x < wallc->sx2; ++x)
	{
		mostbuf[x] = xs_RoundToInt(clamp(z1, 0.0, max));
		z1 += z2;
	}
#else
	double top, bot, i;

	i = wallc->sx1 - centerx;
	top = WallT.UoverZorg + WallT.UoverZstep * i;
	bot = WallT.InvZorg + WallT.InvZstep * i;
	double cy = centeryfrac / 65536.0;

	for (int x = wallc->sx1; x < wallc->sx2; x++)
	{
		double frac = top / bot;
		double scale = frac * WallT.DepthScale + WallT.DepthOrg;
		mostbuf[x] = xs_RoundToInt(clamp(zz / scale + cy, 0.0, max));
		top += WallT.UoverZstep;
		bot += WallT.InvZstep;
	}
#endif
#endif
	if (mostbuf[ix1] < 0) mostbuf[ix1] = 0;
	else if (mostbuf[ix1] > viewheight) mostbuf[ix1] = (short)viewheight;
	if (mostbuf[ix2-1] < 0) mostbuf[ix2-1] = 0;
	else if (mostbuf[ix2-1] > viewheight) mostbuf[ix2-1] = (short)viewheight;

	return bad;
}

int WallMost (short *mostbuf, const secplane_t &plane, const FWallCoords *wallc)
{
	if ((plane.a | plane.b) == 0)
	{
		return OWallMost (mostbuf, ((plane.c < 0) ? plane.d : -plane.d) - viewz, wallc);
	}

	fixed_t x, y, den, z1, z2, oz1, oz2;
	fixed_t s1, s2, s3, s4;
	int bad, ix1, ix2, iy1, iy2;

	if (MirrorFlags & RF_XFLIP)
	{
		x = curline->v2->x;
		y = curline->v2->y;
		if (wallc->sx1 == 0 && 0 != (den = wallc->tx1 - wallc->tx2 + wallc->ty1 - wallc->ty2))
		{
			int frac = SafeDivScale30 (wallc->ty1 + wallc->tx1, den);
			x -= MulScale30 (frac, x - curline->v1->x);
			y -= MulScale30 (frac, y - curline->v1->y);
		}
		z1 = viewz - plane.ZatPoint (x, y);

		if (wallc->sx2 > wallc->sx1 + 1)
		{
			x = curline->v1->x;
			y = curline->v1->y;
			if (wallc->sx2 == viewwidth && 0 != (den = wallc->tx1 - wallc->tx2 - wallc->ty1 + wallc->ty2))
			{
				int frac = SafeDivScale30 (wallc->ty2 - wallc->tx2, den);
				x += MulScale30 (frac, curline->v2->x - x);
				y += MulScale30 (frac, curline->v2->y - y);
			}
			z2 = viewz - plane.ZatPoint (x, y);
		}
		else
		{
			z2 = z1;
		}
	}
	else
	{
		x = curline->v1->x;
		y = curline->v1->y;
		if (wallc->sx1 == 0 && 0 != (den = wallc->tx1 - wallc->tx2 + wallc->ty1 - wallc->ty2))
		{
			int frac = SafeDivScale30 (wallc->ty1 + wallc->tx1, den);
			x += MulScale30 (frac, curline->v2->x - x);
			y += MulScale30 (frac, curline->v2->y - y);
		}
		z1 = viewz - plane.ZatPoint (x, y);

		if (wallc->sx2 > wallc->sx1 + 1)
		{
			x = curline->v2->x;
			y = curline->v2->y;
			if (wallc->sx2 == viewwidth && 0 != (den = wallc->tx1 - wallc->tx2 - wallc->ty1 + wallc->ty2))
			{
				int frac = SafeDivScale30 (wallc->ty2 - wallc->tx2, den);
				x -= MulScale30 (frac, x - curline->v1->x);
				y -= MulScale30 (frac, y - curline->v1->y);
			}
			z2 = viewz - plane.ZatPoint (x, y);
		}
		else
		{
			z2 = z1;
		}
	}

	s1 = MulScale12 (globaluclip, wallc->sz1); s2 = MulScale12 (globaluclip, wallc->sz2);
	s3 = MulScale12 (globaldclip, wallc->sz1); s4 = MulScale12 (globaldclip, wallc->sz2);
	bad = (z1<s1)+((z2<s2)<<1)+((z1>s3)<<2)+((z2>s4)<<3);

	ix1 = wallc->sx1; ix2 = wallc->sx2;
	iy1 = wallc->sz1; iy2 = wallc->sz2;
	oz1 = z1; oz2 = z2;

	if ((bad&3) == 3)
	{
		memset (&mostbuf[ix1], -1, (ix2-ix1)*sizeof(mostbuf[0]));
		return bad;
	}

	if ((bad&12) == 12)
	{
		clearbufshort (&mostbuf[ix1], ix2-ix1, viewheight);
		return bad;

	}

	if (bad&3)
	{
			//inty = intz / (globaluclip>>16)
		int t = SafeDivScale30 (oz1-s1, s2-s1+oz1-oz2);
		int inty = wallc->sz1 + MulScale30 (wallc->sz2-wallc->sz1,t);
		int intz = oz1 + MulScale30 (oz2-oz1,t);
		int xcross = wallc->sx1 + Scale (MulScale30 (wallc->sz2, t), wallc->sx2-wallc->sx1, inty);

		//t = divscale30((x1<<4)-xcross*yb1[w],xcross*(yb2[w]-yb1[w])-((x2-x1)<<4));
		//inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
		//intz = z1 + mulscale30(z2-z1,t);

		if ((bad&3) == 2)
		{
			if (wallc->sx1 <= xcross) { z2 = intz; iy2 = inty; ix2 = xcross; }
			memset (&mostbuf[xcross], 0, (wallc->sx2-xcross)*sizeof(mostbuf[0]));
		}
		else
		{
			if (xcross <= wallc->sx2) { z1 = intz; iy1 = inty; ix1 = xcross; }
			memset (&mostbuf[wallc->sx1], 0, (xcross-wallc->sx1)*sizeof(mostbuf[0]));
		}
	}

	if (bad&12)
	{
			//inty = intz / (globaldclip>>16)
		int t = SafeDivScale30 (oz1-s3, s4-s3+oz1-oz2);
		int inty = wallc->sz1 + MulScale30 (wallc->sz2-wallc->sz1,t);
		int intz = oz1 + MulScale30 (oz2-oz1,t);
		int xcross = wallc->sx1 + Scale (MulScale30 (wallc->sz2, t), wallc->sx2-wallc->sx1,inty);

		//t = divscale30((x1<<4)-xcross*yb1[w],xcross*(yb2[w]-yb1[w])-((x2-x1)<<4));
		//inty = yb1[w] + mulscale30(yb2[w]-yb1[w],t);
		//intz = z1 + mulscale30(z2-z1,t);

		if ((bad&12) == 8)
		{
			if (wallc->sx1 <= xcross) { z2 = intz; iy2 = inty; ix2 = xcross; }
			if (wallc->sx2 > xcross) clearbufshort (&mostbuf[xcross], wallc->sx2-xcross, viewheight);
		}
		else
		{
			if (xcross <= wallc->sx2) { z1 = intz; iy1 = inty; ix1 = xcross; }
			if (xcross > wallc->sx1) clearbufshort (&mostbuf[wallc->sx1], xcross-wallc->sx1, viewheight);
		}
	}

	y = Scale (z1>>4, InvZtoScale, iy1);
	if (ix2 == ix1)
	{
		mostbuf[ix1] = (short)((y + centeryfrac) >> FRACBITS);
	}
	else
	{
		fixed_t yinc = (Scale (z2>>4, InvZtoScale, iy2) - y) / (ix2-ix1);
		qinterpolatedown16short (&mostbuf[ix1], ix2-ix1, y + centeryfrac,yinc);
	}

	if (mostbuf[ix1] < 0) mostbuf[ix1] = 0;
	else if (mostbuf[ix1] > viewheight) mostbuf[ix1] = (short)viewheight;
	if (mostbuf[ix2-1] < 0) mostbuf[ix2-1] = 0;
	else if (mostbuf[ix2-1] > viewheight) mostbuf[ix2-1] = (short)viewheight;

	return bad;
}

static void PrepWallRoundFix(fixed_t *lwall, fixed_t walxrepeat, int x1, int x2)
{
	// fix for rounding errors
	walxrepeat = abs(walxrepeat);
	fixed_t fix = (MirrorFlags & RF_XFLIP) ? walxrepeat-1 : 0;
	int x;

	if (x1 > 0)
	{
		for (x = x1; x < x2; x++)
		{
			if ((unsigned)lwall[x] >= (unsigned)walxrepeat)
			{
				lwall[x] = fix;
			}
			else
			{
				break;
			}
		}
	}
	fix = walxrepeat - 1 - fix;
	for (x = x2-1; x >= x1; x--)
	{
		if ((unsigned)lwall[x] >= (unsigned)walxrepeat)
		{
			lwall[x] = fix;
		}
		else
		{
			break;
		}
	}
}

void PrepWall (fixed_t *swall, fixed_t *lwall, fixed_t walxrepeat, int x1, int x2)
{ // swall = scale, lwall = texturecolumn
	double top, bot, i;
	double xrepeat = fabs((double)walxrepeat);
	double depth_scale = WallT.InvZstep * WallTMapScale2;
	double depth_org = -WallT.UoverZstep * WallTMapScale2;

	i = x1 - centerx;
	top = WallT.UoverZorg + WallT.UoverZstep * i;
	bot = WallT.InvZorg + WallT.InvZstep * i;

	for (int x = x1; x < x2; x++)
	{
		double frac = top / bot;
		if (walxrepeat < 0)
		{
			lwall[x] = xs_RoundToInt(xrepeat - frac * xrepeat);
		}
		else
		{
			lwall[x] = xs_RoundToInt(frac * xrepeat);
		}
		swall[x] = xs_RoundToInt(frac * depth_scale + depth_org);
		top += WallT.UoverZstep;
		bot += WallT.InvZstep;
	}
	PrepWallRoundFix(lwall, walxrepeat, x1, x2);
}

void PrepLWall (fixed_t *lwall, fixed_t walxrepeat, int x1, int x2)
{ // lwall = texturecolumn
	double top, bot, i;
	double xrepeat = fabs((double)walxrepeat);
	double topstep;

	i = x1 - centerx;
	top = WallT.UoverZorg + WallT.UoverZstep * i;
	bot = WallT.InvZorg + WallT.InvZstep * i;

	top *= xrepeat;
	topstep = WallT.UoverZstep * xrepeat;

	for (int x = x1; x < x2; x++)
	{
		if (walxrepeat < 0)
		{
			lwall[x] = xs_RoundToInt(xrepeat - top / bot);
		}
		else
		{
			lwall[x] = xs_RoundToInt(top / bot);
		}
		top += topstep;
		bot += WallT.InvZstep;
	}
	PrepWallRoundFix(lwall, walxrepeat, x1, x2);
}

// pass = 0: when seg is first drawn
//		= 1: drawing masked textures (including sprites)
// Currently, only pass = 0 is done or used

static void R_RenderDecal (side_t *wall, DBaseDecal *decal, drawseg_t *clipper, int pass)
{
	fixed_t lx, ly, lx2, ly2, decalx, decaly;
	int x1, x2;
	fixed_t xscale, yscale;
	fixed_t topoff;
	BYTE flipx;
	fixed_t zpos;
	int needrepeat = 0;
	sector_t *front, *back;
	bool calclighting;
	bool rereadcolormap;
	FDynamicColormap *usecolormap;

	if (decal->RenderFlags & RF_INVISIBLE || !viewactive || !decal->PicNum.isValid())
		return;

	// Determine actor z
	zpos = decal->Z;
	front = curline->frontsector;
	back = (curline->backsector != NULL) ? curline->backsector : curline->frontsector;
	switch (decal->RenderFlags & RF_RELMASK)
	{
	default:
		zpos = decal->Z;
		break;
	case RF_RELUPPER:
		if (curline->linedef->flags & ML_DONTPEGTOP)
		{
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
		}
		else
		{
			zpos = decal->Z + back->GetPlaneTexZ(sector_t::ceiling);
		}
		break;
	case RF_RELLOWER:
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
		}
		else
		{
			zpos = decal->Z + back->GetPlaneTexZ(sector_t::floor);
		}
		break;
	case RF_RELMID:
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::floor);
		}
		else
		{
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
		}
	}

	xscale = decal->ScaleX;
	yscale = decal->ScaleY;

	WallSpriteTile = TexMan(decal->PicNum, true);
	flipx = (BYTE)(decal->RenderFlags & RF_XFLIP);

	if (WallSpriteTile == NULL || WallSpriteTile->UseType == FTexture::TEX_Null)
	{
		return;
	}

	// Determine left and right edges of sprite. Since this sprite is bound
	// to a wall, we use the wall's angle instead of the decal's. This is
	// pretty much the same as what R_AddLine() does.

	FWallCoords savecoord = WallC;

	x2 = WallSpriteTile->GetWidth();
	x1 = WallSpriteTile->LeftOffset;
	x2 = x2 - x1;

	x1 *= xscale;
	x2 *= xscale;

	decal->GetXY (wall, decalx, decaly);

	angle_t ang = R_PointToAngle2 (curline->v1->x, curline->v1->y, curline->v2->x, curline->v2->y) >> ANGLETOFINESHIFT;
	lx  = decalx - FixedMul (x1, finecosine[ang]) - viewx;
	lx2 = decalx + FixedMul (x2, finecosine[ang]) - viewx;
	ly  = decaly - FixedMul (x1, finesine[ang]) - viewy;
	ly2 = decaly + FixedMul (x2, finesine[ang]) - viewy;

	if (WallC.Init(lx, ly, lx2, ly2, TOO_CLOSE_Z))
		goto done;

	x1 = WallC.sx1;
	x2 = WallC.sx2;

	if (x1 >= clipper->x2 || x2 <= clipper->x1)
		goto done;

	WallT.InitFromWallCoords(&WallC);

	// Get the top and bottom clipping arrays
	switch (decal->RenderFlags & RF_CLIPMASK)
	{
	case RF_CLIPFULL:
		if (curline->backsector == NULL)
		{
			if (pass != 0)
			{
				goto done;
			}
			mceilingclip = walltop;
			mfloorclip = wallbottom;
		}
		else if (pass == 0)
		{
			mceilingclip = walltop;
			mfloorclip = ceilingclip;
			needrepeat = 1;
		}
		else
		{
			mceilingclip = openings + clipper->sprtopclip - clipper->x1;
			mfloorclip = openings + clipper->sprbottomclip - clipper->x1;
		}
		break;

	case RF_CLIPUPPER:
		if (pass != 0)
		{
			goto done;
		}
		mceilingclip = walltop;
		mfloorclip = ceilingclip;
		break;

	case RF_CLIPMID:
		if (curline->backsector != NULL && pass != 2)
		{
			goto done;
		}
		mceilingclip = openings + clipper->sprtopclip - clipper->x1;
		mfloorclip = openings + clipper->sprbottomclip - clipper->x1;
		break;

	case RF_CLIPLOWER:
		if (pass != 0)
		{
			goto done;
		}
		mceilingclip = floorclip;
		mfloorclip = wallbottom;
		break;
	}

	topoff = WallSpriteTile->TopOffset << FRACBITS;
	dc_texturemid = topoff + FixedDiv (zpos - viewz, yscale);

	// Clip sprite to drawseg
	x1 = MAX<int>(clipper->x1, x1);
	x2 = MIN<int>(clipper->x2, x2);
	if (x1 >= x2)
	{
		goto done;
	}

	PrepWall (swall, lwall, WallSpriteTile->GetWidth() << FRACBITS, x1, x2);

	if (flipx)
	{
		int i;
		int right = (WallSpriteTile->GetWidth() << FRACBITS) - 1;

		for (i = x1; i < x2; i++)
		{
			lwall[i] = right - lwall[i];
		}
	}

	// Prepare lighting
	calclighting = false;
	usecolormap = basecolormap;
	rereadcolormap = true;

	// Decals that are added to the scene must fade to black.
	if (decal->RenderStyle == LegacyRenderStyles[STYLE_Add] && usecolormap->Fade != 0)
	{
		usecolormap = GetSpecialLights(usecolormap->Color, 0, usecolormap->Desaturate);
		rereadcolormap = false;
	}

	rw_light = rw_lightleft + (x1 - WallC.sx1) * rw_lightstep;
	if (fixedlightlev >= 0)
		dc_colormap = usecolormap->Maps + fixedlightlev;
	else if (fixedcolormap != NULL)
		dc_colormap = fixedcolormap;
	else if (!foggy && (decal->RenderFlags & RF_FULLBRIGHT))
		dc_colormap = usecolormap->Maps;
	else
		calclighting = true;

	// Draw it
	if (decal->RenderFlags & RF_YFLIP)
	{
		sprflipvert = true;
		yscale = -yscale;
		dc_texturemid = dc_texturemid - (WallSpriteTile->GetHeight() << FRACBITS);
	}
	else
	{
		sprflipvert = false;
	}

	// rw_offset is used as the texture's vertical scale
	rw_offset = SafeDivScale30(1, yscale);

	do
	{
		dc_x = x1;
		ESPSResult mode;

		mode = R_SetPatchStyle (decal->RenderStyle, decal->Alpha, decal->Translation, decal->AlphaColor);

		// R_SetPatchStyle can modify basecolormap.
		if (rereadcolormap)
		{
			usecolormap = basecolormap;
		}

		if (mode == DontDraw)
		{
			needrepeat = 0;
		}
		else
		{
			int stop4;

			if (mode == DoDraw0)
			{ // 1 column at a time
				stop4 = dc_x;
			}
			else	 // DoDraw1
			{ // up to 4 columns at a time
				stop4 = x2 & ~3;
			}

			while ((dc_x < stop4) && (dc_x & 3))
			{
				if (calclighting)
				{ // calculate lighting
					dc_colormap = usecolormap->Maps + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
				}
				R_WallSpriteColumn (R_DrawMaskedColumn);
				dc_x++;
			}

			while (dc_x < stop4)
			{
				if (calclighting)
				{ // calculate lighting
					dc_colormap = usecolormap->Maps + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
				}
				rt_initcols();
				for (int zz = 4; zz; --zz)
				{
					R_WallSpriteColumn (R_DrawMaskedColumnHoriz);
					dc_x++;
				}
				rt_draw4cols (dc_x - 4);
			}

			while (dc_x < x2)
			{
				if (calclighting)
				{ // calculate lighting
					dc_colormap = usecolormap->Maps + (GETPALOOKUP (rw_light, wallshade) << COLORMAPSHIFT);
				}
				R_WallSpriteColumn (R_DrawMaskedColumn);
				dc_x++;
			}
		}

		// If this sprite is RF_CLIPFULL on a two-sided line, needrepeat will
		// be set 1 if we need to draw on the lower wall. In all other cases,
		// needrepeat will be 0, and the while will fail.
		mceilingclip = floorclip;
		mfloorclip = wallbottom;
		R_FinishSetPatchStyle ();
	} while (needrepeat--);

	colfunc = basecolfunc;
	hcolfunc_post1 = rt_map1col;
	hcolfunc_post4 = rt_map4cols;

	R_FinishSetPatchStyle ();
done:
	WallC = savecoord;
}

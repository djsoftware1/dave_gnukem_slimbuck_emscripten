// SDL-based graphics routines
/*
djgraph.cpp

Copyright (C) 1997-2019 David Joffe
*/


// The reason I can't use precompiled headers through stdafx.h seems
// to be some sort of stupid visual c++ compiler bug that makes:
//  #ifdef WIN32
//  #include "stdafx.h"
//  #endif
// not work.

#include "../djgraph.h"
#include "../sys_log.h"
#include "SDL.h"
#include "../config.h"//[For CFG_APPLICATION_RENDER_RES_W etc. dj2019-06 slightly ugly dependency direction, conceptually, but not the biggest thing in the world to worry about now, maybe later.]

#include <string.h>

// [dj2016-10] For 'texture' manager'
#include <map>
std::map< djImage*, SDL_Surface *> g_SurfaceMap;

int	rMask=0, gMask=0, bMask=0, aMask=0;
int	rShift=0, gShift=0, bShift=0, aShift=0;

// 0=default
// 1=EGA
// 2=CGA (simulated retro)
int g_nSimulatedGraphics = 0;

// CGA palette 1 (4 colors) cyan magenta black white
const djColor djPALETTE_CGA[4] = {
	djColor(0,0,0),//black
	djColor(0x55,0xFF,0xFF),//bright cyan
	djColor(0xFF,0x55,0xFF),//bright magenta
	djColor(0xFF,0xFF,0xFF)//bright white
	// DITHERTEST
	//djColor(0x55,0x55,0x55)//dark grey as white/black dither
};
// EGA standard 16-colors
const djColor djPALETTE_EGA[16] = {
	djColor(0,0,0),//black
	djColor(0,0,0xAA),//blue
	djColor(0,0xAA,0),//green
	djColor(0,0xAA,0xAA),//cyan
	djColor(0xAA,0,0),//red
	djColor(0xAA,0,0xAA),//magenta
	djColor(0xAA,0x55,0),//brown
	djColor(0xAA,0xAA,0xAA),//light grey
	djColor(0x55,0x55,0x55),//dark grey
	djColor(0x55,0x55,0xFF),//bright blue
	djColor(0x55,0xFF,0x55),//bright green
	djColor(0x55,0xFF,0xFF),//bright cyan
	djColor(0xFF,0x55,0x55),//bright red
	djColor(0xFF,0x55,0xFF),//bright magenta
	djColor(0xFF,0xFF,0x55),//bright yellow
	djColor(0xFF,0xFF,0xFF)//bright white
};

unsigned int djgMapColor( djVisual * pVis, const djColor& color )
{
	return SDL_MapRGB(pVis->pSurface->format, color.r, color.g, color.b);
//	unsigned int ret = 0;
//
//	switch ( pVis->bpp )
//	{
//	case 16:
//		// FIXME: assume 5-6-5 = wrong
//// DEBUG: changed by rtfb
//
//		ret = ((color.r/8)<<11) | ((color.g/4)<<5) | (color.b/8);
///*/
//		ret = ((color.r/8)<<10) | ((color.g/4)<<5) | (color.b/8);
///**/
//		break;
//	case 24:
//	case 32:
//		ret = 0xFF000000 | (color.r<<16) | (color.g<<8) | color.b;
//		break;
//	}
//
//	return ret;
}

djVisual* djgOpenVisual( const char *vistype, int w, int h, int bpp, bool bBackbuffer )
{
	// Create a djVisual
	djVisual * pVis;
	pVis = new djVisual;

	pVis->m_bFullscreen = false;

	//dj2016-10 fixmelow_tocheck i am wondering if the below is doing speed optimal way of doing everything as this hasn't been looked at in years

	// Create a default visual, just a plain non-resizing window
	//static SDL_Surface *p = NULL;
	if (NULL == vistype)
	{
		/*p = */pVis->pSurface = SDL_SetVideoMode(w, h, bpp, SDL_HWSURFACE|(bBackbuffer?SDL_DOUBLEBUF:0));
	}
	else if (0 == strcmp( vistype, "memory" ))
	{
		SDL_Surface *pSurface = SDL_CreateRGBSurface(SDL_HWSURFACE, w, h,
			bpp,
			//fixme [dj2016-10] this doesn't quite seem right to me ? must come back to this and have a closer look at all this again later ..
			// in theory this works but might be less efficient
			0xFF000000,
			0x00FF0000,
			0x0000FF00,
			0x000000FF);
		if (pSurface!=NULL)
		{
			//SDL_SetColorKey(pSurface, SDL_SRCCOLORKEY|SDL_RLEACCEL, SDL_MapRGB(pSurface->format, 255, 0, 255));
			pVis->pSurface = SDL_DisplayFormatAlpha(pSurface);
			SDL_FreeSurface(pSurface);
		}
	}
	else if (0 == strcmp( vistype, "fullscreen" ))
	{
		pVis->pSurface = SDL_SetVideoMode(w, h, bpp, SDL_HWSURFACE|(bBackbuffer?SDL_DOUBLEBUF:0)|SDL_FULLSCREEN);
		pVis->m_bFullscreen = true;
	}

	pVis->bpp = pVis->pSurface->format->BitsPerPixel;
	pVis->width = w;
	pVis->height = h;
	pVis->stride = w * (bpp/8);
	switch (pVis->bpp)
	{
	case  8: pVis->pixwidth = 1; break;
	case 16: pVis->pixwidth = 2; break;
	case 24: pVis->pixwidth = 3; break; // FIXME: 4?
	case 32: pVis->pixwidth = 4; break;
	}

	return pVis;
}

void djgCloseVisual( djVisual * pVis )
{
	SDL_FreeSurface(pVis->pSurface);
	pVis->pSurface = NULL;//?<- dj2017-01 not sure if this is all that's necessary for cleanup here ...
}

void djgLock( djVisual * pVis )
{
	SDL_LockSurface(pVis->pSurface);
}

void djgUnlock( djVisual * pVis )
{
	SDL_UnlockSurface(pVis->pSurface);
}


void djgFlush( djVisual * /*pVis*/)
{
	//pVis = pVis;		// shut up the "unused" warning
}

void djgFlip( djVisual * pVisDest, djVisual * pVisSrc, bool bScaleView )
{
	if (pVisSrc==NULL)//<- Level editor etc.
	{
		SDL_Flip(pVisDest->pSurface);
	}
	else
	{
		// [dj2016-10-08] If have large modern monitor then 320x200 window is really tiny and unpleasant to play - added
		// this quick-n-dirty scaling blit to let the main window be any 'arbitrary' larger resolution to allow the
		// gameplay window to at least be larger - my idea/thinking is just to e.g. try create the main window sort of
		// (basically) the largest 'multiple' of 320x200 (ideally incorporating window dressing) that fits in your
		// screen ... this is not entirely perfect but is a quick and easy way to get the game relatively playable
		// as compared to the tiny gameplay window we have now (and also, NB, makes level editing much more
		// user-friendly).
		// Note pVisSrc->width could be eg 1600 while gamerendering is 320
		CdjRect rcSrc(0, 0, CFG_APPLICATION_RENDER_RES_W, CFG_APPLICATION_RENDER_RES_H);//E.g. 320x200 for DG1
		CdjRect rcDest(0, 0, pVisDest->width, pVisDest->height);
		unsigned int uScaleX = (pVisDest->width / CFG_APPLICATION_RENDER_RES_W); // Note we deliberately do *integer* division as we *want* to round down etc.
		unsigned int uScaleY = (pVisDest->height / CFG_APPLICATION_RENDER_RES_H); // Note we deliberately do *integer* division as we *want* to round down etc.
		unsigned int uScaleMax = djMAX(1,djMIN(uScaleX,uScaleY));//Select smallest of vertical/horizontal scaling factor in order to fit everything in the window
		SDL_Rect rc;
		rc.w = uScaleMax;
		rc.h = uScaleMax;

		//dj2019-07 Hm leaving this change commented out for now (retro settings at very high resolution CFG_APPLICATION_RENDER_RES_W etc.) as it's very slow, and not necessary for anything yet, so still rethink this stuff; want to keep it clean/simple as possible or later this will just seem like spaghetti & I won't know what's what.
		/*
		bool bForceOwnScaleBlitHereEvenIf1to1=false;//dj2019-06
		#if SDL_BYTEORDER==SDL_BIG_ENDIAN
		#else
		// If retro settings requested, 'force' use own blit here so we can do the effect, even if it's a 1-1 pixel ratio [dj2019-06]
		// This is really not that important, it's just 'silly' stuff for the extra-retro settings, but the 'force' option might
		// come in handy later for other viewport-related stuff for genericizing this stuff, not sure.
		if (g_nSimulatedGraphics>0) //'Simulate' CGA/EGA
			bForceOwnScaleBlitHereEvenIf1to1 = true;
		#endif
		//*/

		if (bScaleView
			&& (/*bForceOwnScaleBlitHereEvenIf1to1 || */rcSrc.w!=rcDest.w || rcSrc.h!=rcDest.h)
		//fixme is this righT? waht if bpp == 2 ..
			&& pVisSrc->pSurface->format->BytesPerPixel == 4//Current scaling blit implementation only supports 4BPP [dj2016-10] [TODO: Handle other format, OR if upgrading to libsdl2, just use libsdl's scale blit function]
			)
		{
			//Quick-n-dirty scaling blit [dj2016-10] NB note that if/when we migrate to LibSDL2 we can just use the API functions for this instead
			djgLock(pVisSrc);
			djgLock(pVisDest);
			unsigned int uBPP = pVisSrc->pSurface->format->BytesPerPixel;
			unsigned int *pSurfaceMem = (unsigned int*)pVisSrc->pSurface->pixels;
			unsigned int uMemOffsetRow = 0;


			//fixme this won't work bigendian
			#if SDL_BYTEORDER==SDL_BIG_ENDIAN
			// Not yet supported for big-endian platforms (dj2019-06)
			if (false)
			#else
			if (g_nSimulatedGraphics>0) //'Simulate' CGA/EGA
			#endif
			{
				// Select target simulated-graphics palette
				const unsigned int NUMCOLORS = (g_nSimulatedGraphics==1?16:4);
				const djColor* pPalette = (g_nSimulatedGraphics==1 ? djPALETTE_EGA : djPALETTE_CGA);
				
				register int nPixel;
				// For finding closest-matching pixel in target simulated mode palette
				register int nDistance=0;
				register int nDistanceMin=-1;
				register int nClosest = 0;//black
				
				rc.y=0;
				for ( unsigned int y=0; y<CFG_APPLICATION_RENDER_RES_H; ++y )
				{
					uMemOffsetRow = (y * (pVisSrc->pSurface->pitch/uBPP));
					pSurfaceMem = ((unsigned int*)pVisSrc->pSurface->pixels) + uMemOffsetRow;
					// Note we must be careful here, pVisSrc->pSurface->pitch is in *bytes*, pSurfaceMem is a pointer to unsigned int* so pointer 'math' in multiples of 4
					rc.x = 0;
					for ( unsigned int x=0; x<CFG_APPLICATION_RENDER_RES_W; ++x )
					{
						// Getpixel color from source
						nPixel = *pSurfaceMem;
						djColor Color(
							(nPixel & 0xFF0000) >> 16,//red
							(nPixel & 0xFF00) >> 8,//green
							(nPixel & 0xFF));//blue

						// Look for closest approximate match in simulated palette
						nDistance=0;
						nDistanceMin=-1;
						nClosest = 0;//black
						for (unsigned int z=0;z<NUMCOLORS;++z)
						{
							nDistance =
								(int)ABS((int)Color.r - (int)(pPalette[z]).r) + 
								(int)ABS((int)Color.g - (int)(pPalette[z]).g) +
								(int)ABS((int)Color.b - (int)(pPalette[z]).b);
							if (nDistanceMin==-1 || nDistance<nDistanceMin)
							{
								nDistanceMin = nDistance;
								nClosest = z;
							}
						}
						//cga dark gray
						/*
						if (g_nSimulatedGraphics==2 && nClosest==4)
						{
							if ((x%2) ^ (y%2))
								nClosest = 0;//dither alternating black
							else
								nClosest = 3;//dither alternating white
						}
						*/

						nPixel = SDL_MapRGB(pVisDest->pSurface->format,pPalette[nClosest].r,pPalette[nClosest].g,pPalette[nClosest].b);//djgMapColor( pVis, pVis->colorfore );
						SDL_FillRect(pVisDest->pSurface, &rc, nPixel);
						++pSurfaceMem;
						rc.x += uScaleMax;
					}//x
					rc.y += uScaleMax;
				}//y
			}
			else//Normal default
			{
				//cbsu/sbsu?[low-dj2019-06]
				rc.y=0;
				for ( unsigned int y=0; y<CFG_APPLICATION_RENDER_RES_H; ++y )
				{
					uMemOffsetRow = (y * (pVisSrc->pSurface->pitch/uBPP));
					pSurfaceMem = ((unsigned int*)pVisSrc->pSurface->pixels) + uMemOffsetRow;
					// Note we must be careful here, pVisSrc->pSurface->pitch is in *bytes*, pSurfaceMem is a pointer to unsigned int* so pointer 'math' in multiples of 4
					rc.x = 0;
					for ( unsigned int x=0; x<CFG_APPLICATION_RENDER_RES_W; ++x )
					{
						SDL_FillRect(pVisDest->pSurface, &rc, *pSurfaceMem);
						++pSurfaceMem;
						rc.x += uScaleMax;
					}
					rc.y += uScaleMax;
				}
			}
			djgUnlock(pVisDest);
			djgUnlock(pVisSrc);
		}
		else
		{
			CdjRect rcSrc(0, 0, pVisSrc->width, pVisSrc->height);
			//Non-scaling blit [faster, but can only do if src same size as dest]
			SDL_BlitSurface(pVisSrc->pSurface, &rcSrc, pVisDest->pSurface, &rcDest);
		}
		SDL_Flip(pVisDest->pSurface);
	}
}

void djgClear( djVisual * pVis )
{
	SDL_Rect rc;
	rc.x = 0;
	rc.y = 0;
	rc.w = pVis->width;
	rc.h = pVis->height;
	SDL_FillRect(pVis->pSurface, &rc, djgMapColor(pVis, djColor(0,0,0)));
}

void djgPutPixel( djVisual * pVis, int x, int y, int r, int g, int b )
{
	//djColor       col;
	//col = djColor( r, g, b );
	//pixel = djgMapColor( pVis, col );
	unsigned int pixel = SDL_MapRGB(pVis->pSurface->format, r, g, b);
	SDL_Rect rc;
	rc.x = x;
	rc.y = y;
	rc.w = 1;
	rc.h = 1;
	SDL_FillRect(pVis->pSurface, &rc, pixel);
}

void djgPutPixel( djVisual * pVis, int x, int y, const djColor& color )
{
	djgPutPixel(pVis, x, y, color.r, color.g, color.b);
}

void djgSetColorFore( djVisual * pVis, const djColor& color )
{
	pVis->colorfore.r = color.r;
	pVis->colorfore.g = color.g;
	pVis->colorfore.b = color.b;
}

void djgSetColor( djVisual *pVis, const djColor& clrFore, const djColor& clrBack )
{
	djgSetColorFore( pVis, clrFore );
	djgSetColorBack( pVis, clrBack );
}

void djgSetColorBack( djVisual * pVis, const djColor& color )
{
	pVis->colorback.r = color.r;
	pVis->colorback.g = color.g;
	pVis->colorback.b = color.b;
}

void djgDrawRectangle( djVisual * pVis, int x, int y, int w, int h )
{
	// FIXME: TEST THESE BOUNDS CHECKING
//	if (x<0) x=0;
//	if (y<0) y=0;
//	if (x+w>=pVis->width+1) w = pVis->width - x;
//	if (y+h>=pVis->height+1) h = pVis->height - y;
	djgDrawHLine( pVis, x  , y  , w );
	djgDrawHLine( pVis, x  , y+h-1, w );
	djgDrawVLine( pVis, x  , y  , h );
	djgDrawVLine( pVis, x+w-1, y  , h );
}

void djgDrawBox( djVisual * pVis, int x, int y, int w, int h )
{
	unsigned int pixel = SDL_MapRGB(pVis->pSurface->format, pVis->colorfore.r, pVis->colorfore.g, pVis->colorfore.b);//djgMapColor( pVis, pVis->colorfore );
	CdjRect rc(x, y, w, h);
	SDL_FillRect(pVis->pSurface, &rc, pixel);
}

void djgDrawHLine( djVisual * pVis, int x, int y, int n )
{
	if (y<0 || y>=pVis->height) return;
	if (x<0) { n=n+x; if (n<=0) return; x=0; }
	if (x+n>pVis->width) { n=pVis->width-x; }
	if (n<=0) return;

	unsigned int pixel = SDL_MapRGB(pVis->pSurface->format, pVis->colorfore.r, pVis->colorfore.g, pVis->colorfore.b);//djgMapColor( pVis, pVis->colorfore );

	djgLock( pVis );
	for ( int i=0; i<n; i++ )
		memcpy( (char*)pVis->pSurface->pixels + y*pVis->stride + (x+i)*pVis->pixwidth, &pixel, pVis->pixwidth );
	djgUnlock( pVis );

}

void djgDrawVLine( djVisual * pVis, int x, int y, int n )
{
	if (x<0 || x>=pVis->width) return;
	if (y<0) { n=n+y; if (n<=0) return; y=0; }
	if (y+n>pVis->height) { n=pVis->height-y; }
	if (n<=0) return;

	unsigned int pixel = SDL_MapRGB(pVis->pSurface->format, pVis->colorfore.r, pVis->colorfore.g, pVis->colorfore.b);//djgMapColor( pVis, pVis->colorfore );

	djgLock( pVis );
	for ( int i=0; i<n; i++ )
		memcpy( (char*)pVis->pSurface->pixels + (y+i)*pVis->stride + x*pVis->pixwidth, &pixel, pVis->pixwidth );
	djgUnlock( pVis );

}

void djgDrawImage( djVisual *pVis, djImage *pImage, int x, int y, int w, int h )
{
	djgDrawImage( pVis, pImage, 0, 0, x, y, w, h );
}

void djgDrawImage( djVisual *pVis, djImage *pImage, int xS, int yS, int xD, int yD, int w, int h )
{
	if (pImage==NULL) return;

	if (xS<0 || yS<0) return;


	// IF HARDWARE SURFACE FOR THIS IMAGE, USE IT
	SDL_Surface* pHWSurface = NULL;
	std::map< djImage*, SDL_Surface * >::const_iterator iter = g_SurfaceMap.find( pImage );
	if (iter != g_SurfaceMap.end())
	{
		pHWSurface = iter->second;
	}
	if (pHWSurface)
	{
		//BLIT
		SDL_Rect rectSrc;
		rectSrc.x = xS;
		rectSrc.y = yS;
		rectSrc.w = w;
		rectSrc.h = h;
		SDL_Rect rectDest;
		rectDest.x = xD;
		rectDest.y = yD;
		rectDest.w = w;
		rectDest.h = h;
		SDL_BlitSurface(pHWSurface, &rectSrc, pVis->pSurface, &rectDest);
		return;
	}



	// clipping
	if (xD>=pVis->width || yD>=pVis->height) return;
	if (xD+w>pVis->width)  { w = pVis->width-xD; }
	if (yD+h>pVis->height) { h = pVis->height-yD; }
	if (xD<0) { w=w+xD; if (w<=0) return; xD=0; }
	if (yD<0) { h=h+yD; if (h<=0) return; yD=0; }

	// If same pixel formats, just copy
	if (pVis->bpp==pImage->BPP())
	{
		djgLock(pVis);

		for ( int i=0; i<h; i++ )
		{
			memcpy(
				(char*)pVis->pSurface->pixels + (yD+i)*pVis->stride + xD*pVis->pixwidth,
				pImage->Data() + (yS+i)*pImage->Pitch()+xS*pVis->pixwidth,
				w * pVis->pixwidth );
		}

		djgUnlock(pVis);

		return;
	}

	// 32-bit, no transparency
	if (pVis->bpp==32)
	{
		if (pImage->BPP()==16)
		{
		}
	}
	if (pVis->bpp==16)
	{
		// 32-bit, no transparency
		if (pImage->BPP()==32)
		{
			djgLock( pVis );
			for ( int i=0; i<h; i++ )
			{
				int iOffsetSrc = (yS+i)*pImage->Pitch() + xS*4;
				int iOffsetDest = (yD+i)*pVis->stride + xD*2;
				unsigned int   *pSrc = (unsigned int*)((char*)pImage->Data() + iOffsetSrc);
				unsigned short *pDest = (unsigned short*)((char*)pVis->pSurface->pixels + iOffsetDest);
				for ( int j=0; j<w; j++ )
				{
					*pDest = PIXEL32TO16(*pSrc);
					pSrc++;
					pDest++;
				}
			}
			djgUnlock( pVis );
			return;
		}
	}
}




void djgDrawImage2( djVisual *v, djImage *img, int xS, int yS, int xD, int yD, int w, int h )
{

	// IF HARDWARE SURFACE FOR THIS IMAGE, USE IT
	SDL_Surface* pHWSurface = NULL;
	std::map< djImage*, SDL_Surface * >::const_iterator iter = g_SurfaceMap.find( img );
	if (iter != g_SurfaceMap.end())
	{
		pHWSurface = iter->second;
	}
	if (pHWSurface)
	{
		//BLIT
		SDL_Rect rectSrc;
		rectSrc.x = xS;
		rectSrc.y = yS;
		rectSrc.w = w;
		rectSrc.h = h;
		SDL_Rect rectDest;
		rectDest.x = xD;
		rectDest.y = yD;
		rectDest.w = w;
		rectDest.h = h;
		SDL_BlitSurface(pHWSurface, &rectSrc, v->pSurface, &rectDest);
		return;
	}

	djgLock( v );
	for ( int i=0; i<h; i++ )
	{
		int iOffsetSrc = (yS+i)*img->Pitch() + xS*4;
		int iOffsetDest = (yD+i)*v->stride + xD*2;
		unsigned int   *src = (unsigned int*)((char*)img->Data() + iOffsetSrc);
		unsigned short *dst = (unsigned short*)((char*)v->pSurface->pixels + iOffsetDest);
		for ( int j=0; j<w; j++ )
		{
			*dst = PIXEL32TO16(*src);
			src++;
			dst++;
		}
	}
/**/
	djgUnlock( v );
	return;
}




void djgDrawImageAlpha( djVisual *pVis, djImage *pImage, int xS, int yS, int xD, int yD, int w, int h )
{
	if (pImage==NULL) return;

	// IF HARDWARE SURFACE FOR THIS IMAGE, USE IT
	SDL_Surface* pHWSurface = NULL;
	std::map< djImage*, SDL_Surface * >::const_iterator iter = g_SurfaceMap.find( pImage );
	if (iter != g_SurfaceMap.end())
	{
		pHWSurface = iter->second;
	}
	if (pHWSurface)
	{
		//BLIT
		SDL_Rect rectSrc;
		rectSrc.x = xS;
		rectSrc.y = yS;
		rectSrc.w = w;
		rectSrc.h = h;
		SDL_Rect rectDest;
		rectDest.x = xD;
		rectDest.y = yD;
		rectDest.w = w;
		rectDest.h = h;
		SDL_BlitSurface(pHWSurface, &rectSrc, pVis->pSurface, &rectDest);
		return;
	}
				//dj2018-01 in theory the code from this point below should 'never'
				// activate anymore? I think? Or are there platforms where it might?
	// i.e. ideally we just want to stick to doing hardware blits (or "proper correct" software fall-back in worst case - not our crappy software blit, basically :D)
				//fixmeMedium
	/* comment comment dj2018-01-13
	djgDrawImageAlpha: Tentatively remove 'fallback' old sprite drawing code - in theory this could break some platforms if the new g_SurfaceMap stuff fails - if that happens I think we should discover that through testing, and handle it on ad hoc basis - e.g. either add the fallback code back (not ideal, for multiple reasons, including speed, and lack of proper support for semi-transparent alpha, possible lack of support for all pixel formats etc.), or figure out why the new g_SurfaceMap stuff isn't working on whatever new platform, and fix it (more ideal).
	*/
				return;

	// clipping
	if (xD>=pVis->width || yD>=pVis->height) return;
	if (xD+w>pVis->width)  { w = pVis->width-xD; }
	if (yD+h>pVis->height) { h = pVis->height-yD; }
	if (xD<0) { w=w+xD; if (w<=0) return; xD=0; }
	if (yD<0) { h=h+yD; if (h<=0) return; yD=0; }

	// 32-bit, alpha map
	if (pVis->bpp==32)
	{
		if (pImage->BPP()==32)
		{
			djgLock( pVis );

			for ( int i=0; i<h; i++ )
			{
				int iOffsetSrc = (yS+i)*pImage->Pitch() + xS*4;
				int iOffsetDest = (yD+i)*pVis->stride + xD*4;
				unsigned int *pSrc = (unsigned int*)((char*)pImage->Data() + iOffsetSrc);
				unsigned int *pDest = (unsigned int*)((char*)pVis->pSurface->pixels + iOffsetDest);
				for ( int j=0; j<w; j++ )
				{
					unsigned int pixel = *pSrc;
					// If alpha value non-zero
					if (pixel & 0xFF000000)
					{
						*pDest = pixel;
					}
					pSrc++;
					pDest++;
				}
			}

			djgUnlock( pVis );
			return;
		}
	}
	if (pVis->bpp==16)
	{
		if (pImage->BPP()==32)
		{
			djgLock( pVis );

			for ( int i=0; i<h; i++ )
			{
				int iOffsetSrc = (yS+i)*pImage->Pitch() + xS*4;
				int iOffsetDest = (yD+i)*pVis->stride + xD*2;
				unsigned int   *pSrc = (unsigned int*)((char*)pImage->Data() + iOffsetSrc);
				unsigned short *pDest = (unsigned short*)((char*)pVis->pSurface->pixels + iOffsetDest);
				for ( int j=0; j<w; j++ )
				{
					unsigned int pixel = *pSrc;
					// If alpha value non-zero
					if (pixel & 0xFF000000)
					{
						*pDest = PIXEL32TO16(pixel);
					}
					pSrc++;
					pDest++;
				}
			}

			djgUnlock( pVis );
			return;
		}
	}
}

void djgDrawVisual( djVisual *pDest, djVisual *pSrc, int xD, int yD, int xS, int yS, int w, int h )
{
	CdjRect rcSrc(xS, yS, w, h );
	CdjRect rcDest(xD, yD, w, h );

	SDL_BlitSurface(pSrc->pSurface, &rcSrc, pDest->pSurface, &rcDest);
}

void SetPixelConversion ( djVisual *vis )
{
	SDL_PixelFormat *f = vis->pSurface->format;

	Log ( "Setting up pixel conversion attributes...:\n" );
	rMask = f->Rmask;
	gMask = f->Gmask;
	bMask = f->Bmask;
	aMask = f->Amask;
	rShift = f-> Rshift;
	gShift = f-> Gshift;
	bShift = f-> Bshift;
	aShift = f-> Ashift;

	Log ( "\t[RGBA]Mask: 0x%x; 0x%x; 0x%x 0x%x\n", rMask, gMask, bMask, aMask );
	Log ( "\t[RGBA]Shift: 0x%x; 0x%x; 0x%x 0x%x\n", rShift, gShift, bShift, aShift );
}

void djDestroyImageHWSurface( djImage* pImage )
{
	if (pImage==NULL) return;

	std::map< djImage*, SDL_Surface *>::const_iterator iter = g_SurfaceMap.find( pImage );
	if (iter == g_SurfaceMap.end())
		return; // Hardware texture for this image not found

	// Delete the associated hardware surface
	SDL_Surface* pHardwareSurface = iter->second;
	SDL_FreeSurface(pHardwareSurface);
	//? ?delete pHardwareSurface;
	pHardwareSurface = NULL;

	// Remove from 'map'
	g_SurfaceMap.erase( pImage );
}
bool djCreateImageHWSurface( djImage* pImage/*, djVisual* pVisDisplayBuffer*/ )
{
	//extern djVisual* pVisView;
	//djVisual* pVisDisplayBuffer = pVisView;
	if (pImage==NULL) return false;

	//fixmeLOW should ideally warn or assert or something if pImage already in map here??? [dj2017-06-20]

	//fixme to check are these actually hardware surfaces

	/*
	dj2019-06: There may still be something else wrong here re SDL_BIG_ENDIAN patch for issue 100 below.
	macppc OpenBSD users reported having to seemingly (as I interpret it) BeWorld2018 fix for issue 100,
	to get it working on OpenBSD macppc, or they get blank screen ... see here for more info:

	http://openbsd-archive.7691.n7.nabble.com/NEW-games-gnukem-td365426.html

	NB: (If I'm reading that Raphael Graf patch correctly, I can't incorporate that OpenBSD macppc patch 'blindly'
	here without breaking it again on MorphOS - so this needs further investigation as to what exactly is going on,
	and the most 'correct' way to fix it. If macpcc has SDL_BYTEORDER==SDL_BIG_ENDIAN then why the blank screen,
	why must we pass little-endian bitmasks, could it be something like graphics hardware byte order different from
	CPU byte order? Not sure. Or could it be an issue with libsdl1.2 on macppc itself (less likely).
	Unfortunately I cannot test on PPC MorphOS which makes it difficult to just try things out as I have no way to see
	how they impact that configuration.
	Or could it be something to do with how we load images into RAM.
	See also:
	https://github.com/davidjoffe/dave_gnukem/issues/100
	https://wiki.libsdl.org/CategoryEndian
	https://wiki.libsdl.org/SDL_CreateRGBSurface
	*/

	SDL_Surface* pSurfaceFoo = ::SDL_CreateRGBSurfaceFrom(
		pImage->Data(),
		pImage->Width(),
		pImage->Height(),
		32,
		pImage->Pitch(),
		// R,G,B,A masks (dj2018-03 specify different masks here on PPC etc. - see https://github.com/davidjoffe/dave_gnukem/issues/100 - thanks to @BeWorld2018 for report and patch suggestion)
		#if SDL_BYTEORDER==SDL_BIG_ENDIAN
		0x0000FF00, 0X00FF0000, 0xFF000000, 0x000000FF
		#else
		0xFF0000,
		0xFF00,
		0xFF,
		0xFF000000
		#endif
	);
	g_SurfaceMap[ pImage ] = pSurfaceFoo;

	return true;







	/*	SDL_SetAlpha(pVisDisplayBuffer->pSurface, SDL_SRCALPHA, 255);
	SDL_Surface* pSurfaceImg2 = ::SDL_CreateRGBSurfaceFrom(
		pImage->Data(),
		pImage->Width(),
		pImage->Height(),
		32,
		pImage->Pitch(),//		pImage->Width() * 4,
		0xFF0000,
		0xFF00,
		0xFF,
		0xFF000000);
	if (pSurfaceImg2)
	{
		SDL_Surface* pSurface = SDL_ConvertSurface(pSurfaceImg2,
			pVisDisplayBuffer->pSurface->format,
                                0);
		if (pSurface)
		{
//SDL_SetAlpha(pSurface, SDL_SRCALPHA, 0);
			g_SurfaceMap[ pImage ] = pSurface;




		SDL_LockSurface(pSurface);
		SDL_LockSurface(pSurfaceImg2);
		unsigned int *pSurfaceMemImg = (unsigned int*)pSurfaceImg2->pixels;
		unsigned int *pSurfaceMem = (unsigned int*)pSurface->pixels;
		//unsigned int *pSurfaceMem2 = (unsigned int*)pSurfaceImg->pixels;
		for ( int y=0; y<pImage->Height(); ++y )
		{
			for ( int i=0; i<pImage->Width(); ++i )
			{
				if ((y%3)==0)
				{
					*(pSurfaceMem + i + ((y*pSurface->pitch)/4)) = 0x50FF8822;
					if (y>0)
						*(pSurfaceMem + i + (((y-1)*pSurface->pitch)/4)) = 0x0000FF00;
					//unsigned int vOrig = *(pSurfaceMemImg + i + ((y*pSurfaceImg2->pitch)/4));
					//if ((vOrig & 0xFF000000)==0)
					// *(pSurfaceMem + i + ((y*pSurface->pitch)/4)) = *(pSurfaceMem + i + ((y*pSurface->pitch)/4))  &  0x00FFFFFF;
				}
				// *(pSurfaceMem2 + i + ((y*pSurface->pitch)/4)) = 0x00FF000000;
			}
		}
		SDL_UnlockSurface(pSurfaceImg2);
		SDL_UnlockSurface(pSurface);






			return true;
		}
		return false;
	}
	return false;
*/


								
								
								
								
								// TO CHECK - if fail to create in hardware, will it automatically created in SW or must we do that ourselves (!?)s
	SDL_Surface* pSurface = ::SDL_CreateRGBSurface(
SDL_SWSURFACE,
		pImage->Width(),
		pImage->Height(),
		32,//pVisDisplayBuffer->pSurface->format->BitsPerPixel,
		0xFF0000,//pVisDisplayBuffer->pSurface->format->Rmask,
		0xFF00,//pVisDisplayBuffer->pSurface->format->Gmask,
		0xFF,//pVisDisplayBuffer->pSurface->format->Bmask,
		0xFF000000);//pVisDisplayBuffer->pSurface->format->Amask);
	if (!pSurface)
		return false;
	g_SurfaceMap[ pImage ] = pSurface;
	SDL_Surface* pSurfaceImg = ::SDL_CreateRGBSurfaceFrom(
		pImage->Data(),
		pImage->Width(),
		pImage->Height(),
		32,
		pImage->Pitch(),//		pImage->Width() * 4,
		0xFF0000,
		0xFF00,
		0xFF,
		0xFF000000);

	if (pSurfaceImg)
	{
		//SDL_LockSurface(pSurfaceImg);
		//SDL_LockSurface(pSurface);
		//SDL_SetAlpha(pSurfaceImg, SDL_SRCALPHA, 255);
		//SDL_SetAlpha(pSurfaceImg, SDL_SRCALPHA, 100);

		SDL_Rect rectSrc;
		rectSrc.x = 0;
		rectSrc.y = 0;
		rectSrc.w = pImage->Width();
		rectSrc.h = pImage->Height();
		SDL_Rect rectDest;
		rectDest.x = 0;
		rectDest.y = 0;
		rectDest.w = pImage->Width();
		rectDest.h = pImage->Height();
		SDL_BlitSurface(pSurfaceImg, &rectSrc, pSurface, &rectDest);


		SDL_LockSurface(pSurface);
		SDL_LockSurface(pSurfaceImg);
		unsigned int *pSurfaceMemImg = (unsigned int*)pSurfaceImg->pixels;
		unsigned int *pSurfaceMem = (unsigned int*)pSurface->pixels;
		//unsigned int *pSurfaceMem2 = (unsigned int*)pSurfaceImg->pixels;
		for ( int y=0; y<pImage->Height(); ++y )
		{
			for ( int i=0; i<pImage->Width(); ++i )
			{
				if ((y%3)==0)
				{
					//*(pSurfaceMem + i + ((y*pSurface->pitch)/4)) = 0x50FF8822;
					unsigned int vOrig = *(pSurfaceMemImg + i + ((y*pSurfaceImg->pitch)/4));
					if ((vOrig & 0xFF000000)==0)
					*(pSurfaceMem + i + ((y*pSurface->pitch)/4)) = *(pSurfaceMem + i + ((y*pSurface->pitch)/4))  &  0x00FFFFFF;
				}
				//*(pSurfaceMem2 + i + ((y*pSurface->pitch)/4)) = 0x00FF000000;
			}
		}
		SDL_UnlockSurface(pSurfaceImg);
		SDL_UnlockSurface(pSurface);
SDL_SetAlpha(pSurface, SDL_SRCALPHA, 0);
		//SDL_UnlockSurface(pSurfaceImg);

		//SDL_FreeSurface(pSurfaceImg);
		//delete pSurface;//?
	}
	return true;
}


#include "wl_def.h" 

/* C AsmRefresh() originally from David Haslam -- dch@sirius.demon.co.uk */

// the door is the last picture before the sprites
#define DOORWALL	(PMSpriteStart-8)

#define ACTORSIZE	0x4000

static unsigned wallheight[MAXVIEWWIDTH];

//
// refresh variables
//
fixed viewx,viewy;			// the focal point
int viewangle;

//
// ray casting variables
//
static int focaltx, focalty;

static int midangle;
static unsigned xpartial, ypartial;
static unsigned xpartialup, xpartialdown, ypartialup, ypartialdown;

static unsigned tilehit;

static int xtile, ytile;
static int xtilestep, ytilestep;
static long xintercept, yintercept;
static long xstep, ystep;

static unsigned postx;

static void AsmRefresh();

//#define NOASM

#ifndef NOASM
#define FixedByFrac(x, y) \
 ({ unsigned long z; \
  asm("imull %2; shrdl $16, %%edx, %%eax" : "=a" (z) : "a" (x), "q" (y) : "%edx"); \
  z; \
 })
#endif

void ScaleShape(int xcenter, int shapenum, unsigned height);
void SimpleScaleShape(int xcenter, int shapenum, unsigned height);
 
/* ======================================================================== */

/*
========================
=
= TransformActor
=
= Takes paramaters:
=   gx,gy		: globalx/globaly of point
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=   scale		: conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
========================
*/

static void TransformActor(objtype *ob)
{
	fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
	gx = ob->x-viewx;
	gy = ob->y-viewy;

//
// calculate newx
//
	gxt = FixedByFrac(gx, viewcos);
	gyt = FixedByFrac(gy, viewsin);
	nx = gxt-gyt-ACTORSIZE;		// fudge the shape forward a bit, because
					// the midpoint could put parts of the shape
					// into an adjacent wall

//
// calculate newy
//
	gxt = FixedByFrac(gx,viewsin);
	gyt = FixedByFrac(gy,viewcos);
	ny = gyt+gxt;

//
// calculate perspective ratio
//
	ob->transx = nx;
	ob->transy = ny;

	if (nx < MINDIST) /* too close, don't overflow the divide */
	{
		ob->viewheight = 0;
		return;
	}

	ob->viewx = centerx + ny*scale/nx;	

	ob->viewheight = heightnumerator/(nx>>8);
}

/*
========================
=
= TransformTile
=
= Takes paramaters:
=   tx,ty		: tile the object is centered in
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=   scale		: conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
= Returns true if the tile is withing getting distance
=
========================
*/

static boolean TransformTile(int tx, int ty, int *dispx, int *dispheight)
{
	fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
	gx = ((long)tx<<TILESHIFT)+0x8000-viewx;
	gy = ((long)ty<<TILESHIFT)+0x8000-viewy;

//
// calculate newx
//
	gxt = FixedByFrac(gx,viewcos);
	gyt = FixedByFrac(gy,viewsin);
	nx = gxt-gyt-0x2000;		// 0x2000 is size of object

//
// calculate newy
//
	gxt = FixedByFrac(gx,viewsin);
	gyt = FixedByFrac(gy,viewcos);
	ny = gyt+gxt;


//
// calculate perspective ratio
//
	if (nx<MINDIST)			/* too close, don't overflow the divide */
	{
		*dispheight = 0;
		return false;
	}

	*dispx = centerx + ny*scale/nx;	

	*dispheight = heightnumerator/(nx>>8);

//
// see if it should be grabbed
//
	if ( (nx<TILEGLOBAL) && (ny>-TILEGLOBAL/2) && (ny<TILEGLOBAL/2) )
		return true;
	else
		return false;
}


/* ======================================================================== */

static unsigned int Ceiling[]=
{
#ifndef SPEAR
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xbfbf,
 0x4e4e,0x4e4e,0x4e4e,0x1d1d,0x8d8d,0x4e4e,0x1d1d,0x2d2d,0x1d1d,0x8d8d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x2d2d,0xdddd,0x1d1d,0x1d1d,0x9898,

 0x1d1d,0x9d9d,0x2d2d,0xdddd,0xdddd,0x9d9d,0x2d2d,0x4d4d,0x1d1d,0xdddd,
 0x7d7d,0x1d1d,0x2d2d,0x2d2d,0xdddd,0xd7d7,0x1d1d,0x1d1d,0x1d1d,0x2d2d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xdddd,0xdddd,0x7d7d,0xdddd,0xdddd,0xdddd
#else
 0x6f6f,0x4f4f,0x1d1d,0xdede,0xdfdf,0x2e2e,0x7f7f,0x9e9e,0xaeae,0x7f7f,
 0x1d1d,0xdede,0xdfdf,0xdede,0xdfdf,0xdede,0xe1e1,0xdcdc,0x2e2e,0x1d1d,0xdcdc
#endif
};

/*
=====================
=
= ClearScreen
=
=====================
*/

static void ClearScreen()
{
	unsigned int ceiling = Ceiling[gamestate.episode*10+mapon] & 0xFF;
	unsigned int floor = 0x19;

	VL_Bar(xoffset, yoffset, viewwidth, viewheight / 2, ceiling);
	VL_Bar(xoffset, yoffset + viewheight / 2, viewwidth, viewheight / 2, floor);
}

//==========================================================================

/*
=====================
=
= CalcRotate
=
=====================
*/

static int dirangle[9] = {0,ANGLES/8,2*ANGLES/8,3*ANGLES/8,4*ANGLES/8, 5*ANGLES/8,6*ANGLES/8,7*ANGLES/8,ANGLES};

static int CalcRotate(objtype *ob)
{
	int	angle,viewangle;

	/* this isn't exactly correct, as it should vary by a trig value, */
	/* but it is close enough with only eight rotations */

	viewangle = player->angle + (centerx - ob->viewx)/8;

	if (ob->obclass == rocketobj || ob->obclass == hrocketobj)
		angle =  (viewangle-180)- ob->angle;
	else
		angle =  (viewangle-180)- dirangle[ob->dir];

	angle+=ANGLES/16;
	while (angle>=ANGLES)
		angle-=ANGLES;
	while (angle<0)
		angle+=ANGLES;

	if (gamestates[ob->state].rotate == 2)  // 2 rotation pain frame
		return 4*(angle/(ANGLES/2));    // seperated by 3

	return angle/(ANGLES/8);
}


/*
=====================
=
= DrawScaleds
=
= Draws all objects that are visable
=
=====================
*/

#ifdef DUMBTEST
#define MAXVISABLE	640
#else
#define MAXVISABLE      64
#endif

typedef struct {
	int viewx;
	int viewheight;
	int shapenum;
} visobj_t;

static visobj_t vislist[MAXVISABLE], *visptr, *visstep, *farthest;

static void DrawScaleds()
{
	int 		i,least,numvisable,height;
	byte		*tilespot,*visspot;
	unsigned	spotloc;

	statobj_t	*statptr;
	objtype		*obj;

	visptr = &vislist[0];

//
// place static objects
//
	for (statptr = &statobjlist[0] ; statptr !=laststatobj ; statptr++)
	{
		if ((visptr->shapenum = statptr->shapenum) == -1)
			continue;						// object has been deleted

		if (!*statptr->visspot)
			continue;						// not visable

		if (TransformTile(statptr->tilex, statptr->tiley
			,&visptr->viewx,&visptr->viewheight) && statptr->flags & FL_BONUS)
		{
			GetBonus (statptr);
			continue;
		}

		if (!visptr->viewheight)
			continue;						// to close to the object

		if (visptr < &vislist[MAXVISABLE-1])	/* don't let it overflow */
			visptr++;
	}

//
// place active objects
//
	for (obj = player->next; obj; obj = obj->next)
	{
		if (!(visptr->shapenum = gamestates[obj->state].shapenum))
			continue;  // no shape

		spotloc = (obj->tilex << 6) + obj->tiley;
		visspot = &spotvis[0][0] + spotloc;
		tilespot = &tilemap[0][0] + spotloc;

		//
		// could be in any of the nine surrounding tiles
		//
		if (*visspot
		|| (*(visspot-1) && !*(tilespot-1))
		|| (*(visspot+1) && !*(tilespot+1))
		|| (*(visspot-65) && !*(tilespot-65))
		|| (*(visspot-64) && !*(tilespot-64))
		|| (*(visspot-63) && !*(tilespot-63))
		|| (*(visspot+65) && !*(tilespot+65))
		|| (*(visspot+64) && !*(tilespot+64))
		|| (*(visspot+63) && !*(tilespot+63))) 
		{
			obj->active = true;
			TransformActor(obj);
			if (!obj->viewheight)
				continue;						// too close or far away

			visptr->viewx = obj->viewx;
			visptr->viewheight = obj->viewheight;
			if (visptr->shapenum == -1)
				visptr->shapenum = obj->temp1;	// special shape

			if (gamestates[obj->state].rotate)
				visptr->shapenum += CalcRotate(obj);

			if (visptr < &vislist[MAXVISABLE-1])	/* don't let it overflow */
				visptr++;
			obj->flags |= FL_VISABLE;
		} else
			obj->flags &= ~FL_VISABLE;
	}

//
// draw from back to front
//
	numvisable = visptr-&vislist[0];

	if (!numvisable)
		return;									// no visable objects

	for (i = 0; i < numvisable; i++)
	{
		least = 32000;
		for (visstep = &vislist[0]; visstep < visptr; visstep++)
		{
			height = visstep->viewheight;
			if (height < least)
			{
				least = height;
				farthest = visstep;
			}
		}
		//
		// draw farthest
		//
		ScaleShape(farthest->viewx, farthest->shapenum, farthest->viewheight);

		farthest->viewheight = 32000;
	}

}

//==========================================================================

/*
==============
=
= DrawPlayerWeapon
=
= Draw the player's hands
=
==============
*/

static int weaponscale[NUMWEAPONS] = {SPR_KNIFEREADY,SPR_PISTOLREADY
	,SPR_MACHINEGUNREADY,SPR_CHAINREADY};

static void DrawPlayerWeapon()
{
	int shapenum;

#ifndef SPEAR
	if (gamestate.victoryflag)
	{
		if ((player->state == s_deathcam) && (get_TimeCount() & 32))
			SimpleScaleShape(viewwidth/2,SPR_DEATHCAM,viewheight+1);
		return;
	}
#endif

	if (gamestate.weapon != -1)
	{
		shapenum = weaponscale[gamestate.weapon]+gamestate.weaponframe;
		SimpleScaleShape(viewwidth/2,shapenum,viewheight+1);
	}

	if (demorecord || demoplayback)
		SimpleScaleShape(viewwidth/2,SPR_DEMO,viewheight+1);
}

/*
====================
=
= WallRefresh
=
====================
*/

static void WallRefresh()
{
/*
 set up variables for this view
*/
	viewangle = player->angle;
	midangle = viewangle*(FINEANGLES/ANGLES);
	viewsin = sintable[viewangle];
	viewcos = costable[viewangle];
	viewx = player->x - FixedByFrac(focallength,viewcos);
	viewy = player->y + FixedByFrac(focallength,viewsin);

	focaltx = viewx>>TILESHIFT;
	focalty = viewy>>TILESHIFT;

	xpartialdown = viewx&(TILEGLOBAL-1);
	xpartialup = TILEGLOBAL-xpartialdown;
	ypartialdown = viewy&(TILEGLOBAL-1);
	ypartialup = TILEGLOBAL-ypartialdown;

	AsmRefresh();
}

/* ======================================================================== */

#define	MAXVIEWHEIGHT	(MAXVIEWWIDTH/2)

static int spanstart[MAXVIEWHEIGHT/2];

static fixed basedist[MAXVIEWHEIGHT/2];

static unsigned char planepics[8192];	/* 4k of ceiling, 4k of floor */

static int halfheight = 0;

static byte *planeylookup[MAXVIEWHEIGHT/2];
static unsigned	mirrorofs[MAXVIEWHEIGHT/2];

static unsigned short int mr_rowofs;
static unsigned short int mr_count;
static unsigned short int mr_xstep;
static unsigned short int mr_ystep;
static unsigned short int mr_xfrac;
static unsigned short int mr_yfrac;
static char *mr_dest;

static void MapRow()
{
	unsigned int ebx, edx, esi;
	
	edx = (mr_ystep << 16) | (mr_xstep);
	esi = (mr_yfrac << 16) | (mr_xfrac);
	
	while (mr_count--) {
		ebx = ((esi & 0xFC000000) >> 25) | ((esi & 0xFC00) >> 3);
		esi += edx;
		//ebx = ((mr_yfrac & 0xFC00) >> (25-16)) | ((mr_xfrac & 0xFC00) >> 3);
		//mr_yfrac += mr_ystep;
		//mr_xfrac += mr_xstep;
		
	/*	
		mr_dest[0] = planepics[0x1F00|(planepics[(ebx&0x1FFE)+0]&0xFF)];
		mr_dest[mr_rowofs] = planepics[0x1F00|(planepics[(ebx&0x1FFE)+1]&0xFF)];
	*/
		mr_dest[0] = planepics[ebx+0];
		mr_dest[mr_rowofs] = planepics[ebx+1];
		mr_dest++;
	}
}

/*
==============
=
= DrawSpans
=
= Height ranges from 0 (infinity) to viewheight/2 (nearest)
==============
*/

static void DrawSpans(int x1, int x2, int height)
{
	fixed length;
	int prestep;
	fixed startxfrac, startyfrac; 
	byte *toprow;

	toprow = planeylookup[height]+(vwidth*yoffset+xoffset);
	mr_rowofs = mirrorofs[height];

	mr_xstep = (viewsin / height) >> 1;
	mr_ystep = (viewcos / height) >> 1;

	length = basedist[height];
	startxfrac = (viewx + FixedByFrac(length, viewcos));
	startyfrac = (viewy - FixedByFrac(length, viewsin));

// draw two spans simultaniously

	prestep = viewwidth/2 - x1;

	mr_xfrac = startxfrac - mr_xstep*prestep;
	mr_yfrac = startyfrac - mr_ystep*prestep;

	mr_dest = toprow + x1;
	mr_count = x2 - x1 + 1;

	if (mr_count)
		MapRow();
}

/*
===================
=
= SetPlaneViewSize
=
===================
*/

static void SetPlaneViewSize()
{
	int x, y;
	byte *dest, *src;

	halfheight = viewheight >> 1;

	for (y = 0; y < halfheight; y++) {
		planeylookup[y] = gfxbuf + (halfheight-1-y)*vwidth;
		mirrorofs[y] = (y*2+1)*vwidth;

		if (y > 0)
			basedist[y] = GLOBAL1/2*scale/y;
	}

	src = PM_GetPage(0);
	dest = planepics;
	for (x = 0; x < 4096; x++) {
		*dest = *src++;
		dest += 2;
	}
	
	src = PM_GetPage(1);
	dest = planepics+1;
	for (x = 0; x < 4096; x++) {
		*dest = *src++;
		dest += 2;
	}

}

/*
===================
=
= DrawPlanes
=
===================
*/

void DrawPlanes()
{
	int height, lastheight;
	int x;

	if ((viewheight>>1) != halfheight)
		SetPlaneViewSize();		// screen size has changed

//
// loop over all columns
//
	lastheight = halfheight;

	for (x = 0; x < viewwidth; x++)
	{
		height = wallheight[x]>>3;
		if (height < lastheight) {	// more starts
			do {
				spanstart[--lastheight] = x;
			} while (lastheight > height);
		} else if (height > lastheight) {	// draw spans
			if (height > halfheight)
				height = halfheight;
			for (; lastheight < height; lastheight++)
				DrawSpans(spanstart[lastheight], x-1, lastheight);
		}
	}

	height = halfheight;
	for (; lastheight < height; lastheight++)
		DrawSpans(spanstart[lastheight], x-1, lastheight);
}

/* ======================================================================== */

/*
========================
=
= ThreeDRefresh
=
========================
*/

#ifndef DRAWCEIL
  //#define DRAWCEIL
#endif
 
void ThreeDRefresh()
{

//
// clear out the traced array
//
	memset(spotvis, 0, sizeof(spotvis));

//
// follow the walls from there to the right, drawwing as we go
//
	DrawPlayBorder();
#ifndef DRAWCEIL	
	ClearScreen();
#endif	

	WallRefresh();
#ifdef DRAWCEIL
	DrawPlanes();  /* silly floor/ceiling drawing */
#endif
//
// draw all the scaled images
//
	DrawScaleds();		/* draw scaled stuff */
	DrawPlayerWeapon();	/* draw player's hands */

//
// show screen and time last cycle
//
	if (fizzlein)
	{
		FizzleFade(xoffset, yoffset, viewwidth, viewheight, 20, false);
		fizzlein = false;

		lasttimecount = 0;		/* don't make a big tic count */
		set_TimeCount(0);
	}
	
	VW_UpdateScreen();
	frameon++;
}

/* ======================================================================== */

static void ScaledDraw(byte *gfx, int count, byte *vid, unsigned int frac, unsigned int delta)
{
	while (count--) {
		*vid = gfx[frac >> 16];
		vid += vwidth;
		frac += delta;
	}
}

static void ScaledDrawTrans(byte *gfx, int count, byte *vid, unsigned int frac, unsigned int delta)
{
	while (count--) {
		if (gfx[frac >> 16] != 255)
			*vid = gfx[frac >> 16];
		vid += vwidth;
		frac += delta;
	}
}

static void ScaleLine(unsigned int height, byte *source, int x)
{
	unsigned long TheFrac;
	unsigned long y;
	
	if (height) {
		TheFrac = (64 << 16) / height;
		
		if (height < viewheight) {
			y = yoffset + (viewheight - height) / 2;
			
			ScaledDraw(source, height, gfxbuf + (y * vwidth) + x + xoffset, 
			0, TheFrac);
			
			return;	
		} 
		
		y = (height - viewheight) / 2;
		y *= TheFrac;
		
		ScaledDraw(source, viewheight, gfxbuf + (yoffset * vwidth) + x + xoffset, 
		y, TheFrac);
	}
}

static void ScaleLineTrans(unsigned int height, byte *source, int x)
{
	unsigned long TheFrac;
	unsigned long y;
	
	if (height) {
		TheFrac = (64 << 16) / height;
		
		if (height < viewheight) {
			y = yoffset + (viewheight - height) / 2;
			
			ScaledDrawTrans(source, height, gfxbuf + (y * vwidth) + x + xoffset, 
			0, TheFrac);
			
			return;	
		} 
		
		y = (height - viewheight) / 2;
		y *= TheFrac;
		
		ScaledDrawTrans(&source[y >> 24], viewheight, gfxbuf + (yoffset * vwidth) + x + xoffset, 
		y, TheFrac);
	}
}

static unsigned char *spritegfx[SPR_TOTAL];

static void DeCompileSprite(int shapenum)
{
	unsigned char *ptr;
	unsigned char *buf;
	unsigned char *cmdptr;
	unsigned char *pixels;
	short int yoff; /* int16_t */
	int y, y0, y1;
	int x, left, right;
	int cmd;
	
	MM_GetPtr((void *)&buf, 64 * 64);
	
	memset(buf, 255, 64 * 64);
	
	ptr = PM_GetSpritePage(shapenum);

	/* left = ptr[0] | (ptr[1] << 8); */
	left = ptr[0];
	/* right = ptr[2] | (ptr[3] << 8); */
	right = ptr[2];
	
	cmdptr = &ptr[4];
	
	for (x = left; x <= right; x++) {
		cmd = cmdptr[0] | (cmdptr[1] << 8);
		cmdptr += 2;
					
		/* while (ptr[cmd+0] | (ptr[cmd+1] << 8)) { */
		while (ptr[cmd+0]) {
			/* y1 = (ptr[cmd+0] | (ptr[cmd+1] << 8)) / 2; */
			y1 = ptr[cmd+0] / 2;
			yoff = ptr[cmd+2] | (ptr[cmd+3] << 8);
			/* y0 = (ptr[cmd+4] | (ptr[cmd+5] << 8)) / 2; */
			y0 = ptr[cmd+4] / 2;
			
			pixels = &ptr[y0 + yoff];
			
			for (y = y0; y < y1; y++) {
				/* *(buf + x + (y*64)) = *pixels; */
				*(buf + (x*64) + y) = *pixels;
				pixels++;
			}
			
			cmd += 6;
		}
	}
	
	spritegfx[shapenum] = buf;
}

void ScaleShape(int xcenter, int shapenum, unsigned height)
{
	unsigned int scaler = (64 << 16) / (height >> 2);
	unsigned int x, p;

	if (spritegfx[shapenum] == NULL)
		DeCompileSprite(shapenum);
	
	for (p = xcenter - (height >> 3), x = 0; x < (64 << 16); x += scaler, p++) {
		if ((p < 0) || (p >= viewwidth) || (wallheight[p] >= height))
			continue;
		ScaleLineTrans(height >> 2, spritegfx[shapenum] + ((x >> 16) << 6), p);
	}	
}

void SimpleScaleShape(int xcenter, int shapenum, unsigned height)
{
	unsigned int scaler = (64 << 16) / height;
	unsigned int x, p;
	
	if (spritegfx[shapenum] == NULL)
		DeCompileSprite(shapenum);
	
	for (p = xcenter - (height / 2), x = 0; x < (64 << 16); x += scaler, p++) {
		if ((p < 0) || (p >= viewwidth))
			continue;
		ScaleLineTrans(height, spritegfx[shapenum] + ((x >> 16) << 6), p);
	}
}

/* ======================================================================== */

/*
====================
=
= CalcHeight
=
= Calculates the height of xintercept,yintercept from viewx,viewy
=
====================
*/

static int CalcHeight()
{
	fixed gxt,gyt,nx,gx,gy;

	gx = xintercept - viewx;
	gxt = FixedByFrac(gx, viewcos);

	gy = yintercept - viewy;
	gyt = FixedByFrac(gy, viewsin);

	nx = gxt-gyt;

  //
  // calculate perspective ratio (heightnumerator/(nx>>8))
  //
	if (nx < MINDIST)
		nx = MINDIST;			/* don't let divide overflow */

	return heightnumerator/(nx>>8);
}

static void ScalePost(byte *wall, int texture)
{
	int height;
	byte *source;

	height = (wallheight[postx] & 0xFFF8) >> 2;
	
	source = wall+texture;
	ScaleLine(height, source, postx);
}

static void HitHorizDoor()
{
	unsigned texture, doorpage = 0, doornum;
	byte *wall;

	doornum = tilehit&0x7f;
	texture = ((xintercept-doorposition[doornum]) >> 4) & 0xfc0;

	wallheight[postx] = CalcHeight();

	switch(doorobjlist[doornum].lock) {
		case dr_normal:
			doorpage = DOORWALL;
			break;
		case dr_lock1:
		case dr_lock2:
		case dr_lock3:
		case dr_lock4:
			doorpage = DOORWALL+6;
			break;
		case dr_elevator:
			doorpage = DOORWALL+4;
			break;
	}

	wall = PM_GetPage(doorpage);
	ScalePost(wall, texture);
}

static void HitVertDoor()
{
	unsigned texture, doorpage = 0, doornum;
	byte *wall;

	doornum = tilehit&0x7f;
	texture = ((yintercept-doorposition[doornum]) >> 4) & 0xfc0;

	wallheight[postx] = CalcHeight();

	switch(doorobjlist[doornum].lock) {
		case dr_normal:
			doorpage = DOORWALL;
			break;
		case dr_lock1:
		case dr_lock2:
		case dr_lock3:
		case dr_lock4:
			doorpage = DOORWALL+6;
			break;
		case dr_elevator:
			doorpage = DOORWALL+4;
			break;
	}

	wall = PM_GetPage(doorpage);
	ScalePost(wall, texture);
}

static void HitVertWall()
{
	int wallpic;
	unsigned texture;
	byte *wall;

	texture = (yintercept>>4)&0xfc0;
	
	if (xtilestep == -1) {
		texture = 0xfc0-texture;
		xintercept += TILEGLOBAL;
	}
	
	wallheight[postx] = CalcHeight();

	if (tilehit & 0x40) { // check for adjacent doors
		ytile = yintercept>>TILESHIFT;
		if (tilemap[xtile-xtilestep][ytile] & 0x80)
			wallpic = DOORWALL+3;
		else
			wallpic = vertwall[tilehit & ~0x40];
	} else
		wallpic = vertwall[tilehit];
		
	wall = PM_GetPage(wallpic);
	ScalePost(wall, texture);
}

static void HitHorizWall()
{
	int wallpic;
	unsigned texture;
	byte *wall;

	texture = (xintercept >> 4) & 0xfc0;
	
	if (ytilestep == -1)
		yintercept += TILEGLOBAL;
	else
		texture = 0xfc0 - texture;
		
	wallheight[postx] = CalcHeight();

	if (tilehit & 0x40) { // check for adjacent doors
		xtile = xintercept>>TILESHIFT;
		if (tilemap[xtile][ytile-ytilestep] & 0x80)
			wallpic = DOORWALL+2;
		else
			wallpic = horizwall[tilehit & ~0x40];
	} else
		wallpic = horizwall[tilehit];

	wall = PM_GetPage(wallpic);
	ScalePost(wall, texture);
}

static void HitHorizPWall()
{
	int wallpic;
	unsigned texture, offset;
	byte *wall;
	
	texture = (xintercept >> 4) & 0xfc0;
	
	offset = pwallpos << 10;
	
	if (ytilestep == -1)
		yintercept += TILEGLOBAL-offset;
	else {
		texture = 0xfc0-texture;
		yintercept += offset;
	}

	wallheight[postx] = CalcHeight();

	wallpic = horizwall[tilehit&63];
	wall = PM_GetPage(wallpic);
	ScalePost(wall, texture);
}

static void HitVertPWall()
{
	int wallpic;
	unsigned texture, offset;
	byte *wall;
	
	texture = (yintercept >> 4) & 0xfc0;
	offset = pwallpos << 10;
	
	if (xtilestep == -1) {
		xintercept += TILEGLOBAL-offset;
		texture = 0xfc0-texture;
	} else
		xintercept += offset;

	wallheight[postx] = CalcHeight();
	
	wallpic = vertwall[tilehit&63];

	wall = PM_GetPage(wallpic);
	ScalePost(wall, texture);
}

#define DEG90	900
#define DEG180	1800
#define DEG270	2700
#define DEG360	3600

#define xpartialbyystep() FixedByFrac(xpartial, ystep)
#define ypartialbyxstep() FixedByFrac(ypartial, xstep)

static int samex(int intercept, int tile)
{
	if (xtilestep > 0) {
		if ((intercept>>16) >= tile)
			return 0;
		else
			return 1;
	} else {
		if ((intercept>>16) <= tile)
			return 0;
		else
			return 1;
	}
}

static int samey(int intercept, int tile)
{
    if (ytilestep > 0) {
	if ((intercept>>16) >= tile)
	    return 0;
	else
	    return 1;
    } else {
	if ((intercept>>16) <= tile)
	    return 0;
	else
	    return 1;
    }
}

static void AsmRefresh()
{
	fixed doorxhit, dooryhit;
	long xtemp, ytemp;

	int angle;    /* ray angle through postx */

for (postx = 0; postx < viewwidth; postx++) {
	angle = midangle + pixelangle[postx];

	if (angle < 0) {
		/* -90 - -1 degree arc */
		angle += FINEANGLES;
		goto entry360;
	} else if (angle < DEG90) {
		/* 0-89 degree arc */
	entry90:
		xtilestep = 1;
		ytilestep = -1;
		xstep = finetangent[DEG90-1-angle];
		ystep = -finetangent[angle];
		xpartial = xpartialup;
		ypartial = ypartialdown;
	} else if (angle < DEG180) {
		/* 90-179 degree arc */
		xtilestep = -1;
		ytilestep = -1;
		xstep = -finetangent[angle-DEG90];
		ystep = -finetangent[DEG180-1-angle];
		xpartial = xpartialdown;
		ypartial = ypartialdown;
	} else if (angle < DEG270) {
		/* 180-269 degree arc */
		xtilestep = -1;
		ytilestep = 1;
		xstep = -finetangent[DEG270-1-angle];
		ystep = finetangent[angle-DEG180];
		xpartial = xpartialdown;
		ypartial = ypartialup;
	} else if (angle < DEG360) {
		/* 270-359 degree arc */
	entry360:
		xtilestep = 1;
		ytilestep = 1;
		xstep = finetangent[angle-DEG270];
		ystep = finetangent[DEG360-1-angle];
		xpartial = xpartialup;
		ypartial = ypartialup;
	} else {
		angle -= FINEANGLES;
		goto entry90;
	}
	
	yintercept = viewy + xpartialbyystep();
	xtile = focaltx + xtilestep;

	xintercept = viewx + ypartialbyxstep();
	ytile = focalty + ytilestep;

/* CORE LOOP */

#define TILE(n) (n>>16)

	/* check intersections with vertical walls */
vertcheck:
	if (!samey(yintercept, ytile))
		goto horizentry;
		
vertentry:
	tilehit = tilemap[xtile][TILE(yintercept)];

	if (tilehit != 0) {
		if (tilehit & 0x80) {
			if (tilehit & 0x40) {
				/* vertpushwall */
				ytemp = yintercept + (signed)((signed)pwallpos * ystep) / 64;
			
				if (TILE(ytemp) != TILE(yintercept)) 
					goto passvert;
					
				yintercept = ytemp;
				xintercept = xtile << 16;
				HitVertPWall();
			} else {
				/* vertdoor */
				dooryhit = yintercept + ystep / 2;
	
				if (TILE(dooryhit) != TILE(yintercept))
					goto passvert;
				
				/* check door position */
				if ((dooryhit&0xFFFF) < doorposition[tilehit&0x7f])
					goto passvert;
				
				yintercept = dooryhit;
				xintercept = (xtile << 16) + 32768;
				HitVertDoor();
			}
		} else {
			xintercept = xtile << 16;
			HitVertWall();
		}
		continue;
	}
passvert:
	spotvis[xtile][TILE(yintercept)] = 1;
	xtile += xtilestep;
	yintercept += ystep;
	goto vertcheck;
	
horizcheck:
	/* check intersections with horizontal walls */
	
	if (!samex(xintercept, xtile))
		goto vertentry;

horizentry:
	tilehit = tilemap[TILE(xintercept)][ytile];

	if (tilehit != 0) {
		if (tilehit & 0x80) {
			if (tilehit & 0x40) {
				xtemp = xintercept + (signed)((signed)pwallpos * xstep) / 64;
		    	
				/* horizpushwall */
				if (TILE(xtemp) != TILE(xintercept))
					goto passhoriz;
				
				xintercept = xtemp;
				yintercept = ytile << 16; 
				HitHorizPWall();
			} else {
				doorxhit = xintercept + xstep / 2;
				
				if (TILE(doorxhit) != TILE(xintercept))
					goto passhoriz;
				
				/* check door position */
				if ((doorxhit&0xFFFF) < doorposition[tilehit&0x7f])
					goto passhoriz;
				
				xintercept = doorxhit;
				yintercept = (ytile << 16) + 32768;
				HitHorizDoor();
			}
		} else {
			yintercept = ytile << 16;
			HitHorizWall();
		}
		continue;
	}
passhoriz:
	spotvis[TILE(xintercept)][ytile] = 1;
	ytile += ytilestep;
	xintercept += xstep;
	goto horizcheck;
}
#ifdef DUMBTEST
	memset(spotvis, 1, sizeof(spotvis));
#endif
}

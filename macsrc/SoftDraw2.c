/*
Copyright (C) 1992-1994 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <string.h>
#include "wolfdef.h"

/**********************************

	Draw a space padded list of numbers 
	for the score
	
**********************************/

LongWord pow10[] = {1,10,100,1000,10000,100000,1000000};
Word NumberIndex = 36;		/* First number in the shape list... */

extern int VidSize;

Word ScaleX(Word x) 
{
	switch(VidSize) {
		case 1:
			return x*8/5;
		case 2:
		case 3:
			return x*2;
	}
	return x;
}

Word ScaleY(Word y)
{
	switch(VidSize) {
		case 1:
			y = (y*8/5)+64;
			if (y == 217)
				y++;
			return y;
		case 2:
			return y*2;
		case 3:
			return y*2+80;
	}
	return y;
}

void SetNumber(LongWord number,Word x,Word y,Word digits)
{
    LongWord val;
    Word count;
    Word empty;
    
    empty = 1;			/* No char's drawn yet */
    while (digits) {	/* Any digits left? */
         count = 0;		/* No value yet */
         val = pow10[digits-1];	/* Get the power of 10 */
         while (number >= val) {	/* Any value here? */
             count++;		/* +1 to the count */
             number -= val;		/* Remove the value */
         }
         if (empty && !count && digits!=1) {    /* pad on left with blanks rather than 0 */
              DrawShape(x,y,GameShapes[NumberIndex]);
         } else {
              empty = 0;		/* I have drawn... */
              DrawShape(x,y,GameShapes[count+NumberIndex]);	/* Draw the digit */
         }
         x+=ScaleX(8);
         digits--;		/* Count down */
    }
}

/**********************************

	Draw the gun in the foreground
	
**********************************/

void IO_AttackShape(Word shape)
{
	DrawXMShape(ScaleX(128),ScaleY(96),GameShapes[shape+12]);
}

/**********************************

	Draw the floor and castle #
	
**********************************/

void IO_DrawFloor(Word floor)
{
    SetNumber(MapListPtr->InfoArray[floor].ScenarioNum,ScaleX(8),ScaleY(176),1);
    SetNumber(MapListPtr->InfoArray[floor].FloorNum,ScaleX(32),ScaleY(176),1);
}

/**********************************

	Draw the score
	
**********************************/

void IO_DrawScore(LongWord score)
{
    if (!IntermissionHack) {		/* Don't draw during intermission! */
    	SetNumber(score,ScaleX(56),ScaleY(176),7);
    }
}

/**********************************

	Draw the number of live remaining
	
**********************************/

void IO_DrawLives(Word lives)
{   
   	if (!IntermissionHack) {	/* Don't draw during intermission! */   	
		--lives;		/* Adjust for zero start value */
    		if (lives > 9) {
    			lives = 9;	/* Failsafe */
		}
		SetNumber(lives,ScaleX(188),ScaleY(176),1);	/* Draw the lives count */
	}
}

/**********************************

	Draw the health
	
**********************************/

void IO_DrawHealth(Word health)
{
    SetNumber(health,ScaleX(210),ScaleY(176),3);
}

/**********************************

	Draw the ammo remaining
	
**********************************/

void IO_DrawAmmo(Word ammo)
{
    SetNumber(ammo,ScaleX(268),ScaleY(176),3);
}

/**********************************

	Draw the treasure score
	
**********************************/

void IO_DrawTreasure(Word treasure)
{
    SetNumber(treasure,ScaleX(128),ScaleY(176),2);
}

/**********************************

	Draw the keys held
	
**********************************/

void IO_DrawKeys(Word keys)
{
    if (keys&1) {
         DrawShape(ScaleX(310),ScaleY(164),GameShapes[10]);
    }
    if (keys&2) {
    	DrawShape(ScaleX(310),ScaleY(184),GameShapes[11]);
    }
}

/**********************************

	Draw the BJ's face
	
**********************************/

void IO_DrawFace(Word face)
{
	DrawShape(ScaleX(160),ScaleY(164),GameShapes[face]);		/* Draw the face */
}

/**********************************

	Redraw the main status bar
	
**********************************/

void IO_DrawStatusBar(void)
{
	DrawShape(ScaleX(0),ScaleY(160),GameShapes[46]);
}

/**********************************

	Copy the 3-D screen to display memory
	
**********************************/

void IO_DisplayViewBuffer (void)
{
	Rect r;
	/* BlastScreen(); */
	
	r.left = 0;
	r.top = 0;
	r.right = ScaleX(320);
	r.bottom = ScaleY(160);
	BlastScreen2(&r);
	
/* if this is the first frame rendered, upload everything and fade in */
    if (firstframe) { 
		FadeTo(rGamePal);
		firstframe = 0;
    }
}

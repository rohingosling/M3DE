//****************************************************************************
// Program: GFX-13 (Graphics Mode 13h Library)
// Version: 1.1
// Date:    1991-01-07
// Author:  Rohin Gosling
//
// Description:
//
//   VGA Mode 13h (320x200, 256 color), 2D graphics library.
//
// Notes:
//
//   - Assembler: TASM (Borland Turbo Assembler)
//   - Assemble:  tasm /ml gfx13.asm
//   - Target:    80186+ real mode, medium memory model.
//
// Change Log:
//
// - Version 1.1
//
//   - Added GetPixel to return the color of a pixel at a specified location
//     in the screen buffer. 
//
//   - Added PutImage to place a rectangular block of pixels from source to 
//     destination with clipping.
//
//****************************************************************************

#ifndef _GFX_MODE_13
#define _GFX_MODE_13

//----------------------------------------------------------------------------

typedef unsigned char BYTE;
typedef unsigned      WORD;

//----------------------------------------------------------------------------
// Text Mode Row Constants
//----------------------------------------------------------------------------

#define TEXT_MODE_25_ROWS  25
#define TEXT_MODE_43_ROWS  43
#define TEXT_MODE_50_ROWS  50

//----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Mode Functions

void SetMode13      ( void );
void SetTextMode    ( BYTE rows );
BYTE GetTextMode    ( void );

// Palette and Clipping Functions

void SetPalette     ( BYTE col, WORD count, WORD segment, WORD dataOffset );
void SetClipping    ( int x0,  int y0,  int x1,  int y1 );

// Screen Functions

void ClearScreen    ( BYTE col, WORD dest );
void FlipScreen     ( WORD source, WORD dest );
void WaitRetrace    ( void );

// Pixel Functions

void PutPixel       ( WORD x,  WORD y,  BYTE col, BYTE clip, WORD dest );
BYTE GetPixel       ( WORD x,  WORD y,  BYTE clip, WORD source );

// Unfilled Primitives

void Line           ( WORD x0, WORD y0, WORD x1, WORD y1, BYTE col, BYTE clip, WORD dest );
void Triangle       ( WORD x0, WORD y0, WORD x1, WORD y1, WORD x2, WORD y2, BYTE col, WORD dest );
void Rectangle      ( WORD x0, WORD y0, WORD x1, WORD y1, BYTE col, WORD dest );
void Quad           ( WORD x0, WORD y0, WORD x1, WORD y1, WORD x2,  WORD y2, WORD x3, WORD y3, BYTE col, WORD dest );

// Filled Primitives

void FillRectangle  ( WORD x0, WORD y0, WORD x1, WORD y1, BYTE col, WORD dest );
void FillTriangle   ( int x0, int y0, int x1, int y1, int x2, int y2, BYTE col, WORD dest );
void FillQuad       ( int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, BYTE col, WORD dest );

// Blitting Functions

void PutImage       ( WORD x, WORD y, WORD xs, WORD size, BYTE mask, WORD source_seg, WORD source_offs, WORD dest );

#ifdef __cplusplus
}
#endif

//----------------------------------------------------------------------------

#endif

//----------------------------------------------------------------------------

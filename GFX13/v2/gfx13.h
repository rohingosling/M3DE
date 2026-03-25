//****************************************************************************
// Program: GFX-13 (Graphics Mode 13h Library)
// Version: 2.1
// Date:    1991-11-02
// Author:  Rohin Gosling
//
// Description:
//
//   VGA Mode 13h (320x200, 256 colors) graphics library for C and C++
//   programs targeting DOS. Provides basic drawing primitives, image 
//   blitting, and palette manipulation functions.
//
// Change Log:
//
// - Version 2.0:
//
//   - Hybrid C and inline assembly port of the original assembly library, 
//     with a C interface.
//
// - Version 2.1:
//
//   - Bug Fixes:
//
//     - Fixed clipping logic.
//
//     - Replaced long conditional jumps (Jcc) with inverted-Jcc +
//       near-JMP trampolines in Line, FillTriangle, and ScaleImage to
//       work around BCC's silent truncation of Jcc displacements beyond
//       ±127 bytes. Guard checks in ScaleImage moved to C-level gotos.
//
//   - Features:
//
//     - Added a boolean flag to enable or disable clipping for drawing 
//       functions. If you want to disable clipping, set the flag to 0. This 
//       allows you to draw without worrying about the overhead of clipping, 
//       which can improve performance if you know your coordinates are 
//       always going to be within bounds.
//
//     - Added GetPixel function for reading pixel colors.
//
//     - Added GetImage, PutImage, and ScaleImage functions for blitting 
//       rectangular blocks of pixels, with optional masking.
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
void SetClipping    ( int x0,  int y0,  int x1,  int y1) ;

// Screen Functions

void ClearScreen    ( BYTE col,    WORD dest );
void FlipScreen     ( WORD source, WORD dest );
void WaitRetrace    ( void);

// Pixel Functions

void PutPixel       ( WORD x, WORD y, BYTE col,  BYTE clip, WORD dest );
BYTE GetPixel       ( WORD x, WORD y, BYTE clip, WORD source );

// Unfilled Primitives

void Line           ( WORD x0, WORD y0, WORD x1, WORD y1, BYTE col, BYTE clip, WORD dest );
void Triangle       ( WORD x0, WORD y0, WORD x1, WORD y1, WORD x2,  WORD y2,   BYTE col, WORD dest );
void Rectangle      ( WORD x0, WORD y0, WORD x1, WORD y1, BYTE col, WORD dest );
void Quad           ( WORD x0, WORD y0, WORD x1, WORD y1, WORD x2,  WORD y2,   WORD x3, WORD y3, BYTE col, WORD dest );

// Filled Primitives

void FillRectangle  ( WORD x0, WORD y0, WORD x1, WORD y1, BYTE col, WORD dest );
void FillTriangle   ( int  x0, int  y0, int  x1, int  y1, int  x2,  int  y2, BYTE col, WORD dest );
void FillQuad       ( int  x0, int  y0, int  x1, int  y1, int  x2,  int  y2, int  x3,  int  y3, BYTE col, WORD dest );

// Blitting Functions

void PutImage       ( WORD x,  WORD y,  WORD xs, WORD size, BYTE mask,      WORD source_seg, WORD source_offs, WORD dest );
void GetImage       ( WORD x0, WORD y0, WORD x1, WORD y1,   WORD source,    WORD dest_seg,   WORD dest_offs);
void ScaleImage     ( WORD x0, WORD y0, WORD x1, WORD y1,   WORD source_xs, WORD source_ys,  BYTE mask, WORD source_seg, WORD source_offs, WORD dest );

#ifdef __cplusplus
}
#endif

//----------------------------------------------------------------------------

#endif

//----------------------------------------------------------------------------

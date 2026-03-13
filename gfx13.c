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

#include "gfx13.h"

//----------------------------------------------------------------------------
// File-Scope Clipping Bounds
//----------------------------------------------------------------------------

static int clip_x1 = 0;
static int clip_y1 = 0;
static int clip_x2 = 319;
static int clip_y2 = 199;

//----------------------------------------------------------------------------
// Function: SetMode13
//
// Description:
//
//   Switches the video adapter to VGA Mode 13h (320x200, 256 colors) by
//   calling BIOS INT 0x10 with AX = 0x0013.
//
// Arguments:
//
//   - None
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetMode13 ( void )
{
	asm {
		MOV		AX, 0x0013
		INT		0x10
	}
}

//----------------------------------------------------------------------------
// Function: SetTextMode
//
// Description:
//
//   Sets the video adapter to 80-column text mode (BIOS mode 3) with a
//   configurable number of text rows.
//
//   - 25 rows: Standard VGA text mode with the default 8x16 ROM font.
//              (400 scanlines / 16 pixels per character = 25 rows.)
//
//   - 43 rows: EGA-compatible extended text mode. Selects 350 vertical
//              scanlines via INT 10h AX=1201h BL=30h, then loads the 8x8
//              ROM font via INT 10h AX=1112h.
//              (350 scanlines / 8 pixels per character = 43 rows.)
//
//   - 50 rows: VGA extended text mode. Uses the default 400 vertical
//              scanlines and loads the 8x8 ROM font via INT 10h AX=1112h.
//              (400 scanlines / 8 pixels per character = 50 rows.)
//
//   Any value other than 43 or 50 defaults to 25 rows.
//
// Arguments:
//
//   - rows : Number of text rows. Use the constants TEXT_MODE_25_ROWS (25),
//            TEXT_MODE_43_ROWS (43), or TEXT_MODE_50_ROWS (50).
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetTextMode ( BYTE rows )
{
	asm {

		// For 43-line mode, select 350 vertical scanlines before setting
		// the video mode. The VGA BIOS defaults to 400 scanlines, which
		// would produce 50 rows with an 8x8 font. By switching to 350
		// scanlines first, the subsequent 8x8 font load yields 43 rows.

		CMP		BYTE PTR [rows], 43
		JNE		SETTEXTMODE_SETMODE

		MOV		AX, 0x1201
		MOV		BL, 0x30
		INT		0x10

	} SETTEXTMODE_SETMODE: asm {

		// Set 80-column text mode (BIOS mode 3).

		MOV		AX, 0x0003
		INT		0x10

		// For 25-row mode we are done. For 43 or 50 rows, fall through
		// to load the 8x8 ROM font.

		CMP		BYTE PTR [rows], 25
		JE		SETTEXTMODE_DONE
		CMP		BYTE PTR [rows], 43
		JE		SETTEXTMODE_LOADFONT
		CMP		BYTE PTR [rows], 50
		JE		SETTEXTMODE_LOADFONT
		JMP		SETTEXTMODE_DONE

	} SETTEXTMODE_LOADFONT: asm {

		// Load the 8x8 ROM font into the active character generator.
		// This halves the character cell height, doubling the number of
		// visible text rows (from 25 to 50 at 400 scanlines, or from
		// ~21 to 43 at 350 scanlines).

		MOV		AX, 0x1112
		MOV		BL, 0x00
		INT		0x10

	} SETTEXTMODE_DONE:;
}

//----------------------------------------------------------------------------
// Function: GetTextMode
//
// Description:
//
//   Returns the number of text rows for the current video mode.
//
//   Queries the BIOS via INT 10h AH=0Fh to determine the active video
//   mode. If the mode is a standard 80-column text mode (mode 2 or 3),
//   the row count is read from the BIOS Data Area at 0040:0084h, which
//   stores (rows - 1). If the current mode is not a text mode (e.g.,
//   Mode 13h), the function returns 0.
//
// Arguments:
//
//   - None
//
// Returns:
//
//   - The number of text rows (25, 43, or 50) if a text mode is active,
//     or 0 if the current video mode is not a text mode.
//
//----------------------------------------------------------------------------

BYTE GetTextMode ( void )
{
	BYTE textRows;

	asm {

		// Query the current video mode. INT 10h AH=0Fh returns
		// the active mode number in AL.

		MOV		AH, 0x0F
		INT		0x10

		// Check for 80-column text modes (mode 2 = monochrome,
		// mode 3 = color).

		CMP		AL, 0x03
		JE		GETTEXTMODE_READROWS
		CMP		AL, 0x02
		JE		GETTEXTMODE_READROWS

		// Not a text mode. Return 0.

		XOR		AL, AL
		JMP		GETTEXTMODE_STORE

	} GETTEXTMODE_READROWS: asm {

		// Read the row count from the BIOS Data Area.
		// Address 0040:0084h holds (rows - 1).

		PUSH	ES
		MOV		AX, 0x0040
		MOV		ES, AX
		MOV		AL, ES:[0x0084]
		INC		AL
		POP		ES

	} GETTEXTMODE_STORE: asm {

		MOV		[textRows], AL

	}

	return textRows;
}

//----------------------------------------------------------------------------
// Function: SetPalette
//
// Description:
//
//   Programs the VGA DAC palette registers by writing RGB triplets to
//   port 0x3C8 (write index) and 0x3C9 (data).
//
//   Writes the starting color index to 0x3C8 once, then streams count
//   RGB triplets to 0x3C9 using LODSB. The DAC auto-increments its
//   internal index after every third byte.
//
// Arguments:
//
//   - col   : Starting palette color index (0-255).
//   - count : Number of consecutive colors to set.
//   - segment  : Segment address of the source RGB data array.
//   - dataOffset : Offset of the source RGB data array within its segment.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetPalette
(
	BYTE col,
	WORD count,
	WORD segment,
	WORD dataOffset
)
{
	asm {
		
		PUSH	DS

		MOV		AL, [col]
		MOV		DX, 0x3C8
		OUT		DX, AL

		MOV		AX, [segment]
		MOV		SI, [dataOffset]
		MOV		DS, AX
		MOV		CX, [count]

		MOV		BX, CX
		SHL		CX, 1
		ADD		CX, BX

		MOV		DX, 0x3C9
		REP		OUTSB

		POP		DS
	}
}

//----------------------------------------------------------------------------
// Function: SetClipping
//
// Description:
//
//   Sets the clipping rectangle used by PutPixel, GetPixel, Line,
//   FillRectangle, FillTriangle, FillQuad, and ScaleImage.
//   Coordinates are inclusive.
//
// Arguments:
//
//   - x0 : left edge of the clipping rectangle.
//   - y0 : top edge of the clipping rectangle.
//   - x1 : right edge of the clipping rectangle.
//   - y1 : bottom edge of the clipping rectangle.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetClipping ( int x0, int y0, int x1, int y1 )
{
	asm {

		// Set clip_x1

		MOV		AX, [x0]
		MOV		[clip_x1], AX

		// Set clip_y1

		MOV		AX, [y0]
		MOV		[clip_y1], AX

		// Set clip_x2

		MOV		AX, [x1]
		MOV		[clip_x2], AX

		// Set clip_y2

		MOV		AX, [y1]
		MOV		[clip_y2], AX
	}
}

//----------------------------------------------------------------------------
// Function: ClearScreen
//
// Description:
//
//   Fills the entire 64000-byte screen buffer with a single color using a
//   REP STOSW block fill (32000 words).
//
//   The color byte is replicated into both the high and low bytes of AX for
//   word-aligned writes.
//
// Arguments:
//
//   - col  : Palette color index to fill the screen with.
//   - dest : Segment address of the screen buffer to clear.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void ClearScreen ( BYTE col, WORD dest )
{
	asm {

		PUSH	ES

		MOV		AL, [col]
		MOV		BX, [dest]
		MOV		ES, BX
		XOR		DI, DI
		MOV		AH, AL
		MOV		CX, 32000
		REP		STOSW

		POP		ES
	}
}

//----------------------------------------------------------------------------
// Function: WaitRetrace
//
// Description:
//
//   Synchronizes with the VGA vertical retrace by polling the Input Status
//   Register at port 0x3DA.
//
//   - First waits for any active retrace to end, then waits for the next
//     retrace to begin.
//
//   - This prevents tearing when updating the screen or palette during the
//     blanking interval.
//
// Arguments:
//
//   - None
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void WaitRetrace ( void )
{
	// Wait for any active retrace to end.

	asm		MOV		DX, 0x3DA

RETRACE:;

	// Wait for the next retrace to begin.

	asm		IN		AL, DX
	asm		TEST	AL, 0x08
	asm		JNZ		RETRACE

NO_RETRACE:;

	// 

	asm		IN		AL, DX
	asm		TEST	AL, 0x08
	asm		JZ		NO_RETRACE
}

//----------------------------------------------------------------------------
// Function: FlipScreen
//
// Description:
//
//   Copies the entire 64000-byte virtual screen buffer to the destination
//   buffer using a REP MOVSW block transfer (32000 words).
//
//   Used for double-buffering by flipping the off-screen buffer to VGA
//   memory.
//
// Arguments:
//
//   - source : Segment address of the source buffer (the virtual screen).
//   - dest   : Segment address of the destination buffer (e.g., 0xA000 for
//              VGA video memory).
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void FlipScreen ( WORD source, WORD dest )
{
	asm {
		
		PUSH	DS
		PUSH	ES

		MOV		BX, [source]
		MOV		AX, [dest]

		MOV		DS, BX
		MOV		ES, AX
		XOR		SI, SI
		XOR		DI, DI
		MOV		CX, 32000
		REP		MOVSW

		POP		ES
		POP		DS
	}
}

//----------------------------------------------------------------------------
// Function: PutPixel
//
// Description:
//
//   Plots a single pixel at the given (x, y) coordinate in a 320-byte-wide
//   screen buffer.
//
//   The destination is specified by its segment address. The linear offset
//   is computed as y * 320 + x using bit shifts and addition.
//
// Arguments:
//
//   - x    : Horizontal pixel coordinate (0-319).
//   - y    : Vertical pixel coordinate (0-199).
//   - col  : Palette color index to write.
//   - clip : When nonzero, the pixel is clipped to the rectangle set by
//            SetClipping. When zero, no clipping is performed.
//   - dest : Segment address of the destination buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void PutPixel
(
	WORD x,
	WORD y,
	BYTE col,
	BYTE clip,
	WORD dest
)
{
	asm {

		// Skip clipping if clip == 0.

		CMP   BYTE PTR [clip], 0
		JE    PUTPIXEL_DRAW

		// Clip against clipping rectangle.

		MOV   AX, [x]
		CMP   AX, [clip_x1]
		JL    PUTPIXEL_DONE
		CMP   AX, [clip_x2]
		JG    PUTPIXEL_DONE
		MOV   AX, [y]
		CMP   AX, [clip_y1]
		JL    PUTPIXEL_DONE
		CMP   AX, [clip_y2]
		JG    PUTPIXEL_DONE

	} PUTPIXEL_DRAW: asm {

		PUSH  ES

		MOV   BX, [x]
		MOV   CX, [y]
		MOV   AL, [col]
		MOV   DX, [dest]

		XCHG  CH, CL
		ADD   BX, CX
		SHR   CX, 2
		ADD   BX, CX

		MOV   ES, DX
		MOV   DI, BX
		STOSB

		POP   ES

	} PUTPIXEL_DONE:;
}

//----------------------------------------------------------------------------
// Function: GetPixel
//
// Description:
//
//   Reads a single pixel from the given (X, Y) coordinate in a 320-byte-wide
//   screen buffer.
//
//   The source is specified by its segment address. The linear offset is
//   computed as Y * 320 + X using bit shifts and addition.
//
// Arguments:
//
//   - x      : Horizontal pixel coordinate (0-319).
//   - y      : Vertical pixel coordinate (0-199).
//   - clip   : When nonzero, the read is clipped to the rectangle set by
//              SetClipping and returns 0 for out-of-bounds coordinates.
//              When zero, no clipping is performed.
//   - source : Segment address of the source buffer.
//
// Returns:
//
//   - The palette color index of the pixel at (x, y), or 0 if clipped.
//
//----------------------------------------------------------------------------

BYTE GetPixel ( WORD x, WORD y, BYTE clip, WORD source )
{
	BYTE color = 0;

	asm {

		// Skip clipping if clip == 0.

		CMP   BYTE PTR [clip], 0
		JE    GETPIXEL_READ

		// Clip against clipping rectangle.

		MOV   AX, [x]
		CMP   AX, [clip_x1]
		JL    GETPIXEL_DONE
		CMP   AX, [clip_x2]
		JG    GETPIXEL_DONE
		MOV   AX, [y]
		CMP   AX, [clip_y1]
		JL    GETPIXEL_DONE
		CMP   AX, [clip_y2]
		JG    GETPIXEL_DONE

	} GETPIXEL_READ: asm {

		PUSH  DS

		MOV   BX, [x]
		MOV   CX, [y]
		MOV   DX, [source]

		XCHG  CH, CL
		ADD   BX, CX
		SHR   CX, 2
		ADD   BX, CX

		MOV   DS, DX
		MOV   SI, BX
		LODSB
		MOV   [color], AL

		POP   DS

	} GETPIXEL_DONE:;

	return color;
}

//----------------------------------------------------------------------------
// Function: Line
//
// Description:
//
//   Draws a line from (x0, y0) to (x1, y1) using Bresenham's integer line
//   algorithm. Each pixel is plotted with PutPixel.
//
//   When clip is nonzero, the line is clipped to the rectangle set by
//   SetClipping using the Cohen-Sutherland algorithm before drawing. When
//   clip is zero, no clipping is performed and the caller must ensure the
//   coordinates are within bounds.
//
// Arguments:
//
//   - x0   : Starting X coordinate.
//   - y0   : Starting Y coordinate.
//   - x1   : Ending X coordinate.
//   - y1   : Ending Y coordinate.
//   - col  : Palette color index.
//   - clip : When nonzero, the line is clipped to the rectangle set by
//            SetClipping. When zero, no clipping is performed.
//   - dest : Segment address of the destination buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void Line ( WORD x0, WORD y0, WORD x1, WORD y1, BYTE col, BYTE clip, WORD dest )
{
	int current_x, current_y;
	int delta_x, delta_y;
	int step_x, step_y;
	int error, double_error;
	int outcode_a, outcode_b;

	// Skip clipping if clip == 0.

	if ( !clip ) goto LINE_CLIP_ACCEPT;

	asm {

		// --- Compute outcode_a for (x0, y0) ---
		//
		// Bit 0 (1) = LEFT   : x < clip_x1
		// Bit 1 (2) = RIGHT  : x > clip_x2
		// Bit 2 (4) = BOTTOM : y > clip_y2
		// Bit 3 (8) = TOP    : y < clip_y1

		XOR		AX, AX
		MOV		BX, [x0]
		CMP		BX, [clip_x1]
		JGE		LINE_OA_NOT_LEFT
		OR		AX, 1

	} LINE_OA_NOT_LEFT: asm {

		CMP		BX, [clip_x2]
		JLE		LINE_OA_NOT_RIGHT
		OR		AX, 2

	} LINE_OA_NOT_RIGHT: asm {

		MOV		BX, [y0]
		CMP		BX, [clip_y2]
		JLE		LINE_OA_NOT_BOTTOM
		OR		AX, 4

	} LINE_OA_NOT_BOTTOM: asm {

		CMP		BX, [clip_y1]
		JGE		LINE_OA_NOT_TOP
		OR		AX, 8

	} LINE_OA_NOT_TOP: asm {

		MOV		[outcode_a], AX

		// Compute outcode_b for (x1, y1)

		XOR		AX, AX
		MOV		BX, [x1]
		CMP		BX, [clip_x1]
		JGE		LINE_OB_NOT_LEFT
		OR		AX, 1

	} LINE_OB_NOT_LEFT: asm {

		CMP		BX, [clip_x2]
		JLE		LINE_OB_NOT_RIGHT
		OR		AX, 2

	} LINE_OB_NOT_RIGHT: asm {

		MOV		BX, [y1]
		CMP		BX, [clip_y2]
		JLE		LINE_OB_NOT_BOTTOM
		OR		AX, 4

	} LINE_OB_NOT_BOTTOM: asm {

		CMP		BX, [clip_y1]
		JGE		LINE_OB_NOT_TOP
		OR		AX, 8

	} LINE_OB_NOT_TOP: asm {

		MOV		[outcode_b], AX

	} LINE_CLIP_LOOP: asm {

		// Trivial accept: both endpoints inside.
		// (Near-jump trampoline to avoid short-Jcc range limit.)

		MOV		AX, [outcode_a]
		OR		AX, [outcode_b]
		JNZ		LINE_NOT_ACCEPTED
		JMP		LINE_CLIP_ACCEPT

	} LINE_NOT_ACCEPTED: asm {

		// Trivial reject: both endpoints on same outside side.
		// (Near-jump trampoline to avoid short-Jcc range limit.)

		MOV		AX, [outcode_a]
		AND		AX, [outcode_b]
		JZ		LINE_NOT_REJECTED
		JMP		LINE_DONE

	} LINE_NOT_REJECTED: asm {

		// Pick the endpoint that is outside the clip rectangle.

		MOV		AX, [outcode_a]
		TEST	AX, AX
		JNZ		LINE_CLIP_A
		JMP		LINE_CLIP_B

		// --- Clip endpoint A (x0, y0) ---

	} LINE_CLIP_A: asm {

		TEST	WORD PTR [outcode_a], 8
		JZ		LINE_CLIP_A_NOT_TOP

		// x0 = x0 + (x1 - x0) * (clip_y1 - y0) / (y1 - y0)
		// y0 = clip_y1

		MOV		AX, [x1]
		SUB		AX, [x0]
		MOV		CX, [clip_y1]
		SUB		CX, [y0]
		IMUL	CX
		MOV		CX, [y1]
		SUB		CX, [y0]
		IDIV	CX
		ADD		AX, [x0]
		MOV		[x0], AX
		MOV		AX, [clip_y1]
		MOV		[y0], AX
		JMP		LINE_RECOMPUTE_OA

	} LINE_CLIP_A_NOT_TOP: asm {

		TEST	WORD PTR [outcode_a], 4
		JZ		LINE_CLIP_A_NOT_BOTTOM

		// x0 = x0 + (x1 - x0) * (clip_y2 - y0) / (y1 - y0)
		// y0 = clip_y2

		MOV		AX, [x1]
		SUB		AX, [x0]
		MOV		CX, [clip_y2]
		SUB		CX, [y0]
		IMUL	CX
		MOV		CX, [y1]
		SUB		CX, [y0]
		IDIV	CX
		ADD		AX, [x0]
		MOV		[x0], AX
		MOV		AX, [clip_y2]
		MOV		[y0], AX
		JMP		LINE_RECOMPUTE_OA

	} LINE_CLIP_A_NOT_BOTTOM: asm {

		TEST	WORD PTR [outcode_a], 2
		JZ		LINE_CLIP_A_NOT_RIGHT

		// y0 = y0 + (y1 - y0) * (clip_x2 - x0) / (x1 - x0)
		// x0 = clip_x2

		MOV		AX, [y1]
		SUB		AX, [y0]
		MOV		CX, [clip_x2]
		SUB		CX, [x0]
		IMUL	CX
		MOV		CX, [x1]
		SUB		CX, [x0]
		IDIV	CX
		ADD		AX, [y0]
		MOV		[y0], AX
		MOV		AX, [clip_x2]
		MOV		[x0], AX
		JMP		LINE_RECOMPUTE_OA

	} LINE_CLIP_A_NOT_RIGHT: asm {

		// LEFT bit (1) must be set if we reached here.

		// y0 = y0 + (y1 - y0) * (clip_x1 - x0) / (x1 - x0)
		// x0 = clip_x1

		MOV		AX, [y1]
		SUB		AX, [y0]
		MOV		CX, [clip_x1]
		SUB		CX, [x0]
		IMUL	CX
		MOV		CX, [x1]
		SUB		CX, [x0]
		IDIV	CX
		ADD		AX, [y0]
		MOV		[y0], AX
		MOV		AX, [clip_x1]
		MOV		[x0], AX

	} LINE_RECOMPUTE_OA: asm {

		// Recompute outcode_a for updated (x0, y0).

		XOR		AX, AX
		MOV		BX, [x0]
		CMP		BX, [clip_x1]
		JGE		LINE_ROA_NOT_LEFT
		OR		AX, 1

	} LINE_ROA_NOT_LEFT: asm {

		CMP		BX, [clip_x2]
		JLE		LINE_ROA_NOT_RIGHT
		OR		AX, 2

	} LINE_ROA_NOT_RIGHT: asm {

		MOV		BX, [y0]
		CMP		BX, [clip_y2]
		JLE		LINE_ROA_NOT_BOTTOM
		OR		AX, 4

	} LINE_ROA_NOT_BOTTOM: asm {

		CMP		BX, [clip_y1]
		JGE		LINE_ROA_NOT_TOP
		OR		AX, 8

	} LINE_ROA_NOT_TOP: asm {

		MOV		[outcode_a], AX
		JMP		LINE_CLIP_LOOP

		// --- Clip endpoint B (x1, y1) ---

	} LINE_CLIP_B: asm {

		TEST	WORD PTR [outcode_b], 8
		JZ		LINE_CLIP_B_NOT_TOP

		// x1 = x1 + (x0 - x1) * (clip_y1 - y1) / (y0 - y1)
		// y1 = clip_y1

		MOV		AX, [x0]
		SUB		AX, [x1]
		MOV		CX, [clip_y1]
		SUB		CX, [y1]
		IMUL	CX
		MOV		CX, [y0]
		SUB		CX, [y1]
		IDIV	CX
		ADD		AX, [x1]
		MOV		[x1], AX
		MOV		AX, [clip_y1]
		MOV		[y1], AX
		JMP		LINE_RECOMPUTE_OB

	} LINE_CLIP_B_NOT_TOP: asm {

		TEST	WORD PTR [outcode_b], 4
		JZ		LINE_CLIP_B_NOT_BOTTOM

		// x1 = x1 + (x0 - x1) * (clip_y2 - y1) / (y0 - y1)
		// y1 = clip_y2

		MOV		AX, [x0]
		SUB		AX, [x1]
		MOV		CX, [clip_y2]
		SUB		CX, [y1]
		IMUL	CX
		MOV		CX, [y0]
		SUB		CX, [y1]
		IDIV	CX
		ADD		AX, [x1]
		MOV		[x1], AX
		MOV		AX, [clip_y2]
		MOV		[y1], AX
		JMP		LINE_RECOMPUTE_OB

	} LINE_CLIP_B_NOT_BOTTOM: asm {

		TEST	WORD PTR [outcode_b], 2
		JZ		LINE_CLIP_B_NOT_RIGHT

		// y1 = y1 + (y0 - y1) * (clip_x2 - x1) / (x0 - x1)
		// x1 = clip_x2

		MOV		AX, [y0]
		SUB		AX, [y1]
		MOV		CX, [clip_x2]
		SUB		CX, [x1]
		IMUL	CX
		MOV		CX, [x0]
		SUB		CX, [x1]
		IDIV	CX
		ADD		AX, [y1]
		MOV		[y1], AX
		MOV		AX, [clip_x2]
		MOV		[x1], AX
		JMP		LINE_RECOMPUTE_OB

	} LINE_CLIP_B_NOT_RIGHT: asm {

		// LEFT bit (1) must be set if we reached here.

		// y1 = y1 + (y0 - y1) * (clip_x1 - x1) / (x0 - x1)
		// x1 = clip_x1

		MOV		AX, [y0]
		SUB		AX, [y1]
		MOV		CX, [clip_x1]
		SUB		CX, [x1]
		IMUL	CX
		MOV		CX, [x0]
		SUB		CX, [x1]
		IDIV	CX
		ADD		AX, [y1]
		MOV		[y1], AX
		MOV		AX, [clip_x1]
		MOV		[x1], AX

	} LINE_RECOMPUTE_OB: asm {

		// Recompute outcode_b for updated (x1, y1).

		XOR		AX, AX
		MOV		BX, [x1]
		CMP		BX, [clip_x1]
		JGE		LINE_ROB_NOT_LEFT
		OR		AX, 1

	} LINE_ROB_NOT_LEFT: asm {

		CMP		BX, [clip_x2]
		JLE		LINE_ROB_NOT_RIGHT
		OR		AX, 2

	} LINE_ROB_NOT_RIGHT: asm {

		MOV		BX, [y1]
		CMP		BX, [clip_y2]
		JLE		LINE_ROB_NOT_BOTTOM
		OR		AX, 4

	} LINE_ROB_NOT_BOTTOM: asm {

		CMP		BX, [clip_y1]
		JGE		LINE_ROB_NOT_TOP
		OR		AX, 8

	} LINE_ROB_NOT_TOP: asm {

		MOV		[outcode_b], AX
		JMP		LINE_CLIP_LOOP

	} LINE_CLIP_ACCEPT:

	// Bresenham setup.

	current_x = (int) x0;
	current_y = (int) y0;

	delta_x = (int) x1 - current_x;
	delta_y = (int) y1 - current_y;

	step_x = 1;
	step_y = 1;

	if ( delta_x < 0 ) { delta_x = -delta_x; step_x = -1; }
	if ( delta_y < 0 ) { delta_y = -delta_y; step_y = -1; }

	error = delta_x - delta_y;

	// Bresenham loop.

	for ( ;; )
	{
		// Plot pixel at (current_x, current_y) — inline PutPixel.

		asm {
			PUSH	ES
			PUSH	DI

			MOV		BX, [current_x]
			MOV		CX, [current_y]
			MOV		AL, [col]
			MOV		DX, [dest]

			XCHG	CH, CL
			ADD		BX, CX
			SHR		CX, 2
			ADD		BX, CX

			MOV		ES, DX
			MOV		DI, BX
			STOSB

			POP		DI
			POP		ES
		}

		// Termination: stop after plotting the endpoint.

		if ( current_x == (int) x1 && current_y == (int) y1 ) break;

		// Bresenham step.

		double_error = error << 1;

		if ( double_error > -delta_y ) { error -= delta_y; current_x += step_x; }
		if ( double_error <  delta_x ) { error += delta_x; current_y += step_y; }
	}

	LINE_DONE:;
}

//----------------------------------------------------------------------------
// Function: Triangle
//
// Description:
//
//   Draws a triangle outline connecting three vertices.
//   Implemented as three calls to Line.
//
// Arguments:
//
//   - x0, y0 : First vertex.
//   - x1, y1 : Second vertex.
//   - x2, y2 : Third vertex.
//   - col    : Palette color index.
//   - dest   : Segment address of the destination buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void Triangle ( WORD x0, WORD y0, WORD x1, WORD y1, WORD x2, WORD y2, BYTE col, WORD dest )
{
	Line ( x0, y0, x1, y1, col, 1, dest );
	Line ( x1, y1, x2, y2, col, 1, dest );
	Line ( x2, y2, x0, y0, col, 1, dest );
}

//----------------------------------------------------------------------------
// Function: Rectangle
//
// Description:
//
//   Draws an axis-aligned rectangle outline from (x0, y0) to (x1, y1).
//   Implemented as four calls to Line with clipping enabled.
//
// Arguments:
//
//   - x0   : left X coordinate.
//   - y0   : top Y coordinate.
//   - x1   : right X coordinate.
//   - y1   : bottom Y coordinate.
//   - col  : Palette color index.
//   - dest : Segment address of the destination buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void Rectangle ( WORD x0, WORD y0, WORD x1, WORD y1, BYTE col, WORD dest )
{
	Line ( x0, y0, x1, y0, col, 1, dest );
	Line ( x0, y1, x1, y1, col, 1, dest );
	Line ( x0, y0, x0, y1, col, 1, dest );
	Line ( x1, y0, x1, y1, col, 1, dest );
}

//----------------------------------------------------------------------------
// Function: Quad
//
// Description:
//
//   Draws a quadrilateral outline connecting four vertices.
//   Implemented as four calls to Line.
//
// Arguments:
//
//   - x0, y0 : First vertex.
//   - x1, y1 : Second vertex.
//   - x2, y2 : Third vertex.
//   - x3, y3 : Fourth vertex.
//   - col    : Palette color index.
//   - dest   : Segment address of the destination buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void Quad ( WORD x0, WORD y0, WORD x1, WORD y1, WORD x2, WORD y2, WORD x3, WORD y3, BYTE col, WORD dest )
{
	Line ( x0, y0, x1, y1, col, 1, dest );
	Line ( x1, y1, x2, y2, col, 1, dest );
	Line ( x2, y2, x3, y3, col, 1, dest );
	Line ( x3, y3, x0, y0, col, 1, dest );
}

//----------------------------------------------------------------------------
// Function: FillRectangle
//
// Description:
//
//   Draws a filled axis-aligned rectangle from (x0, y0) to (x1, y1).
//   Implemented as a series of horizontal lines from y0 to y1.
//
// Arguments:
//
//   - x0   : left X coordinate.
//   - y0   : top Y coordinate.
//   - x1   : right X coordinate.
//   - y1   : bottom Y coordinate.
//   - col  : Palette color index.
//   - dest : Segment address of the destination buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void FillRectangle ( WORD x0, WORD y0, WORD x1, WORD y1, BYTE col, WORD dest )
{
	int left, right, top, bottom, row;
	WORD length;

	asm {

		// Sort x0, x1 into left, right.

		MOV		AX, [x0]
		MOV		BX, [x1]
		CMP		AX, BX
		JBE		FRECT_X_SORTED
		XCHG	AX, BX

	} FRECT_X_SORTED: asm {

		MOV		[left], AX
		MOV		[right], BX

		// Sort y0, y1 into top, bottom.

		MOV		AX, [y0]
		MOV		BX, [y1]
		CMP		AX, BX
		JBE		FRECT_Y_SORTED
		XCHG	AX, BX

	} FRECT_Y_SORTED: asm {

		MOV		[top], AX
		MOV		[bottom], BX

		// Full rejection: rectangle entirely outside clip bounds.

		MOV		AX, [bottom]
		CMP		AX, [clip_y1]
		JL		FRECT_DONE
		MOV		AX, [top]
		CMP		AX, [clip_y2]
		JG		FRECT_DONE
		MOV		AX, [right]
		CMP		AX, [clip_x1]
		JL		FRECT_DONE
		MOV		AX, [left]
		CMP		AX, [clip_x2]
		JG		FRECT_DONE

		// Clamp top to clip_y1.

		MOV		AX, [top]
		CMP		AX, [clip_y1]
		JGE		FRECT_TOP_OK
		MOV		AX, [clip_y1]
		MOV		[top], AX

	} FRECT_TOP_OK: asm {

		// Clamp bottom to clip_y2.

		MOV		AX, [bottom]
		CMP		AX, [clip_y2]
		JLE		FRECT_BOT_OK
		MOV		AX, [clip_y2]
		MOV		[bottom], AX

	} FRECT_BOT_OK: asm {

		// Clamp left to clip_x1.

		MOV		AX, [left]
		CMP		AX, [clip_x1]
		JGE		FRECT_LEFT_OK
		MOV		AX, [clip_x1]
		MOV		[left], AX

	} FRECT_LEFT_OK: asm {

		// Clamp right to clip_x2.

		MOV		AX, [right]
		CMP		AX, [clip_x2]
		JLE		FRECT_RIGHT_OK
		MOV		AX, [clip_x2]
		MOV		[right], AX

	} FRECT_RIGHT_OK: asm {

		// Compute length = right - left + 1 (constant for all rows).

		MOV		AX, [right]
		SUB		AX, [left]
		INC		AX
		MOV		[length], AX

		// Initialize row counter and destination segment.

		MOV		AX, [top]
		MOV		[row], AX

		PUSH	ES
		MOV		AX, [dest]
		MOV		ES, AX

	} FRECT_LOOP: asm {

		MOV		AX, [row]
		CMP		AX, [bottom]
		JG		FRECT_LOOP_END

		// Compute offset = row * 320 + left.

		MOV		BX, [left]
		MOV		CX, [row]
		XCHG	CH, CL
		ADD		BX, CX
		SHR		CX, 2
		ADD		BX, CX

		MOV		DI, BX
		MOV		AL, [col]
		MOV		CX, [length]
		REP		STOSB

		INC		WORD PTR [row]
		JMP		FRECT_LOOP

	} FRECT_LOOP_END: asm {

		POP		ES

	} FRECT_DONE:;
}

//----------------------------------------------------------------------------
// Function: FillTriangle
//
// Description:
//
//   Draws a filled triangle using scan-line conversion with 16.16 fixed-point
//   edge slopes. Vertices are sorted top-to-bottom, then the triangle is
//   split at the middle vertex into a flat-bottom half and a flat-top half.
//   Each half is rasterized with horizontal line fills.
//
// Arguments:
//
//   - x0, y0 : First vertex.
//   - x1, y1 : Second vertex.
//   - x2, y2 : Third vertex.
//   - col    : Palette color index.
//   - dest   : Segment address of the destination buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void FillTriangle ( int x0, int y0, int x1, int y1, int x2, int y2, BYTE col, WORD dest )
{
	int top_x, top_y;
	int mid_x, mid_y;
	int bot_x, bot_y;
	int scan_y;
	WORD left_x_low,  left_x_high;
	WORD right_x_low, right_x_high;
	WORD left_slope_low,  left_slope_high;
	WORD right_slope_low, right_slope_high;
	
	// Sort vertices by Y coordinate (top to bottom).
	
	asm {

		// Load vertices into local variables for easier access.

		MOV		AX, [x0]
		MOV		[top_x], AX
		MOV		AX, [y0]
		MOV		[top_y], AX
		MOV		AX, [x1]
		MOV		[mid_x], AX
		MOV		AX, [y1]
		MOV		[mid_y], AX
		MOV		AX, [x2]
		MOV		[bot_x], AX
		MOV		AX, [y2]
		MOV		[bot_y], AX

		// Pass 1: if top_y > mid_y, swap top and mid.

		MOV		AX, [top_y]
		CMP		AX, [mid_y]
		JLE		FT_SORT1_OK
		MOV		AX, [top_x]
		XCHG	AX, [mid_x]
		MOV		[top_x], AX
		MOV		AX, [top_y]
		XCHG	AX, [mid_y]
		MOV		[top_y], AX

	} FT_SORT1_OK: asm {

		// Pass 2: if top_y > bot_y, swap top and bot.

		MOV		AX, [top_y]
		CMP		AX, [bot_y]
		JLE		FT_SORT2_OK
		MOV		AX, [top_x]
		XCHG	AX, [bot_x]
		MOV		[top_x], AX
		MOV		AX, [top_y]
		XCHG	AX, [bot_y]
		MOV		[top_y], AX

	} FT_SORT2_OK: asm {

		// Pass 3: if mid_y > bot_y, swap mid and bot.

		MOV		AX, [mid_y]
		CMP		AX, [bot_y]
		JLE		FT_SORT3_OK
		MOV		AX, [mid_x]
		XCHG	AX, [bot_x]
		MOV		[mid_x], AX
		MOV		AX, [mid_y]
		XCHG	AX, [bot_y]
		MOV		[mid_y], AX

	} FT_SORT3_OK: asm {

		// Degenerate: all vertices on one scan line.
		// Trampoline: short JE stays close; near JMP reaches far label.

		MOV		AX, [top_y]
		CMP		AX, [bot_y]
		JE		FT_IS_DEGENERATE
		JMP		FT_NOT_DEGENERATE
	}

	FT_IS_DEGENERATE:

	// Inline clipped horizontal fill for degenerate triangle.

	asm {

		// Y clip: skip if top_y outside clip bounds.

		MOV		AX, [top_y]
		CMP		AX, [clip_y1]
		JL		FTD1_SKIP
		CMP		AX, [clip_y2]
		JG		FTD1_SKIP

		// Sort: ensure left <= right.

		MOV		AX, [top_x]
		MOV		BX, [bot_x]
		CMP		AX, BX
		JLE		FTD1_SORTED
		XCHG	AX, BX

	} FTD1_SORTED: asm {

		// X rejection: skip if span is entirely outside clip bounds.

		CMP		BX, [clip_x1]
		JL		FTD1_SKIP
		CMP		AX, [clip_x2]
		JG		FTD1_SKIP

		// Clamp left to clip_x1.

		CMP		AX, [clip_x1]
		JGE		FTD1_LEFT_OK
		MOV		AX, [clip_x1]

	} FTD1_LEFT_OK: asm {

		// Clamp right to clip_x2.

		CMP		BX, [clip_x2]
		JLE		FTD1_RIGHT_OK
		MOV		BX, [clip_x2]

	} FTD1_RIGHT_OK: asm {

		// length = right - left + 1

		SUB		BX, AX
		INC		BX

		// Set up ES:DI for the fill.

		PUSH	ES
		MOV		DX, [dest]
		MOV		ES, DX

		// offset = top_y * 320 + left

		MOV		DX, BX
		MOV		BX, AX
		MOV		CX, [top_y]
		XCHG	CH, CL
		ADD		BX, CX
		SHR		CX, 2
		ADD		BX, CX

		MOV		DI, BX
		MOV		CX, DX
		MOV		AL, [col]
		REP		STOSB

		POP		ES

	} FTD1_SKIP:;

	goto FT_DONE;

	FT_NOT_DEGENERATE:

	// Flat-bottom half (top_y to mid_y).

	if ( mid_y != top_y )
	{
		asm {

		// left_slope = ((mid_x - top_x) << 16) / (mid_y - top_y)
		//
		// Signed 32/16 division — dividend DX:AX = (mid_x-top_x):0, divisor CX.

		MOV		DX, [mid_x]
		SUB		DX, [top_x]
		XOR		AX, AX
		MOV		CX, [mid_y]
		SUB		CX, [top_y]

		// Determine result sign.

		MOV		BX, DX
		XOR		BX, CX
		PUSH	BX

		// Absolute value of dividend high word.

		TEST	DX, DX
		JGE		FT_DIV1_NUM_POS
		NEG		DX

	} FT_DIV1_NUM_POS: asm {

		// Absolute value of divisor.

		TEST	CX, CX
		JGE		FT_DIV1_DEN_POS
		NEG		CX

	} FT_DIV1_DEN_POS: asm {

		// Step 1: high word / divisor.

		MOV		AX, DX
		XOR		DX, DX
		DIV		CX
		PUSH	AX

		// Step 2: (remainder:0) / divisor.

		XOR		AX, AX
		DIV		CX
		POP		DX

		// Apply sign.

		POP		BX
		TEST	BX, BX
		JGE		FT_DIV1_DONE
		NEG		DX
		NEG		AX
		SBB		DX, 0

	} FT_DIV1_DONE: asm {

		// Store left slope as 16.16 fixed-point.

		MOV		[left_slope_low], AX
		MOV		[left_slope_high], DX

		// right_slope = ((bot_x - top_x) << 16) / (bot_y - top_y)

		MOV		DX, [bot_x]
		SUB		DX, [top_x]
		XOR		AX, AX
		MOV		CX, [bot_y]
		SUB		CX, [top_y]

		MOV		BX, DX
		XOR		BX, CX
		PUSH	BX

		TEST	DX, DX
		JGE		FT_DIV2_NUM_POS
		NEG		DX

	} FT_DIV2_NUM_POS: asm {

		// Absolute value of divisor.

		TEST	CX, CX
		JGE		FT_DIV2_DEN_POS
		NEG		CX

	} FT_DIV2_DEN_POS: asm {

		// Step 1: high word / divisor.

		MOV		AX, DX
		XOR		DX, DX
		DIV		CX
		PUSH	AX

		// Step 2: (remainder:0) / divisor.

		XOR		AX, AX
		DIV		CX
		POP		DX

		// Apply sign.

		POP		BX
		TEST	BX, BX
		JGE		FT_DIV2_DONE
		NEG		DX
		NEG		AX
		SBB		DX, 0

	} FT_DIV2_DONE: asm {

		// Store right slope as 16.16 fixed-point.

		MOV		[right_slope_low], AX
		MOV		[right_slope_high], DX

		// left_x = right_x = top_x << 16

		MOV		AX, [top_x]
		MOV		[left_x_high], AX
		MOV		[right_x_high], AX
		MOV		WORD PTR [left_x_low], 0
		MOV		WORD PTR [right_x_low], 0

		// scan_y = top_y

		MOV		AX, [top_y]
		MOV		[scan_y], AX
	}

	// Flat-bottom scan loop.

	while ( scan_y < mid_y )
	{
		// Inline clipped horizontal fill.

		asm {

			// Y clip: skip if scan_y outside clip bounds.

			MOV		AX, [scan_y]
			CMP		AX, [clip_y1]
			JL		FTFB_SKIP
			CMP		AX, [clip_y2]
			JG		FTFB_SKIP

			// Sort: ensure left <= right.

			MOV		AX, [left_x_high]
			MOV		BX, [right_x_high]
			CMP		AX, BX
			JLE		FTFB_SORTED
			XCHG	AX, BX

		} FTFB_SORTED: asm {

			// X rejection: skip if span is entirely outside clip bounds.

			CMP		BX, [clip_x1]
			JL		FTFB_SKIP
			CMP		AX, [clip_x2]
			JG		FTFB_SKIP

			// Clamp left to clip_x1.

			CMP		AX, [clip_x1]
			JGE		FTFB_LEFT_OK
			MOV		AX, [clip_x1]

		} FTFB_LEFT_OK: asm {

			// Clamp right to clip_x2.

			CMP		BX, [clip_x2]
			JLE		FTFB_RIGHT_OK
			MOV		BX, [clip_x2]

		} FTFB_RIGHT_OK: asm {

			// length = right - left + 1

			SUB		BX, AX
			INC		BX

			// Set up ES:DI for the fill.

			PUSH	ES
			MOV		DX, [dest]
			MOV		ES, DX

			// offset = scan_y * 320 + left

			MOV		DX, BX
			MOV		BX, AX
			MOV		CX, [scan_y]
			XCHG	CH, CL
			ADD		BX, CX
			SHR		CX, 2
			ADD		BX, CX

			MOV		DI, BX
			MOV		CX, DX
			MOV		AL, [col]
			REP		STOSB

			POP		ES

		} FTFB_SKIP: asm {

			// Advance DDA accumulators.

			MOV		AX, [left_x_low]
			ADD		AX, [left_slope_low]
			MOV		[left_x_low], AX
			MOV		AX, [left_x_high]
			ADC		AX, [left_slope_high]
			MOV		[left_x_high], AX

			MOV		AX, [right_x_low]
			ADD		AX, [right_slope_low]
			MOV		[right_x_low], AX
			MOV		AX, [right_x_high]
			ADC		AX, [right_slope_high]
			MOV		[right_x_high], AX

			INC		WORD PTR [scan_y]
		}
	}
	}

	// Flat-top half (mid_y to bot_y).

	if ( bot_y == mid_y )
	{
		// Inline clipped horizontal fill for flat-bottom degenerate.

		asm {

			// Y clip: skip if mid_y outside clip bounds.

			MOV		AX, [mid_y]
			CMP		AX, [clip_y1]
			JL		FTD2_SKIP
			CMP		AX, [clip_y2]
			JG		FTD2_SKIP

			// Sort: ensure left <= right.

			MOV		AX, [mid_x]
			MOV		BX, [bot_x]
			CMP		AX, BX
			JLE		FTD2_SORTED
			XCHG	AX, BX

		} FTD2_SORTED: asm {

			// X rejection: skip if span is entirely outside clip bounds.

			CMP		BX, [clip_x1]
			JL		FTD2_SKIP
			CMP		AX, [clip_x2]
			JG		FTD2_SKIP

			// Clamp left to clip_x1.

			CMP		AX, [clip_x1]
			JGE		FTD2_LEFT_OK
			MOV		AX, [clip_x1]

		} FTD2_LEFT_OK: asm {

			// Clamp right to clip_x2.

			CMP		BX, [clip_x2]
			JLE		FTD2_RIGHT_OK
			MOV		BX, [clip_x2]

		} FTD2_RIGHT_OK: asm {

			// length = right - left + 1

			SUB		BX, AX
			INC		BX

			// Set up ES:DI for the fill.

			PUSH	ES
			MOV		DX, [dest]
			MOV		ES, DX

			// offset = mid_y * 320 + left

			MOV		DX, BX
			MOV		BX, AX
			MOV		CX, [mid_y]
			XCHG	CH, CL
			ADD		BX, CX
			SHR		CX, 2
			ADD		BX, CX

			MOV		DI, BX
			MOV		CX, DX
			MOV		AL, [col]
			REP		STOSB

			POP		ES

		} FTD2_SKIP:;
	}
	else
	{
		asm {
		// left_slope = ((bot_x - mid_x) << 16) / (bot_y - mid_y)

		MOV		DX, [bot_x]
		SUB		DX, [mid_x]
		XOR		AX, AX
		MOV		CX, [bot_y]
		SUB		CX, [mid_y]

		MOV		BX, DX
		XOR		BX, CX
		PUSH	BX

		TEST	DX, DX
		JGE		FT_DIV3_NUM_POS
		NEG		DX

	} FT_DIV3_NUM_POS: asm {

		// Absolute value of divisor.

		TEST	CX, CX
		JGE		FT_DIV3_DEN_POS
		NEG		CX

	} FT_DIV3_DEN_POS: asm {

		MOV		AX, DX
		XOR		DX, DX
		DIV		CX
		PUSH	AX

		XOR		AX, AX
		DIV		CX
		POP		DX

		POP		BX
		TEST	BX, BX
		JGE		FT_DIV3_DONE
		NEG		DX
		NEG		AX
		SBB		DX, 0

	} FT_DIV3_DONE: asm {

		// Store left slope as 16.16 fixed-point.

		MOV		[left_slope_low], AX
		MOV		[left_slope_high], DX

		// right_slope = ((bot_x - top_x) << 16) / (bot_y - top_y)

		MOV		DX, [bot_x]
		SUB		DX, [top_x]
		XOR		AX, AX
		MOV		CX, [bot_y]
		SUB		CX, [top_y]

		// Determine result sign.

		MOV		BX, DX
		XOR		BX, CX
		PUSH	BX

		// Absolute value of dividend high word.

		TEST	DX, DX
		JGE		FT_DIV4_NUM_POS
		NEG		DX

	} FT_DIV4_NUM_POS: asm {

		// Absolute value of divisor.

		TEST	CX, CX
		JGE		FT_DIV4_DEN_POS
		NEG		CX

	} FT_DIV4_DEN_POS: asm {

		// Step 1: high word / divisor.

		MOV		AX, DX
		XOR		DX, DX
		DIV		CX
		PUSH	AX

		// Step 2: (remainder:0) / divisor.

		XOR		AX, AX
		DIV		CX
		POP		DX

		// Apply sign.	

		POP		BX
		TEST	BX, BX
		JGE		FT_DIV4_DONE
		NEG		DX
		NEG		AX
		SBB		DX, 0

	} FT_DIV4_DONE: asm {

		// Store right slope as 16.16 fixed-point.

		MOV		[right_slope_low], AX
		MOV		[right_slope_high], DX

		// left_x = mid_x << 16

		MOV		AX, [mid_x]
		MOV		[left_x_high], AX
		MOV		WORD PTR [left_x_low], 0

		// right_x = top_x << 16

		MOV		AX, [top_x]
		MOV		[right_x_high], AX
		MOV		WORD PTR [right_x_low], 0

		// right_x += right_slope * (mid_y - top_y)
		//
		// Signed 32x16 multiply: right_slope (32-bit) * (mid_y - top_y) (16-bit).
		// The multiplier (mid_y - top_y) is non-negative after sorting.

		MOV		CX, [mid_y]
		SUB		CX, [top_y]

		// Low part: slope_low * CX (unsigned 16x16 -> 32).

		MOV		AX, [right_slope_low]
		MUL		CX
		MOV		BX, DX
		PUSH	AX

		// High part: slope_high * CX (signed 16x16 -> 32, only AX needed).

		MOV		AX, [right_slope_high]
		IMUL	CX
		ADD		AX, BX

		// Add product to right_x.

		POP		BX
		ADD		[right_x_low], BX
		ADC		[right_x_high], AX

		// scan_y = mid_y

		MOV		AX, [mid_y]
		MOV		[scan_y], AX
	}

	// Flat-top scan loop.

	while ( scan_y <= bot_y )
	{
		// Inline clipped horizontal fill.

		asm {

			// Y clip: skip if scan_y outside clip bounds.

			MOV		AX, [scan_y]
			CMP		AX, [clip_y1]
			JL		FTFT_SKIP
			CMP		AX, [clip_y2]
			JG		FTFT_SKIP

			// Sort: ensure left <= right.

			MOV		AX, [left_x_high]
			MOV		BX, [right_x_high]
			CMP		AX, BX
			JLE		FTFT_SORTED
			XCHG	AX, BX

		} FTFT_SORTED: asm {

			// X rejection: skip if span is entirely outside clip bounds.

			CMP		BX, [clip_x1]
			JL		FTFT_SKIP
			CMP		AX, [clip_x2]
			JG		FTFT_SKIP

			// Clamp left to clip_x1.

			CMP		AX, [clip_x1]
			JGE		FTFT_LEFT_OK
			MOV		AX, [clip_x1]

		} FTFT_LEFT_OK: asm {

			// Clamp right to clip_x2.

			CMP		BX, [clip_x2]
			JLE		FTFT_RIGHT_OK
			MOV		BX, [clip_x2]

		} FTFT_RIGHT_OK: asm {

			// length = right - left + 1

			SUB		BX, AX
			INC		BX

			// Set up ES:DI for the fill.

			PUSH	ES
			MOV		DX, [dest]
			MOV		ES, DX

			// offset = scan_y * 320 + left

			MOV		DX, BX
			MOV		BX, AX
			MOV		CX, [scan_y]
			XCHG	CH, CL
			ADD		BX, CX
			SHR		CX, 2
			ADD		BX, CX

			MOV		DI, BX
			MOV		CX, DX
			MOV		AL, [col]
			REP		STOSB

			POP		ES

		} FTFT_SKIP: asm {

			// Advance DDA accumulators.

			MOV		AX, [left_x_low]
			ADD		AX, [left_slope_low]
			MOV		[left_x_low], AX
			MOV		AX, [left_x_high]
			ADC		AX, [left_slope_high]
			MOV		[left_x_high], AX

			MOV		AX, [right_x_low]
			ADD		AX, [right_slope_low]
			MOV		[right_x_low], AX
			MOV		AX, [right_x_high]
			ADC		AX, [right_slope_high]
			MOV		[right_x_high], AX

			INC		WORD PTR [scan_y]
		}
	}
	}

	FT_DONE:;
}

//----------------------------------------------------------------------------
// Function: FillQuad
//
// Description:
//
//   Draws a filled quadrilateral by splitting it into two triangles along
//   the diagonal from vertex 1 to vertex 3 and calling FillTriangle twice.
//
// Arguments:
//
//   - x0, y0 : First vertex.
//   - x1, y1 : Second vertex.
//   - x2, y2 : Third vertex.
//   - x3, y3 : Fourth vertex.
//   - col    : Palette color index.
//   - dest   : Segment address of the destination buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void FillQuad ( int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, BYTE col, WORD dest )
{
	FillTriangle ( x0, y0, x1, y1, x2, y2, col, dest );
	FillTriangle ( x0, y0, x2, y2, x3, y3, col, dest );
}

//----------------------------------------------------------------------------
// Function: GetImage
//
// Description:
//
//   Captures a rectangular region from a 320-byte-wide screen buffer into a
//   contiguous destination buffer.
//
//   The source is specified by its segment address. The destination is
//   specified by a segment:offset pair pointing to a pre-allocated buffer.
//   Coordinates are normalized if necessary (x0 > x1 or y0 > y1).
//
//   The output buffer contains width * height bytes stored row by row, and
//   is directly compatible with PutImage where xs = width and
//   size = width * height.
//
// Arguments:
//
//   - x0        : Left X coordinate of the region.
//   - y0        : Top Y coordinate of the region.
//   - x1        : Right X coordinate of the region.
//   - y1        : Bottom Y coordinate of the region.
//   - source    : Segment address of the source screen buffer.
//   - dest_seg  : Segment address of the destination buffer.
//   - dest_offs : Offset of the destination buffer within its segment.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void GetImage
(
	WORD x0,
	WORD y0,
	WORD x1,
	WORD y1,
	WORD source,
	WORD dest_seg,
	WORD dest_offs
)
{
	WORD width, height, row_skip;

	asm {

		// Normalize x0, x1.

		MOV		AX, [x0]
		MOV		BX, [x1]
		CMP		AX, BX
		JBE		GIMG_X_SORTED
		XCHG	AX, BX
		MOV		[x0], AX
		MOV		[x1], BX

	} GIMG_X_SORTED: asm {

		// Normalize y0, y1.

		MOV		AX, [y0]
		MOV		BX, [y1]
		CMP		AX, BX
		JBE		GIMG_Y_SORTED
		XCHG	AX, BX
		MOV		[y0], AX
		MOV		[y1], BX

	} GIMG_Y_SORTED: asm {

		// width = x1 - x0 + 1

		MOV		AX, [x1]
		SUB		AX, [x0]
		INC		AX
		MOV		[width], AX

		// height = y1 - y0 + 1

		MOV		AX, [y1]
		SUB		AX, [y0]
		INC		AX
		MOV		[height], AX

		// row_skip = 320 - width

		MOV		AX, 320
		SUB		AX, [width]
		MOV		[row_skip], AX

		// DS:SI = source segment at y0 * 320 + x0

		PUSH	DS
		PUSH	ES
		PUSH	SI
		PUSH	DI

		MOV		AX, [source]
		MOV		DS, AX

		MOV		BX, [x0]
		MOV		CX, [y0]
		XCHG	CH, CL
		ADD		BX, CX
		SHR		CX, 2
		ADD		BX, CX
		MOV		SI, BX

		// ES:DI = dest_seg:dest_offs

		MOV		AX, [dest_seg]
		MOV		ES, AX
		MOV		DI, [dest_offs]

		MOV		DX, [height]

	} GET_IMAGE_ROW: asm {

		MOV		CX, [width]
		REP		MOVSB
		ADD		SI, [row_skip]
		DEC		DX
		JNZ		GET_IMAGE_ROW

	} asm {

		POP		DI
		POP		SI
		POP		ES
		POP		DS
	}
}

//----------------------------------------------------------------------------
// Function: PutImage
//
// Description:
//
//   Blits a rectangular sprite image onto a 320-byte-wide screen buffer.
//
//   - Pixels matching the mask byte are treated as transparent and skipped.
//
//   - The source image is read linearly and wrapped to the next row after
//     every xs pixels.
//
// Arguments:
//
//   - x           : destination X coordinate (top-left corner).
//   - y           : destination Y coordinate (top-left corner).
//   - xs          : Width of the source image in pixels.
//   - size        : Total number of pixels in the source image (xs * ys).
//   - mask        : Palette index used as the transparency key.
//   - source_seg  : Segment address of the source image data.
//   - source_offs : Offset of the source image data within its segment.
//   - dest        : Segment address of the destination screen buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void PutImage
(
	WORD x,
	WORD y,
	WORD xs,
	WORD size,
	BYTE mask,
	WORD source_seg,
	WORD source_offs,
	WORD dest
)
{
	asm {

		PUSH	DS
		PUSH	ES
		PUSH	SI
		PUSH	DI

		MOV		AX, [dest]
		MOV		BX, [source_seg]
		MOV		CX, [source_offs]
		MOV		ES, AX
		MOV		DS, BX
		MOV		SI, CX

		MOV		BX, [x]
		MOV		CX, [y]
		XCHG	CH, CL
		ADD		BX, CX
		SHR		CX, 2
		ADD		BX, CX

		MOV		DI, BX

		XOR		AX, AX
		XOR		BX, BX
		MOV		DL, [mask]

	} NEXT_PIXEL: asm {

		CMP		DS:[SI], DL
		JNZ		PLOT_PIXEL

		INC		SI
		INC		DI
		JMP		UPDATE_DATA

	} PLOT_PIXEL: asm {

		MOVSB

	} UPDATE_DATA: asm {

		INC		BX
		CMP		BX, [size]
		JZ		END_PUT_IMAGE

		INC		AX
		CMP		AX, [xs]
		JNZ		NEXT_PIXEL

		MOV		AX, 320
		SUB		AX, [xs]
		ADD		DI, AX

		XOR		AX, AX

		JMP		NEXT_PIXEL

	} END_PUT_IMAGE: asm {

		POP		DI
		POP		SI
		POP		ES
		POP		DS
	}
}

//----------------------------------------------------------------------------
// Function: ScaleImage
//
// Description:
//
//   Draws a source image scaled to fit a destination rectangle using
//   nearest-neighbor sampling with 16.16 fixed-point DDA.
//
//   The destination rectangle is clipped against the current clipping
//   bounds. DDA start accumulators are adjusted to skip clipped rows and
//   columns so that the source-to-destination mapping remains correct.
//
//   Pixels matching the mask byte are treated as transparent and skipped,
//   using the same convention as PutImage.
//
// Arguments:
//
//   - x0          : Left X coordinate of the destination rectangle.
//   - y0          : Top Y coordinate of the destination rectangle.
//   - x1          : Right X coordinate of the destination rectangle.
//   - y1          : Bottom Y coordinate of the destination rectangle.
//   - source_xs   : Width of the source image in pixels.
//   - source_ys   : Height of the source image in pixels.
//   - mask        : Palette index used as the transparency key.
//   - source_seg  : Segment address of the source image data.
//   - source_offs : Offset of the source image data within its segment.
//   - dest        : Segment address of the destination screen buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void ScaleImage
(
	WORD x0,
	WORD y0,
	WORD x1,
	WORD y1,
	WORD source_xs,
	WORD source_ys,
	BYTE mask,
	WORD source_seg,
	WORD source_offs,
	WORD dest
)
{
	WORD dest_width, dest_height;
	int  dest_x1, dest_y1, dest_x2, dest_y2;
	WORD x_step_low, x_step_high;
	WORD y_step_low, y_step_high;
	WORD y_accumulator_low, y_accumulator_high;
	WORD x_accumulator_start_low, x_accumulator_start_high;
	WORD x_accumulator_low, x_accumulator_high;
	WORD clipped_width;
	WORD source_row, source_row_offset;
	WORD dest_row_offset;
	int  row;

	asm {
		
		// Normalize coordinates.		

		MOV		AX, [x0]
		MOV		BX, [x1]
		CMP		AX, BX
		JBE		SCALE_X_SORTED
		XCHG	AX, BX
		MOV		[x0], AX
		MOV		[x1], BX

	} SCALE_X_SORTED: asm {

		MOV		AX, [y0]
		MOV		BX, [y1]
		CMP		AX, BX
		JBE		SCALE_Y_SORTED
		XCHG	AX, BX
		MOV		[y0], AX
		MOV		[y1], BX

	} SCALE_Y_SORTED: asm {
		
		// Compute destination dimensions.
		
		MOV		AX, [x1]
		SUB		AX, [x0]
		INC		AX
		MOV		[dest_width], AX

		MOV		AX, [y1]
		SUB		AX, [y0]
		INC		AX
		MOV		[dest_height], AX
	}

	// Guard against zero-size source or destination.
	// C-level check avoids long conditional jumps to SCALE_DONE.

	if ( source_xs == 0 || source_ys == 0 || dest_width == 0 || dest_height == 0 )
		goto SCALE_DONE;

	asm {

		// Compute x_step = (source_xs << 16) / dest_width.
		// Unsigned 32/16 division (two-step to avoid overflow).
		
		MOV		AX, [source_xs]
		XOR		DX, DX
		MOV		CX, [dest_width]
		DIV		CX
		PUSH	AX

		XOR		AX, AX
		DIV		CX
		POP		DX

		MOV		[x_step_low], AX
		MOV		[x_step_high], DX
		
		// Compute y_step = (source_ys << 16) / dest_height.
		
		MOV		AX, [source_ys]
		XOR		DX, DX
		MOV		CX, [dest_height]
		DIV		CX
		PUSH	AX

		XOR		AX, AX
		DIV		CX
		POP		DX

		MOV		[y_step_low], AX
		MOV		[y_step_high], DX
		
		// Copy destination coords to signed int locals.
		
		MOV		AX, [x0]
		MOV		[dest_x1], AX
		MOV		AX, [y0]
		MOV		[dest_y1], AX
		MOV		AX, [x1]
		MOV		[dest_x2], AX
		MOV		AX, [y1]
		MOV		[dest_y2], AX
	}

	// Full clip rejection.
	// C-level check avoids long conditional jumps to SCALE_DONE.

	if ( dest_x2 < clip_x1 || dest_x1 > clip_x2 || dest_y2 < clip_y1 || dest_y1 > clip_y2 )
		goto SCALE_DONE;

	asm {

		// Initialize DDA accumulators to zero.
		
		MOV		WORD PTR [y_accumulator_low],        0
		MOV		WORD PTR [y_accumulator_high],       0
		MOV		WORD PTR [x_accumulator_start_low],  0
		MOV		WORD PTR [x_accumulator_start_high], 0

		
		// Adjust y_accumulator if top edge is clipped.
		// y_accumulator = y_step * (clip_y1 - dest_y1)
		// Unsigned 32x16 multiply.
		
		MOV		AX, [dest_y1]
		CMP		AX, [clip_y1]
		JGE		SCALE_Y_CLIP_OK

		MOV		CX, [clip_y1]
		SUB		CX, [dest_y1]

		// Low part: y_step_low * CX.

		MOV		AX, [y_step_low]
		MUL		CX
		MOV		[y_accumulator_low], AX
		MOV		BX, DX

		// High part: y_step_high * CX.

		MOV		AX, [y_step_high]
		MUL		CX
		ADD		AX, BX
		MOV		[y_accumulator_high], AX

		MOV		AX, [clip_y1]
		MOV		[dest_y1], AX

	} SCALE_Y_CLIP_OK: asm {
		
		// Adjust x_accumulator_start if left edge is clipped.
		// x_accumulator_start = x_step * (clip_x1 - dest_x1)
		
		MOV		AX, [dest_x1]
		CMP		AX, [clip_x1]
		JGE		SCALE_X_CLIP_OK

		MOV		CX, [clip_x1]
		SUB		CX, [dest_x1]

		// Low part: x_step_low * CX.

		MOV		AX, [x_step_low]
		MUL		CX
		MOV		[x_accumulator_start_low], AX
		MOV		BX, DX

		// High part: x_step_high * CX.

		MOV		AX, [x_step_high]
		MUL		CX
		ADD		AX, BX
		MOV		[x_accumulator_start_high], AX

		MOV		AX, [clip_x1]
		MOV		[dest_x1], AX

	} SCALE_X_CLIP_OK: asm {
		
		// Clamp bottom and right edges.
		
		MOV		AX, [dest_y2]
		CMP		AX, [clip_y2]
		JLE		SCALE_BOT_OK
		MOV		AX, [clip_y2]
		MOV		[dest_y2], AX

	} SCALE_BOT_OK: asm {

		MOV		AX, [dest_x2]
		CMP		AX, [clip_x2]
		JLE		SCALE_RIGHT_OK
		MOV		AX, [clip_x2]
		MOV		[dest_x2], AX

	} SCALE_RIGHT_OK: asm {
		
		// Compute clipped_width = dest_x2 - dest_x1 + 1.
		
		MOV		AX, [dest_x2]
		SUB		AX, [dest_x1]
		INC		AX
		MOV		[clipped_width], AX
		
		// Row loop: row = dest_y1.
		
		MOV		AX, [dest_y1]
		MOV		[row], AX

	} SCALE_ROW_LOOP: asm {

		// Trampoline: short JLE stays close; near JMP reaches far label.

		MOV		AX, [row]
		CMP		AX, [dest_y2]
		JLE		SCALE_ROW_CONTINUE
		JMP		SCALE_DONE

	} SCALE_ROW_CONTINUE: asm {

		// source_row = y_accumulator >> 16 (high word)

		MOV		AX, [y_accumulator_high]
		MOV		[source_row], AX

		// source_row_offset = source_row * source_xs

		MUL		WORD PTR [source_xs]
		MOV		[source_row_offset], AX

		// dest_row_offset = row * 320 + dest_x1

		MOV		AX, [row]
		MOV		CX, 320
		MUL		CX
		ADD		AX, [dest_x1]
		MOV		[dest_row_offset], AX

		// Reset per-row x accumulator from clipping-adjusted start.

		MOV		AX, [x_accumulator_start_low]
		MOV		[x_accumulator_low], AX
		MOV		AX, [x_accumulator_start_high]
		MOV		[x_accumulator_high], AX

		// Inner pixel loop.

		PUSH	DS
		PUSH	ES
		PUSH	SI
		PUSH	DI

		// ES:DI = dest segment : row offset

		MOV		AX, [dest]
		MOV		ES, AX
		MOV		DI, [dest_row_offset]

		// DS = source segment, BX = source_offs + source_row_offset

		MOV		AX, [source_seg]
		MOV		DS, AX
		MOV		BX, [source_offs]
		ADD		BX, [source_row_offset]

		MOV		DL, [mask]
		MOV		CX, [clipped_width]

	} SCALE_NEXT_PIXEL: asm {

		MOV		SI, [x_accumulator_high]
		MOV		AL, [BX + SI]
		CMP		AL, DL
		JZ		SCALE_SKIP_PIXEL
		MOV		ES:[DI], AL

	} SCALE_SKIP_PIXEL: asm {

		INC		DI

		MOV		AX, [x_accumulator_low]
		ADD		AX, [x_step_low]
		MOV		[x_accumulator_low], AX
		MOV		AX, [x_accumulator_high]
		ADC		AX, [x_step_high]
		MOV		[x_accumulator_high], AX

		DEC		CX
		JNZ		SCALE_NEXT_PIXEL

	    // Restore registers and update y_accumulator for next row.

		POP		DI
		POP		SI
		POP		ES
		POP		DS

		// y_accumulator += y_step (32-bit add)

		MOV		AX, [y_accumulator_low]
		ADD		AX, [y_step_low]
		MOV		[y_accumulator_low], AX
		MOV		AX, [y_accumulator_high]
		ADC		AX, [y_step_high]
		MOV		[y_accumulator_high], AX

		INC		WORD PTR [row]
		JMP		SCALE_ROW_LOOP

	} SCALE_DONE:;
}

//----------------------------------------------------------------------------

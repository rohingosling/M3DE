;****************************************************************************
; Program: GFX-13 (Graphics Mode 13h Library)
; Version: 1.1
; Date:    1991-01-07
; Author:  Rohin Gosling
;
; Description:
;
;   VGA Mode 13h (320x200, 256 color), 2D graphics library.
;
; Assembly:
;
;   - Assembler: TASM (Borland Turbo Assembler)
;   - Assemble:  tasm /ml gfx13.asm
;   - Target:    80186+ real mode, medium memory model.
;
; Notes:
;
;   We use the MEDIUM memory model (FAR code, NEAR data) for compatibility
;   with BCC -ml compiled code (M3DE). All public procedures use FAR
;   entry/exit sequences, while data (clip variables) stays in DGROUP
;   and is accessed via DS.
;
;   Segment addresses are passed as explicit WORD parameters (e.g., 0xA000
;   for VGA memory) and ES is set manually via MOV ES, segment_value.
;   The entire library code fits in one code segment, and the data segment
;   only holds 4 clip variables.
;
; Change Log:
;
; - Version 1.1
;
;   - Added GetPixel to return the color of a pixel at a specified location
;     in the screen buffer. 
;
;   - Added PutImage to place a rectangular block of pixels from source to 
;     destination with clipping.
;
;****************************************************************************

				.MODEL	MEDIUM, C
				.186
				JUMPS

;============================================================================
; Public Declarations (C linkage, underscores added automatically)
;============================================================================

				; Video mode management

				PUBLIC	SetMode13		; Switch to VGA Mode 13h.
				PUBLIC	SetTextMode		; Switch to 80-column text mode with specified row count.
				PUBLIC	GetTextMode		; Query current text mode row count (0 if not a text mode).

				; Palette and clipping

				PUBLIC	SetPalette		; Set VGA DAC palette entries.
				PUBLIC	SetClipping		; Set clipping rectangle for drawing functions.

				; Screen management

				PUBLIC	ClearScreen		; Fill entire screen buffer with a single color.
				PUBLIC	FlipScreen		; Copy entire screen buffer from source to dest.
				PUBLIC	WaitRetrace		; Synchronize with VGA vertical retrace.

				; Pixel operations

				PUBLIC	PutPixel		; Plot a single pixel with optional clipping.
				PUBLIC	GetPixel		; Read a single pixel with optional clipping.

				; Drawing primitives

				PUBLIC	Line		    ; Draw a line with clipping.
				PUBLIC	Rectangle		; Draw a rectangle with clipping.
				PUBLIC	Triangle		; Draw a triangle with clipping.
				PUBLIC	Quad			; Draw a quadrilateral with clipping.

				; Filled primitives

				PUBLIC	FillRectangle	; Draw a filled rectangle with clipping.
				PUBLIC	FillTriangle	; Draw a filled triangle with clipping.
				PUBLIC	FillQuad		; Draw a filled quadrilateral with clipping.

				; Image blitting and scaling

				PUBLIC	PutImage		; Place a rectangular block of pixels from source to destination with clipping.

;============================================================================
; Macros
;============================================================================

;----------------------------------------------------------------------------
; FIXED_DIVIDE
;
; Description:
;
;   Signed 16.16 fixed-point division: result = (DX << 16) / CX
;
; Input:  DX = signed 16-bit numerator
;         CX = signed 16-bit denominator (must be nonzero)
;
; Output: DX:AX = 16.16 signed fixed-point result
;         DX    = integer part, AX = fractional part
;
; Clobbers: AX, BX, CX, DX
;
;----------------------------------------------------------------------------

FIXED_DIVIDE	MACRO
				LOCAL	num_pos, den_pos, div_done

				MOV		BX, DX
				XOR		BX, CX			; sign of result in bit 15
				PUSH	BX

				XOR		AX, AX			; DX:AX = numerator << 16

				TEST	DX, DX
				JGE		num_pos
				NEG		DX

num_pos:
				TEST	CX, CX
				JGE		den_pos
				NEG		CX

den_pos:
				; Two-step unsigned 32/16 division.

				; Step 1:
				; high word / divisor.

				MOV		AX, DX
				XOR		DX, DX
				DIV		CX				; AX = quotient high, DX = remainder
				PUSH	AX

				; Step 2:
				; (remainder:0) / divisor.

				XOR		AX, AX
				DIV		CX				; AX = quotient low
				POP		DX				; DX = quotient high

				; Apply sign.

				POP		BX
				TEST	BX, BX
				JGE		div_done
				NEG		DX
				NEG		AX
				SBB		DX, 0

div_done:

ENDM

;----------------------------------------------------------------------------
; CLIPPED_HFILL
;
;   Draws a clipped horizontal line fill at scanline [y_var], from [left_var]
;   to [right_var]. 
;
;   Clips against clip_x1/y1/x2/y2. Preserves ES.
;
;   Expects `dest` and `col` to be accessible PROC parameters.
;
; Parameters:
;
;   y_var     (WORD): Name of the WORD variable holding the Y coordinate.
;
;   left_var  (WORD): Name of the WORD variable holding the left X coordinate.
;
;   right_var (WORD): Name of the WORD variable holding the right X coordinate.
;
;----------------------------------------------------------------------------

CLIPPED_HFILL	MACRO 	y_var, left_var, right_var
				LOCAL	sorted, skip, left_ok, right_ok

				; Y clip: skip if scanline outside clip bounds.

				MOV		AX,	y_var
				CMP		AX,	clip_y1
				JL		skip
				CMP		AX,	clip_y2
				JG		skip

				; Sort: ensure left <= right.

				MOV		AX,	left_var
				MOV		BX,	right_var
				CMP		AX,	BX
				JLE		sorted
				XCHG	AX,	BX

sorted:
				; X rejection: skip if span is entirely outside clip bounds.

				CMP		BX,	clip_x1
				JL		skip
				CMP		AX,	clip_x2
				JG		skip

				; Clamp left to clip_x1.

				CMP		AX,	clip_x1
				JGE		left_ok
				MOV		AX,	clip_x1

left_ok:
				; Clamp right to clip_x2.

				CMP		BX,	clip_x2
				JLE		right_ok
				MOV		BX,	clip_x2

right_ok:
				; length = right - left + 1

				SUB		BX,	AX
				INC		BX

				; Set up ES:DI for the fill.

				PUSH	ES
				MOV		DX,	dest
				MOV		ES,	DX

				; offset = y * 320 + left

				MOV		DX,	BX				; save length
				MOV		BX,	AX				; BX = left
				MOV		CX,	y_var
				XCHG	CH,	CL				; CX = y * 256
				ADD		BX,	CX
				SHR		CX,	2				; CX = y * 64
				ADD		BX,	CX				; BX = y * 320 + left

				; Fill the span with the specified color.

				MOV		DI,	BX
				MOV		CX,	DX				; CX = length
				MOV		AL,	BYTE PTR col
				REP		STOSB

				POP		ES

skip:

ENDM

;----------------------------------------------------------------------------
; DDA_ADVANCE
;
; Advances the left and right 16.16 fixed-point DDA accumulators by their
; respective slopes.
;
; Expects the following accessible PROC locals.
; - left_x_low
; - left_x_high
; - left_slope_low
; - left_slope_high
; - right_x_low
; - right_x_high
; - right_slope_low
; - right_slope_high
;         
;----------------------------------------------------------------------------

DDA_ADVANCE	MACRO

				; Advance left edge: left_x += left_slope

				MOV		AX,           left_x_low
				ADD		AX,           left_slope_low
				MOV		left_x_low,   AX
				MOV		AX,           left_x_high
				ADC		AX,           left_slope_high
				MOV		left_x_high,  AX

				; Advance right edge: right_x += right_slope

				MOV		AX,           right_x_low
				ADD		AX,           right_slope_low
				MOV		right_x_low,  AX
				MOV		AX,           right_x_high
				ADC		AX,           right_slope_high
				MOV		right_x_high, AX
ENDM

;============================================================================
; Data Segment
;============================================================================

				.DATA

clip_x1			DW		0
clip_y1			DW		0
clip_x2			DW		319
clip_y2			DW		199

;============================================================================
; Code Segment
;============================================================================

				.CODE

;----------------------------------------------------------------------------
; SetMode13
;
;   Switches the video adapter to VGA Mode 13h (320x200, 256 colors).
;
;----------------------------------------------------------------------------

SetMode13		PROC	C

				MOV		AX,	0013h
				INT		10h
				RET

SetMode13 		ENDP

;----------------------------------------------------------------------------
; SetTextMode
;
;   Sets the video adapter to 80-column text mode (mode 3) with a
;   configurable number of text rows (25, 43, or 50).
;
; Parameters:
;
;   rows (WORD): Number of text rows to display (25, 43, or 50).
;
;----------------------------------------------------------------------------

SetTextMode		PROC	C USES BX, rows:WORD

				; For 43-line mode, select 350 vertical scanlines first.

				CMP		BYTE PTR rows, 43
				JNE		SETTEXTMODE_SETMODE

				MOV		AX,	1201h
				MOV		BL,	30h
				INT		10h

SETTEXTMODE_SETMODE:

				; Set 80-column text mode (BIOS mode 3).

				MOV		AX,	0003h
				INT		10h

				; For 25-row mode we are done. For 43 or 50 rows, load the
				; 8x8 ROM font.

				CMP		BYTE PTR rows, 25
				JE		SETTEXTMODE_DONE
				CMP		BYTE PTR rows, 43
				JE		SETTEXTMODE_LOADFONT
				CMP		BYTE PTR rows, 50
				JE		SETTEXTMODE_LOADFONT
				JMP		SETTEXTMODE_DONE

SETTEXTMODE_LOADFONT:

				; Load the 8x8 ROM font into the active character generator.

				MOV		AX,	1112h
				MOV		BL,	00h
				INT		10h

SETTEXTMODE_DONE:

				RET

SetTextMode		ENDP

;----------------------------------------------------------------------------
; GetTextMode
;
;   Returns the number of text rows for the current video mode (25, 43,
;   or 50), or 0 if the current mode is not a text mode.
;
;----------------------------------------------------------------------------

GetTextMode		PROC	C
				LOCAL	textRows:WORD

				; Query the current video mode.

				MOV		AH,	0Fh
				INT		10h

				; Check for 80-column text modes (mode 2 or 3).

				CMP		AL,	03h
				JE		GETTEXTMODE_READROWS
				CMP		AL,	02h
				JE		GETTEXTMODE_READROWS

				; Not a text mode. Return 0.

				XOR		AL,	AL
				JMP		GETTEXTMODE_STORE

GETTEXTMODE_READROWS:

				; Read the row count from the BIOS Data Area (0040:0084h).

				PUSH	ES
				MOV		AX,	0040h
				MOV		ES,	AX
				MOV		AL,	ES:[0084h]
				INC		AL
				POP		ES

GETTEXTMODE_STORE:

				; Store the row count in textRows and return it in AX.
				; If not a text mode, this will return 0.

				XOR		AH,	AH
				MOV		textRows, AX

				MOV		AX,	textRows
				RET

GetTextMode		ENDP

;----------------------------------------------------------------------------
; SetPalette
;
;   Programs the VGA DAC palette registers by streaming RGB triplets
;   to port 03C8h/03C9h using REP OUTSB.
;
; Parameters:
;
;   col        (WORD): Starting palette index (0-255).
;
;   colorCount (WORD): Number of palette entries to set.
;
;   palSeg     (WORD): Segment address of the RGB data array.
;
;   dataOffset (WORD): Offset of the RGB data within palSeg.
;
;----------------------------------------------------------------------------

SetPalette		PROC	C USES BX SI, col:WORD, colorCount:WORD, palSeg:WORD, dataOffset:WORD

				PUSH	DS

				; Write starting color index to port 03C8h.

				MOV		AL,	BYTE PTR col
				MOV		DX,	03C8h
				OUT		DX,	AL

				; Load source pointer DS:SI.

				MOV		AX,	palSeg
				MOV		SI,	dataOffset
				MOV		DS,	AX

				; CX = count * 3 (three bytes per RGB triplet).

				MOV		CX,	colorCount
				MOV		BX,	CX
				SHL		CX,	1
				ADD		CX,	BX

				; Stream RGB data to port 03C9h.

				MOV		DX,	03C9h
				REP		OUTSB

				POP		DS
				RET

SetPalette		ENDP

;----------------------------------------------------------------------------
; SetClipping
;
;   Sets the clipping rectangle used by drawing functions.
;
; Parameters:
;
;   x0 (WORD): Left edge of the clipping rectangle.
;
;   y0 (WORD): Top edge of the clipping rectangle.
;
;   x1 (WORD): Right edge of the clipping rectangle.
;
;   y1 (WORD): Bottom edge of the clipping rectangle.
;
;----------------------------------------------------------------------------

SetClipping		PROC	C x0:WORD, y0:WORD, x1:WORD, y1:WORD

				; Set x0 clipping boundary.

				MOV		AX,	x0
				MOV		clip_x1, AX

				; Set y0 clipping boundary.

				MOV		AX,	y0
				MOV		clip_y1, AX

				; Set x1 clipping boundary.

				MOV		AX,	x1
				MOV		clip_x2, AX

				; Set y1 clipping boundary.

				MOV		AX,	y1
				MOV		clip_y2, AX

				RET

SetClipping		ENDP

;----------------------------------------------------------------------------
; ClearScreen
;
;   Fills the entire 64000-byte screen buffer with a single color using
;   REP STOSW (32000 words).
;
; Parameters:
;
;   col  (WORD): Fill color (palette index, low byte used).
;
;   dest (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

ClearScreen		PROC	C USES BX DI, col:WORD, dest:WORD

				PUSH	ES

				; Set up ES:DI for the fill. 

				MOV		AL,	BYTE PTR col
				MOV		BX,	dest
				MOV		ES,	BX
				XOR		DI,	DI

				; Fill the screen buffer with the specified color. 
				
				MOV		AH,	AL			; replicate color into both bytes
				MOV		CX,	32000
				REP		STOSW

				POP		ES
				RET

ClearScreen		ENDP

;----------------------------------------------------------------------------
; WaitRetrace
;
;   Synchronizes with the VGA vertical retrace by polling port 03DAh.
;
;----------------------------------------------------------------------------

WaitRetrace		PROC	C

				MOV		DX,	03DAh

				; Wait for any active retrace to end.

WR_RETRACE:
				IN		AL,	DX
				TEST	AL,	08h
				JNZ		WR_RETRACE

				; Wait for the next retrace to begin.

WR_NO_RETRACE:
				IN		AL,	DX
				TEST	AL,	08h
				JZ		WR_NO_RETRACE

				RET

WaitRetrace		ENDP

;----------------------------------------------------------------------------
; FlipScreen
;
;   Copies the entire 64000-byte buffer from source to dest using
;   REP MOVSW (32000 words).
;
; Parameters:
;
;   source (WORD): Segment address of the source buffer.
;
;   dest   (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

FlipScreen		PROC	C USES BX SI DI, source:WORD, dest:WORD

				PUSH	DS
				PUSH	ES
				
				; Set up DS:SI and ES:DI for the copy.

				MOV		BX,	source
				MOV		AX,	dest

				; Compute pixel offset = y * 320 + x for the start of the buffer (0,0).		

				MOV		DS,	BX
				MOV		ES,	AX
				XOR		SI,	SI
				XOR		DI,	DI
				MOV		CX,	32000
				REP		MOVSW

				; Restore DS and ES.

				POP		ES
				POP		DS
				RET

FlipScreen		ENDP

;----------------------------------------------------------------------------
; PutPixel
;
;   Plots a single pixel at (x, y) in a 320-byte-wide screen buffer.
;
;   Optionally clips against the current clipping rectangle.
;
; Parameters:
;
;   x    (WORD): X coordinate of the pixel.
;
;   y    (WORD): Y coordinate of the pixel.
;
;   col  (WORD): Pixel color (palette index, low byte used).
;
;   clip (WORD): Nonzero to enable clipping, 0 to skip.
;
;   dest (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

PutPixel		PROC	C USES BX DI, x:WORD, y:WORD, col:WORD, clip:WORD, dest:WORD

				; Skip clipping if clip == 0.

				CMP		WORD PTR clip, 0
				JE		PUTPIXEL_DRAW

				; Clip against clipping rectangle.

				MOV		AX,	x
				CMP		AX,	clip_x1
				JL		PUTPIXEL_DONE
				CMP		AX,	clip_x2
				JG		PUTPIXEL_DONE
				MOV		AX,	y
				CMP		AX,	clip_y1
				JL		PUTPIXEL_DONE
				CMP		AX,	clip_y2
				JG		PUTPIXEL_DONE

PUTPIXEL_DRAW:

				PUSH	ES

				; Set up BX, CX, DX for pixel plotting. BX = x, CX = y, DX = dest.

				MOV		BX,	x
				MOV		CX,	y
				MOV		AL,	BYTE PTR col
				MOV		DX,	dest

				; Compute pixel offset = y * 320 + x.

				XCHG	CH,	CL			; CX = y * 256
				ADD		BX,	CX
				SHR		CX,	2			; CX = y * 64
				ADD		BX,	CX			; BX = y * 320 + x

				; Plot the pixel by writing the color to ES:[DI].

				MOV		ES,	DX
				MOV		DI,	BX
				STOSB

				POP		ES

PUTPIXEL_DONE:
				RET

PutPixel		ENDP

;----------------------------------------------------------------------------
; GetPixel
;
;   Reads a single pixel from (x, y) in a 320-byte-wide screen buffer.
;
;   Returns the palette color index, or 0 if the pixel is clipped.
;
; Parameters:
;
;   x      (WORD): X coordinate of the pixel.
;
;   y      (WORD): Y coordinate of the pixel.
;
;   clip   (WORD): Nonzero to enable clipping, 0 to skip.
;
;   source (WORD): Segment address of the source buffer.
;
;----------------------------------------------------------------------------

GetPixel		PROC	C USES BX SI, x:WORD, y:WORD, clip:WORD, source:WORD

				LOCAL	color:WORD

				; Initialize color to 0 (black) in case of clipping.

				MOV		WORD PTR color,	0

				; Skip clipping if clip == 0.

				CMP		WORD PTR clip, 0
				JE		GETPIXEL_READ

				; Clip against clipping rectangle.

				MOV		AX,	x
				CMP		AX,	clip_x1
				JL		GETPIXEL_DONE
				CMP		AX,	clip_x2
				JG		GETPIXEL_DONE
				MOV		AX,	y
				CMP		AX,	clip_y1
				JL		GETPIXEL_DONE
				CMP		AX,	clip_y2
				JG		GETPIXEL_DONE

GETPIXEL_READ:
				PUSH	DS

				; Set up BX, CX, DX for pixel reading. BX = x, CX = y, DX = source.

				MOV		BX,	x
				MOV		CX,	y
				MOV		DX,	source

				; Compute pixel offset = y * 320 + x.

				XCHG	CH,	CL
				ADD		BX,	CX
				SHR		CX,	2
				ADD		BX,	CX

				; Read the pixel color from DS:[SI].

				MOV		DS,	DX
				MOV		SI,	BX
				LODSB

				POP		DS

				; Store the color in the local variable `color`. The color is returned in AX.

				XOR		AH,	AH
				MOV		color, AX

GETPIXEL_DONE:
				MOV		AX,	color
				RET

GetPixel		ENDP

;----------------------------------------------------------------------------
; Line
;
;   Draws a line from (x0, y0) to (x1, y1) in two phases:
;
;   Phase 1 - Cohen-Sutherland clipping:
;     Classifies each endpoint into one of nine screen regions using a
;     4-bit outcode. Iteratively clips the line against whichever edge
;     an outside endpoint violates, until both endpoints lie inside the
;     clipping rectangle (accept) or are proven entirely outside (reject).
;
;   Phase 2 - Bresenham's line algorithm:
;     Rasterizes the clipped segment one pixel at a time, using an
;     integer error accumulator to decide when to step along the
;     minor axis.
;
; Parameters:
;
;   x0   (WORD): X coordinate of the first endpoint.
;
;   y0   (WORD): Y coordinate of the first endpoint.
;
;   x1   (WORD): X coordinate of the second endpoint.
;
;   y1   (WORD): Y coordinate of the second endpoint.
;
;   col  (WORD): Line color (palette index, low byte used).
;
;   clip (WORD): Nonzero to enable clipping, 0 to skip.
;
;   dest (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

Line			PROC	C USES BX, x0:WORD, y0:WORD, x1:WORD, y1:WORD, col:WORD, clip:WORD, dest:WORD

				LOCAL	current_x:WORD		; Pixel cursor X position during Bresenham traversal.
				LOCAL	current_y:WORD		; Pixel cursor Y position during Bresenham traversal.
				LOCAL	delta_x:WORD		; Absolute horizontal distance between endpoints.
				LOCAL	delta_y:WORD		; Absolute vertical distance between endpoints.
				LOCAL	step_x:WORD			; X direction per pixel: +1 (right) or -1 (left).
				LOCAL	step_y:WORD			; Y direction per pixel: +1 (down) or -1 (up).
				LOCAL	error:WORD			; Bresenham error accumulator (delta_x - delta_y).
				LOCAL	outcode_a:WORD		; 4-bit region code for endpoint A (x0, y0).
				LOCAL	outcode_b:WORD		; 4-bit region code for endpoint B (x1, y1).

				; Skip clipping if clip == 0.

				CMP		WORD PTR clip,	0
				JE		LINE_CLIP_ACCEPT

				; Compute outcode_a for (x0, y0)
				;
				; The Cohen-Sutherland algorithm divides the plane around the
				; clipping rectangle into nine regions, each encoded as a 4-bit
				; outcode. A zero outcode means the point is inside.
				;
				;          Left (1)     Inside (0)    Right (2)
				;         +----------+------------+----------+
				;   1001  |          |            |          |  1010
				;   Top+L |  Top (8) |  Top only  | Top+R    |
				;         +----------+============+----------+ - clip_y1
				;   0001  |          |            |          |  0010
				;   Left  |  Left    |  INSIDE 0  | Right    |
				;         +----------+============+----------+ - clip_y2
				;   0101  |          |            |          |  0110
				;   Bot+L | Bot (4)  | Bot only   | Bot+R    |
				;         +----------+------------+----------+
				;              |                        |
				;           clip_x1                  clip_x2
				;
				; Bit 0 (1): Left   - point is left of clip_x1
				; Bit 1 (2): Right  - point is right of clip_x2
				; Bit 2 (4): Bottom - point is below clip_y2
				; Bit 3 (8): Top    - point is above clip_y1

				XOR		AX,	AX			; Clear outcode accumulator.
				MOV		BX,	x0

				; Check X coordinate against clip_x1 to set the Left bit.

				CMP		BX,	clip_x1
				JGE		LINE_OA_NOT_LEFT
				OR		AX,	1

LINE_OA_NOT_LEFT:

				; Check X coordinate against clip_x2 to set the Right bit.

				CMP		BX,	clip_x2
				JLE		LINE_OA_NOT_RIGHT
				OR		AX,	2

LINE_OA_NOT_RIGHT:

				; Check Y coordinate against clip_y2 to set the Bottom bit.
				
				MOV		BX,	y0
				CMP		BX,	clip_y2
				JLE		LINE_OA_NOT_BOTTOM
				OR		AX,	4

LINE_OA_NOT_BOTTOM:

				; Check Y coordinate against clip_y1 to set the Top bit.

				CMP		BX,	clip_y1
				JGE		LINE_OA_NOT_TOP
				OR		AX,	8

LINE_OA_NOT_TOP:
				MOV		outcode_a,	AX

				;Compute outcode_b for (x1, y1)  

				XOR		AX,	AX			; Clear outcode accumulator.
				MOV		BX,	x1

				; Check X coordinate against clip_x1 to set the Left bit.

				CMP		BX,	clip_x1
				JGE		LINE_OB_NOT_LEFT
				OR		AX,	1

LINE_OB_NOT_LEFT:

				; Check X coordinate against clip_x2 to set the Right bit.

				CMP		BX,	clip_x2
				JLE		LINE_OB_NOT_RIGHT
				OR		AX,	2

LINE_OB_NOT_RIGHT:

				; Check Y coordinate against clip_y2 to set the Bottom bit.

				MOV		BX,	y1
				CMP		BX,	clip_y2
				JLE		LINE_OB_NOT_BOTTOM
				OR		AX,	4

LINE_OB_NOT_BOTTOM:

				; Check Y coordinate against clip_y1 to set the Top bit.

				CMP		BX,	clip_y1
				JGE		LINE_OB_NOT_TOP
				OR		AX,	8

LINE_OB_NOT_TOP:
				MOV		outcode_b,	AX

				; Cohen-Sutherland clip loop  
				;
				; Each iteration either accepts, rejects, or clips one endpoint
				; against a single edge and recomputes its outcode. The loop
				; terminates when both outcodes are zero (accept) or share a
				; common bit (reject).

LINE_CLIP_LOOP:

				; Trivial accept: if (outcode_a | outcode_b) == 0, both
				; endpoints lie inside the clipping rectangle.

				MOV		AX,	outcode_a
				OR		AX,	outcode_b
				JZ		LINE_CLIP_ACCEPT

				; Trivial reject: if (outcode_a & outcode_b) != 0, both
				; endpoints share a region outside the same edge, so the
				; entire line is outside the clipping rectangle.

				MOV		AX,	outcode_a
				AND		AX,	outcode_b
				JNZ		LINE_DONE

				; At least one endpoint is outside. Pick endpoint A if its
				; outcode is nonzero; otherwise clip endpoint B.

				MOV		AX,	outcode_a
				TEST	AX,	AX
				JZ		LINE_CLIP_B

				; Clip endpoint A (x0, y0) 
				;
				; Test each outcode bit and clip against the corresponding
				; edge. For horizontal edges (top/bottom), compute the new
				; X by intersecting the line with that edge's Y coordinate:
				;   x = x0 + (x1 - x0) * (edge_y - y0) / (y1 - y0)
				; For vertical edges (left/right), compute the new Y:
				;   y = y0 + (y1 - y0) * (edge_x - x0) / (x1 - x0)

				TEST	WORD PTR outcode_a,	8
				JZ		LINE_CLIP_A_NOT_TOP

				; Clip A against top edge.
				; x0 = x0 + (x1 - x0) * (clip_y1 - y0) / (y1 - y0)
				; y0 = clip_y1

				MOV		AX,	x1
				SUB		AX,	x0
				MOV		CX,	clip_y1
				SUB		CX,	y0
				IMUL	CX
				MOV		CX,	y1
				SUB		CX,	y0
				IDIV	CX
				ADD		AX,	x0
				MOV		x0,	AX
				MOV		AX,	clip_y1
				MOV		y0,	AX
				JMP		LINE_RECOMPUTE_OA

LINE_CLIP_A_NOT_TOP:
				TEST	WORD PTR outcode_a,	4
				JZ		LINE_CLIP_A_NOT_BOTTOM

				; Clip A against bottom edge.
				; x0 = x0 + (x1 - x0) * (clip_y2 - y0) / (y1 - y0)
				; y0 = clip_y2

				MOV		AX,	x1
				SUB		AX,	x0
				MOV		CX,	clip_y2
				SUB		CX,	y0
				IMUL	CX
				MOV		CX,	y1
				SUB		CX,	y0
				IDIV	CX
				ADD		AX,	x0
				MOV		x0,	AX
				MOV		AX,	clip_y2
				MOV		y0,	AX
				JMP		LINE_RECOMPUTE_OA

LINE_CLIP_A_NOT_BOTTOM:
				TEST	WORD PTR outcode_a,	2
				JZ		LINE_CLIP_A_NOT_RIGHT

				; Clip A against right edge.
				; y0 = y0 + (y1 - y0) * (clip_x2 - x0) / (x1 - x0)
				; x0 = clip_x2

				MOV		AX,	y1
				SUB		AX,	y0
				MOV		CX,	clip_x2
				SUB		CX,	x0
				IMUL	CX
				MOV		CX,	x1
				SUB		CX,	x0
				IDIV	CX
				ADD		AX,	y0
				MOV		y0,	AX
				MOV		AX,	clip_x2
				MOV		x0,	AX
				JMP		LINE_RECOMPUTE_OA

LINE_CLIP_A_NOT_RIGHT:

				; If none of the above bits were set, the Left bit must be.
				; Clip A against left edge.
				; y0 = y0 + (y1 - y0) * (clip_x1 - x0) / (x1 - x0)
				; x0 = clip_x1

				MOV		AX,	y1
				SUB		AX,	y0
				MOV		CX,	clip_x1
				SUB		CX,	x0
				IMUL	CX
				MOV		CX,	x1
				SUB		CX,	x0
				IDIV	CX
				ADD		AX,	y0
				MOV		y0,	AX
				MOV		AX,	clip_x1
				MOV		x0,	AX

LINE_RECOMPUTE_OA:

				; Recompute outcode_a for the updated (x0, y0), then loop
				; back to re-test for trivial accept or reject.

				XOR		AX,	AX			; Clear outcode accumulator.
				MOV		BX,	x0

				; Check X coordinate against clip_x1 to set the Left bit.

				CMP		BX,	clip_x1
				JGE		LINE_ROA_NOT_LEFT
				OR		AX,	1

LINE_ROA_NOT_LEFT:

				; Check X coordinate against clip_x2 to set the Right bit.

				CMP		BX,	clip_x2
				JLE		LINE_ROA_NOT_RIGHT
				OR		AX,	2

LINE_ROA_NOT_RIGHT:
				MOV		BX,	y0

				; Check Y coordinate against clip_y2 to set the Bottom bit.

				CMP		BX,	clip_y2
				JLE		LINE_ROA_NOT_BOTTOM
				OR		AX,	4

LINE_ROA_NOT_BOTTOM:

				; Check Y coordinate against clip_y1 to set the Top bit.

				CMP		BX,	clip_y1
				JGE		LINE_ROA_NOT_TOP
				OR		AX,	8

LINE_ROA_NOT_TOP:
				MOV		outcode_a,	AX
				JMP		LINE_CLIP_LOOP

				; Clip endpoint B (x1, y1) 
				;
				; Same intersection formulas as endpoint A, but applied to
				; (x1, y1) against the edge indicated by outcode_b.

LINE_CLIP_B:

				TEST	WORD PTR outcode_b,	8
				JZ		LINE_CLIP_B_NOT_TOP

				; Clip B against top edge.
				; x1 = x1 + (x0 - x1) * (clip_y1 - y1) / (y0 - y1)
				; y1 = clip_y1

				MOV		AX,	x0
				SUB		AX,	x1
				MOV		CX,	clip_y1
				SUB		CX,	y1
				IMUL	CX
				MOV		CX,	y0
				SUB		CX,	y1
				IDIV	CX
				ADD		AX,	x1
				MOV		x1,	AX
				MOV		AX,	clip_y1
				MOV		y1,	AX
				JMP		LINE_RECOMPUTE_OB

LINE_CLIP_B_NOT_TOP:
				TEST	WORD PTR outcode_b,	4
				JZ		LINE_CLIP_B_NOT_BOTTOM

				; Clip B against bottom edge.
				; x1 = x1 + (x0 - x1) * (clip_y2 - y1) / (y0 - y1)
				; y1 = clip_y2

				MOV		AX,	x0
				SUB		AX,	x1
				MOV		CX,	clip_y2
				SUB		CX,	y1
				IMUL	CX
				MOV		CX,	y0
				SUB		CX,	y1
				IDIV	CX
				ADD		AX,	x1
				MOV		x1,	AX
				MOV		AX,	clip_y2
				MOV		y1,	AX
				JMP		LINE_RECOMPUTE_OB

LINE_CLIP_B_NOT_BOTTOM:
				TEST	WORD PTR outcode_b,	2
				JZ		LINE_CLIP_B_NOT_RIGHT

				; Clip B against right edge.
				; y1 = y1 + (y0 - y1) * (clip_x2 - x1) / (x0 - x1)
				; x1 = clip_x2

				MOV		AX,	y0
				SUB		AX,	y1
				MOV		CX,	clip_x2
				SUB		CX,	x1
				IMUL	CX
				MOV		CX,	x0
				SUB		CX,	x1
				IDIV	CX
				ADD		AX,	y1
				MOV		y1,	AX
				MOV		AX,	clip_x2
				MOV		x1,	AX
				JMP		LINE_RECOMPUTE_OB

LINE_CLIP_B_NOT_RIGHT:

				; If none of the above bits were set, the Left bit must be.
				; Clip B against left edge.
				; y1 = y1 + (y0 - y1) * (clip_x1 - x1) / (x0 - x1)
				; x1 = clip_x1

				MOV		AX,	y0
				SUB		AX,	y1
				MOV		CX,	clip_x1
				SUB		CX,	x1
				IMUL	CX
				MOV		CX,	x0
				SUB		CX,	x1
				IDIV	CX
				ADD		AX,	y1
				MOV		y1,	AX
				MOV		AX,	clip_x1
				MOV		x1,	AX

LINE_RECOMPUTE_OB:

				; Recompute outcode_b for the updated (x1, y1), then loop
				; back to re-test for trivial accept or reject.

				XOR		AX,	AX			; Clear outcode accumulator.
				MOV		BX,	x1

				; Check X coordinate against clip_x1 to set the Left bit.

				CMP		BX,	clip_x1
				JGE		LINE_ROB_NOT_LEFT
				OR		AX,	1

LINE_ROB_NOT_LEFT:

				; Check X coordinate against clip_x2 to set the Right bit.

				CMP		BX,	clip_x2
				JLE		LINE_ROB_NOT_RIGHT
				OR		AX,	2

LINE_ROB_NOT_RIGHT:
				MOV		BX,	y1

				; Check Y coordinate against clip_y2 to set the Bottom bit.

				CMP		BX,	clip_y2
				JLE		LINE_ROB_NOT_BOTTOM
				OR		AX,	4

LINE_ROB_NOT_BOTTOM:

				; Check Y coordinate against clip_y1 to set the Top bit.

				CMP		BX,	clip_y1
				JGE		LINE_ROB_NOT_TOP
				OR		AX,	8

LINE_ROB_NOT_TOP:
				MOV		outcode_b, AX
				JMP		LINE_CLIP_LOOP

				; Clipping accepted. Begin Bresenham setup. 
				;
				; Compute absolute deltas and step directions (+1 or -1) for
				; each axis, then initialize the error accumulator to
				; (delta_x - delta_y).

LINE_CLIP_ACCEPT:

				; Start at the first endpoint.

				MOV		AX,	x0
				MOV		current_x, AX
				MOV		AX,	y0
				MOV		current_y, AX

				; Compute signed deltas between endpoints.

				MOV		AX,	x1
				SUB		AX,	current_x
				MOV		delta_x, AX

				MOV		AX,	y1
				SUB		AX,	current_y
				MOV		delta_y, AX

				; Default step direction is +1 on both axes. If a delta is
				; negative, negate it to get the absolute distance and flip
				; the step direction to -1.

				MOV		WORD PTR step_x, 1
				MOV		WORD PTR step_y, 1

				CMP		WORD PTR delta_x, 0
				JGE		LINE_DX_POS
				NEG		WORD PTR delta_x
				MOV		WORD PTR step_x, -1

LINE_DX_POS:

				CMP		WORD PTR delta_y, 0
				JGE		LINE_DY_POS
				NEG		WORD PTR delta_y
				MOV		WORD PTR step_y, -1

LINE_DY_POS:

				; Initialize the error accumulator. A positive bias toward
				; delta_x means the X axis steps first when the line is
				; more horizontal than vertical, and vice versa.

				; error = delta_x - delta_y

				MOV		AX,	delta_x
				SUB		AX,	delta_y
				MOV		error,   AX

				; Bresenham main loop 
				;
				; Each iteration plots one pixel, then checks whether we
				; have reached the endpoint. If not, the doubled error term
				; is tested against the two deltas to decide whether to
				; advance along X, Y, or both.

LINE_BRES_LOOP:

				; Plot pixel at (current_x, current_y).
				; Compute the linear framebuffer offset as y * 320 + x
				; using the shift decomposition y*256 + y*64 + x.

				PUSH	ES					; Save ES and DI, since STOSB
				PUSH	DI					; writes to ES:DI and increments DI.

				MOV		BX,	current_x
				MOV		CX,	current_y
				MOV		AL,	BYTE PTR col	; AL = pixel color for STOSB.
				MOV		DX,	dest			; DX = destination video segment.

				XCHG	CH,	CL			; CX = y * 256
				ADD		BX,	CX
				SHR		CX,	2			; CX = y * 64
				ADD		BX,	CX			; BX = y * 320 + x

				MOV		ES,	DX			; Point ES:DI to the target pixel.
				MOV		DI,	BX
				STOSB					; Store AL at ES:DI (write pixel).

				POP		DI					; Restore DI and ES to their
				POP		ES					; pre-plot values.

				; Check if we have reached the endpoint (x1, y1). The pixel
				; is always plotted before this test, so the endpoint is
				; guaranteed to be drawn.

				MOV		AX,	current_x
				CMP		AX,	x1
				JNE		LINE_BRES_STEP
				MOV		AX,	current_y
				CMP		AX,	y1
				JE		LINE_DONE

LINE_BRES_STEP:

				; Double the error term (using a left shift) to avoid
				; fractional arithmetic.

				MOV		AX,	error
				SHL		AX,	1

				; If double_error > -delta_y, the error has accumulated
				; enough to warrant an X step. Subtract delta_y from the
				; error and advance current_x by step_x.

				MOV		BX,	delta_y
				NEG		BX
				CMP		AX,	BX
				JLE		LINE_SKIP_X
				MOV		BX,	delta_y
				SUB		error, BX
				MOV		BX,	step_x
				ADD		current_x, BX

LINE_SKIP_X:

				; If double_error < delta_x, the error has accumulated
				; enough to warrant a Y step. Add delta_x to the error
				; and advance current_y by step_y.

				CMP		AX,	delta_x
				JGE		LINE_SKIP_Y
				MOV		BX,	delta_x
				ADD		error, BX
				MOV		BX,	step_y
				ADD		current_y, BX

LINE_SKIP_Y:
				JMP		LINE_BRES_LOOP

LINE_DONE:
				RET

Line			ENDP

;----------------------------------------------------------------------------
; Triangle
;
;   Draws a triangle outline by calling Line three times with clipping
;   enabled.
;
; Parameters:
;
;   x0   (WORD): X coordinate of the first vertex.
;
;   y0   (WORD): Y coordinate of the first vertex.
;
;   x1   (WORD): X coordinate of the second vertex.
;
;   y1   (WORD): Y coordinate of the second vertex.
;
;   x2   (WORD): X coordinate of the third vertex.
;
;   y2   (WORD): Y coordinate of the third vertex.
;
;   col  (WORD): Line color (palette index, low byte used).
;
;   dest (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

Triangle		PROC	C USES BX, x0:WORD, y0:WORD, x1:WORD, y1:WORD, x2:WORD, y2:WORD, col:WORD, dest:WORD

				; Line(x0, y0, x1, y1, col, 1, dest)

				PUSH	dest
				PUSH	1			; clip = 1
				PUSH	col
				PUSH	y1
				PUSH	x1
				PUSH	y0
				PUSH	x0
				CALL	Line
				ADD		SP,	14

				; Line(x1, y1, x2, y2, col, 1, dest)

				PUSH	dest
				PUSH	1
				PUSH	col
				PUSH	y2
				PUSH	x2
				PUSH	y1
				PUSH	x1
				CALL	Line
				ADD		SP,	14

				; Line(x2, y2, x0, y0, col, 1, dest)

				PUSH	dest
				PUSH	1
				PUSH	col
				PUSH	y0
				PUSH	x0
				PUSH	y2
				PUSH	x2
				CALL	Line
				ADD		SP,	14

				RET

Triangle		ENDP

;----------------------------------------------------------------------------
; Rectangle
;
;   Draws an axis-aligned rectangle outline by calling Line four times
;   with clipping enabled.
;
; Parameters:
;
;   x0   (WORD): X coordinate of the top-left corner.
;
;   y0   (WORD): Y coordinate of the top-left corner.
;
;   x1   (WORD): X coordinate of the bottom-right corner.
;
;   y1   (WORD): Y coordinate of the bottom-right corner.
;
;   col  (WORD): Line color (palette index, low byte used).
;
;   dest (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

Rectangle		PROC	C USES BX, x0:WORD, y0:WORD, x1:WORD, y1:WORD, col:WORD, dest:WORD

				; Line(x0, y0, x1, y0, col, 1, dest) - top edge

				PUSH	dest
				PUSH	1
				PUSH	col
				PUSH	y0
				PUSH	x1
				PUSH	y0
				PUSH	x0
				CALL	Line
				ADD		SP,	14

				; Line(x0, y1, x1, y1, col, 1, dest) - bottom edge

				PUSH	dest
				PUSH	1
				PUSH	col
				PUSH	y1
				PUSH	x1
				PUSH	y1
				PUSH	x0
				CALL	Line
				ADD		SP,	14

				; Line(x0, y0, x0, y1, col, 1, dest) - left edge

				PUSH	dest
				PUSH	1
				PUSH	col
				PUSH	y1
				PUSH	x0
				PUSH	y0
				PUSH	x0
				CALL	Line
				ADD		SP,	14

				; Line(x1, y0, x1, y1, col, 1, dest) - right edge

				PUSH	dest
				PUSH	1
				PUSH	col
				PUSH	y1
				PUSH	x1
				PUSH	y0
				PUSH	x1
				CALL	Line
				ADD		SP,	14

				RET

Rectangle		ENDP

;----------------------------------------------------------------------------
; Quad
;
;   Draws a quadrilateral outline by calling Line four times with
;   clipping enabled.
;
; Parameters:
;
;   x0   (WORD): X coordinate of the first vertex.
;
;   y0   (WORD): Y coordinate of the first vertex.
;
;   x1   (WORD): X coordinate of the second vertex.
;
;   y1   (WORD): Y coordinate of the second vertex.
;
;   x2   (WORD): X coordinate of the third vertex.
;
;   y2   (WORD): Y coordinate of the third vertex.
;
;   x3   (WORD): X coordinate of the fourth vertex.
;
;   y3   (WORD): Y coordinate of the fourth vertex.
;
;   col  (WORD): Line color (palette index, low byte used).
;
;   dest (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

Quad			PROC	C USES BX, x0:WORD, y0:WORD, x1:WORD, y1:WORD, x2:WORD, y2:WORD, x3:WORD, y3:WORD, col:WORD, dest:WORD

				; Line(x0, y0, x1, y1, col, 1, dest)

				PUSH	dest
				PUSH	1
				PUSH	col
				PUSH	y1
				PUSH	x1
				PUSH	y0
				PUSH	x0
				CALL	Line
				ADD		SP,	14

				; Line(x1, y1, x2, y2, col, 1, dest)

				PUSH	dest
				PUSH	1
				PUSH	col
				PUSH	y2
				PUSH	x2
				PUSH	y1
				PUSH	x1
				CALL	Line
				ADD		SP,	14

				; Line(x2, y2, x3, y3, col, 1, dest)

				PUSH	dest
				PUSH	1
				PUSH	col
				PUSH	y3
				PUSH	x3
				PUSH	y2
				PUSH	x2
				CALL	Line
				ADD		SP,	14

				; Line(x3, y3, x0, y0, col, 1, dest)

				PUSH	dest
				PUSH	1
				PUSH	col
				PUSH	y0
				PUSH	x0
				PUSH	y3
				PUSH	x3
				CALL	Line
				ADD		SP,	14

				RET

Quad			ENDP

;----------------------------------------------------------------------------
; FillRectangle
;
;   Draws a filled axis-aligned rectangle with clipping.
;
;   Sorts corners, clips against the clipping rectangle, then fills
;   scanlines using REP STOSB.
;
; Parameters:
;
;   x0   (WORD): X coordinate of the first corner.
;
;   y0   (WORD): Y coordinate of the first corner.
;
;   x1   (WORD): X coordinate of the opposite corner.
;
;   y1   (WORD): Y coordinate of the opposite corner.
;
;   col  (WORD): Fill color (palette index, low byte used).
;
;   dest (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

FillRectangle	PROC	C USES BX DI, x0:WORD, y0:WORD, x1:WORD, y1:WORD, col:WORD, dest:WORD
				LOCAL	left:WORD, right:WORD, top:WORD, bottom:WORD
				LOCAL	row:WORD, length:WORD

				; Sort x0, x1 into left, right.

				MOV		AX,	x0
				MOV		BX,	x1
				CMP		AX,	BX
				JBE		FRECT_X_SORTED
				XCHG	AX,	BX

FRECT_X_SORTED:
				MOV		left, AX
				MOV		right, BX

				; Sort y0, y1 into top, bottom.

				MOV		AX,	y0
				MOV		BX,	y1
				CMP		AX,	BX
				JBE		FRECT_Y_SORTED
				XCHG	AX,	BX

FRECT_Y_SORTED:
				MOV		top, AX
				MOV		bottom,	BX

				; Full rejection: rectangle entirely outside clip bounds.

				MOV		AX,	bottom
				CMP		AX,	clip_y1
				JL		FRECT_DONE
				MOV		AX,	top
				CMP		AX,	clip_y2
				JG		FRECT_DONE
				MOV		AX,	right
				CMP		AX,	clip_x1
				JL		FRECT_DONE
				MOV		AX,	left
				CMP		AX,	clip_x2
				JG		FRECT_DONE

				; Clamp top to clip_y1.

				MOV		AX,	top
				CMP		AX,	clip_y1
				JGE		FRECT_TOP_OK
				MOV		AX,	clip_y1
				MOV		top, AX

FRECT_TOP_OK:

				; Clamp bottom to clip_y2.

				MOV		AX,	bottom
				CMP		AX,	clip_y2
				JLE		FRECT_BOT_OK
				MOV		AX,	clip_y2
				MOV		bottom,	AX

FRECT_BOT_OK:

				; Clamp left to clip_x1.

				MOV		AX,	left
				CMP		AX,	clip_x1
				JGE		FRECT_LEFT_OK
				MOV		AX,	clip_x1
				MOV		left, AX

FRECT_LEFT_OK:

				; Clamp right to clip_x2.

				MOV		AX,	right
				CMP		AX,	clip_x2
				JLE		FRECT_RIGHT_OK
				MOV		AX,	clip_x2
				MOV		right, AX

FRECT_RIGHT_OK:

				; Compute length = right - left + 1.

				MOV		AX,	right
				SUB		AX,	left
				INC		AX
				MOV		length,	AX

				; Initialize row counter and destination segment.

				MOV		AX,	top
				MOV		row, AX

				PUSH	ES
				MOV		AX,	dest
				MOV		ES,	AX

FRECT_LOOP:
				MOV		AX,	row
				CMP		AX,	bottom
				JG		FRECT_LOOP_END

				; Compute offset = row * 320 + left.

				MOV		BX,	left
				MOV		CX,	row
				XCHG	CH,	CL
				ADD		BX,	CX
				SHR		CX,	2
				ADD		BX,	CX

				MOV		DI,	BX
				MOV		AL,	BYTE PTR col
				MOV		CX,	length
				REP		STOSB

				INC		WORD PTR row
				JMP		FRECT_LOOP

FRECT_LOOP_END:
				POP		ES

FRECT_DONE:
				RET

FillRectangle	ENDP

;----------------------------------------------------------------------------
; FillTriangle
;
;   Draws a filled triangle using scanline conversion with 16.16
;   fixed-point edge slopes.
;
;   Vertices are sorted top-to-bottom, then the triangle is split at
;   the middle vertex into a flat-bottom half and a flat-top half.
;
; Parameters:
;
;   x0   (WORD): X coordinate of the first vertex.
;
;   y0   (WORD): Y coordinate of the first vertex.
;
;   x1   (WORD): X coordinate of the second vertex.
;
;   y1   (WORD): Y coordinate of the second vertex.
;
;   x2   (WORD): X coordinate of the third vertex.
;
;   y2   (WORD): Y coordinate of the third vertex.
;
;   col  (WORD): Fill color (palette index, low byte used).
;
;   dest (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

FillTriangle	PROC	C USES BX DI, x0:WORD, y0:WORD, x1:WORD, y1:WORD, x2:WORD, y2:WORD, col:WORD, dest:WORD
				LOCAL	top_x:WORD, top_y:WORD
				LOCAL	mid_x:WORD, mid_y:WORD
				LOCAL	bot_x:WORD, bot_y:WORD
				LOCAL	scan_y:WORD
				LOCAL	left_x_low:WORD, left_x_high:WORD
				LOCAL	right_x_low:WORD, right_x_high:WORD
				LOCAL	left_slope_low:WORD, left_slope_high:WORD
				LOCAL	right_slope_low:WORD, right_slope_high:WORD

				; Sort vertices by Y coordinate (top to bottom)

				; Load vertices into locals.

				MOV		AX,	x0
				MOV		top_x,	AX
				MOV		AX,	y0
				MOV		top_y,	AX
				MOV		AX,	x1
				MOV		mid_x,	AX
				MOV		AX,	y1
				MOV		mid_y,	AX
				MOV		AX,	x2
				MOV		bot_x,	AX
				MOV		AX,	y2
				MOV		bot_y,	AX

				; Pass 1: if top_y > mid_y, swap top and mid.

				MOV		AX,	top_y
				CMP		AX,	mid_y
				JLE		FT_SORT1_OK
				MOV		AX,	top_x
				XCHG	AX,	mid_x
				MOV		top_x,	AX
				MOV		AX,	top_y
				XCHG	AX,	mid_y
				MOV		top_y,	AX

FT_SORT1_OK:

				; Pass 2: if top_y > bot_y, swap top and bot.

				MOV		AX,	top_y
				CMP		AX,	bot_y
				JLE		FT_SORT2_OK
				MOV		AX,	top_x
				XCHG	AX,	bot_x
				MOV		top_x,	AX
				MOV		AX,	top_y
				XCHG	AX,	bot_y
				MOV		top_y,	AX

FT_SORT2_OK:

				; Pass 3: if mid_y > bot_y, swap mid and bot.

				MOV		AX,	mid_y
				CMP		AX,	bot_y
				JLE		FT_SORT3_OK
				MOV		AX,	mid_x
				XCHG	AX,	bot_x
				MOV		mid_x,	AX
				MOV		AX,	mid_y
				XCHG	AX,	bot_y
				MOV		mid_y,	AX

FT_SORT3_OK:

				; Degenerate: all vertices on one scanline.

				MOV		AX,	top_y
				CMP		AX,	bot_y
				JNE		FT_NOT_DEGENERATE

				; Draw single horizontal fill for degenerate triangle.

				CLIPPED_HFILL	top_y, top_x, bot_x
				JMP		FT_DONE

FT_NOT_DEGENERATE:

				; ===================================================================
				; Flat-bottom half: top_y to mid_y - 1
				; ===================================================================

				MOV		AX,	mid_y
				CMP		AX,	top_y
				JE		FT_FLAT_BOTTOM_DONE

				; left_slope = ((mid_x - top_x) << 16) / (mid_y - top_y)

				MOV		DX,	mid_x
				SUB		DX,	top_x
				MOV		CX,	mid_y
				SUB		CX,	top_y
				FIXED_DIVIDE
				MOV		left_slope_low,	AX
				MOV		left_slope_high,	DX

				; right_slope = ((bot_x - top_x) << 16) / (bot_y - top_y)

				MOV		DX,	bot_x
				SUB		DX,	top_x
				MOV		CX,	bot_y
				SUB		CX,	top_y
				FIXED_DIVIDE
				MOV		right_slope_low,	AX
				MOV		right_slope_high,	DX

				; Initialize accumulators: left_x = right_x = top_x.

				MOV		AX,	top_x
				MOV		left_x_high,	AX
				MOV		right_x_high,	AX
				MOV		WORD PTR left_x_low,	0
				MOV		WORD PTR right_x_low,	0

				; scan_y = top_y

				MOV		AX,	top_y
				MOV		scan_y,	AX

				; Flat-bottom scan loop.

FTFB_LOOP:
				MOV		AX,	scan_y
				CMP		AX,	mid_y
				JGE		FT_FLAT_BOTTOM_DONE

				CLIPPED_HFILL	scan_y, left_x_high, right_x_high
				DDA_ADVANCE
				INC		WORD PTR scan_y
				JMP		FTFB_LOOP

FT_FLAT_BOTTOM_DONE:

				; ===================================================================
				; Flat-top half: mid_y to bot_y
				; ===================================================================

				MOV		AX,	bot_y
				CMP		AX,	mid_y
				JNE		FT_FLAT_TOP_COMPUTE

				; Degenerate flat-top: draw single horizontal line.

				CLIPPED_HFILL	mid_y, mid_x, bot_x
				JMP		FT_DONE

FT_FLAT_TOP_COMPUTE:

				; left_slope = ((bot_x - mid_x) << 16) / (bot_y - mid_y)

				MOV		DX,	bot_x
				SUB		DX,	mid_x
				MOV		CX,	bot_y
				SUB		CX,	mid_y
				FIXED_DIVIDE
				MOV		left_slope_low,	AX
				MOV		left_slope_high,	DX

				; right_slope = ((bot_x - top_x) << 16) / (bot_y - top_y)

				MOV		DX,	bot_x
				SUB		DX,	top_x
				MOV		CX,	bot_y
				SUB		CX,	top_y
				FIXED_DIVIDE
				MOV		right_slope_low,	AX
				MOV		right_slope_high,	DX

				; left_x = mid_x, right_x = top_x

				MOV		AX,	mid_x
				MOV		left_x_high,	AX
				MOV		WORD PTR left_x_low,	0

				MOV		AX,	top_x
				MOV		right_x_high,	AX
				MOV		WORD PTR right_x_low,	0

				; Advance right_x by right_slope * (mid_y - top_y).
				; Signed 32x16 multiply.

				MOV		CX,	mid_y
				SUB		CX,	top_y

				; Low part: right_slope_low * CX (unsigned 16x16 -> 32).

				MOV		AX,	right_slope_low
				MUL		CX
				MOV		BX,	DX			; BX = carry from low mul
				PUSH	AX			; save low result

				; High part: right_slope_high * CX (signed 16x16 -> 32).

				MOV		AX,	right_slope_high
				IMUL	CX
				ADD		AX,	BX			; add carry

				; Add product to right_x.

				POP		BX			; BX = low result
				ADD		right_x_low,	BX
				ADC		right_x_high,	AX

				; scan_y = mid_y

				MOV		AX,	mid_y
				MOV		scan_y,	AX

				; Flat-top scan loop.

FTFT_LOOP:
				MOV		AX,	scan_y
				CMP		AX,	bot_y
				JG		FT_DONE

				CLIPPED_HFILL	scan_y, left_x_high, right_x_high
				DDA_ADVANCE
				INC		WORD PTR scan_y
				JMP		FTFT_LOOP

FT_DONE:
				RET

FillTriangle	ENDP

;----------------------------------------------------------------------------
; FillQuad
;
;   Draws a filled quadrilateral by splitting into two triangles:
;   (0,1,2) and (0,2,3).
;
; Parameters:
;
;   x0   (WORD): X coordinate of the first vertex.
;
;   y0   (WORD): Y coordinate of the first vertex.
;
;   x1   (WORD): X coordinate of the second vertex.
;
;   y1   (WORD): Y coordinate of the second vertex.
;
;   x2   (WORD): X coordinate of the third vertex.
;
;   y2   (WORD): Y coordinate of the third vertex.
;
;   x3   (WORD): X coordinate of the fourth vertex.
;
;   y3   (WORD): Y coordinate of the fourth vertex.
;
;   col  (WORD): Fill color (palette index, low byte used).
;
;   dest (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

FillQuad		PROC	C USES BX DI, x0:WORD, y0:WORD, x1:WORD, y1:WORD, x2:WORD, y2:WORD, x3:WORD, y3:WORD, col:WORD, dest:WORD

				; FillTriangle(x0, y0, x1, y1, x2, y2, col, dest)

				PUSH	dest
				PUSH	col
				PUSH	y2
				PUSH	x2
				PUSH	y1
				PUSH	x1
				PUSH	y0
				PUSH	x0
				CALL	FillTriangle
				ADD		SP,	16

				; FillTriangle(x0, y0, x2, y2, x3, y3, col, dest)

				PUSH	dest
				PUSH	col
				PUSH	y3
				PUSH	x3
				PUSH	y2
				PUSH	x2
				PUSH	y0
				PUSH	x0
				CALL	FillTriangle
				ADD		SP,	16

				RET

FillQuad		ENDP

;----------------------------------------------------------------------------
; PutImage
;
;   Blits a rectangular sprite onto a screen buffer with transparency.
;
;   Pixels matching the mask color are skipped (not drawn).
;
; Parameters:
;
;   x          (WORD): X coordinate of the top-left destination corner.
;
;   y          (WORD): Y coordinate of the top-left destination corner.
;
;   xs         (WORD): Width of the sprite in pixels (columns per row).
;
;   imageSize  (WORD): Total number of pixels in the sprite (width x height).
;
;   mask       (WORD): Transparent color index (low byte). Matching pixels
;                      are skipped.
;
;   sourceSeg  (WORD): Segment address of the source sprite data.
;
;   sourceOffs (WORD): Offset of the source sprite data within sourceSeg.
;
;   dest       (WORD): Segment address of the destination buffer.
;
;----------------------------------------------------------------------------

PutImage		PROC	C USES BX, x:WORD, y:WORD, xs:WORD, imageSize:WORD, mask:WORD, sourceSeg:WORD, sourceOffs:WORD, dest:WORD

				PUSH	DS
				PUSH	ES
				PUSH	SI
				PUSH	DI

				MOV		AX,	dest
				MOV		BX,	sourceSeg
				MOV		CX,	sourceOffs
				MOV		ES,	AX
				MOV		DS,	BX
				MOV		SI,	CX

				; Compute destination offset = y * 320 + x.

				MOV		BX,	x
				MOV		CX,	y
				XCHG	CH,	CL
				ADD		BX,	CX
				SHR		CX,	2
				ADD		BX,	CX

				MOV		DI,	BX

				XOR		AX,	AX				; column counter
				XOR		BX,	BX				; pixel counter
				MOV		DL,	BYTE PTR mask

PIMG_NEXT_PIXEL:
				CMP		DS:[SI],	DL
				JNZ		PIMG_PLOT_PIXEL

				INC		SI
				INC		DI
				JMP		PIMG_UPDATE

PIMG_PLOT_PIXEL:
				MOVSB

PIMG_UPDATE:
				INC		BX
				CMP		BX,	imageSize
				JZ		PIMG_DONE

				INC		AX
				CMP		AX,	xs
				JNZ		PIMG_NEXT_PIXEL

				; Advance DI to next row.

				MOV		AX,	320
				SUB		AX,	xs
				ADD		DI,	AX

				XOR		AX,	AX
				JMP		PIMG_NEXT_PIXEL

PIMG_DONE:
				POP		DI
				POP		SI
				POP		ES
				POP		DS
				RET

PutImage		ENDP

;============================================================================

				END

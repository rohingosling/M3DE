//****************************************************************************
// Program: M3DE (Micro-3D Engine)
// Version: 1.0
// Date:    1991-12-22
// Author:  Rohin Gosling
//
// Description:
//
//   Test program to test the M3DE 3D graphics library.
//   Renders a rotating red icosphere.
//
// Compilation:
//
//   - Borland Turbo C++ 3.1
//   - GFX-13 compiled as C with inline assembly (no TASM required)
//
//****************************************************************************

#include <CONIO.H>
#include <DOS.H>
#include <MATH.H>
#include "GFX13.H"
#include "M3DE.H"

//----------------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------------

#define SCREEN_SEGMENT          0xA000		// VGA video memory segment
#define SCREEN_WIDTH            320
#define SCREEN_HEIGHT           200
#define SHADE_COUNT             32			// Number of shading levels per color
#define BACKGROUND_COLOR        2			// Background palette index

#define BASE_COLOR_GREYSCALE    1			// Palette indices  32 -  63
#define BASE_COLOR_RED          2			// Palette indices  64 -  95
#define BASE_COLOR_MAGENTA      3			// Palette indices  96 - 127
#define BASE_COLOR_BLUE         4			// Palette indices 128 - 159
#define BASE_COLOR_CYAN         5			// Palette indices 160 - 191
#define BASE_COLOR_GREEN        6			// Palette indices 192 - 223
#define BASE_COLOR_YELLOW       7			// Palette indices 224 - 255

#define SCAN_ESCAPE             0x01
#define SCAN_LEFT_SHIFT         0x2A
#define SCAN_RIGHT_SHIFT        0x36
#define SCAN_UP                 0x48
#define SCAN_DOWN               0x50
#define SCAN_LEFT               0x4B
#define SCAN_RIGHT              0x4D
#define SCAN_O                  0x18
#define SCAN_C                  0x2E
#define SCAN_PLUS               0x0D		// =/+ key on main keyboard
#define SCAN_MINUS              0x0C		// -/_ key on main keyboard

#define NUMBER_OBJECTS          8

#define REVOLUTIONS_PER_MINUTE  12
#define TRANSLATION_SPEED       1.0			// Units per second
#define PIT_FREQUENCY           1193182.0	// 8254 PIT clock frequency (Hz)

//----------------------------------------------------------------------------
// Key state struct
//----------------------------------------------------------------------------

typedef struct
{
	volatile int escape;
	volatile int shift;
	volatile int up;
	volatile int down;
	volatile int left;
	volatile int right;
	volatile int color_cycle;
	volatile int object_cycle;
	volatile int brightness_up;
	volatile int brightness_down;
} KEY_STATE;

//----------------------------------------------------------------------------
// Static variables (file-scoped, accessed by interrupt handler)
//----------------------------------------------------------------------------

static KEY_STATE key_state = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static void      interrupt far ( *old_keyboard_handler )( ... );

//----------------------------------------------------------------------------
// Function: KeyboardHandler
//
// Description:
//
//   Custom INT 9h keyboard handler. Reads the scan code from port 0x60,
//   updates key-down/key-up state flags, then chains to the original BIOS
//   handler.
//
// Arguments:
//
//   - Interrupt handler (no explicit arguments).
//
// Returns:
//
//   - None.
//
//----------------------------------------------------------------------------

void interrupt far KeyboardHandler ( ... )
{
	unsigned char scan_code = inportb ( 0x60 );
	int pressed = !( scan_code & 0x80 );
	unsigned char key = scan_code & 0x7F;

	switch ( key )
	{
		case SCAN_ESCAPE:      key_state.escape = pressed; break;
		case SCAN_LEFT_SHIFT:
		case SCAN_RIGHT_SHIFT: key_state.shift  = pressed; break;
		case SCAN_UP:          key_state.up     = pressed; break;
		case SCAN_DOWN:        key_state.down   = pressed; break;
		case SCAN_LEFT:        key_state.left            = pressed; break;
		case SCAN_RIGHT:       key_state.right           = pressed; break;
		case SCAN_O:           key_state.object_cycle    = pressed; break;
		case SCAN_C:           key_state.color_cycle     = pressed; break;
		case SCAN_PLUS:        key_state.brightness_up   = pressed; break;
		case SCAN_MINUS:       key_state.brightness_down = pressed; break;
	}

	( *old_keyboard_handler )();
}

//----------------------------------------------------------------------------
// Function: InstallKeyboardHandler
//
// Description:
//
//   Saves the current INT 9h vector and installs the custom handler.
//
// Returns:
//
//   - Pointer to the KEY_STATE struct updated by the interrupt handler.
//
//----------------------------------------------------------------------------

KEY_STATE *InstallKeyboardHandler ( void )
{
	// Save existing keyboard handler so we can restore it later. 

	old_keyboard_handler = getvect ( 0x09 );

	// Set the keyboard handler interrupt vector to point to our keyboard handler. 

	setvect ( 0x09, KeyboardHandler );

	// {{Add Comment}}

	return &key_state;
}

//----------------------------------------------------------------------------
// Function: RestoreKeyboardHandler
//
// Description:
//
//   Restores the original INT 9h vector.
//
//----------------------------------------------------------------------------

void RestoreKeyboardHandler ( void )
{
	// Restore original keyboard handler. 

	setvect ( 0x09, old_keyboard_handler );

	// Clear keyboard buffer before we exit, so that we don't send stray
	// keys to the terminal after exiting. 

	while ( kbhit () ) getch ();
}

//----------------------------------------------------------------------------
// Function: GetHighResolutionTicks
//
// Description:
//
//   Returns a high-resolution tick count by combining the BIOS tick counter
//   (18.2 Hz) with the 8254 PIT channel 0 counter for sub-tick precision.
//   The result is in units of the PIT clock (1,193,182 Hz).
//
// Returns:
//
//   - Tick count as unsigned long.
//
//----------------------------------------------------------------------------

unsigned long GetHighResolutionTicks ( void )
{
	// Disable interrupts for an atomic read of the BIOS tick count
	// and PIT counter.

	disable ();

	// Read BIOS tick count from 0040:006C

	unsigned long bios_ticks = *( unsigned long far * ) MK_FP ( 0x0040, 0x006C );

	// Latch PIT counter 0 and read its current countdown value

	outportb ( 0x43, 0x00 );
	unsigned char low  = inportb ( 0x40 );
	unsigned char high = inportb ( 0x40 );

	enable ();

	unsigned int pit_counter = ( ( unsigned int ) high << 8 ) | low;

	// PIT counts down from 65536, so invert to get elapsed sub-ticks.
	// Combined with bios_ticks shifted left by 16, this gives a
	// monotonically increasing tick count.

	return ( bios_ticks << 16 ) + ( 65536UL - ( unsigned long ) pit_counter );
}

//----------------------------------------------------------------------------
// Function: InitializePalette
//
// Description:
//
//   Sets up the VGA palette with 8 gradients of 32 shades each.
//
//   - Indices   0 -  31 : Background gradient (black to white)
//   - Indices  32 -  63 : Greyscale object color
//   - Indices  64 -  95 : Red object color
//   - Indices  96 - 127 : Magenta object color
//   - Indices 128 - 159 : Blue object color
//   - Indices 160 - 191 : Cyan object color
//   - Indices 192 - 223 : Green object color
//   - Indices 224 - 255 : Yellow object color
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

void InitializePalette ( void )
{
	// Each palette entry is 3 bytes (R, G, B), values 0-63 for VGA DAC

	unsigned char palette [ 256 * 3 ];

	int index = 0;

	// Ambient and specular lighting parameters for object color gradients

	int ambient_light = 10;		// Darkest shade (0-63)
	int specularity   = 50;	// Brightest shade (0-63)

	// Indices 0-31: Background gradient from black to white

	for ( int shade = 0; shade < SHADE_COUNT; shade++ )
	{
		int brightness = ( shade * 63 ) / ( SHADE_COUNT - 1 );

		palette [ index++ ] = brightness;
		palette [ index++ ] = brightness;
		palette [ index++ ] = brightness;
	}

	// Indices 32-255: 7 object color gradients x 32 shades each
	//
	// Each color is defined by which RGB channels are active (1) or
	// inactive (0). Active channels are interpolated from ambient_light
	// to specularity across the 32 shades.
	//
	//   Color     | R | G | B
	//   ----------+---+---+---
	//   Greyscale | 1 | 1 | 1
	//   Red       | 1 | 0 | 0
	//   Magenta   | 1 | 0 | 1
	//   Blue      | 0 | 0 | 1
	//   Cyan      | 0 | 1 | 1
	//   Green     | 0 | 1 | 0
	//   Yellow    | 1 | 1 | 0

	int red_channel   [ 7 ] = { 1, 1, 1, 0, 0, 0, 1 };
	int green_channel [ 7 ] = { 1, 0, 0, 0, 1, 1, 1 };
	int blue_channel  [ 7 ] = { 1, 0, 1, 1, 1, 0, 0 };

	for ( int color = 0; color < 7; color++ )
	{
		for ( shade = 0; shade < SHADE_COUNT; shade++ )
		{
			// Interpolate from ambient_light to specularity

			int brightness = ambient_light + ( shade * ( specularity - ambient_light ) ) / ( SHADE_COUNT - 1 );

			palette [ index++ ] = red_channel   [ color ] ? brightness : 0;
			palette [ index++ ] = green_channel [ color ] ? brightness : 0;
			palette [ index++ ] = blue_channel  [ color ] ? brightness : 0;
		}
	}

	// Upload palette to VGA DAC

	SetPalette ( 0, 256, FP_SEG ( palette ), FP_OFF ( palette ) );
}

//----------------------------------------------------------------------------
// Function: CreateWorld
//
// Description:
//
//   Initializes the WORLD structure with screen dimensions, color settings,
//   and scaling parameters.
//
// Arguments:
//
//   - world: Pointer to the WORLD structure to initialize.
//
// Returns:
//
//   - None.
//
//----------------------------------------------------------------------------

void CreateWorld ( WORLD *world )
{
	// Load default values. 

	int shade_count   = SHADE_COUNT;
	int screen_width  = SCREEN_WIDTH;
	int screen_height = SCREEN_HEIGHT;

	// Place the center of the world in the center of the screen. 

	CALC xd = screen_width / 2;
	CALC yd = screen_height / 2;

	// Initialise the 3D world. 

	InitWorld ( world );

	world->first_color  = 0;
	world->color_length = shade_count;
	world->xr           = screen_width;
	world->yr           = screen_height;
	world->xd           = xd;
	world->yd           = yd;
	world->scale        = 100.0;
	world->xa           = 1.0 * world->scale;
	world->ya           = 1.0 * world->scale;
}

//----------------------------------------------------------------------------
// Function: CreateObjects
//
// Description:
//
//   Creates one of each object type supported by the engine and adds them
//   to the world. Only the first object (icosphere) is initially visible.
//
// Arguments:
//
//   - world: Pointer to the WORLD structure to add objects to.
//
// Returns:
//
//   - None.
//
//----------------------------------------------------------------------------

void CreateObjects ( WORLD *world )
{
	// Shared generation parameters

	CALC box_height          = 1.5;
	CALC box_width           = 1.5;
	CALC box_length          = 1.5;
	CALC sphere_radius       = 1.0;		// Used for the icosphere, geosphere, and hemisphere.
	CALC cone_height         = 2.0;
	CALC cone_radius         = 1.0;
	CALC cylinder_height     = 2.0;
	CALC cylinder_radius     = 1.0;
	CALC tube_height         = 2.0;
	CALC tube_radius         = 1.0;
	CALC tube_thickness      = 0.25;
	CALC torus_inner_radius  = 0.33;
	CALC torus_outer_radius  = 1.0;
	int  box_grid            = 5;
	int  icosphere_frequency = 3;
	int  geosphere_sides     = 16;
	int  hemisphere_sides    = 16;
	int  cylinder_sides      = 16;
	int  cylinder_segments   = 8;
	int  cone_sides          = 16;
	int  cone_segments       = 8;
	int  tube_sides          = 16;
	int  tube_segments       = 8;
	int  torus_sides         = 12;
	int  torus_segments      = 24;

	// Shared object placement

	CALC object_x = 0.0;
	CALC object_y = 2.0;
	CALC object_z = 0.0;

	// Allocate all objects

	SetNumberObjects ( NUMBER_OBJECTS, world );

	int object_index = 0;

	//------------------------------------------------------------------------
	// Create Box (object 0)
	//------------------------------------------------------------------------

	OBJECT far *box = &( world->object [ object_index++ ] );

	GenerateBox           ( box_width, box_grid, box_height, box_grid, box_length, box_grid, box );
	SetObjectColor        ( BASE_COLOR_BLUE, box );
	SetObjectShading      ( TRUE,            box );
	SetObjectConstruction ( SOLID,           box );

	box->backface_culling_enabled = TRUE;
	box->visible                  = TRUE;
	box->xd                       = object_x;
	box->yd                       = object_y;
	box->zd                       = object_z;

	//------------------------------------------------------------------------
	// Create Icosphere (object 1)
	//------------------------------------------------------------------------

	OBJECT far *icosphere = &( world->object [ object_index++ ] );

	GenerateIcosphere     ( sphere_radius, icosphere_frequency, icosphere );
	SetObjectColor        ( BASE_COLOR_BLUE, icosphere );
	SetObjectShading      ( TRUE,            icosphere );
	SetObjectConstruction ( SOLID,           icosphere );

	icosphere->backface_culling_enabled = TRUE;
	icosphere->visible                  = FALSE;
	icosphere->xd                       = object_x;
	icosphere->yd                       = object_y;
	icosphere->zd                       = object_z;
	

	//------------------------------------------------------------------------
	// Create Geosphere (object 2)
	//------------------------------------------------------------------------

	OBJECT far *geosphere = &( world->object [ object_index++ ] );

	GenerateGeosphere     ( sphere_radius, geosphere_sides, geosphere );
	SetObjectColor        ( BASE_COLOR_BLUE, geosphere );
	SetObjectShading      ( TRUE,            geosphere );
	SetObjectConstruction ( SOLID,           geosphere );

	geosphere->backface_culling_enabled = TRUE;
	geosphere->visible                  = FALSE;
	geosphere->xd                       = object_x;
	geosphere->yd                       = object_y;
	geosphere->zd                       = object_z;
	

	//------------------------------------------------------------------------
	// Create Hemisphere (object 3)
	//------------------------------------------------------------------------

	OBJECT far *hemisphere = &( world->object [ object_index++ ] );

	GenerateHemisphere    ( sphere_radius, hemisphere_sides, hemisphere );
	SetObjectColor        ( BASE_COLOR_BLUE, hemisphere );
	SetObjectShading      ( TRUE,            hemisphere );
	SetObjectConstruction ( SOLID,           hemisphere );

	hemisphere->backface_culling_enabled = TRUE;
	hemisphere->visible                  = FALSE;
	hemisphere->xd                       = object_x;
	hemisphere->yd                       = object_y;
	hemisphere->zd                       = object_z;
	

	//------------------------------------------------------------------------
	// Create Cylinder (object 4)
	//------------------------------------------------------------------------

	OBJECT far *cylinder = &( world->object [ object_index++ ] );

	GenerateCylinder      ( cylinder_height, cylinder_radius, cylinder_sides, cylinder_segments, cylinder );
	SetObjectColor        ( BASE_COLOR_BLUE, cylinder );
	SetObjectShading      ( TRUE,            cylinder );
	SetObjectConstruction ( SOLID,           cylinder );

	cylinder->backface_culling_enabled = TRUE;
	cylinder->visible                  = FALSE;
	cylinder->xd                       = object_x;
	cylinder->yd                       = object_y;
	cylinder->zd                       = object_z;
	

	//------------------------------------------------------------------------
	// Create Cone (object 5)
	//------------------------------------------------------------------------

	OBJECT far *cone = &( world->object [ object_index++ ] );

	GenerateCone          ( cone_height, cone_radius, cone_sides, cone_segments, cone );
	SetObjectColor        ( BASE_COLOR_BLUE, cone );
	SetObjectShading      ( TRUE,            cone );
	SetObjectConstruction ( SOLID,           cone );

	cone->backface_culling_enabled = TRUE;
	cone->visible                  = FALSE;
	cone->xd                       = object_x;
	cone->yd                       = object_y;
	cone->zd                       = object_z;
	

	//------------------------------------------------------------------------
	// Create Tube (object 6)
	//------------------------------------------------------------------------

	OBJECT far *tube = &( world->object [ object_index++ ] );

	GenerateTube          ( tube_height, tube_radius - tube_thickness, tube_radius, tube_sides, tube_segments, tube );
	SetObjectColor        ( BASE_COLOR_BLUE, tube );
	SetObjectShading      ( TRUE,            tube );
	SetObjectConstruction ( SOLID,           tube );

	tube->backface_culling_enabled = TRUE;
	tube->visible                  = FALSE;
	tube->xd                       = object_x;
	tube->yd                       = object_y;
	tube->zd                       = object_z;
	

	//------------------------------------------------------------------------
	// Create Torus (object 7)
	//------------------------------------------------------------------------

	OBJECT far *torus = &( world->object [ object_index++ ] );

	GenerateTorus         ( torus_inner_radius, torus_outer_radius, torus_segments, torus_sides, torus );
	SetObjectColor        ( BASE_COLOR_BLUE, torus );
	SetObjectShading      ( TRUE,            torus );
	SetObjectConstruction ( SOLID,           torus );

	torus->backface_culling_enabled = TRUE;
	torus->visible                  = FALSE;
	torus->xd                       = object_x;
	torus->yd                       = object_y;
	torus->zd                       = object_z;	
}

//----------------------------------------------------------------------------
// Function: CreateLights
//
// Description:
//
//   Creates a single omnidirectional light source positioned in front of
//   the icosphere, above and to the left.
//
// Arguments:
//
//   - world: Pointer to the WORLD structure to add lights to.
//
// Returns:
//
//   - None.
//
//----------------------------------------------------------------------------

void CreateLights ( WORLD *world )
{
	SetNumberLights ( 1, world );
	LIGHT far *light = &( world->light [ 0 ] );
	InitLight ( light );

	light->source.x   = -4.0;
	light->source.y   = -2.0;
	light->source.z   =  4.0;
	light->light_type =  OMNI;
	light->enabled    =  TRUE;
}

//----------------------------------------------------------------------------
// Function: CreateCameras
//
// Description:
//
//   Creates a single camera with a focal length of 10 and sets it as
//   the active camera.
//
// Arguments:
//
//   - world: Pointer to the WORLD structure to add cameras to.
//
// Returns:
//
//   - None.
//
//----------------------------------------------------------------------------

void CreateCameras ( WORLD *world )
{
	SetNumberCameras ( 1, world );
	CAMERA far *camera = &( world->camera [ 0 ] );
	InitCamera ( camera );

	world->active_camera = 0;
	camera->f            = 2.0;
}

//----------------------------------------------------------------------------
// Function: main
//
// Description:
//
//   Entry point. Initializes graphics, creates an icosphere, and renders
//   it rotating around the z-axis until Escape is pressed.
//
// Arguments:
//
//   - None
//
// Returns:
//
//   - 0 on success.
//
//----------------------------------------------------------------------------

int main ( void )
{
	// Initialize graphics mode

	SetMode13         ();
	SetClipping       ( 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 );
	InitializePalette ();

	// Initialize world

	WORLD world;

	CreateWorld   ( &world );
	CreateObjects ( &world );
	CreateLights  ( &world );
	CreateCameras ( &world );

	InitDepthBuffer ( &world );

	// Active object pointer for per-frame updates

	int active_object = 0;
	OBJECT far *object = &( world.object [ active_object ] );

	// Allocate off-screen back buffer for double buffering.
	//
	// - Rendering to system memory and copying to VGA during vertical retrace
	//   eliminates tearing caused by the scan line racing the renderer.
	//
	// - "allocmem" takes size in 16-byte paragraphs: 64000 / 16 = 4000.

	WORD back_buffer_segment;
	allocmem ( 4000, &back_buffer_segment );

	// Rotation rates in radians per second
	//
	// Z-axis (yaw):   12 RPM = 0.2 rev/s = 1.257 rad/s
	// X-axis (pitch):  4 RPM             = 0.419 rad/s

	CALC yaw_rate   = ( REVOLUTIONS_PER_MINUTE * 2.0 * M_PI ) / 60.0;
	CALC pitch_rate = ( 4.0 * 2.0 * M_PI ) / 60.0;

	// Install custom keyboard handler

	KEY_STATE *keys = InstallKeyboardHandler ();

	// Initialize high-resolution timer for frame delta calculation

	unsigned long previous_ticks = GetHighResolutionTicks ();

	// Runtime state for color cycling and background brightness

	int object_color     = BASE_COLOR_BLUE;
	int background_color = BACKGROUND_COLOR;

	int previous_object_cycle    = 0;
	int previous_color_cycle     = 0;
	int previous_brightness_up   = 0;
	int previous_brightness_down = 0;

	// Main rendering loop

	while ( !keys->escape )
	{
		// Calculate frame delta time

		unsigned long current_ticks = GetHighResolutionTicks ();
		CALC delta_time = ( CALC )( current_ticks - previous_ticks ) / PIT_FREQUENCY;
		previous_ticks  = current_ticks;

		// Guard against abnormal delta (timer wrap, stall, etc.)

		if ( delta_time <= 0.0 || delta_time > 0.5 ) delta_time = 1.0 / 70.0;

		// Clear back buffer

		ClearScreen ( background_color, back_buffer_segment );

		// Translate and render world to back buffer

		TranslateWorld ( &world );
		DisplayWorld   ( world, back_buffer_segment );

		// Copy back buffer to VGA during vertical retrace

		WaitRetrace ();
		FlipScreen  ( back_buffer_segment, SCREEN_SEGMENT );

		// Rotate active object around z-axis and x-axis

		object->yaw   += yaw_rate   * delta_time;
		object->pitch += pitch_rate * delta_time;

		// Translate active object along world axes
		//
		// Left/Right arrows : x-axis
		// Up/Down arrows    : z-axis (up/down in world)
		// Shift+Up/Down     : y-axis (depth toward/away from camera)

		CALC translation_step = TRANSLATION_SPEED * delta_time;

		if ( keys->left  ) object->xd -= translation_step;
		if ( keys->right ) object->xd += translation_step;

		if ( keys->shift )
		{
			if ( keys->up   ) object->yd += translation_step;
			if ( keys->down ) object->yd -= translation_step;
		}
		else
		{
			if ( keys->up   ) object->zd += translation_step;
			if ( keys->down ) object->zd -= translation_step;
		}

		// Cycle active object (O / Shift+O — edge triggered)
		//
		// O       : cycle forward through object list
		// Shift+O : cycle backward through object list
		//
		// Copies rotation, position, and color from the current object
		// to the next so the transition is seamless.

		if ( keys->object_cycle && !previous_object_cycle )
		{
			OBJECT far *previous_object = object;

			object->visible = FALSE;

			if ( keys->shift )
			{
				active_object--;
				if ( active_object < 0 ) active_object = NUMBER_OBJECTS - 1;
			}
			else
			{
				active_object++;
				if ( active_object >= NUMBER_OBJECTS ) active_object = 0;
			}

			object = &( world.object [ active_object ] );

			object->pitch = previous_object->pitch;
			object->yaw   = previous_object->yaw;
			object->roll  = previous_object->roll;
			object->xd    = previous_object->xd;
			object->yd    = previous_object->yd;
			object->zd    = previous_object->zd;

			SetObjectColor ( object_color, object );

			object->visible = TRUE;
		}
		previous_object_cycle = keys->object_cycle;

		// Cycle object color (C key — edge triggered)

		if ( keys->color_cycle && !previous_color_cycle )
		{
			object_color++;
			if ( object_color > BASE_COLOR_YELLOW ) object_color = BASE_COLOR_GREYSCALE;
			SetObjectColor ( object_color, object );
		}
		previous_color_cycle = keys->color_cycle;

		// Adjust background brightness (+/- keys — edge triggered)

		if ( keys->brightness_up && !previous_brightness_up )
		{
			if ( background_color < 31 ) background_color++;
		}
		previous_brightness_up = keys->brightness_up;

		if ( keys->brightness_down && !previous_brightness_down )
		{
			if ( background_color > 0 ) background_color--;
		}
		previous_brightness_down = keys->brightness_down;
	}

	// Restore original keyboard handler

	RestoreKeyboardHandler ();

	// Clean up

	freemem      ( back_buffer_segment );
	DestroyWorld ( &world );

	// Restore text mode

	SetTextMode ( TEXT_MODE_25_ROWS );

	return 0;
}

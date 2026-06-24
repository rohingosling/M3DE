//****************************************************************************
// Program: M3DE (Micro-3D Engine)
// Version: 1.0
// Date:    1991-12-22
// Author:  Rohin Gosling
//
// Description:
//
//   3D graphics demo engine. Provides 3D object generation, transformation, 
//   lighting, and rendering capabilities.
//
//   Uses GFX-13 2D graphics library for 2D primitives. 
//
// Compilation:
//
//   - M3DE:                 Borland Turbo C++ 3.1
//   - GFX-13 (version 2.0): compiled as C with inline assembly
//                           (no TASM required)
//
//****************************************************************************


#ifndef _3D_Engine
#define _3D_Engine

//****************************************************************************
// Typedefs
//****************************************************************************

typedef float CALC;  // Calculation type


//****************************************************************************
// Enums
//****************************************************************************

enum BOOLEAN      { FALSE        = 0, TRUE    = 1 };
enum LIGHT_TYPE   { SPOT         = 0, OMNI    = 1 };
enum CONSTRUCTION { WIRE_FRAME   = 0, SOLID   = 1 };


//****************************************************************************
// Structs
//****************************************************************************

//----------------------------------------------------------------------------
// 2D polygon vertex
//----------------------------------------------------------------------------

struct VERTEX
{
	int x, y;
};

//----------------------------------------------------------------------------
// Polygon
//----------------------------------------------------------------------------

struct POLYGON
{
	int          number_vertices;  	// Number of vertices defining polygon
	VERTEX       v [ 4 ];           // Polygon vertices
	char         color;             // Polygon color
	CONSTRUCTION construction;      // Polygon construction
	CALC         key;               // Sorting key (3D depth coordinate)
};

//----------------------------------------------------------------------------
// 3D Vector
//----------------------------------------------------------------------------

struct VECTOR
{
	CALC x, y, z;
};

//----------------------------------------------------------------------------
// Light
//----------------------------------------------------------------------------

struct LIGHT
{
	VECTOR     source;				// light source vector
	VECTOR     target;				// light target vector
	LIGHT_TYPE light_type;			// Type of light (omnidirectional or spot light)
	BOOLEAN    enabled;				// light switch (TRUE or FALSE)
};

//----------------------------------------------------------------------------
// Camera
//----------------------------------------------------------------------------

struct CAMERA
{
	VECTOR source;              	// camera source vector
	CALC   f;                   	// Focal length of camera
	CALC   near_plane;          	// Near clipping plane distance
	CALC   xd,    yd,   zd;   		// Lateral camera displacement
	CALC   xr,    yr,   zr;			// Rotational camera displacement
	CALC   pitch, yaw, roll;		// camera rotation (pitch=X, yaw=Z, roll=Y)
};

//----------------------------------------------------------------------------
// Face
//----------------------------------------------------------------------------

struct FACE
{
	int			 number_vertices;	// Number of vertices defining face
	VECTOR       far *v;			// Model face coordinates
	VECTOR       c;					// center of face
	char         color;  			// 3D face color
	BOOLEAN      visible;			// visibility flag
	CONSTRUCTION construction;		// Wire-frame or solid
	BOOLEAN      shading_enabled;	// TRUE = reflective, FALSE = self illuminating
	BOOLEAN      flip_normal;			// TRUE = flip normal, FALSE = leave alone
};

//----------------------------------------------------------------------------
// Object
//----------------------------------------------------------------------------

struct OBJECT
{
	int        number_faces;              // Number of faces defining object
	FACE       far *face;                 // array of faces
	BOOLEAN    visible;                   // visibility
	BOOLEAN    backface_culling_enabled;  // TRUE = cull backfaces, FALSE = render both sides
	BOOLEAN    hidden_surfaces_removed;   // TRUE = remove hidden surfaces, FALSE = show them
	CALC       xc,    yc,   zc;           // center of rotation
	CALC       xs,    ys,   zs;           // scale
	CALC       pitch, yaw, roll;	      // Rotation (pitch=X, yaw=Z, roll=Y)
	CALC       xd,    yd,   zd;           // Displacement
};

//----------------------------------------------------------------------------
// World
//----------------------------------------------------------------------------

struct WORLD
{
	// Construction

	int         number_objects;		// Number of objects in world
	int         number_lights;		// Number of lights in world
	int         number_cameras;		// Number of cameras in world
	int         active_camera;		// Active camera
	OBJECT far *object;				// Array of objects
	LIGHT  far *light;				// Array of lights
	CAMERA far *camera;				// Array of cameras

	// Depth buffer

	int          max_polygons;		// Maximum number of faces in depth buffer
	int          number_polygons;	// Number of faces in depth buffer
	POLYGON far *depth_buffer;  	// Depth buffer (array of polygons)

	// Colors and palette

	int first_color;				// Offset of first color in palette
	int color_length;				// Number of palette colors used for shading one color

	// Screen

	CALC xa, ya;					// 2D screen aspect
	CALC scale;						// 2D screen scale
	CALC xd, yd;					// 2D screen displacement
	CALC xr, yr;					// 2D screen resolution

	// Camera translation

	BOOLEAN camera_translation_enabled;		// TRUE = camera translation enabled
};

//****************************************************************************
// Functions
//****************************************************************************

//----------------------------------------------------------------------------
// Mathematical functions
//----------------------------------------------------------------------------

VECTOR GetVector         ( VECTOR a, VECTOR b );
CALC   GetMagnitude      ( VECTOR v );
CALC   GetDistance       ( VECTOR a, VECTOR b );
VECTOR GetNormal         ( VECTOR a, VECTOR b, VECTOR c );
CALC   GetVectorAngle	 ( VECTOR u, VECTOR v );
VECTOR GetCenter         ( VECTOR *v, int n );
CALC   GetVisibility     ( VECTOR n1, VECTOR p, WORLD world );
 
VECTOR ObjectTranslation ( VECTOR c, OBJECT object );
VECTOR CameraTranslation ( VECTOR c, WORLD world );
VERTEX ScreenTranslation ( VECTOR c, WORLD world );
CALC   LightTranslation  ( VECTOR n1, VECTOR n2, VECTOR c, OBJECT object, WORLD world );

//----------------------------------------------------------------------------
// General functions
//----------------------------------------------------------------------------

void InitWorld         ( WORLD  far *world );
void InitObject        ( OBJECT far *object );
void InitFace          ( FACE   far *face );
void InitLight         ( LIGHT  far *light );
void InitCamera        ( CAMERA far *camera );
void InitDepthBuffer   ( WORLD  far *world );

void DestroyFace       ( FACE   far *face );
void DestroyObject     ( OBJECT far *object );
void DestroyWorld      ( WORLD  far *world );

void SetNumberObjects  ( int n, WORLD  far *world );
void SetNumberFaces    ( int n, OBJECT far *object );
void SetNumberVertices ( int n, FACE   far *face );
void SetNumberLights   ( int n, WORLD  far *world );
void SetNumberCameras  ( int n, WORLD  far *world );

int  DepthSort         ( const void *, const void * );
void TranslateWorld    ( WORLD far *world );
void DisplayWorld      ( WORLD world, unsigned int screen );

//----------------------------------------------------------------------------
// Object manipulation functions
//----------------------------------------------------------------------------

void SetObjectColor        ( int          color,           OBJECT *object );
void SetObjectConstruction ( CONSTRUCTION construction,    OBJECT *object );
void SetObjectShading      ( BOOLEAN      shading_enabled, OBJECT *object );
void FlipNormals           ( OBJECT      *object );

//----------------------------------------------------------------------------
// Object generating functions
//----------------------------------------------------------------------------

int GenerateBox
(
	CALC        width,  
	CALC        width_grid,
	CALC        height,  
	CALC        height_grid,
	CALC        length, 
	CALC        length_grid,
	OBJECT far *object
);

int GenerateCone
(
	CALC        height,
	CALC        radius,
	int         sides,
	int         segments,
	OBJECT far *object
);

int GenerateCylinder
(
	CALC        height,
	CALC        radius,
	int         sides,
	int         segments,
	OBJECT far *object
);

int GenerateIcosphere
(
	CALC radius,
	int  frequency,
	OBJECT far *object
);

int GenerateGeosphere
(
	CALC        radius,
	int         equatorial_sides,
	OBJECT far *object
);

int GenerateHemisphere
(
	CALC        radius,
	int         equatorial_sides,
	OBJECT far *object
);

int GenerateTorus
(
	CALC        inner_radius,
	CALC        outer_radius,
	int         segments,
	int         circle_sides,
	OBJECT far *object
);

int GenerateTube
(
	CALC        height,
	CALC        inner_radius,
	CALC        outer_radius,
	int         sides,
	int         segments,
	OBJECT far *object
);

int GenerateSquareLamina 
(
	VECTOR      a,
	VECTOR      b,
	VECTOR      c,
	VECTOR      d,
	int         grid1,
	int         grid2,
	OBJECT far *object
);

int GenerateTriangularLamina 
(
	VECTOR      a,
	VECTOR      b,
	VECTOR      c,
	OBJECT far *object
);

int GeneratePolygonalLamina 
(
	CALC        radius,
	int         segments,
	OBJECT far *object
);

int GenerateLine
(
	VECTOR      a,
	VECTOR      b,
	int         segments,
	OBJECT far *object
);

int GeneratePoint
(
	VECTOR      a,
	OBJECT far *object
);

//----------------------------------------------------------------------------

#endif

//----------------------------------------------------------------------------

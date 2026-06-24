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
//   - Borland Turbo C++ 3.1
//   - GFX-13 compiled as C with inline assembly (no TASM required)
//
//****************************************************************************

#include <ALLOC.H>
#include <CONIO.H>
#include <PROCESS.H>
#include <SEARCH.H>
#include <MATH.H>
#include <DOS.H>
#include "GFX13.H"
#include "M3DE.H"

//****************************************************************************
// General Functions
//****************************************************************************

//----------------------------------------------------------------------------
// Function: InitWorld
//
// Description:
//
//   Initialize a world with default values.
//
// Arguments:
//
//   - world : Pointer to the WORLD structure to initialize.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void InitWorld ( WORLD  far *world )
{

	// Construction

	world->number_objects = 0;
	world->number_lights  = 0;
	world->number_cameras = 0;
	world->active_camera  = 0;

	world->object = NULL;
	world->light  = NULL;
	world->camera = NULL;

	// Depth buffer

	world->max_polygons    = 0;
	world->number_polygons = 0;
	world->depth_buffer    = NULL;

	// Palette and color (8 colors or 32 shades, by default)

	world->first_color  = 0;
	world->color_length = 32;

	// 2D screen data

	world->scale = 1.0;
	world->xa    = 1.1 * world->scale;
	world->ya    = 1.0 * world->scale;
	world->xr    = 320;
	world->yr    = 200;
	world->xd    = ( world->xr ) / 2.0;
	world->yd    = ( world->yr ) / 2.0;

	// Camera translation

	world->camera_translation_enabled = FALSE;	// Disabled by default
}

//----------------------------------------------------------------------------
// Function: InitObject
//
// Description:
//
//   Initialize an object with default values.
//
// Arguments:
//
//   - object : Pointer to the OBJECT structure to initialize.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void InitObject ( OBJECT far *object)
{
	// Construction

	object->number_faces    = 0;
	object->face            = NULL;
	object->hidden_surfaces_removed = TRUE;
	object->visible         = TRUE;
	object->backface_culling_enabled = TRUE;

	// Translation

	object->xc = 0.0;
	object->yc = 0.0;
	object->zc = 0.0;

	object->xs = 1.0;
	object->ys = 1.0;
	object->zs = 1.0;

	object->xd = 0.0;
	object->yd = 0.0;
	object->zd = 0.0;

	object->pitch = 0.0;
	object->yaw   = 0.0;
	object->roll  = 0.0;
}

//----------------------------------------------------------------------------
// Function: InitFace
//
// Description:
//
//   Initialize a face with default values.
//
// Arguments:
//
//   - face : Pointer to the FACE structure to initialize.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void InitFace ( FACE far *face )
{
	VECTOR c = { 0,0,0 };

	face->number_vertices = 0;
	face->v               = NULL;
	face->c               = c;
	face->color           = 0;
	face->visible         = TRUE;
	face->construction    = SOLID;
	face->shading_enabled = TRUE;
	face->flip_normal     = FALSE;
}

//----------------------------------------------------------------------------
// Function: InitLight
//
// Description:
//
//   Initialize a light with default values.
//
// Arguments:
//
//   - light : Pointer to the LIGHT structure to initialize.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void InitLight ( LIGHT far *light )
{
	// Default light source

	light->source.x = 0.0;
	light->source.y = 100.0;
	light->source.z = 10.0;

	// Default targit

	light->target.x = 0.0;
	light->target.y = 0.0;
	light->target.z = 0.0;

	// Default light type and status

	light->light_type = OMNI;
	light->enabled    = TRUE;
}

//----------------------------------------------------------------------------
// Function: InitCamera
//
// Description:
//
//   Initialize a camera with default values.
//
// Arguments:
//
//   - camera : Pointer to the CAMERA structure to initialize.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void InitCamera ( CAMERA far *camera )
{
	// Default camera view point and focal length

	camera->source.x = 0.0;
	camera->source.y = 0.0;
	camera->source.z = 0.0;

	camera->f          = 30.0;
	camera->near_plane = 1.0;

	// Default translation

	camera->xd = 0.0;
	camera->yd = 0.0;
	camera->zd = 0.0;

	camera->xr = 0.0;
	camera->yr = 0.0;
	camera->zr = 0.0;

	camera->pitch = 0.0;
	camera->yaw   = 0.0;
	camera->roll  = 0.0;
}

//----------------------------------------------------------------------------
// Function: InitDepthBuffer
//
// Description:
//
//   Initialize the depth buffer and allocate appropriate memory for it.
//
// Arguments:
//
//   - world : Pointer to the WORLD structure containing the depth buffer.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void InitDepthBuffer ( WORLD far *world )
{
	// Get maximum number of faces (some faces might be invisible)

	for ( int oc = 0, fc = 0; oc < world->number_objects; oc++ )
	{
		fc += world->object [ oc ].number_faces;
	}

	// Store total face count as both max and current polygon count

	world->max_polygons = world->number_polygons = fc;

	// Allocate memory

	if ( world->depth_buffer == NULL )
	{
		// First allocation of depth buffer

		world->depth_buffer = ( POLYGON far * ) farmalloc	( fc*sizeof ( POLYGON ) );
	}
	else
	{
		// Reallocate depth buffer to new size

		world->depth_buffer = ( POLYGON far * ) farrealloc
		(
			world->depth_buffer, 
			fc*sizeof ( POLYGON )
		);
	}
}

//----------------------------------------------------------------------------
// Function: DestroyFace
//
// Description:
//
//   Free memory used by a face.
//
// Arguments:
//
//   - face : Pointer to the FACE structure to destroy.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void DestroyFace ( FACE far *face )
{
	// Free the vertex array

	farfree ( face->v );
}

//----------------------------------------------------------------------------
// Function: DestroyObject
//
// Description:
//
//   Free memory used by an object.
//
// Arguments:
//
//   - object : Pointer to the OBJECT structure to destroy.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void DestroyObject ( OBJECT far *object )
{
	// Destroy faces

	for ( int fc = 0; fc < object->number_faces; fc++ )
    {
        DestroyFace ( &( object->face [ fc ] ) );
    }

	// free face pointer

	farfree ( object->face );
}

//----------------------------------------------------------------------------
// Function: DestroyWorld
//
// Description:
//
//   Free memory used by a world.
//
// Arguments:
//
//   - world : Pointer to the WORLD structure to destroy.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void DestroyWorld ( WORLD far *world )
{
	// Destroy objects

	for ( int oc = 0; oc < world->number_objects; oc++ )
    {
		DestroyObject ( &( world->object [ oc ] ) );
    }

	// free object pointer

	farfree ( world->object );

	// free light pointer

	farfree ( world->light );

	// free camera pointer

	farfree ( world->camera );

	// free depth buffer

	farfree ( world->depth_buffer );
}

//----------------------------------------------------------------------------
// Function: SetNumberObjects
//
// Description:
//
//   Allocate memory for n objects.
//
// Arguments:
//
//   - n     : Number of objects to allocate.
//   - world : Pointer to the WORLD structure.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetNumberObjects ( int n, WORLD far *world )
{
	// Allocate or reallocate the object array

	if ( world->object == NULL )
    {
		// First allocation

		world->object = ( OBJECT far * ) farmalloc ( n*sizeof ( OBJECT ) );
    }
	else
    {
		// Reallocate to new size

		world->object = ( OBJECT far * ) farrealloc
        (
            world->object, 
            n*sizeof ( OBJECT )
        );
    }

	// Update the object count

	world->number_objects = n;
}

//----------------------------------------------------------------------------
// Function: SetNumberFaces
//
// Description:
//
//   Allocate memory for n faces.
//
// Arguments:
//
//   - n      : Number of faces to allocate.
//   - object : Pointer to the OBJECT structure.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetNumberFaces ( int n, OBJECT far *object )
{
	// Allocate or reallocate the face array

	if (object->face==NULL)
    {
		// First allocation

		object->face = ( FACE far * ) farmalloc ( n*sizeof ( FACE ) );
    }
	else
    {
		// Reallocate to new size

		object->face = ( FACE far * ) farrealloc
        (
            object->face, 
            n*sizeof ( FACE )
        );
    }

	// Update the face count

	object->number_faces = n;
}

//----------------------------------------------------------------------------
// Function: SetNumberVertices
//
// Description:
//
//   Allocate memory for n vertices.
//
// Arguments:
//
//   - n    : Number of vertices to allocate.
//   - face : Pointer to the FACE structure.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetNumberVertices ( int n, FACE far *face )
{
	// Validate vertex count (must be 1 to 4)

	if ( ( n<=0 ) || ( n>4 ) )
    {
		textmode ( C80 );
		cprintf  ( "\n\rERROR : SetNumberVertices : Incorrect number of vertices...\n\r" );
		exit ( 1 );
	}

	// Allocate or reallocate the vertex array

	if ( face->v == NULL )
    {
		// First allocation

		face->v = ( VECTOR far * ) farmalloc ( n*sizeof ( VECTOR ) );
    }
	else
    {
		// Reallocate to new size

		face->v = ( VECTOR far * ) farrealloc ( face->v, n*sizeof ( VECTOR ) );
    }

	// Update the vertex count

	face->number_vertices = n;
}

//----------------------------------------------------------------------------
// Function: SetNumberLights
//
// Description:
//
//   Allocates memory for n lights.
//
// Arguments:
//
//   - n     : Number of lights to allocate.
//   - world : Pointer to the WORLD structure.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetNumberLights ( int n, WORLD far *world )
{
	// Allocate or reallocate the light array

	if ( world->light == NULL )
    {
		// First allocation

		world->light = ( LIGHT far * ) farmalloc ( n*sizeof ( LIGHT ) );
    }
	else
    {
		// Reallocate to new size

		world->light = ( LIGHT far * ) farrealloc ( world->light, n*sizeof ( LIGHT ) );
    }

	// Update the light count

	world->number_lights = n;
}

//----------------------------------------------------------------------------
// Function: SetNumberCameras
//
// Description:
//
//   Allocate memory for n cameras.
//
// Arguments:
//
//   - n     : Number of cameras to allocate.
//   - world : Pointer to the WORLD structure.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetNumberCameras ( int n, WORLD far *world )
{
	// Allocate or reallocate the camera array

	if ( world->camera == NULL )
    {
		// First allocation

		world->camera = ( CAMERA far * ) farmalloc ( n*sizeof ( CAMERA ) );
    }
	else
    {
		// Reallocate to new size

		world->camera = ( CAMERA far * ) farrealloc
        (
            world->camera, 
            n*sizeof ( CAMERA )
        );
    }

	// Update the camera count

	world->number_cameras = n;
}

//----------------------------------------------------------------------------
// Function: DepthSort
//
// Description:
//
//   Quick sort swap routine used by the library quick sort function.
//
// Arguments:
//
//   - a : Pointer to first polygon for comparison.
//   - b : Pointer to second polygon for comparison.
//
// Returns:
//
//   - Sort comparison result (-1, 0, or 1).
//
//----------------------------------------------------------------------------

int DepthSort ( const void *a, const void *b )
{
	// Cast void pointers to polygon pointers for comparison

	POLYGON far *polygon_a = ( POLYGON far * ) a;
	POLYGON far *polygon_b = ( POLYGON far * ) b;

	// Compare depth keys for back-to-front ordering

	if ( polygon_a->key >  polygon_b->key ) return ( -1 );
	if ( polygon_a->key == polygon_b->key ) return (  0 );
	if ( polygon_a->key <  polygon_b->key ) return (  1 );
}

//----------------------------------------------------------------------------
// Function: TranslateWorld
//
// Description:
//
//   Perform all world translations and initialize depth buffer.
//
// Arguments:
//
//   - world : Pointer to the WORLD structure to translate.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void TranslateWorld ( WORLD far *world )
{
	// Initialise local variables. 

	VECTOR  t [ 4 ];           				// 3D Translation coordinates
	VERTEX  v [ 4 ];           				// 2D Translated coordinates

	VECTOR n1 = { 0, 0, 0 };    			// First normal
	VECTOR n2 = { 0, 0, 0 };    			// Second normal (Used for double sided faces)
	VECTOR c  = { 0, 0, 0 };				// center point of face

	CALC visibility = -1.0;					// Used for setting the visibility of hidden surfaces

	int oc = 0;               				// object counter
	int fc = 0;               				// face counter
	int vc = 0;               				// Vertex counter
	int dc = 0;               				// Depthbuffer polygon counter

	CALC illumination = 0.0;   				// illumination
	char color        = 0;   				// Polygon color and palette color
	BOOLEAN clipped   = FALSE;				// Near-plane clipping flag

	int first_color  = world->first_color;  // Offset in palette of first color
	int color_length = world->color_length;	// length of color in palette

	CALC f          = world->camera [ world->active_camera ].f;
	CALC near_plane = world->camera [ world->active_camera ].near_plane;

	// Transform all visible world geometry into 2D screen polygons and load
	// them into the depth buffer for sorted rendering.

	for ( oc = 0, dc = 0; oc<world->number_objects; oc++ )
	{
		// Skip invisible objects

		if ( world->object [ oc ] .visible == TRUE )
		{
			// Iterate over all faces of the current object

			for ( fc = 0; fc < world->object [ oc ].number_faces; fc++ )
			{
				// Skip invisible faces

				if ( world->object [ oc ].face [ fc ].visible == TRUE )
				{
					// Transform each vertex through the 3D-to-2D pipeline

					for ( vc = 0; vc < world->object [ oc ].face [ fc ].number_vertices; vc++ )
					{
						// object translation

						t [ vc ] = ObjectTranslation
						(
							world->object [ oc ].face [ fc ].v [ vc ],
							world->object [ oc ]
						);

						// camera translation
					#if 0
						if ( world->camera_translation_enabled == TRUE )
						t [ vc ] = CameraTranslation ( t [ vc ], *world );
					#endif

						// screen translation

						v [ vc ] = ScreenTranslation ( t [ vc ], *world );

					} // for

					// Near-plane clip — skip face if any vertex is behind the near plane

					clipped = FALSE;

					for ( vc = 0; vc < world->object [ oc ].face [ fc ].number_vertices; vc++ )
					{
						if ( ( f + t [ vc ].y ) < near_plane )
						{
							clipped = TRUE;
							break;
						}
					}

					if ( clipped == TRUE ) continue;

					// Calculate center of face

					c = GetCenter ( t, world->object [ oc ].face [ fc ].number_vertices );

					world->object [ oc ].face [ fc ].c.x = c.x;
					world->object [ oc ].face [ fc ].c.y = c.y;
					world->object [ oc ].face [ fc ].c.z = c.z;

					// Calculate normals to face

					if ( world->object [ oc ].face [ fc ].flip_normal == TRUE )
					{
						n1 = GetNormal ( t [ 0 ], t [ 1 ], t [ 2 ] );
						n2 = GetNormal ( t [ 2 ], t [ 1 ], t [ 0 ] );
					}
					else
					{
						n1 = GetNormal ( t [ 2 ], t [ 1 ], t [ 0 ] );

						if ( world->object [ oc ].backface_culling_enabled == FALSE )
						{
							n2 = GetNormal ( t [ 0 ], t [ 1 ], t [ 2 ] );
						}
					}

					// Do shading

					color = world->object [ oc ].face [ fc ].color;

					if (world->object [ oc ].face [ fc ].shading_enabled == TRUE )
					{
						// Do illumination calculations

						illumination = LightTranslation ( n1, n2, c, world->object [ oc ], *world );

						// Calculate palette color

						color = ( color - illumination + 1 ) * color_length + first_color;
					}

					// Initialize polygon coordinates

					for ( vc = 0; vc < 4; vc++ )
					{
						world->depth_buffer [ dc ].v [ vc ].x = v [ vc ].x;
						world->depth_buffer [ dc ].v [ vc ].y = v [ vc ].y;
					}

					// Get visibility of surface (hidden surfaces)

					if ( world->object [ oc ].hidden_surfaces_removed == TRUE )
					{
						visibility = GetVisibility ( n1, t [ 0 ], *world );
					}

					// Load depth buffer with polygon data

					BOOLEAN hidden_surfaces_removed  = world->object [ oc ].hidden_surfaces_removed;
					BOOLEAN backface_culling_enabled = world->object [ oc ].backface_culling_enabled;

					if
					(
						( visibility > 0 )
						||
						(( backface_culling_enabled == FALSE ) || ( hidden_surfaces_removed == FALSE ))
					)
					{
						world->depth_buffer [ dc ].number_vertices = world->object [ oc ].face [ fc ].number_vertices;
						world->depth_buffer [ dc ].key             = world->object [ oc ].face [ fc ].c.y;
						world->depth_buffer [ dc ].construction    = world->object [ oc ].face [ fc ].construction;
						world->depth_buffer [ dc ].color           = color;

						// Update depth buffer polygon counter

						dc++;

					} // if

				} // if

			} // for

		} // if

	} // for

	// Update number of polygons

	world->number_polygons = dc;

	// Sort depth buffer

	qsort 
    (
		world->depth_buffer,
		world->number_polygons,
		sizeof ( POLYGON ),
		DepthSort
	);
}

//----------------------------------------------------------------------------
// Function: DisplayWorld
//
// Description:
//
//   Display a 3D world on a screen.
//
// Arguments:
//
//   - world  : The WORLD structure to display.
//   - screen : Destination screen segment.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void DisplayWorld ( WORLD world, unsigned int screen )
{
	int number_vertices = 0;   	// Number of polygon vertices
	int color           = 0;   	// Polygon color

	VERTEX       v [ 4 ];          // Polygon vertices
	CONSTRUCTION construction;  // Polygon construction

	// Get the number of polygons to plot

	int number_polygons = world.number_polygons;

	// plot polygons

	for ( int dc = 0; dc < number_polygons; dc++ )
    {

		// Get polygon data

		number_vertices = world.depth_buffer [ dc ].number_vertices;
		construction    = world.depth_buffer [ dc ].construction;
		color           = world.depth_buffer [ dc ].color;
		v [ 0 ]         = world.depth_buffer [ dc ].v [ 0 ];
		v [ 1 ]         = world.depth_buffer [ dc ].v [ 1 ];
		v [ 2 ]         = world.depth_buffer [ dc ].v [ 2 ];
		v [ 3 ]         = world.depth_buffer [ dc ].v [ 3 ];

		// Do graphics

		switch ( number_vertices )
        {
            // Point

			case 1:
            {
				// TODO

			} break;

            // Line

			case 2:
            {
				// TODO

			} break;

            // Triangle

			case 3:
            {	
			    if ( construction == WIRE_FRAME )
                {
					Triangle
                    (
						v [ 0 ].x, v [ 0 ].y,
						v [ 1 ].x, v [ 1 ].y,
						v [ 2 ].x, v [ 2 ].y,
						color, 
						screen
                    );
                }
				else
                {
				    FillTriangle
                    (
						v [ 0 ].x, v [ 0 ].y,
						v [ 1 ].x, v [ 1 ].y,
						v [ 2 ].x, v [ 2 ].y,
						color, 
						screen
                    );
                }

			} break;

            // Quadrangle

			case 4:
            {	

				if ( construction == WIRE_FRAME )
                {
					Quad
                    (
						v [ 0 ].x, v [ 0 ].y,
						v [ 1 ].x, v [ 1 ].y,
						v [ 2 ].x, v [ 2 ].y,
						v [ 3 ].x, v [ 3 ].y,
						color, 
						screen
                    );
                }
				else
                {
				    FillQuad
                    (
						v [ 0 ].x, v [ 0 ].y,
						v [ 1 ].x, v [ 1 ].y,
						v [ 2 ].x, v [ 2 ].y,
						v [ 3 ].x, v [ 3 ].y,
						color, 
						screen
                    );
                }

			} break;

		} // switch

	} // for
}

//****************************************************************************
//
// Object Manipulation Functions
//
//****************************************************************************

//----------------------------------------------------------------------------
// Function: SetObjectColor
//
// Description:
//
//   Sets an objects color.
//
// Arguments:
//
//   - color  : color index to set.
//   - object : Pointer to the OBJECT structure.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetObjectColor ( int color, OBJECT *object )
{
	// Apply color to all faces in the object

	for ( int fc = 0; fc < object->number_faces; fc++ )
    {
		object->face [ fc ].color = color;
    }
}

//----------------------------------------------------------------------------
// Function: SetObjectConstruction
//
// Description:
//
//   Sets an objects construction (wire frame or solid).
//
// Arguments:
//
//   - construction : Construction type (WIRE_FRAME or SOLID).
//   - object       : Pointer to the OBJECT structure.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetObjectConstruction ( CONSTRUCTION construction, OBJECT *object )
{
	// Apply construction type to all faces in the object

	for ( int fc = 0; fc < object->number_faces; fc++ )
    {
		object->face [ fc ].construction = construction;
    }
}

//----------------------------------------------------------------------------
// Function: SetObjectShading
//
// Description:
//
//   Sets the shading flag of an object.
//   TRUE = Shaded, FALSE = Self illuminating.
//
// Arguments:
//
//   - shading_enabled : shading flag (TRUE or FALSE).
//   - object          : Pointer to the OBJECT structure.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void SetObjectShading ( BOOLEAN shading_enabled, OBJECT *object )
{
	// Apply shading mode to all faces in the object

	for ( int fc = 0; fc < object->number_faces; fc++ )
    {
		object->face [ fc ].shading_enabled = shading_enabled;
    }
}

//----------------------------------------------------------------------------
// Function: FlipNormals
//
// Description:
//
//   Flips the normals of all faces in an object.
//
// Arguments:
//
//   - object : Pointer to the OBJECT structure.
//
// Returns:
//
//   - None
//
//----------------------------------------------------------------------------

void FlipNormals ( OBJECT *object )
{
	for ( int fc = 0; fc < object->number_faces; fc++ )
	{
		if ( object->face [ fc ].flip_normal == TRUE )
		{
			object->face [ fc ].flip_normal = FALSE;
		}
		else
		{
			object->face [ fc ].flip_normal = TRUE;
		}
	}
}

//----------------------------------------------------------------------------
// Mathematical Functions
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Function: GetVector
//
// Description:
//
//   Calculate the vector from point a to point b.
//   (The parameters a and b are 3D points and not vectors)
//
// Arguments:
//
//   - a : Start point.
//   - b : End point.
//
// Returns:
//
//   - The vector from a to b.
//
//----------------------------------------------------------------------------

VECTOR GetVector ( VECTOR a, VECTOR b )
{
	VECTOR v;

	// Compute direction vector by subtracting components

	v.x = b.x - a.x;
	v.y = b.y - a.y;
	v.z = b.z - a.z;

	// Return the direction vector

	return ( v );
}

//----------------------------------------------------------------------------
// Function: GetMagnitude
//
// Description:
//
//   Calculate the magnitude of v.
//
//
// Arguments:
//
//   - v : The vector whose magnitude is to be calculated.
//
// Returns:
//
//   - The magnitude of v.
//
//----------------------------------------------------------------------------

CALC GetMagnitude ( VECTOR v )
{
	// Compute the Euclidean norm of the vector

	CALC m = sqrt ( v.x*v.x + v.y*v.y + v.z*v.z );

	// Return the magnitude

	return ( m );
}

//----------------------------------------------------------------------------
// Function: GetDistance
//
// Description:
//
//   Calculate the distance between point a and point b.
//   (The parameters a and b are 3D points and not vectors)
//
//
// Arguments:
//
//   - a : First point.
//   - b : Second point.
//
// Returns:
//
//   - The distance between a and b.
//
//----------------------------------------------------------------------------

CALC GetDistance ( VECTOR a, VECTOR b )
{
	// Compute component differences and Euclidean distance

	CALC dx = b.x - a.x;
	CALC dy = b.y - a.y;
	CALC dz = b.z - a.z;

	CALC d = sqrt ( dx*dx + dy*dy + dz*dz );

	// Return the distance

	return ( d );
}

//----------------------------------------------------------------------------
// Function: GetNormal
//
// Description:
//
//   Calculate the normal vector to a plane containing points a,b and c.
//   (The parameters a,b and c are 3D points and not vectors)
//
// Arguments:
//
//   - a : First point on the plane.
//   - b : Second point on the plane.
//   - c : Third point on the plane.
//
// Returns:
//
//   - The normal vector to the plane.
//
//----------------------------------------------------------------------------

VECTOR GetNormal ( VECTOR a, VECTOR b, VECTOR c )
{
	VECTOR n;

	// Calculate vector from a to b

	CALC ux = b.x - a.x;
	CALC uy = b.y - a.y;
	CALC uz = b.z - a.z;

	// Calculate vector from a to c

	CALC vx = c.x - a.x;
	CALC vy = c.y - a.y;
	CALC vz = c.z - a.z;

	// Calculate normal (cross product)

	n.x = uy*vz - uz*vy;
	n.y = uz*vx - ux*vz;
	n.z = ux*vy - uy*vx;

	// Return the normal vector

	return ( n );
}

//----------------------------------------------------------------------------
// Function: GetVectorAngle
//
// Description:
//
//   Calculate the angle between u and v.
//
//             u·v
//
// Arguments:
//
//   - u : First vector.
//   - v : Second vector.
//
// Returns:
//
//   - The angle between u and v in radians.
//
//----------------------------------------------------------------------------

CALC GetVectorAngle ( VECTOR u, VECTOR v )
{
	// Calculate dot product

	CALC dot = u.x*v.x + u.y*v.y + u.z*v.z;

	// Calculate vector magnitudes

	CALC mu = GetMagnitude ( u );
	CALC mv = GetMagnitude ( v );

	// Calculate angle

	CALC t = acos ( dot / ( mu*mv ) );

	// Return the angle in radians

	return ( t );
}

//----------------------------------------------------------------------------
// Function: GetCenter
//
// Description:
//
//   Calculates the center point of a face with n vertices.
//   (The parameter v is an array of 3D vertices not vectors)
//
// Arguments:
//
//   - v : Array of 3D vertices.
//   - n : Number of vertices.
//
// Returns:
//
//   - The center point of the face.
//
//----------------------------------------------------------------------------

VECTOR GetCenter ( VECTOR *v, int n )
{
	VECTOR center = { 0, 0, 0 };    // face center

	// Only find the center if we have more than a point
	// ( Pointless to find the center of a point, ha,ha ! )

	if ( n > 1 )
    {
		// Sum up vertices

		for ( int vc = 0; vc < n; vc++ )
        {
			center.x += v [ vc ].x;
			center.y += v [ vc ].y;
			center.z += v [ vc ].z;

		} // for

		// Find average

		center.x /= n;
		center.y /= n;
		center.z /= n;

	} // if

	// Return the center point

	return ( center );
}

//----------------------------------------------------------------------------
// Function: GetVisibility
//
// Description:
//
//   Calculate visibility (Back plane method).
//
// Arguments:
//
//   - n1    : Normal vector of the face.
//   - p     : A point on the face.
//   - world : The WORLD structure.
//
// Returns:
//
//   - visibility value.
//
//----------------------------------------------------------------------------

CALC GetVisibility ( VECTOR n1, VECTOR p, WORLD world )
{
	CALC v = 0.0; // visibility

	// Focal length of camera

	CALC f = world.camera [ world.active_camera ].f;

	// Arbitrary point in the plane of the face (Arbitrary face vertex)

	CALC x = p.x;
	CALC y = p.y;
	CALC z = p.z;

	// Coefficients of plane equation

	CALC a = n1.x;
	CALC b = n1.y;
	CALC c = n1.z;

	// Calculate constant of plane equation

	CALC d = -( a*x + b*y + c*z );

	// Calculate visibility

	v = d - f*b;

	// Return the visibility value

	return ( v );
}

//----------------------------------------------------------------------------
// Function: ObjectTranslation
//
// Description:
//
//   Do object translation calculations.
//
// Arguments:
//
//   - c      : Input coordinate vector.
//   - object : The OBJECT containing translation data.
//
// Returns:
//
//   - The translated coordinate vector.
//
//----------------------------------------------------------------------------

VECTOR ObjectTranslation ( VECTOR c, OBJECT object )
{
	VECTOR t = { 0, 0, 0 };	// Vector to store results of translation

	// Temporary variables

	CALC xt = 0.0;
	CALC yt = 0.0;
	CALC zt = 0.0;

	// Get object translation data

	CALC xc = object.xc;
	CALC yc = object.yc;
	CALC zc = object.zc;

	CALC xs = object.xs;
	CALC ys = object.ys;
	CALC zs = object.zs;

	CALC pitch = object.pitch;
	CALC yaw   = object.yaw;
	CALC roll  = object.roll;

	CALC xd = object.xd;
	CALC yd = object.yd;
	CALC zd = object.zd;

	// Get coordinates

	CALC x = c.x;
	CALC y = c.y;
	CALC z = c.z;

	// Set object center of rotation

	x += xc;
	y += yc;
	z += zc;

	// object scale

	x *= xs;
	y *= ys;
	z *= zs;

	// pitch rotation (around X-axis, tilts in YZ plane)

	yt = y*cos ( pitch ) - z*sin ( pitch );
	zt = y*sin ( pitch ) + z*cos ( pitch );
	y  = yt;
	z  = zt;

	// roll rotation (around Y-axis, tilts in XZ plane)

	xt = x*cos ( roll ) - z*sin ( roll );
	zt = x*sin ( roll ) + z*cos ( roll );
	x  = xt;
	z  = zt;

	// yaw rotation (around Z-axis, turns in XY plane)

	xt = x*cos ( yaw ) - y*sin ( yaw );
	yt = x*sin ( yaw ) + y*cos ( yaw );
	x  = xt;
	y  = yt;

	// object displacement

	x += xd;
	y += yd;
	z += zd;

	// Return translated coordinates

	t.x = x;
	t.y = y;
	t.z = z;

	// Return the fully translated coordinate

	return ( t );
}

//----------------------------------------------------------------------------
// Function: CameraTranslation
//
// Description:
//
//   Do camera translation calculations.
//
// Arguments:
//
//   - c     : Input coordinate vector.
//   - world : The WORLD containing camera data.
//
// Returns:
//
//   - The camera-translated coordinate vector.
//
//----------------------------------------------------------------------------

VECTOR CameraTranslation ( VECTOR c, WORLD world )
{
	VECTOR t = { 0, 0, 0 };	// Vector to store results of translation

	// Temporary variables

	CALC xt = 0.0;
	CALC yt = 0.0;
	CALC zt = 0.0;

	// Get object translation data

	int ac = world.active_camera;

	CALC xc = world.camera [ ac ].source.x;
	CALC yc = world.camera [ ac ].source.y;
	CALC zc = world.camera [ ac ].source.z;

	CALC xr = world.camera [ ac ].xr;
	CALC yr = world.camera [ ac ].yr;
	CALC zr = world.camera [ ac ].zr;

	CALC pitch = world.camera [ ac ].pitch;
	CALC yaw   = world.camera [ ac ].yaw;
	CALC roll  = world.camera [ ac ].roll;

	// Get coordinates

	CALC x = c.x;
	CALC y = c.y;
	CALC z = c.z;

	// Displace camera view point

	xc += world.camera [ ac ].xd;
	yc += world.camera [ ac ].yd;
	zc += world.camera [ ac ].zd;

	// Set camera position to be the center of rotation

	x -= xc;
	y -= yc;
	z -= zc;

	// pitch rotation (around X-axis, tilts in YZ plane)

	yt = y*cos ( pitch ) - z*sin ( pitch );
	zt = y*sin ( pitch ) + z*cos ( pitch );
	y  = yt;
	z  = zt;

	// roll rotation (around Y-axis, tilts in XZ plane)

	xt = x*cos ( roll ) - z*sin ( roll );
	zt = x*sin ( roll ) + z*cos ( roll );
	x  = xt;
	z  = zt;

	// yaw rotation (around Z-axis, turns in XY plane)

	xt = x*cos ( yaw ) - y*sin ( yaw );
	yt = x*sin ( yaw ) + y*cos ( yaw );
	x  = xt;
	y  = yt;

	// Rotational displacement

	x -= xr;
	y -= yr;
	z -= zr;

	// Return translated coordinates

	t.x = x;
	t.y = y;
	t.z = z;

	// Return the camera-translated coordinate

	return ( t );
}

//----------------------------------------------------------------------------
// Function: ScreenTranslation
//
// Description:
//
//   Do screen translation calculations.
//
// Arguments:
//
//   - c     : Input 3D coordinate vector.
//   - world : The WORLD containing screen data.
//
// Returns:
//
//   - The 2D screen-translated vertex.
//
//----------------------------------------------------------------------------

VERTEX ScreenTranslation ( VECTOR c, WORLD world )
{
	VERTEX t = { 0,0 };	// Polygon vertex

	// Get screen translation data

	CALC f = world.camera [ world.active_camera ].f;	// camera focal length

	CALC xa = world.xa;
	CALC ya = world.ya;

	CALC xd = world.xd;
	CALC yd = world.yd;

	// Get 3D translated coordinates

	CALC x = c.x;
	CALC y = c.y;
	CALC z = c.z;

	// Near plane guard — vertices behind the near plane project to screen center

	CALC near_plane = world.camera [ world.active_camera ].near_plane;

	if ( ( f + y ) < near_plane )
	{
		t.x = xd;
		t.y = yd;
		return ( t );
	}

	// Perform 3D to 2D projection

	CALC sx =  ( f*x ) / ( f + y );
	CALC sy = -( f*z ) / ( f + y );

	// Adjust aspect

	sx *= xa;
	sy *= ya;

	// Displace screen (center screen by default)

	sx += xd;
	sy += yd;

	// Return 2D translated coordinates

	t.x = sx;
	t.y = sy;

	// Return the projected 2D screen vertex

	return ( t );
}

//----------------------------------------------------------------------------
// Function: LightTranslation
//
// Description:
//
//   Do illumination calculations.
//
// Arguments:
//
//   - n1     : First normal vector.
//   - n2     : Second normal vector (for double sided faces).
//   - c      : Center point of the face.
//   - object : The OBJECT containing face data.
//   - world  : The WORLD containing light data.
//
// Returns:
//
//   - The illumination value.
//
//----------------------------------------------------------------------------

CALC LightTranslation ( VECTOR n1, VECTOR n2, VECTOR c, OBJECT object, WORLD world )
{
	VECTOR  l                        = { 0, 0, 0};       				// light source vector
	CALC    t1                       = 0.0;              				// Angle between vectors
	CALC    t2                       = 0.0;              				// Angle between vectors
	CALC    i                        = 0.0;              				// Temporary illumination variable
	CALC    illumination             = 1.0;              				// illumination
	BOOLEAN backface_culling_enabled = object.backface_culling_enabled;

	// Calculate illumination of all lights (only if there light switch is on)

	for ( int lc = 0; lc < world.number_lights; lc++ )
	{
		// Process only enabled lights

		if ( world.light [ lc ].enabled == TRUE )
        {
			// Get light vector

			switch ( world.light [ lc ].light_type )
            {
                // Get vector from center of face to light source

				case OMNI:
                {
					l = GetVector ( c, world.light [ lc ].source );
                }
                break;

                // Get vector from light target to light source

				case SPOT:
                {
					l = GetVector ( world.light [ lc ].target, world.light [ lc ].source );
                }
				break;
			}

			// Get angle between light vector and normal of face

			t1 = GetVectorAngle ( n1, l );

			if ( backface_culling_enabled == FALSE )
            {
				t2 = GetVectorAngle ( n2, l );
            }

			// Calculate illumination

			if ( ( t1 > 0 ) && ( t1 < M_PI_2 ) )
            {
				i = t1 / M_PI_2;
            }
			else
            {
				// Front face not lit; use back normal or apply full shadow

				if ( backface_culling_enabled == FALSE )
				{
					i = t2 / M_PI_2;
				}
				else
				{
					i = 1;
				}

			} // if else

			// Set maximum illumination for face

			if ( i < illumination ) illumination = i;
		}
        else
        {
			i = 1;
        }

	} // for

	// Return the minimum illumination across all lights

	return ( illumination );
}

//****************************************************************************
//
// Object Generating Functions
//
// Coordinate system :
//
//          z (up)
//          |
//          |
//          +---- x (right)
//         /
//        y (into screen / depth)
//
//****************************************************************************

//----------------------------------------------------------------------------
// Function: GenerateBox
//
// Description:
//
//   Generate a box object.
//
// Arguments:
//
//   - width            : width of the box.
//   - width_division   : Number of width grid divisions.
//   - height           : height of the box.
//   - height_division  : Number of hight grid divisions.
//   - length           : length of the box.
//   - length_division  : Number of length grid divisions.
//   - object           : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateBox
(
	CALC width,  CALC width_division,
	CALC height, CALC height_division,
	CALC length, CALC length_division,
	OBJECT far *object
)
{
	// Grid counters.

	CALC x_count = 0.0;
	CALC y_count = 0.0;
	CALC z_count = 0.0;

	// center displacements

	CALC xc = width  / 2.0;
	CALC yc = length / 2.0;
	CALC zc = height / 2.0;

	// Grid increments

	CALC xi = width  / width_division;
	CALC yi = length / length_division;
	CALC zi = height / height_division;

	// Initialize object

	int number_faces =
		2*width_division*height_division +
		2*width_division*length_division +
		2*length_division*height_division;

	// Initialize the object and allocate memory for all faces

	InitObject     ( object );
	SetNumberFaces ( number_faces, object );

	// Initialize face count

	int fc=0;

	// Create front face

	for ( z_count = 0; z_count < height; z_count += zi )
	{
		// Generate quads along width subdivisions

		for ( x_count = 0; x_count < width; x_count += xi )
        {
			// Initialize face and allocate 4 vertices

			InitFace          (    &( object->face [ fc ] ) );
			SetNumberVertices ( 4, &( object->face [ fc ] ) );

			// Set quad vertex positions

			object->face [ fc ].v [ 0 ].x = x_count-xc;
			object->face [ fc ].v [ 0 ].y = yc;
			object->face [ fc ].v [ 0 ].z = z_count-zc;

			object->face [ fc ].v [ 1 ].x = x_count-xc+xi;
			object->face [ fc ].v [ 1 ].y = yc;
			object->face [ fc ].v [ 1 ].z = z_count-zc;

			object->face [ fc ].v [ 2 ].x = x_count-xc+xi;
			object->face [ fc ].v [ 2 ].y = yc;
			object->face [ fc ].v [ 2 ].z = z_count-zc+zi;

			object->face [ fc ].v [ 3 ].x = x_count-xc;
			object->face [ fc ].v [ 3 ].y = yc;
			object->face [ fc ].v [ 3 ].z = z_count-zc+zi;

			// Advance to next face

			fc++;

		} // for

	} // for

	// Create back face

	for ( z_count = 0; z_count < height; z_count += zi )
	{
		// Generate quads along width subdivisions

		for ( x_count = 0; x_count < width; x_count += xi )
        {
			// Initialize face and allocate 4 vertices

			InitFace          (    &( object->face [ fc ] ) );
			SetNumberVertices ( 4, &( object->face [ fc ] ) );

			// Set quad vertex positions

			object->face [ fc ].v [ 0 ].x = x_count-xc;
			object->face [ fc ].v [ 0 ].y = -yc;
			object->face [ fc ].v [ 0 ].z = z_count-zc+zi;

			object->face [ fc ].v [ 1 ].x = x_count-xc+xi;
			object->face [ fc ].v [ 1 ].y = -yc;
			object->face [ fc ].v [ 1 ].z = z_count-zc+zi;

			object->face [ fc ].v [ 2 ].x = x_count-xc+xi;
			object->face [ fc ].v [ 2 ].y = -yc;
			object->face [ fc ].v [ 2 ].z = z_count-zc;

			object->face [ fc ].v [ 3 ].x = x_count-xc;
			object->face [ fc ].v [ 3 ].y = -yc;
			object->face [ fc ].v [ 3 ].z = z_count-zc;

			// Advance to next face

			fc++;

		} // for

	} // for

	// Create right side face

	for ( z_count = 0; z_count < height; z_count += zi )
	{
		// Generate quads along length subdivisions

		for ( y_count = 0; y_count < length; y_count += yi )
        {
			// Initialize face and allocate 4 vertices

			InitFace          (    &( object->face [ fc ] ) );
			SetNumberVertices ( 4, &( object->face [ fc ] ) );

			// Set quad vertex positions

			object->face [ fc ].v [ 3 ].x = xc;
			object->face [ fc ].v [ 3 ].y = y_count-yc;
			object->face [ fc ].v [ 3 ].z = z_count-zc;

			object->face [ fc ].v [ 2 ].x = xc;
			object->face [ fc ].v [ 2 ].y = y_count-yc+yi;
			object->face [ fc ].v [ 2 ].z = z_count-zc;

			object->face [ fc ].v [ 1 ].x = xc;
			object->face [ fc ].v [ 1 ].y = y_count-yc+yi;
			object->face [ fc ].v [ 1 ].z = z_count-zc+zi;

			object->face [ fc ].v [ 0 ].x = xc;
			object->face [ fc ].v [ 0 ].y = y_count-yc;
			object->face [ fc ].v [ 0 ].z = z_count-zc+zi;

			// Advance to next face

			fc++;

		} // for

	} // for

	// Create left side face

	for ( z_count = 0; z_count < height; z_count += zi )
	{
		// Generate quads along length subdivisions

		for ( y_count = 0; y_count < length; y_count += yi )
        {
			// Initialize face and allocate 4 vertices

			InitFace          (    &( object->face [ fc ] ) );
			SetNumberVertices ( 4, &( object->face [ fc ] ) );

			// Set quad vertex positions

			object->face [ fc ].v [ 0 ].x = -xc;
			object->face [ fc ].v [ 0 ].y = y_count-yc;
			object->face [ fc ].v [ 0 ].z = z_count-zc;

			object->face [ fc ].v [ 1 ].x = -xc;
			object->face [ fc ].v [ 1 ].y = y_count-yc+yi;
			object->face [ fc ].v [ 1 ].z = z_count-zc;

			object->face [ fc ].v [ 2 ].x = -xc;
			object->face [ fc ].v [ 2 ].y = y_count-yc+yi;
			object->face [ fc ].v [ 2 ].z = z_count-zc+zi;

			object->face [ fc ].v [ 3 ].x = -xc;
			object->face [ fc ].v [ 3 ].y = y_count-yc;
			object->face [ fc ].v [ 3 ].z = z_count-zc+zi;

			// Advance to next face

			fc++;

		} // for

	} // for

	// Create top face

	for ( y_count = 0; y_count < length; y_count += yi )
	{
		// Generate quads along width subdivisions

		for ( x_count = 0; x_count < width; x_count += xi )
        {
			// Initialize face and allocate 4 vertices

			InitFace          (    &( object->face [ fc ] ) );
			SetNumberVertices ( 4, &( object->face [ fc ] ) );

			// Set quad vertex positions

			object->face [ fc ].v [ 3 ].x = x_count-xc;
			object->face [ fc ].v [ 3 ].y = y_count-yc;
			object->face [ fc ].v [ 3 ].z = zc;

			object->face [ fc ].v [ 2 ].x = x_count-xc+xi;
			object->face [ fc ].v [ 2 ].y = y_count-yc;
			object->face [ fc ].v [ 2 ].z = zc;

			object->face [ fc ].v [ 1 ].x = x_count-xc+xi;
			object->face [ fc ].v [ 1 ].y = y_count-yc+yi;
			object->face [ fc ].v [ 1 ].z = zc;

			object->face [ fc ].v [ 0 ].x = x_count-xc;
			object->face [ fc ].v [ 0 ].y = y_count-yc+yi;
			object->face [ fc ].v [ 0 ].z = zc;

			// Advance to next face

			fc++;

		} // for

	} // for

	// Create bottom face

	for ( y_count = 0; y_count < length; y_count += yi )
	{
		// Generate quads along width subdivisions

		for ( x_count = 0; x_count < width; x_count += xi )
        {
			// Initialize face and allocate 4 vertices

			InitFace          (    &( object->face [ fc ] ) );
			SetNumberVertices ( 4, &( object->face [ fc ] ) );

			// Set quad vertex positions

			object->face [ fc ].v [ 0 ].x = x_count-xc;
			object->face [ fc ].v [ 0 ].y = y_count-yc;
			object->face [ fc ].v [ 0 ].z = -zc;

			object->face [ fc ].v [ 1 ].x = x_count-xc+xi;
			object->face [ fc ].v [ 1 ].y = y_count-yc;
			object->face [ fc ].v [ 1 ].z = -zc;

			object->face [ fc ].v [ 2 ].x = x_count-xc+xi;
			object->face [ fc ].v [ 2 ].y = y_count-yc+yi;
			object->face [ fc ].v [ 2 ].z = -zc;

			object->face [ fc ].v [ 3 ].x = x_count-xc;
			object->face [ fc ].v [ 3 ].y = y_count-yc+yi;
			object->face [ fc ].v [ 3 ].z = -zc;

			// Advance to next face

			fc++;

		} // for

	} // for

	// Return total number of generated faces

	return ( number_faces );
}

//----------------------------------------------------------------------------
// Function: GenerateCylinder
//
// Description:
//
//   Generate a cylinder object.
//
// Arguments:
//
//   - height   : height of the cylinder.
//   - radius   : radius of the cylinder.
//   - sides    : Number of sides.
//   - segments : Number of vertical segments.
//   - object   : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateCylinder
(
	CALC height,
	CALC radius,
	int  sides,
	int  segments,
	OBJECT far *object
)
{
	int  fc            = 0;                     	// face counter
	int  segment_count = 0;                     	// segment counter
	int  side_count    = 0;                     	// Side counter
	CALC t             = 0.0;                   	// Angle of rotation
	CALC ti            = ( 2.0*M_PI ) / sides;  	// Rotation increment
	CALC r             = radius;                	// radius of cylinder
	CALC h             = 0.0;                   	// height of the cylinder
	CALC hi            = height / segments;       	// height increment
	int  number_faces  = sides * ( segments + 2 );	// Number of faces

	// Initialize object

	InitObject     ( object );
	SetNumberFaces ( number_faces, object );

	// Generate top cap

	fc = 0;
	h  = height / 2.0;
	t  = 0.0;

	for ( side_count = 0; side_count < sides; side_count++ )
    {
		// Initialize face and allocate 3 vertices

		InitFace          (    &( object->face [ fc ] ) );
		SetNumberVertices ( 3, &( object->face [ fc ] ) );

		// Set triangle vertex positions

		object->face [ fc ].v [ 2 ].x = 0;
		object->face [ fc ].v [ 2 ].y = 0;
		object->face [ fc ].v [ 2 ].z = h;

		object->face [ fc ].v [ 1 ].x = r*cos ( t );
		object->face [ fc ].v [ 1 ].y = r*sin ( t );
		object->face [ fc ].v [ 1 ].z = h;

		object->face [ fc ].v [ 0 ].x = r*cos ( t + ti );
		object->face [ fc ].v [ 0 ].y = r*sin ( t + ti );
		object->face [ fc ].v [ 0 ].z = h;

		// Advance angle and face counter

		t += ti;
		fc++;

	} //for

	// Generate bottom cap

	t = 0.0;

	for ( side_count = 0; side_count < sides; side_count++ )
    {
		// Initialize face and allocate 3 vertices

		InitFace          (    &( object->face [ fc ] ) );
		SetNumberVertices ( 3, &( object->face [ fc ] ) );

		// Set triangle vertex positions

		object->face [ fc ].v [ 0 ].x = 0;
		object->face [ fc ].v [ 0 ].y = 0;
		object->face [ fc ].v [ 0 ].z = -h;

		object->face [ fc ].v [ 1 ].x = r*cos ( t );
		object->face [ fc ].v [ 1 ].y = r*sin ( t );
		object->face [ fc ].v [ 1 ].z = -h;

		object->face [ fc ].v [ 2 ].x = r*cos ( t + ti );
		object->face [ fc ].v [ 2 ].y = r*sin ( t + ti );
		object->face [ fc ].v [ 2 ].z = -h;

		// Advance angle and face counter

		t += ti;
		fc++;

	} // for


	// Generate cylinder

	h = -height / 2.0;

	for ( segment_count = 0; segment_count < segments; segment_count++ )
    {
		for ( side_count = 0, t = 0; side_count < sides; side_count++ )
        {
			// Initialize face and allocate 4 vertices

			InitFace          (    &( object->face [ fc ] ) );
			SetNumberVertices ( 4, &( object->face [ fc ] ) );

			// Set quad vertex positions

			object->face [ fc ].v [ 3 ].x = r*cos ( t );
			object->face [ fc ].v [ 3 ].y = r*sin ( t );
			object->face [ fc ].v [ 3 ].z = h;

			object->face [ fc ].v [ 2 ].x = r*cos ( t + ti );
			object->face [ fc ].v [ 2 ].y = r*sin ( t + ti );
			object->face [ fc ].v [ 2 ].z = h;

			object->face [ fc ].v [ 1 ].x = r*cos ( t + ti );
			object->face [ fc ].v [ 1 ].y = r*sin ( t + ti );
			object->face [ fc ].v [ 1 ].z = h+hi;

			object->face [ fc ].v [ 0 ].x = r*cos ( t );
			object->face [ fc ].v [ 0 ].y = r*sin ( t );
			object->face [ fc ].v [ 0 ].z = h + hi;

			// Advance angle and face counter

			t += ti;
			fc++;

		} // for

		// Advance to next segment height

		h += hi;

	} // for

	// Return the total number of generated faces

	return ( number_faces );
}

//----------------------------------------------------------------------------
// Function: GenerateCone
//
// Description:
//
//   Generate a cone object.
//
// Arguments:
//
//   - height   : height of the cone.
//   - radius   : radius of the cone base.
//   - sides    : Number of sides.
//   - segments : Number of vertical segments.
//   - object   : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateCone
(
	CALC height,
	CALC radius,
	int  sides,
	int  segments,
	OBJECT far *object
)
{
	int  fc            = 0;     // face counter
	int  segment_count = 0;     // segment counter
	int  side_count    = 0;     // Side counter
	CALC t             = 0.0;   // Angle of rotation

	CALC ti = ( 2.0*M_PI ) / sides; // Rotation increment
	CALC r  = 0.0;                  // radius of cone
	CALC ri = radius / segments;    // radius increment
	CALC h  = 0.0;                  // height of the cone
	CALC hi = height / segments;    // height increment

	// Initialize object

	int number_faces = sides * ( segments + 1 );   // Number of faces

	InitObject     ( object );
	SetNumberFaces ( number_faces, object );

	// Generate base

	fc =  0;				// Reset face counter
	t  =  0.0;				// Reset rotation angle
	h  = -height / 2.0;		// Start at bottom of cone
	r  =  radius;			// Start at full base radius

	for ( side_count = 0; side_count < sides; side_count++ )
    {
		// Initialize face and allocate 3 vertices

		InitFace          (    &( object->face [ fc ] ) );
		SetNumberVertices ( 3, &( object->face [ fc ] ) );

		// Set triangle vertex positions

		object->face [ fc ].v [ 0 ].x = 0;
		object->face [ fc ].v [ 0 ].y = 0;
		object->face [ fc ].v [ 0 ].z = h;

		object->face [ fc ].v [ 1 ].x = r*cos ( t );
		object->face [ fc ].v [ 1 ].y = r*sin ( t );
		object->face [ fc ].v [ 1 ].z = h;

		object->face [ fc ].v [ 2 ].x = r*cos ( t + ti );
		object->face [ fc ].v [ 2 ].y = r*sin ( t + ti );
		object->face [ fc ].v [ 2 ].z = h;

		// Advance angle and face counter

		t += ti;
		fc++;

	} // for

	// Generate Cone

	h = -height / 2.0;		// Reset to bottom of cone
	r = radius;				// Reset to full base radius

	for ( segment_count = 0; segment_count < segments; segment_count++ )
    {
		for ( side_count = 0, t = 0; side_count < sides; side_count++ )
        {
			InitFace ( &( object->face [ fc ] ) );

			if ( segment_count == segments - 1 )
            {
				SetNumberVertices ( 3, &( object->face [ fc ] ) );

				object->face [ fc ].v [ 2 ].x = 0;
				object->face [ fc ].v [ 2 ].y = 0;
				object->face [ fc ].v [ 2 ].z = h + hi;

				object->face [ fc ].v [ 1 ].x = r*cos ( t );
				object->face [ fc ].v [ 1 ].y = r*sin ( t );
				object->face [ fc ].v [ 1 ].z = h;

				object->face [ fc ].v [ 0 ].x = r*cos ( t + ti );
				object->face [ fc ].v [ 0 ].y = r*sin ( t + ti );
				object->face [ fc ].v [ 0 ].z = h;

			}
            else
            {
				SetNumberVertices ( 4, &( object->face [ fc ] ) );

				object->face [ fc ].v [ 0 ].x = ( r - ri )*cos ( t );
				object->face [ fc ].v [ 0 ].y = ( r - ri )*sin ( t );
				object->face [ fc ].v [ 0 ].z = h + hi;

				object->face [ fc ].v [ 1 ].x = ( r - ri )*cos ( t + ti );
				object->face [ fc ].v [ 1 ].y = ( r - ri )*sin ( t + ti );
				object->face [ fc ].v [ 1 ].z = h + hi;

				object->face [ fc ].v [ 2 ].x = r*cos ( t + ti );
				object->face [ fc ].v [ 2 ].y = r*sin ( t + ti );
				object->face [ fc ].v [ 2 ].z = h;

				object->face [ fc ].v [ 3 ].x = r*cos ( t );
				object->face [ fc ].v [ 3 ].y = r*sin ( t );
				object->face [ fc ].v [ 3 ].z = h;

			} // if else

			// Advance angle and face counter

			t += ti;
			fc++;

		} // for

		// Advance to next segment

		h += hi;
		r -= ri;

	} // for

	// Return the total number of generated faces

	return ( number_faces );
}

//----------------------------------------------------------------------------
// Function: GenerateIcosphere
//
// Description:
//
//   Generate an icosphere object by recursive subdivision of a regular
//   icosahedron. Each subdivision splits every triangle into four smaller
//   triangles and projects the new vertices onto the sphere surface.
//
//   Frequency 1 produces the base icosahedron (20 faces).
//   Frequency n produces 20 * n^2 faces.
//
// Arguments:
//
//   - radius    : Radius of the sphere.
//   - frequency : Number of subdivisions (1 = base icosahedron).
//   - object    : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateIcosphere
(
	CALC   radius,
	int    frequency,
	OBJECT far *object
)
{
	int fc = 0;		// Face counter
	int i  = 0;		// Loop counter
	int j  = 0;		// Loop counter

	// Golden ratio for icosahedron construction

	CALC golden_ratio = ( 1.0 + sqrt ( 5.0 ) ) / 2.0;

	// Normalize so that each icosahedron vertex lies at distance = radius

	CALC magnitude = sqrt ( 1.0 + golden_ratio * golden_ratio );
	CALC scale     = radius / magnitude;

	// 12 vertices of a regular icosahedron (normalized to radius)

	VECTOR icosahedron_vertices [ 12 ];

	icosahedron_vertices [  0 ].x = -1.0          * scale;
	icosahedron_vertices [  0 ].y =  golden_ratio * scale;
	icosahedron_vertices [  0 ].z =  0.0;

	icosahedron_vertices [  1 ].x =  1.0          * scale;
	icosahedron_vertices [  1 ].y =  golden_ratio * scale;
	icosahedron_vertices [  1 ].z =  0.0;

	icosahedron_vertices [  2 ].x = -1.0          * scale;
	icosahedron_vertices [  2 ].y = -golden_ratio * scale;
	icosahedron_vertices [  2 ].z =  0.0;

	icosahedron_vertices [  3 ].x =  1.0          * scale;
	icosahedron_vertices [  3 ].y = -golden_ratio * scale;
	icosahedron_vertices [  3 ].z =  0.0;

	icosahedron_vertices [  4 ].x =  0.0;
	icosahedron_vertices [  4 ].y = -1.0          * scale;
	icosahedron_vertices [  4 ].z =  golden_ratio * scale;

	icosahedron_vertices [  5 ].x =  0.0;
	icosahedron_vertices [  5 ].y =  1.0          * scale;
	icosahedron_vertices [  5 ].z =  golden_ratio * scale;

	icosahedron_vertices [  6 ].x =  0.0;
	icosahedron_vertices [  6 ].y = -1.0          * scale;
	icosahedron_vertices [  6 ].z = -golden_ratio * scale;

	icosahedron_vertices [  7 ].x =  0.0;
	icosahedron_vertices [  7 ].y =  1.0          * scale;
	icosahedron_vertices [  7 ].z = -golden_ratio * scale;

	icosahedron_vertices [  8 ].x =  golden_ratio * scale;
	icosahedron_vertices [  8 ].y =  0.0;
	icosahedron_vertices [  8 ].z = -1.0          * scale;

	icosahedron_vertices [  9 ].x =  golden_ratio * scale;
	icosahedron_vertices [  9 ].y =  0.0;
	icosahedron_vertices [  9 ].z =  1.0          * scale;

	icosahedron_vertices [ 10 ].x = -golden_ratio * scale;
	icosahedron_vertices [ 10 ].y =  0.0;
	icosahedron_vertices [ 10 ].z = -1.0          * scale;

	icosahedron_vertices [ 11 ].x = -golden_ratio * scale;
	icosahedron_vertices [ 11 ].y =  0.0;
	icosahedron_vertices [ 11 ].z =  1.0          * scale;

	// 20 triangular faces of the icosahedron (vertex indices, CCW winding)

	int icosahedron_faces [ 20 ][ 3 ] =
	{
		{  0, 11,  5 }, {  0,  5,  1 }, {  0,  1,  7 }, {  0,  7, 10 }, {  0, 10, 11 },
		{  1,  5,  9 }, {  5, 11,  4 }, { 11, 10,  2 }, { 10,  7,  6 }, {  7,  1,  8 },
		{  3,  9,  4 }, {  3,  4,  2 }, {  3,  2,  6 }, {  3,  6,  8 }, {  3,  8,  9 },
		{  4,  9,  5 }, {  2,  4, 11 }, {  6,  2, 10 }, {  8,  6,  7 }, {  9,  8,  1 }
	};

	// Calculate total number of faces after subdivision
	//
	// Each base triangle is divided into frequency^2 sub-triangles.

	int number_faces = 20 * frequency * frequency;

	// Initialize object

	InitObject     ( object );
	SetNumberFaces ( number_faces, object );

	// Generate faces by subdividing each icosahedron triangle

	fc = 0;

	for ( i = 0; i < 20; i++ )
	{
		// Get the three corner vertices of this base triangle

		VECTOR vertex_a = icosahedron_vertices [ icosahedron_faces [ i ][ 0 ] ];
		VECTOR vertex_b = icosahedron_vertices [ icosahedron_faces [ i ][ 1 ] ];
		VECTOR vertex_c = icosahedron_vertices [ icosahedron_faces [ i ][ 2 ] ];

		// Subdivide the triangle into frequency^2 sub-triangles
		//
		// We step through rows (j) from the top vertex down to the base.
		// Row j has j upward-pointing triangles and (j-1) downward-pointing
		// triangles, for a total of (2*j - 1) triangles per row.

		for ( j = 0; j < frequency; j++ )
		{
			for ( int k = 0; k <= 2 * j; k++ )
			{
				VECTOR triangle_vertex_0;
				VECTOR triangle_vertex_1;
				VECTOR triangle_vertex_2;
				CALC   magnitude_0;
				CALC   magnitude_1;
				CALC   magnitude_2;

				if ( k % 2 == 0 )
				{
					// Upward-pointing triangle
					//
					// Grid points: (j, c), (j+1, c), (j+1, c+1)
					// where grid(j, c) = (freq-j)*A + (j-c)*B + c*C

					int c = k / 2;

					triangle_vertex_0.x = ( frequency - j ) * vertex_a.x + ( j - c )     * vertex_b.x + c       * vertex_c.x;
					triangle_vertex_0.y = ( frequency - j ) * vertex_a.y + ( j - c )     * vertex_b.y + c       * vertex_c.y;
					triangle_vertex_0.z = ( frequency - j ) * vertex_a.z + ( j - c )     * vertex_b.z + c       * vertex_c.z;

					triangle_vertex_1.x = ( frequency - j - 1 ) * vertex_a.x + ( j + 1 - c ) * vertex_b.x + c       * vertex_c.x;
					triangle_vertex_1.y = ( frequency - j - 1 ) * vertex_a.y + ( j + 1 - c ) * vertex_b.y + c       * vertex_c.y;
					triangle_vertex_1.z = ( frequency - j - 1 ) * vertex_a.z + ( j + 1 - c ) * vertex_b.z + c       * vertex_c.z;

					triangle_vertex_2.x = ( frequency - j - 1 ) * vertex_a.x + ( j - c )     * vertex_b.x + ( c + 1 ) * vertex_c.x;
					triangle_vertex_2.y = ( frequency - j - 1 ) * vertex_a.y + ( j - c )     * vertex_b.y + ( c + 1 ) * vertex_c.y;
					triangle_vertex_2.z = ( frequency - j - 1 ) * vertex_a.z + ( j - c )     * vertex_b.z + ( c + 1 ) * vertex_c.z;
				}
				else
				{
					// Downward-pointing triangle
					//
					// Grid points: (j, c), (j+1, c+1), (j, c+1)
					// where grid(j, c) = (freq-j)*A + (j-c)*B + c*C

					int c = ( k - 1 ) / 2;

					triangle_vertex_0.x = ( frequency - j )     * vertex_a.x + ( j - c )     * vertex_b.x + c       * vertex_c.x;
					triangle_vertex_0.y = ( frequency - j )     * vertex_a.y + ( j - c )     * vertex_b.y + c       * vertex_c.y;
					triangle_vertex_0.z = ( frequency - j )     * vertex_a.z + ( j - c )     * vertex_b.z + c       * vertex_c.z;

					triangle_vertex_1.x = ( frequency - j - 1 ) * vertex_a.x + ( j - c )     * vertex_b.x + ( c + 1 ) * vertex_c.x;
					triangle_vertex_1.y = ( frequency - j - 1 ) * vertex_a.y + ( j - c )     * vertex_b.y + ( c + 1 ) * vertex_c.y;
					triangle_vertex_1.z = ( frequency - j - 1 ) * vertex_a.z + ( j - c )     * vertex_b.z + ( c + 1 ) * vertex_c.z;

					triangle_vertex_2.x = ( frequency - j )     * vertex_a.x + ( j - c - 1 ) * vertex_b.x + ( c + 1 ) * vertex_c.x;
					triangle_vertex_2.y = ( frequency - j )     * vertex_a.y + ( j - c - 1 ) * vertex_b.y + ( c + 1 ) * vertex_c.y;
					triangle_vertex_2.z = ( frequency - j )     * vertex_a.z + ( j - c - 1 ) * vertex_b.z + ( c + 1 ) * vertex_c.z;

				} // if else

				// Project each vertex onto the sphere surface

				magnitude_0 = sqrt ( triangle_vertex_0.x * triangle_vertex_0.x + triangle_vertex_0.y * triangle_vertex_0.y + triangle_vertex_0.z * triangle_vertex_0.z );
				magnitude_1 = sqrt ( triangle_vertex_1.x * triangle_vertex_1.x + triangle_vertex_1.y * triangle_vertex_1.y + triangle_vertex_1.z * triangle_vertex_1.z );
				magnitude_2 = sqrt ( triangle_vertex_2.x * triangle_vertex_2.x + triangle_vertex_2.y * triangle_vertex_2.y + triangle_vertex_2.z * triangle_vertex_2.z );

				triangle_vertex_0.x = triangle_vertex_0.x * radius / magnitude_0;
				triangle_vertex_0.y = triangle_vertex_0.y * radius / magnitude_0;
				triangle_vertex_0.z = triangle_vertex_0.z * radius / magnitude_0;

				triangle_vertex_1.x = triangle_vertex_1.x * radius / magnitude_1;
				triangle_vertex_1.y = triangle_vertex_1.y * radius / magnitude_1;
				triangle_vertex_1.z = triangle_vertex_1.z * radius / magnitude_1;

				triangle_vertex_2.x = triangle_vertex_2.x * radius / magnitude_2;
				triangle_vertex_2.y = triangle_vertex_2.y * radius / magnitude_2;
				triangle_vertex_2.z = triangle_vertex_2.z * radius / magnitude_2;

				// Initialize face and allocate 3 vertices

				InitFace          (    &( object->face [ fc ] ) );
				SetNumberVertices ( 3, &( object->face [ fc ] ) );

				// Set triangle vertex positions

				object->face [ fc ].v [ 0 ] = triangle_vertex_0;
				object->face [ fc ].v [ 1 ] = triangle_vertex_2;
				object->face [ fc ].v [ 2 ] = triangle_vertex_1;

				// Advance to next face

				fc++;

			} // for k

		} // for j

	} // for i

	// Return the total number of generated faces

	return ( number_faces );
}

//----------------------------------------------------------------------------
// Function: GenerateGeosphere
//
// Description:
//
//   Generate a geosphere object.
//
// Arguments:
//
//   - radius           : radius of the sphere.
//   - equatorial_sides : Number of equatorial sides.
//   - object           : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateGeosphere
(
	CALC radius,
	int  equatorial_sides,
	OBJECT far *object
)
{
	int fc = 0;     // face counter
	int ec = 0;     // Equatorial increment counter
	int lc = 0;     // Longitudinal increment counter

	CALC r  = 0.0;  // radius of sphere
	CALC a  = 0.0;  // Longitudinal angle
	CALC ai = 0.0;  // Longitudinal angle increment
	CALC b  = 0.0;  // Lateral angle
	CALC bi = 0.0;  // Lateral angle increment

	int longitudinal_sides = ceil ( (float) equatorial_sides / 2 );

	// Initialize object

	int number_faces = equatorial_sides * longitudinal_sides;

	InitObject     ( object );
	SetNumberFaces ( number_faces, object );

	// Generate sphere

	fc = 0;									// Reset face counter
	ai = M_PI / longitudinal_sides;			// Longitudinal angle increment (pole to pole)
	bi = ( 2.0*M_PI ) / equatorial_sides;	// Equatorial angle increment (full rotation)
	r  = radius;							// Sphere radius

	for ( lc = 0, a = 0; lc < longitudinal_sides; lc++ )
    {
		for ( ec = 0, b = 0; ec < equatorial_sides; ec++ )
        {
			InitFace ( &( object->face [ fc ] ) );

			if ( lc == 0 )
            {
				SetNumberVertices ( 3, &( object->face [ fc ] ) );

				object->face [ fc ].v [ 2 ].x = r*sin ( a )*cos ( b );
				object->face [ fc ].v [ 2 ].y = r*sin ( a )*sin ( b );
				object->face [ fc ].v [ 2 ].z = r*cos ( a );

				object->face [ fc ].v [ 1 ].x = r*sin ( a + ai )*cos ( b );
				object->face [ fc ].v [ 1 ].y = r*sin ( a + ai )*sin ( b );
				object->face [ fc ].v [ 1 ].z = r*cos ( a + ai );

				object->face [ fc ].v [ 0 ].x = r*sin ( a + ai )*cos ( b + bi );
				object->face [ fc ].v [ 0 ].y = r*sin ( a + ai )*sin ( b + bi );
				object->face [ fc ].v [ 0 ].z = r*cos ( a + ai );
			}
            else if ( lc == longitudinal_sides - 1 )
            {
				SetNumberVertices ( 3, &( object->face [ fc ] ) );

				object->face [ fc ].v [ 0 ].x = r*sin ( a )*cos ( b );
				object->face [ fc ].v [ 0 ].y = r*sin ( a )*sin ( b );
				object->face [ fc ].v [ 0 ].z = r*cos ( a );

				object->face [ fc ].v [ 1 ].x = r*sin ( a )*cos ( b + bi );
				object->face [ fc ].v [ 1 ].y = r*sin ( a )*sin ( b + bi );
				object->face [ fc ].v [ 1 ].z = r*cos ( a );

				object->face [ fc ].v [ 2 ].x = r*sin ( a + ai )*cos ( b + bi );
				object->face [ fc ].v [ 2 ].y = r*sin ( a + ai )*sin ( b + bi );
				object->face [ fc ].v [ 2 ].z = r*cos ( a + ai );

			}
            else
            {
				SetNumberVertices ( 4, &( object->face [ fc ] ) );

				object->face [ fc ].v [ 0 ].x = r*sin ( a )*cos ( b );
				object->face [ fc ].v [ 0 ].y = r*sin ( a )*sin ( b );
				object->face [ fc ].v [ 0 ].z = r*cos ( a );

				object->face [ fc ].v [ 1 ].x = r*sin ( a )*cos ( b + bi );
				object->face [ fc ].v [ 1 ].y = r*sin ( a )*sin ( b + bi );
				object->face [ fc ].v [ 1 ].z = r*cos ( a );

				object->face [ fc ].v [ 2 ].x = r*sin ( a + ai )*cos ( b + bi );
				object->face [ fc ].v [ 2 ].y = r*sin ( a + ai )*sin ( b + bi );
				object->face [ fc ].v [ 2 ].z = r*cos ( a + ai );

				object->face [ fc ].v [ 3 ].x = r*sin ( a + ai )*cos ( b );
				object->face [ fc ].v [ 3 ].y = r*sin ( a + ai )*sin ( b );
				object->face [ fc ].v [ 3 ].z = r*cos ( a + ai );

			} // if else if else

			// Advance lateral angle and face counter

			b += bi;
			fc++;

		} // for

		// Advance to next longitudinal band

		a += ai;

	} // for

	// Return the total number of generated faces

	return ( number_faces );
}


//----------------------------------------------------------------------------
// Function: GenerateHemisphere
//
// Description:
//
//   Generate a hemisphere object (half of a geosphere).
//
// Arguments:
//
//   - radius           : radius of the hemisphere.
//   - equatorial_sides : Number of equatorial sides.
//   - object           : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateHemisphere
(
	CALC radius,
	int  equatorial_sides,
	OBJECT far *object
)
{
	int fc = 0;     // face counter
	int ec = 0;     // Equatorial increment counter
	int lc = 0;     // Longitudinal increment counter

	CALC r  = 0.0;  // radius of sphere
	CALC a  = 0.0;  // Longitudinal angle
	CALC ai = 0.0;  // Longitudinal angle increment
	CALC b  = 0.0;  // Lateral angle
	CALC bi = 0.0;  // Lateral angle increment

	int longitudinal_sides = ceil ( (float) equatorial_sides / 4 );

	// Initialize object

	int number_faces = equatorial_sides * (longitudinal_sides + 1);

	InitObject     ( object );
	SetNumberFaces ( number_faces, object );

	// Generate sphere

	fc = 0;
	ai = M_PI_2 / longitudinal_sides;
	bi = ( 2.0*M_PI ) / equatorial_sides;
	r  = radius;

	// Generate hemisphere

	for ( lc = 0, a = 0; lc < longitudinal_sides; lc++ )
    {
		for ( ec = 0, b = 0; ec < equatorial_sides; ec++ )
        {
			InitFace ( &( object->face [ fc ] ) );

			if ( lc == 0 )
            {
				SetNumberVertices ( 3, &( object->face [ fc ] ) );

				object->face [ fc ].v [ 2 ].x = r*sin ( a )*cos ( b );
				object->face [ fc ].v [ 2 ].y = r*sin ( a )*sin ( b );
				object->face [ fc ].v [ 2 ].z = r*cos ( a );

				object->face [ fc ].v [ 1 ].x = r*sin ( a + ai )*cos ( b );
				object->face [ fc ].v [ 1 ].y = r*sin ( a + ai )*sin ( b );
				object->face [ fc ].v [ 1 ].z = r*cos ( a + ai );

				object->face [ fc ].v [ 0 ].x = r*sin ( a + ai )*cos ( b + bi );
				object->face [ fc ].v [ 0 ].y = r*sin ( a + ai )*sin ( b + bi );
				object->face [ fc ].v [ 0 ].z = r*cos ( a + ai );

			}
            else
            {
				SetNumberVertices ( 4, &( object->face [ fc ] ) );

				object->face [ fc ].v [ 0 ].x = r*sin ( a )*cos ( b );
				object->face [ fc ].v [ 0 ].y = r*sin ( a )*sin ( b );
				object->face [ fc ].v [ 0 ].z = r*cos ( a );

				object->face [ fc ].v [ 1 ].x = r*sin ( a )*cos ( b + bi );
				object->face [ fc ].v [ 1 ].y = r*sin ( a )*sin ( b + bi );
				object->face [ fc ].v [ 1 ].z = r*cos ( a );

				object->face [ fc ].v [ 2 ].x = r*sin ( a + ai )*cos ( b + bi );
				object->face [ fc ].v [ 2 ].y = r*sin ( a + ai )*sin ( b + bi );
				object->face [ fc ].v [ 2 ].z = r*cos ( a + ai );

				object->face [ fc ].v [ 3 ].x = r*sin ( a + ai )*cos ( b );
				object->face [ fc ].v [ 3 ].y = r*sin ( a + ai )*sin ( b );
				object->face [ fc ].v [ 3 ].z = r*cos ( a + ai );

			} // if else if else

			b += bi;
			fc++;

		} // for

		a += ai;

	} // for

	// Generate base

	for ( ec = 0, b = 0; ec < equatorial_sides; ec++ )
    {
		InitFace          (    &( object->face [ fc ] ) );
		SetNumberVertices ( 3, &( object->face [ fc ] ) );

		object->face [ fc ].v [ 0 ].x = 0;
		object->face [ fc ].v [ 0 ].y = 0;
		object->face [ fc ].v [ 0 ].z = 0;

		object->face [ fc ].v [ 1 ].x = r*cos ( b );
		object->face [ fc ].v [ 1 ].y = r*sin ( b );
		object->face [ fc ].v [ 1 ].z = 0;

		object->face [ fc ].v [ 2 ].x = r*cos ( b + bi );
		object->face [ fc ].v [ 2 ].y = r*sin ( b + bi );
		object->face [ fc ].v [ 2 ].z = 0;

		b += bi;
		fc++;

	} // for

	// Return the total number of generated faces

	return ( number_faces );
}

//----------------------------------------------------------------------------
// Function: GenerateTorus
//
// Description:
//
//   Generate a torus object.
//
// Arguments:
//
//   - inner_radius  : Inner radius of the torus.
//   - outer_radius  : Outer radius of the torus.
//   - segments      : Number of segments around the torus ring.
//   - circle_sides  : Number of sides on the revolved circle.
//   - object        : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateTorus
(
	CALC inner_radius,
	CALC outer_radius,
	int  segments,
	int  circle_sides,
	OBJECT far *object
)
{
	int fc = 0;     // face counter
	int ec = 0;     // Lateral increment (Equatorial increment)
	int lc = 0;     // Longitudinal increment counter

	CALC r             = 0.0;  	// radius of torus (origin to center of revolved circle)
	CALC major_radius  = 0.0;  	// radius of revolved circle
	CALC a             = 0.0;  	// Longitudinal angle
	CALC ai            = 0.0;  	// Longitudinal angle increment
	CALC b             = 0.0;  	// Lateral angle
	CALC bi            = 0.0;  	// Lateral angle increment

	VECTOR temp = { 0, 0, 0 };	// Temporary vector
	CALC   t    = 0.0;      	// Reference angle for interior faces

	// Initialize object

	int number_faces = segments * circle_sides;

	InitObject     ( object );
	SetNumberFaces ( number_faces, object );

	// Generate torus

	fc           = 0;
	r            = ( outer_radius - inner_radius ) / 2.0;
	major_radius = inner_radius + r;
	ai           = ( 2.0*M_PI ) / circle_sides;
	bi           = ( 2.0*M_PI ) / segments;
	t            = M_PI_2 - asin ( r / major_radius );

	for ( ec = 0, b = 0; ec < segments; ec++ )
    {
		for ( lc = 0, a = 0; lc < circle_sides; lc++ )
        {
			InitFace          (    &( object->face [ fc ] ) );
			SetNumberVertices ( 4, &( object->face [ fc ] ) );

			object->face [ fc ].v [ 0 ].x = r*sin ( a )*cos ( b ) + major_radius*cos ( b );
			object->face [ fc ].v [ 0 ].y = r*sin ( a )*sin ( b ) + major_radius*sin ( b );
			object->face [ fc ].v [ 0 ].z = r*cos ( a );

			object->face [ fc ].v [ 1 ].x = r*sin ( a )*cos ( b + bi ) + major_radius*cos ( b + bi );
			object->face [ fc ].v [ 1 ].y = r*sin ( a )*sin ( b + bi ) + major_radius*sin ( b + bi );
			object->face [ fc ].v [ 1 ].z = r*cos ( a );

			object->face [ fc ].v [ 2 ].x = r*sin ( a + ai )*cos ( b + bi ) + major_radius*cos ( b + bi );
			object->face [ fc ].v [ 2 ].y = r*sin ( a + ai )*sin ( b + bi ) + major_radius*sin ( b + bi );
			object->face [ fc ].v [ 2 ].z = r*cos ( a + ai );

			object->face [ fc ].v [ 3 ].x = r*sin ( a + ai )*cos ( b ) + major_radius*cos ( b );
			object->face [ fc ].v [ 3 ].y = r*sin ( a + ai )*sin ( b ) + major_radius*sin ( b );
			object->face [ fc ].v [ 3 ].z = r*cos ( a + ai );

			// Set normal for interior faces

			float a_min = 3.0*M_PI_2 - t - ( ai / 2.0 );		// Lower bound of interior angle range
			float a_max = 3.0*M_PI_2 + t - ( ai / 2.0 );		// Upper bound of interior angle range

			if ( ( a > a_min ) && ( a < a_max ) )
            {
				// Interior faces: swap vertex winding and set flip_normal

				object->face [ fc ].flip_normal = TRUE;

				temp                        = object->face [ fc ].v [ 0 ];
				object->face [ fc ].v [ 0 ] = object->face [ fc ].v [ 3 ];
				object->face [ fc ].v [ 3 ] = temp;

				temp                        = object->face [ fc ].v [ 1 ];
				object->face [ fc ].v [ 1 ] = object->face [ fc ].v [ 2 ];
				object->face [ fc ].v [ 2 ] = temp;
			}

			a += ai;
			fc++;

		} // for

		b += bi;

	} // for

	// Return the total number of generated faces

	return ( number_faces );
}

//----------------------------------------------------------------------------
// Function: GenerateTube
//
// Description:
//
//   Generate a tube object (bored cylinder).
//
// Arguments:
//
//   - height       : height of the tube.
//   - inner_radius : Inner radius of the tube.
//   - outer_radius : Outer radius of the tube.
//   - sides        : Number of sides.
//   - segments     : Number of vertical segments.
//   - object       : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateTube
(
	CALC height,
	CALC inner_radius,
	CALC outer_radius,
	int  sides,
	int  segments,
	OBJECT far *object
)
{
	int  fc            = 0;                         	// face counter
	int  segment_count = 0;                         	// segment counter
	int  side_count    = 0;                         	// Side counter
	CALC t             = 0.0;                       	// Angle of rotation
	CALC ti            = ( 2.0*M_PI ) / sides;      	// Rotation increment
	CALC r1            = inner_radius;              	// Inner radius
	CALC r2            = outer_radius;              	// Outer radius
	CALC h             = 0.0;                       	// height of the cylinder
	CALC hi            = height / segments;          	// height increment
	int  number_faces  = 2 * sides * ( segments + 1 );	// Number of faces

	// Initialize object

	InitObject     ( object );
	SetNumberFaces ( number_faces, object );

	// Generate top cap

	fc = 0;
	h  = height / 2.0;
	t  = 0.0;

	for ( side_count = 0; side_count < sides; side_count++ )
    {
		InitFace          (    &( object->face [ fc ] ) );
		SetNumberVertices ( 4, &( object->face [ fc ] ) );

		object->face [ fc ].v [ 0 ].x = r1*cos ( t );
		object->face [ fc ].v [ 0 ].y = r1*sin ( t );
		object->face [ fc ].v [ 0 ].z = h;

		object->face [ fc ].v [ 1 ].x = r1*cos ( t + ti );
		object->face [ fc ].v [ 1 ].y = r1*sin ( t + ti );
		object->face [ fc ].v [ 1 ].z = h;

		object->face [ fc ].v [ 2 ].x = r2*cos ( t + ti );
		object->face [ fc ].v [ 2 ].y = r2*sin ( t + ti );
		object->face [ fc ].v [ 2 ].z = h;

		object->face [ fc ].v [ 3 ].x = r2*cos ( t );
		object->face [ fc ].v [ 3 ].y = r2*sin ( t );
		object->face [ fc ].v [ 3 ].z = h;

		t += ti;
		fc++;

	} // for

	// Generate bottom cap

	t = 0.0;

	for ( side_count = 0; side_count < sides; side_count++ )
    {
		InitFace          (    &( object->face [ fc ] ) );
		SetNumberVertices ( 4, &( object->face [ fc ] ) );

		object->face [ fc ].v [ 3 ].x = r1*cos ( t );
		object->face [ fc ].v [ 3 ].y = r1*sin ( t );
		object->face [ fc ].v [ 3 ].z = -h;

		object->face [ fc ].v [ 2 ].x = r1*cos ( t + ti );
		object->face [ fc ].v [ 2 ].y = r1*sin ( t + ti );
		object->face [ fc ].v [ 2 ].z = -h;

		object->face [ fc ].v [ 1 ].x = r2*cos ( t + ti );
		object->face [ fc ].v [ 1 ].y = r2*sin ( t + ti );
		object->face [ fc ].v [ 1 ].z = -h;

		object->face [ fc ].v [ 0 ].x = r2*cos ( t );
		object->face [ fc ].v [ 0 ].y = r2*sin ( t );
		object->face [ fc ].v [ 0 ].z = -h;

		t += ti;
		fc++;

	} // for


	// Generate Inner_cylinder

	h = -height / 2.0;

	for ( segment_count = 0; segment_count < segments; segment_count++ )
    {
		for ( side_count = 0, t = 0; side_count < sides; side_count++ )
        {
			InitFace          (    &( object->face [ fc ] ) );
			SetNumberVertices ( 4, &( object->face [ fc ] ) );

			object->face [ fc ].v [ 3 ].x = r1*cos ( t );
			object->face [ fc ].v [ 3 ].y = r1*sin ( t );
			object->face [ fc ].v [ 3 ].z = h;

			object->face [ fc ].v [ 2 ].x = r1*cos ( t + ti );
			object->face [ fc ].v [ 2 ].y = r1*sin ( t + ti );
			object->face [ fc ].v [ 2 ].z = h;

			object->face [ fc ].v [ 1 ].x = r1*cos ( t + ti );
			object->face [ fc ].v [ 1 ].y = r1*sin ( t + ti );
			object->face [ fc ].v [ 1 ].z = h + hi;

			object->face [ fc ].v [ 0 ].x = r1*cos ( t );
			object->face [ fc ].v [ 0 ].y = r1*sin ( t );
			object->face [ fc ].v [ 0 ].z = h + hi;

			object->face [ fc ].flip_normal = TRUE;	// Set normal for interior faces

			t += ti;
			fc++;

		} // for

		h += hi;

	} // for

	// Generate Outer_cylinder

	h = -height / 2.0;

	for ( segment_count = 0; segment_count < segments; segment_count++ )
    {
		for ( side_count = 0, t = 0; side_count < sides; side_count++ )
        {
			InitFace          (    &( object->face [ fc ] ) );
			SetNumberVertices ( 4, &( object->face [ fc ] ) );

			object->face [ fc ].v [ 3 ].x = r2*cos ( t );
			object->face [ fc ].v [ 3 ].y = r2*sin ( t );
			object->face [ fc ].v [ 3 ].z = h;

			object->face [ fc ].v [ 2 ].x = r2*cos ( t + ti );
			object->face [ fc ].v [ 2 ].y = r2*sin ( t + ti );
			object->face [ fc ].v [ 2 ].z = h;

			object->face [ fc ].v [ 1 ].x = r2*cos ( t + ti );
			object->face [ fc ].v [ 1 ].y = r2*sin ( t + ti );
			object->face [ fc ].v [ 1 ].z = h + hi;

			object->face [ fc ].v [ 0 ].x = r2*cos ( t );
			object->face [ fc ].v [ 0 ].y = r2*sin ( t );
			object->face [ fc ].v [ 0 ].z = h + hi;

			t += ti;
			fc++;

		} // for

		h += hi;

	} // for

	// Return the total number of generated faces

	return ( number_faces );
}

//----------------------------------------------------------------------------
// Function: GenerateSquareLamina
//
// Description:
//
//   Generate a square lamina object from four corner vertices.
//
// Arguments:
//
//   - a      : First corner vertex.
//   - b      : Second corner vertex.
//   - c      : Third corner vertex.
//   - d      : Fourth corner vertex.
//   - grid1  : Grid subdivision parameter 1.
//   - grid2  : Grid subdivision parameter 2.
//   - object : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateSquareLamina
(
	VECTOR a,
	VECTOR b,
	VECTOR c,
	VECTOR d,
	int    grid1,
	int    grid2,
	OBJECT far *object
)
{
	// Initialize object

	InitObject     ( object );
	SetNumberFaces ( 1, object );

	// Initialize face

	InitFace          (    &( object->face [ 0 ] ) );
	SetNumberVertices ( 4, &( object->face [ 0 ] ) );

	// Set coordinates

	object->face [ 0 ].v [ 0 ].x = a.x;
	object->face [ 0 ].v [ 0 ].y = a.y;
	object->face [ 0 ].v [ 0 ].z = a.z;

	object->face [ 0 ].v [ 1 ].x = b.x;
	object->face [ 0 ].v [ 1 ].y = b.y;
	object->face [ 0 ].v [ 1 ].z = b.z;

	object->face [ 0 ].v [ 2 ].x = c.x;
	object->face [ 0 ].v [ 2 ].y = c.y;
	object->face [ 0 ].v [ 2 ].z = c.z;

	object->face [ 0 ].v [ 3 ].x = d.x;
	object->face [ 0 ].v [ 3 ].y = d.y;
	object->face [ 0 ].v [ 3 ].z = d.z;

	// Return the face count (single quad generated)

	return ( 1 );
}

//----------------------------------------------------------------------------
// Function: GenerateTriangularLamina
//
// Description:
//
//   Generate a triangular lamina object from three corner vertices.
//
// Arguments:
//
//   - a      : First corner vertex.
//   - b      : Second corner vertex.
//   - c      : Third corner vertex.
//   - object : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateTriangularLamina
(
	VECTOR a,
	VECTOR b,
	VECTOR c,
	OBJECT far *object
)
{
	// Initialize object

	InitObject     (    object );
	SetNumberFaces ( 1, object );

	// Initialize face

	InitFace          (    &( object->face [ 0 ] ) );
	SetNumberVertices ( 3, &( object->face [ 0 ] ) );

	// Set coordinates

	object->face [ 0 ].v [ 0 ].x = a.x;
	object->face [ 0 ].v [ 0 ].y = a.y;
	object->face [ 0 ].v [ 0 ].z = a.z;

	object->face [ 0 ].v [ 1 ].x = b.x;
	object->face [ 0 ].v [ 1 ].y = b.y;
	object->face [ 0 ].v [ 1 ].z = b.z;

	object->face [ 0 ].v [ 2 ].x = c.x;
	object->face [ 0 ].v [ 2 ].y = c.y;
	object->face [ 0 ].v [ 2 ].z = c.z;

	// Return the face count (single triangle generated)

	return ( 1 );
}

//----------------------------------------------------------------------------
// Function: GeneratePolygonalLamina
//
// Description:
//
//   Generate a polygonal lamina object.
//
// Arguments:
//
//   - radius   : radius of the polygon.
//   - segments : Number of polygon segments.
//   - object   : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GeneratePolygonalLamina
(
	CALC radius,
	int  segments,
	OBJECT far *object
)
{
	// TODO

	// Placeholder, not yet implemented

	return ( 1 );
}

//----------------------------------------------------------------------------
// Function: GenerateLine
//
// Description:
//
//   Generate a 3D line object from two endpoints.
//
// Arguments:
//
//   - a        : Start point.
//   - b        : End point.
//   - segments : Number of line segments.
//   - object   : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GenerateLine
(
	VECTOR a,
	VECTOR b,
	int    segments,
	OBJECT far *object
)
{

	// Initialize object

	InitObject     (    object );
	SetNumberFaces ( 1, object );

	// Initialize face

	InitFace          (    &( object->face [ 0 ] ) );
	SetNumberVertices ( 2, &( object->face [ 0 ] ) );

	// Set coordinates

	object->face [ 0 ].v [ 0 ].x = a.x;
	object->face [ 0 ].v [ 0 ].y = a.y;
	object->face [ 0 ].v [ 0 ].z = a.z;

	object->face [ 0 ].v [ 1 ].x = b.x;
	object->face [ 0 ].v [ 1 ].y = b.y;
	object->face [ 0 ].v [ 1 ].z = b.z;

	// Return the face count (single line segment generated)

	return ( 1 );
}

//----------------------------------------------------------------------------
// Function: GeneratePoint
//
// Description:
//
//   Yes, you guessed it, a 3D point.
//
//   I have to admit that using this 3D engine to describe points as
//   objects is a bit of an over kill, not to mention inefficient, but
//   you can do it !
//
// Arguments:
//
//   - a      : The 3D point.
//   - object : Pointer to the OBJECT structure to populate.
//
// Returns:
//
//   - Number of faces generated.
//
//----------------------------------------------------------------------------

int GeneratePoint
(
	VECTOR a,
	OBJECT far *object
)
{
	// Initialize object

	InitObject     (    object );
	SetNumberFaces ( 1, object );

	// Initialize face

	InitFace          (    &( object->face [ 0 ] ) );
	SetNumberVertices ( 1, &( object->face [ 0 ] ) );

	// Set coordinates

	object->face [ 0 ].v [ 0 ].x = a.x;
	object->face [ 0 ].v [ 0 ].y = a.y;
	object->face [ 0 ].v [ 0 ].z = a.z;

	// Return the face count (single point generated)

	return ( 1 );
}


//----------------------------------------------------------------------------

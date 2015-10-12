#include <stdbool.h>
#include <GL/gl.h>

#include "../matrix.h"
#include "../world.h"
#include "../viewport.h"
#include "../layers.h"
#include "../tilepicker.h"
#include "../inlinebin.h"
#include "../programs.h"
#include "../programs/solid.h"

#define SIZE	256.0f
#define MARGIN	 10.0f

// Projection matrix:
static float mat_proj[16];

static GLuint vao_bkgd;
static GLuint vbo_bkgd;

// Each point has 2D space coordinates and color:
struct vertex {
	float x;
	float y;
	float r;
	float g;
	float b;
	float a;
} __attribute__((packed));

static bool
occludes (void)
{
	// The overview never occludes:
	return false;
}

static inline void
setup_viewport (void)
{
	int xpos = viewport_get_wd() - MARGIN - SIZE;
	int ypos = viewport_get_ht() - MARGIN - SIZE;

	glLineWidth(1.0);
	glViewport(xpos, ypos, SIZE, SIZE);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(mat_proj);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void
paint_background (void)
{
	// Array of indices. We define two counterclockwise triangles:
	// 0-1-3 and 1-2-3
	GLubyte index[6] = {
		0, 1, 3,
		1, 2, 3,
	};

	// Draw solid background:
	glBindVertexArray(vao_bkgd);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_bkgd);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, index);
}

static void
paint (void)
{
	// Draw 1:1 to screen coordinates, origin bottom left:
	setup_viewport();

	double world_size = world_get_size();

	// Use the solid program:
	program_solid_use(&((struct program_solid) {
		.matrix = mat_proj,
	}));

	// Paint background:
	paint_background();

	// Reset program:
	program_none();

	// Draw tiles from tile picker:
	int zoom;
	float x, y, tile_wd, tile_ht, p[4][3];

	for (int iter = tilepicker_first(&x, &y, &tile_wd, &tile_ht, &zoom, p); iter; iter = tilepicker_next(&x, &y, &tile_wd, &tile_ht, &zoom, p)) {
		float zoomcolors[6][3] = {
			{ 1.0, 0.0, 0.0 },
			{ 0.0, 1.0, 0.0 },
			{ 0.0, 0.0, 1.0 },
			{ 0.5, 0.5, 0.0 },
			{ 0.0, 0.5, 0.5 },
			{ 0.5, 0.0, 0.5 }
		};
		glColor4f(zoomcolors[zoom % 3][0], zoomcolors[zoom % 3][1], zoomcolors[zoom % 3][2], 0.5);
		glBegin(GL_QUADS);
			glVertex2f(x, world_size - y);
			glVertex2f(x + tile_wd, world_size - y);
			glVertex2f(x + tile_wd, world_size - y - tile_ht);
			glVertex2f(x, world_size - y - tile_ht);
		glEnd();
		glColor4f(1.0, 1.0, 1.0, 0.5);
		glBegin(GL_LINE_LOOP);
			glVertex2d(x, world_size - y);
			glVertex2d(x + tile_wd, world_size - y);
			glVertex2d(x + tile_wd, world_size - y - tile_ht);
			glVertex2d(x, world_size - y - tile_ht);
		glEnd();
	}
	// Draw frustum (view region):
	double *wx, *wy;
	viewport_get_frustum(&wx, &wy);

	glColor4f(1.0, 0.3, 0.3, 0.5);
	glBegin(GL_QUADS);
	for (int i = 0; i < 4; i++) {
		glVertex2d(wx[i], wy[i]);
	}
	glEnd();

	// Actual center:
	double cx = viewport_get_center_x();
	double cy = viewport_get_center_y();

	glColor4f(1.0, 1.0, 1.0, 0.7);
	glBegin(GL_LINES);
		glVertex2d(cx - 0.5, cy);
		glVertex2d(cx + 0.5, cy);
		glVertex2d(cx, cy - 0.5);
		glVertex2d(cx, cy + 0.5);
	glEnd();

	// Draw bounding box:
	double *bx, *by;
	viewport_get_bbox(&bx, &by);

	glColor4f(0.3, 0.3, 1.0, 0.5);
	glBegin(GL_LINE_LOOP);
		glVertex2d(bx[0], by[1]);
		glVertex2d(bx[1], by[1]);
		glVertex2d(bx[1], by[0]);
		glVertex2d(bx[0], by[0]);
	glEnd();

	glDisable(GL_BLEND);
}

static void
zoom (const unsigned int zoom)
{
	double size = world_get_size();

	// Make room within the world for one extra pixel at each side,
	// to keep the outlines on the far tiles within frame:
	double one_pixel_at_scale = (1 << zoom) / SIZE;
	size += one_pixel_at_scale;

	mat_ortho(mat_proj, 0, size, 0, size, 0, 1);

	// Background quad is array of counterclockwise vertices:
	//
	//   3--2
	//   |  |
	//   0--1
	//
	struct vertex bkgd[4] = {
		{ 0.0f, 0.0f, 0.3f, 0.3f, 0.3f, 0.5f },
		{ size, 0.0f, 0.3f, 0.3f, 0.3f, 0.5f },
		{ size, size, 0.3f, 0.3f, 0.3f, 0.5f },
		{ 0.0f, size, 0.3f, 0.3f, 0.3f, 0.5f },
	};

	// Bind vertex buffer object and upload vertices:
	glBindBuffer(GL_ARRAY_BUFFER, vbo_bkgd);
	glBufferData(GL_ARRAY_BUFFER, sizeof(bkgd), bkgd, GL_STREAM_DRAW);
}

static bool
init (void)
{
	// Generate vertex buffer object:
	glGenBuffers(1, &vbo_bkgd);

	// Generate vertex array object:
	glGenVertexArrays(1, &vao_bkgd);

	// Bind buffer and vertex array:
	glBindBuffer(GL_ARRAY_BUFFER, vbo_bkgd);
	glBindVertexArray(vao_bkgd);

	// Add pointer to 'vertex' attribute:
	glEnableVertexAttribArray(program_solid_loc_vertex());
	glVertexAttribPointer(program_solid_loc_vertex(), 2, GL_FLOAT, GL_FALSE,
		sizeof(struct vertex),
		(void *)(&((struct vertex *)0)->x));

	// Add pointer to 'color' attribute:
	glEnableVertexAttribArray(program_solid_loc_color());
	glVertexAttribPointer(program_solid_loc_color(), 4, GL_FLOAT, GL_FALSE,
		sizeof(struct vertex),
		(void *)(&((struct vertex *)0)->r));

	zoom(0);

	return true;
}

static void
destroy (void)
{
	// Delete vertex array object:
	glDeleteVertexArrays(1, &vao_bkgd);

	// Delete vertex buffer:
	glDeleteBuffers(1, &vbo_bkgd);
}

struct layer *
layer_overview (void)
{
	static struct layer layer = {
		.init     = &init,
		.occludes = &occludes,
		.paint    = &paint,
		.zoom     = &zoom,
		.destroy  = &destroy,
	};

	return &layer;
}

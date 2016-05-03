#include <stdbool.h>
#include <stdlib.h>
#include <GL/gl.h>

#include "../matrix.h"
#include "../inlinebin.h"
#include "../glutil.h"
#include "../layers.h"
#include "../programs.h"
#include "../programs/cursor.h"
#include "../viewport.h"
#include "../png.h"

// Array of counterclockwise vertices:
//
//   3--2
//   |  |
//   0--1
//
static struct glutil_vertex_uv vertex[4] = GLUTIL_VERTEX_UV_DEFAULT;

// Projection matrix:
static struct {
	float proj[16];
} matrix;

// Screen size:
static struct {
	float width;
	float height;
} screen;

// Cursor texture:
static struct {
	char		*raw;
	const char	*png;
	size_t		 len;
	GLuint		 id;
	unsigned int	 width;
	unsigned int	 height;
} tex;

static GLuint vao, vbo;

static void
paint (void)
{
	// Viewport is screen:
	glViewport(0, 0, screen.width, screen.height);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Use the cursor program:
	program_cursor_use(&((struct program_cursor) {
		.mat_proj = matrix.proj,
	}));

	// Activate cursor texture:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex.id);

	// Draw all triangles in the buffer:
	glBindVertexArray(vao);
	glutil_draw_quad();

	program_none();
}

static void
resize (const unsigned int width, const unsigned int height)
{
	screen.width  = width;
	screen.height = height;

	// Projection matrix maps 1:1 to screen:
	mat_scale(matrix.proj, 2.0f / screen.width, 2.0f / screen.height, 0.0f);
}

static void
init_texture (void)
{
	// Generate texture:
	glGenTextures(1, &tex.id);
	glBindTexture(GL_TEXTURE_2D, tex.id);

	// Load cursor texture:
	inlinebin_get(TEXTURE_CURSOR, &tex.png, &tex.len);
	png_load(tex.png, tex.len, &tex.height, &tex.width, &tex.raw);

	// Upload texture:
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.raw);

	// Free memory:
	free(tex.raw);

	int halfwd = tex.width  / 2;
	int halfht = tex.height / 2;

	// Size vertex array to texture:
	vertex[0].x = -halfwd; vertex[0].y = -halfht;
	vertex[1].x =  halfwd; vertex[1].y = -halfht;
	vertex[2].x =  halfwd; vertex[2].y =  halfht;
	vertex[3].x = -halfwd; vertex[3].y =  halfht;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

static bool
init (void)
{
	// Init texture:
	init_texture();

	// Generate vertex buffer object:
	glGenBuffers(1, &vbo);

	// Generate vertex array object:
	glGenVertexArrays(1, &vao);

	// Bind buffer and vertex array:
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBindVertexArray(vao);

	// Link 'vertex' and 'texture' attributes:
	glutil_vertex_uv_link(
		program_cursor_loc(LOC_CURSOR_VERTEX),
		program_cursor_loc(LOC_CURSOR_TEXTURE));

	// Copy vertices to buffer:
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);

	return true;
}

static void
destroy (void)
{
	// Delete texture:
	glDeleteTextures(1, &tex.id);

	// Delete vertex array object:
	glDeleteVertexArrays(1, &vao);

	// Delete vertex buffer:
	glDeleteBuffers(1, &vbo);
}

const struct layer *
layer_cursor (void)
{
	static struct layer layer = {
		.init     = &init,
		.paint    = &paint,
		.resize   = &resize,
		.destroy  = &destroy,
	};

	return &layer;
}

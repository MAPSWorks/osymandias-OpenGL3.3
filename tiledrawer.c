#include "tiledrawer.h"
#include "programs.h"
#include "programs/spherical.h"

// Array of indices. If we have a quad defined by these corners:
//
//   3--2
//   |  |
//   0--1
//
// we define two counterclockwise triangles, 0-2-3 and 0-1-2:
static const uint8_t vertex_index[] = {
	0, 2, 3,
	0, 1, 2,
};

static struct {
	uint32_t vao;
} state;

void
tiledrawer (const struct tiledrawer *td)
{
	if (td->data == NULL)
		return;

	// Set tile zoom level:
	program_spherical_set_tile(td->tile, td->data);

	// Bind texture:
	glBindTexture(GL_TEXTURE_2D, td->data->u32);

	// Draw two triangles which together form one quad:
	glDrawElements(GL_TRIANGLES, sizeof (vertex_index), GL_UNSIGNED_BYTE, vertex_index);
}

void
tiledrawer_start (const struct camera *cam, const struct viewport *vp)
{
	static bool init = false;

	// Lazy init:
	if (init == false) {
		glGenVertexArrays(1, &state.vao);
		init = true;
	}

	glBindVertexArray(state.vao);

	program_spherical_use(&(struct program_spherical) {
		.cam           = vp->cam_pos,
		.mat_viewproj  = vp->matrix.viewproj,
		.mat_view_inv  = vp->invert.view,
		.mat_model     = vp->matrix.model,
		.mat_model_inv = vp->invert.model,
		.vp_angle      = cam->view_angle,
		.vp_width      = vp->width,
	});

	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
}

#include <string.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include "globe.h"
#include "camera.h"
#include "tilepicker.h"
#include "layers.h"
#include "matrix.h"
#include "programs.h"
#include "viewport.h"

// Screen dimensions:
static struct viewport vp;

void
viewport_destroy (void)
{
	layers_destroy();
	programs_destroy();
}

bool
viewport_unproject (const struct viewport_pos *pos, float *lat, float *lon)
{
	const struct globe *globe = globe_get();
	union vec p1, p2;

	// Ignore if point is outside viewport:
	if (pos->x < 0 || (uint32_t) pos->x >= vp.width)
		return false;

	if (pos->y < 0 || (uint32_t) pos->y >= vp.height)
		return false;

	// Unproject two points at different z index through the
	// view-projection matrix to get two points in world coordinates:
	camera_unproject(&p1, &p2, pos->x, pos->y, &vp);

	// Unproject through the inverse model matrix to get two points in
	// model space:
	mat_vec_multiply(p1.elem.f, globe->invert.model, p1.elem.f);
	mat_vec_multiply(p2.elem.f, globe->invert.model, p2.elem.f);

	// Direction vector is difference between points:
	const union vec dir = vec_sub(p1, p2);

	// Intersect these two points with the globe:
	return globe_intersect(&p1, &dir, lat, lon);
}

void
viewport_paint (void)
{
	// Clear the depth buffer:
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	const struct camera *cam  = camera_get();
	const struct globe *globe = globe_get();

	// Gather and combine view and model matrices:
	if (globe->updated.model) {
		memcpy(vp.matrix.model, globe->matrix.model, sizeof (vp.matrix.model));
		memcpy(vp.invert.model, globe->invert.model, sizeof (vp.invert.model));
	}

	if (cam->updated.view) {
		memcpy(vp.matrix.view, cam->matrix.view, sizeof (vp.matrix.view));
		memcpy(vp.invert.view, cam->invert.view, sizeof (vp.invert.view));
	}

	if (cam->updated.proj || cam->updated.view) {
		mat_multiply(vp.matrix.viewproj, cam->matrix.proj, cam->matrix.view);
		mat_invert(vp.invert.viewproj, vp.matrix.viewproj);
	}

	if (globe->updated.model || cam->updated.proj || cam->updated.view) {
		mat_multiply(vp.matrix.modelviewproj, vp.matrix.viewproj, vp.matrix.model);
		mat_multiply(vp.invert.modelview, vp.invert.model, vp.invert.view);
	}

	if (globe->updated.model || cam->updated.view) {
		float cam_pos[4], origin[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		// Find camera position in model space by unprojecting origin:
		mat_vec_multiply(cam_pos, vp.invert.modelview, origin);
		vp.cam_pos[0] = cam_pos[0];
		vp.cam_pos[1] = cam_pos[1];
		vp.cam_pos[2] = cam_pos[2];
	}

	// Reset matrix update flags:
	camera_updated_reset();
	globe_updated_reset();

	// Paint all layers:
	layers_paint(camera_get(), &vp);
}

void
viewport_resize (const unsigned int width, const unsigned int height)
{
	vp.width  = width;
	vp.height = height;

	// Setup viewport:
	glViewport(0, 0, vp.width, vp.height);

	// Update camera's projection matrix:
	camera_set_aspect_ratio(&vp);

	// Alert layers:
	layers_resize(&vp);
}

void
viewport_gl_setup_world (void)
{
	// FIXME: only do this when moved?
	tilepicker_recalc(&vp, camera_get());

	glDisable(GL_BLEND);
}

const struct viewport *
viewport_get (void)
{
	return &vp;
}

bool
viewport_init (const uint32_t width, const uint32_t height)
{
	vp.width  = width;
	vp.height = height;

	if (!programs_init())
		return false;

	if (!layers_init(&vp))
		return false;

	if (!camera_init(&vp))
		return false;

	globe_init();
	return true;
}

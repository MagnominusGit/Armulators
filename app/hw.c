/* Copyright (c) 2026 Magnominus
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A core that draws with the GPU (RETRO_ENVIRONMENT_SET_HW_RENDER) gets a GL context of its own beside
// the renderer's, current while the core runs, and a framebuffer it draws each frame into. The frame
// is read back as a picture, which then shows, records and goes under the menus as any core's does.

#include "frontend.h"

#include <stdlib.h>
#include <string.h>

// The few GL names used here, the same in GL and GLES.
typedef unsigned GLuint;
typedef unsigned GLenum;
typedef int GLint;
typedef int GLsizei;
enum {
	GL_TEXTURE_2D = 0x0DE1,
	GL_UNSIGNED_BYTE = 0x1401,
	GL_RGBA = 0x1908,
	GL_PACK_ALIGNMENT = 0x0D05,
	GL_TEXTURE_MIN_FILTER = 0x2801,
	GL_NEAREST = 0x2600,
	GL_RGBA8 = 0x8058,
	GL_DEPTH24_STENCIL8 = 0x88F0,
	GL_DEPTH_COMPONENT16 = 0x81A5,
	GL_FRAMEBUFFER = 0x8D40,
	GL_RENDERBUFFER = 0x8D41,
	GL_COLOR_ATTACHMENT0 = 0x8CE0,
	GL_DEPTH_ATTACHMENT = 0x8D00,
	GL_DEPTH_STENCIL_ATTACHMENT = 0x821A,
	GL_FRAMEBUFFER_COMPLETE = 0x8CD5,
};

static struct {
	void (*GenFramebuffers)(GLsizei, GLuint*);
	void (*DeleteFramebuffers)(GLsizei, const GLuint*);
	void (*BindFramebuffer)(GLenum, GLuint);
	void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
	void (*FramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
	GLenum (*CheckFramebufferStatus)(GLenum);
	void (*GenRenderbuffers)(GLsizei, GLuint*);
	void (*DeleteRenderbuffers)(GLsizei, const GLuint*);
	void (*BindRenderbuffer)(GLenum, GLuint);
	void (*RenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
	void (*GenTextures)(GLsizei, GLuint*);
	void (*DeleteTextures)(GLsizei, const GLuint*);
	void (*BindTexture)(GLenum, GLuint);
	void (*TexParameteri)(GLenum, GLenum, GLint);
	void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
	void (*PixelStorei)(GLenum, GLint);
	void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
} gl;

// What the core asked for, its context, how deeply it's current, and its framebuffer.
static struct retro_hw_render_callback hw;
static bool asked;
static SDL_GLContext context;
static int depth;
static GLuint framebuffer, color, depthBuffer;
static uint8_t *readBack, *picture;
static size_t pictureSize;

// GLES on Android and where the renderer is SDL's GLES one, desktop GL where it's SDL's GL one; none
// with the software renderer.
static bool gles(void) {
	SDL_RendererInfo info;
	return SDL_GetRendererInfo(app.renderer, &info) == 0 && !strncmp(info.name, "opengles", 8);
}

static bool glRenderer(void) {
	SDL_RendererInfo info;
	return app.renderer && SDL_GetRendererInfo(app.renderer, &info) == 0 && !strncmp(info.name, "opengl", 6);
}

bool hwPreferred(unsigned* type) {
	if (!glRenderer()) {
		return false;
	}
	*type = gles() ? RETRO_HW_CONTEXT_OPENGLES3 : RETRO_HW_CONTEXT_OPENGL_CORE;
	return true;
}

static uintptr_t currentFramebuffer(void) {
	return framebuffer;
}

static retro_proc_address_t procAddress(const char* name) {
	return (retro_proc_address_t) SDL_GL_GetProcAddress(name);
}

bool hwAsk(struct retro_hw_render_callback* callback) {
	bool const es = callback->context_type == RETRO_HW_CONTEXT_OPENGLES2 || callback->context_type == RETRO_HW_CONTEXT_OPENGLES3 ||
	                callback->context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION;
	bool const desktop = callback->context_type == RETRO_HW_CONTEXT_OPENGL || callback->context_type == RETRO_HW_CONTEXT_OPENGL_CORE;
	if (!glRenderer() || (es ? !gles() : !desktop || gles())) {
		return false;
	}
	callback->get_current_framebuffer = currentFramebuffer;
	callback->get_proc_address = procAddress;
	hw = *callback;
	asked = true;
	return true;
}

void hwBegin(void) {
	if (context && depth++ == 0) {
		SDL_GL_MakeCurrent(app.window, context);
	}
}

// The renderer makes its own context current again as it next draws.
void hwEnd(void) {
	if (context && --depth == 0) {
		SDL_GL_MakeCurrent(app.window, NULL);
	}
}

static void dropFramebuffer(void) {
	if (framebuffer) {
		gl.DeleteFramebuffers(1, &framebuffer);
		gl.DeleteTextures(1, &color);
		if (depthBuffer) {
			gl.DeleteRenderbuffers(1, &depthBuffer);
		}
	}
	framebuffer = color = depthBuffer = 0;
}

void hwStop(void) {
	if (context) {
		hwBegin();
		if (hw.context_destroy) {
			hw.context_destroy();
		}
		dropFramebuffer();
		SDL_GL_MakeCurrent(app.window, NULL);
		SDL_GL_DeleteContext(context);
	}
	context = NULL;
	depth = 0;
	asked = false;
	free(readBack);
	free(picture);
	readBack = picture = NULL;
	pictureSize = 0;
}

// After the game loads: the context the core asked for, its framebuffer as large as the core's
// largest picture, and the core told it has them.
bool hwStart(const struct retro_game_geometry* geometry) {
	if (!asked) {
		return true;
	}
	bool const es = hw.context_type != RETRO_HW_CONTEXT_OPENGL && hw.context_type != RETRO_HW_CONTEXT_OPENGL_CORE;
	unsigned major = hw.version_major, minor = hw.version_minor;
	if (hw.context_type == RETRO_HW_CONTEXT_OPENGLES2 || hw.context_type == RETRO_HW_CONTEXT_OPENGLES3) {
		major = hw.context_type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3;
		minor = 0;
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, es ? SDL_GL_CONTEXT_PROFILE_ES
	                                                 : hw.context_type == RETRO_HW_CONTEXT_OPENGL_CORE ? SDL_GL_CONTEXT_PROFILE_CORE : 0);
	if (major) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, (int) major);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, (int) minor);
	}
	context = SDL_GL_CreateContext(app.window);
	// The renderer's contexts, made again when it is, keep its own.
	SDL_GL_ResetAttributes();
	if (!context) {
		osd("Can't make the GL context the core draws with: %s", SDL_GetError());
		asked = false;
		return false;
	}
	depth = 1;
	void** const functions = (void**) &gl;
	static const char* const names[] = { "glGenFramebuffers", "glDeleteFramebuffers", "glBindFramebuffer", "glFramebufferTexture2D",
		"glFramebufferRenderbuffer", "glCheckFramebufferStatus", "glGenRenderbuffers", "glDeleteRenderbuffers", "glBindRenderbuffer",
		"glRenderbufferStorage", "glGenTextures", "glDeleteTextures", "glBindTexture", "glTexParameteri", "glTexImage2D",
		"glPixelStorei", "glReadPixels" };
	bool found = true;
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i) {
		functions[i] = SDL_GL_GetProcAddress(names[i]);
		found &= functions[i] != NULL;
	}
	int const w = (int) geometry->max_width, h = (int) geometry->max_height;
	if (found) {
		gl.GenTextures(1, &color);
		gl.BindTexture(GL_TEXTURE_2D, color);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		gl.BindTexture(GL_TEXTURE_2D, 0);
		gl.GenFramebuffers(1, &framebuffer);
		gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
		if (hw.depth) {
			gl.GenRenderbuffers(1, &depthBuffer);
			gl.BindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
			gl.RenderbufferStorage(GL_RENDERBUFFER, hw.stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT16, w, h);
			gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, hw.stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
			                           depthBuffer);
		}
		found = gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	if (!found) {
		osd("Can't make the framebuffer the core draws in");
		hwStop();
		return false;
	}
	if (hw.context_reset) {
		hw.context_reset();
	}
	hwEnd();
	return true;
}

bool hwRunning(void) {
	return context != NULL;
}

// The frame the core drew, as XRGB8888 rows from the top (pitch width * 4), with the core's context
// current.
const void* hwRead(unsigned width, unsigned height) {
	size_t const size = (size_t) width * height * 4;
	if (size > pictureSize) {
		free(readBack);
		free(picture);
		readBack = malloc(size);
		picture = malloc(size);
		pictureSize = readBack && picture ? size : 0;
		if (!pictureSize) {
			return NULL;
		}
	}
	gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	gl.PixelStorei(GL_PACK_ALIGNMENT, 4);
	gl.ReadPixels(0, 0, (GLsizei) width, (GLsizei) height, GL_RGBA, GL_UNSIGNED_BYTE, readBack);
	gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
	for (unsigned y = 0; y < height; ++y) {
		const uint8_t* in = readBack + (size_t) (hw.bottom_left_origin ? height - 1 - y : y) * width * 4;
		uint8_t* out = picture + (size_t) y * width * 4;
		for (unsigned x = 0; x < width; ++x, in += 4, out += 4) {
			out[0] = in[2];
			out[1] = in[1];
			out[2] = in[0];
			out[3] = 0xFF;
		}
	}
	return picture;
}

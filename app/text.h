/* Copyright (c) 2026 Magnominus
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Licensed under the GNU General Public License, version 2 or (at your option) any later version: see LICENSE. */

// Sharp text at any size (text.c).

#pragma once

#include <SDL.h>
#include <stdbool.h>
#include <stddef.h>

enum FaceId {
	FACE_BODY,
	FACE_STRONG,
	FACE_TITLE,
	FACES,
};

bool textInit(void);
void textReset(void);
int textWidth(enum FaceId face, int size, const char* text);
// Draws text whose line box starts at y; returns its width.
int textDraw(enum FaceId face, int size, int x, int y, SDL_Color color, const char* text);
void textFit(enum FaceId face, int size, const char* text, int width, bool keepEnd, char* out, size_t outSize);

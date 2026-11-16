/* Copyright (c) 2026 Magnominus
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Licensed under the GNU General Public License, version 2 or (at your option) any later version: see LICENSE. */

// Cheats, a file for each game in ~/.local/share/armgba/cheats, in RetroArch's .cht form, so its
// cheat files work here too: cheats = N, then cheatI_desc, cheatI_code and cheatI_enable, and for a
// code with XX or ?? in it, cheatI_fill, the hex digits that go there (which RetroArch passes over).

#include "frontend.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Cheat* cheats;
int cheatCount;

// The game's cheat file, by its name, as RetroArch names it.
static void cheatPath(char out[FILE_SIZE]) {
	snprintf(out, FILE_SIZE, "%s/%s.cht", cheatDirectory, app.gameName);
}

// Room for count cheats, the new ones empty; returns the last, or NULL without the memory.
static struct Cheat* growTo(int count) {
	if (count > cheatCount) {
		struct Cheat* grown = realloc(cheats, count * sizeof(*cheats));
		if (!grown) {
			return NULL;
		}
		cheats = grown;
		memset(&cheats[cheatCount], 0, (count - cheatCount) * sizeof(*cheats));
		cheatCount = count;
	}
	return &cheats[count - 1];
}

// Reads a .cht file's cheats into the list after the first base. Returns false if it can't be read.
static bool readCheats(const char* path, int base) {
	FILE* f = fopen(path, "r");
	if (!f) {
		return false;
	}
	char line[1024];
	while (fgets(line, sizeof(line), f)) {
		int index;
		char field[16];
		char* equals = strchr(line, '=');
		if (!equals || sscanf(line, "cheat%d_%15[a-z]", &index, field) != 2 || index < 0 || index > 999) {
			continue;
		}
		char* value = equals + 1 + strspn(equals + 1, " \t\"");
		value[strcspn(value, "\"\r\n")] = '\0';
		struct Cheat* const c = growTo(base + index + 1);
		if (!c) {
			break;
		}
		if (!strcmp(field, "desc")) {
			snprintf(c->description, sizeof(c->description), "%s", value);
		} else if (!strcmp(field, "code")) {
			snprintf(c->code, sizeof(c->code), "%s", value);
		} else if (!strcmp(field, "enable")) {
			c->enabled = !strcmp(value, "true");
		} else if (!strcmp(field, "fill")) {
			snprintf(c->fill, sizeof(c->fill), "%s", value);
		}
	}
	fclose(f);
	return true;
}

// The game's cheats, from its file, as it loads.
void cheatListLoad(void) {
	free(cheats);
	cheats = NULL;
	cheatCount = 0;
	char path[FILE_SIZE];
	cheatPath(path);
	readCheats(path, 0);
}

// A .cht file's cheats added after the game's; how many, or -1 if it can't be read.
int cheatListImport(const char* path) {
	int const before = cheatCount;
	if (!readCheats(path, before)) {
		return -1;
	}
	return cheatCount - before;
}

// The game's cheats to its file; with none, the file goes.
void cheatListSave(void) {
	char path[FILE_SIZE];
	cheatPath(path);
	if (!cheatCount) {
		remove(path);
		return;
	}
	FILE* f = fopen(path, "w");
	if (!f) {
		osd("Can't write %s", path);
		return;
	}
	fprintf(f, "cheats = %d\n\n", cheatCount);
	for (int i = 0; i < cheatCount; ++i) {
		fprintf(f, "cheat%d_desc = \"%s\"\ncheat%d_code = \"%s\"\ncheat%d_enable = %s\n", i, cheats[i].description, i, cheats[i].code, i,
		        cheats[i].enabled ? "true" : "false");
		if (cheats[i].fill[0]) {
			fprintf(f, "cheat%d_fill = \"%s\"\n", i, cheats[i].fill);
		}
		fputc('\n', f);
	}
	fclose(f);
}

static bool isFillMark(char c) {
	return c == 'X' || c == 'x' || c == 'Y' || c == 'y' || c == 'Z' || c == 'z' || c == '?';
}

// A run of one mark: XX, YY, ZZ or ??, as long as it is.
static int fillRun(const char* p) {
	int run = 0;
	while (isFillMark(p[run]) && (p[run] | 0x20) == (p[0] | 0x20)) {
		++run;
	}
	return run;
}

// A template is a hex code (it has a digit, as no NES Game Genie code does, where X, Y and Z are
// letters of the code) whose only other characters are runs of one mark and separators. Returns its
// longest run, 0 for a code that isn't one.
int cheatFillDigits(const char* code) {
	int longest = 0;
	bool digit = false;
	for (const char* p = code; *p;) {
		int const run = fillRun(p);
		if (run) {
			longest = run > longest ? run : longest;
			p += run;
			continue;
		}
		if (isdigit((unsigned char) *p)) {
			digit = true;
		} else if (!isxdigit((unsigned char) *p) && !strchr("+-: ", *p)) {
			return 0;
		}
		++p;
	}
	return digit ? longest : 0;
}

// Each run of XX or ?? takes the fill's last digits, as many as it has, the fill padded with zeros.
bool cheatFilledCode(const struct Cheat* c, char* out, size_t size) {
	int const digits = cheatFillDigits(c->code);
	snprintf(out, size, "%s", c->code);
	if (!digits) {
		return true;
	}
	if (!c->fill[0] || digits > 64) {
		return false;
	}
	// The fill, zeros before it up to the longest run
	char fill[sizeof(c->fill) + 64];
	size_t const given = strlen(c->fill);
	size_t const pad = (size_t) digits > given ? (size_t) digits - given : 0;
	memset(fill, '0', pad);
	memcpy(fill + pad, c->fill, given + 1);
	size_t const length = pad + given;
	for (char* p = out; *p;) {
		int const run = fillRun(p);
		for (int k = 0; k < run; ++k) {
			p[k] = fill[length - run + k];
		}
		p += run ? run : 1;
	}
	return true;
}

// The core takes the enabled ones, their XX or ?? filled in (one without its fill waits); none in
// hardcore mode.
void cheatListApply(void) {
	if (!app.loaded) {
		return;
	}
	retro_cheat_reset();
	for (int i = 0; i < cheatCount && !cheevosHardcore(); ++i) {
		char code[sizeof(cheats[i].code)];
		if (cheats[i].enabled && cheats[i].code[0] && cheatFilledCode(&cheats[i], code, sizeof(code))) {
			retro_cheat_set(i, true, code);
		}
	}
}

// A new cheat, turned on.
void cheatListAdd(const char* description, const char* code) {
	struct Cheat* c = growTo(cheatCount + 1);
	if (c) {
		snprintf(c->description, sizeof(c->description), "%s", description);
		snprintf(c->code, sizeof(c->code), "%s", code);
		c->enabled = true;
	}
}

void cheatListRemove(int index) {
	if (index >= 0 && index < cheatCount) {
		memmove(&cheats[index], &cheats[index + 1], (cheatCount - index - 1) * sizeof(*cheats));
		--cheatCount;
	}
}

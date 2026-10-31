/* Copyright (c) 2026 Magnominus
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Whole files: games, also out of a .zip or .7z, and saves and states, written so that one cut off
// leaves the old file whole.

#include "frontend.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <file/archive_file.h>
#include <file/file_path.h>

void* readFile(const char* path, size_t* size) {
	*size = 0;
	FILE* f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	long const length = ftell(f);
	fseek(f, 0, SEEK_SET);
	void* data = length > 0 ? malloc(length) : NULL;
	if (data && fread(data, 1, length, f) == (size_t) length) {
		*size = length;
	} else {
		free(data);
		data = NULL;
	}
	fclose(f);
	return data;
}

void* readGame(const char* path, size_t* size) {
	char archive[PATH_SIZE * 2];
	snprintf(archive, sizeof(archive), "%s", path);
	char* const hash = strchr(archive, '#');
	if (hash) {
		*hash = '\0';
	}
	if (!hash || !path_is_compressed_file(archive)) {
		return readFile(archive, size);
	}
	// RetroArch's own archive code, so the same files open as in RetroArch.
	void* data = NULL;
	int64_t length = 0;
	if (!file_archive_compressed_read(path, &data, NULL, &length) || length <= 0) {
		free(data);
		return NULL;
	}
	*size = (size_t) length;
	return data;
}

// Flushes a folder's list of files to the card, so a file just put in place stays there.
static void syncFolder(const char* path) {
	char folder[FILE_SIZE];
	snprintf(folder, sizeof(folder), "%s", path);
	char* const slash = strrchr(folder, '/');
	if (!slash) {
		return;
	}
	*slash = '\0';
	int const fd = open(*folder ? folder : "/", O_RDONLY);
	if (fd >= 0) {
		fsync(fd);
		close(fd);
	}
}

// The new file goes beside the old one first, flushed to the card, and then takes its place in one
// step; unless the core has a Safe Saves option (<id>_safe_saves) and it's off.
bool writeFile(const char* path, const void* data, size_t size) {
	struct Option* const option = optionFind(APP_ID "_safe_saves");
	bool const safe = !option || strcmp(option->value, "disabled");
	char temporary[FILE_SIZE + 8];
	bool const fits = snprintf(temporary, sizeof(temporary), "%s.tmp", path) < (int) sizeof(temporary);
	FILE* f = !safe || fits ? fopen(safe ? temporary : path, "wb") : NULL;
	bool written = f && fwrite(data, 1, size, f) == size && fflush(f) == 0 && (!safe || fsync(fileno(f)) == 0);
	if (f) {
		written &= fclose(f) == 0;
	}
	if (safe && written && rename(temporary, path) == 0) {
		syncFolder(path);
	} else if (safe) {
		written = false;
		if (fits) {
			remove(temporary);
		}
	}
	if (!written) {
		osd(APP_NAME " can't write %.120s", path);
	}
	return written;
}

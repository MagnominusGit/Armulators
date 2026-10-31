/* Copyright (c) 2026 Magnominus
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Licensed under the GNU General Public License, version 2 or (at your option) any later version: see LICENSE. */
#ifndef ARMULATORS_WORKERS_H
#define ARMULATORS_WORKERS_H

// Threads that run a job for each of a few indices at once: index 0 on the caller's thread, the
// others on threads of their own, which wait between runs spinning briefly, then asleep.

#ifdef __cplusplus
extern "C" {
#endif

// Starts threads for indices 1 to count - 1, as many as there are other CPU cores for.
void workersStart(int count);
void workersStop(void);
// The count workersStart was last asked for, 0 once stopped.
int workersAsked(void);
// Runs job(i) for each index below count, and returns when all have finished.
void workersRun(void (*job)(int index), int count);

#ifdef __cplusplus
}
#endif

#endif

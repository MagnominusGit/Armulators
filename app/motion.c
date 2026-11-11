/* Copyright (c) 2026 Magnominus
 * SPDX-License-Identifier: GPL-2.0-or-later */

// The device's motion sensors, Android's through SDL, for the core and for the emulator's own use
// (frontend.h).

#include "frontend.h"

#ifdef __ANDROID__
#include <jni.h>
#endif

static SDL_Sensor* sensors[MOTION_SENSORS];
static int found[MOTION_SENSORS] = { -1, -1 };
static unsigned users[MOTION_SENSORS];

bool motionAvailable(int sensor) {
#ifdef __ANDROID__
	static bool looked;
	if (!looked) {
		looked = true;
		if (SDL_InitSubSystem(SDL_INIT_SENSOR) == 0) {
			// Their events are ignored, which stops SDL updating them, so motionRead updates them.
			SDL_EventState(SDL_SENSORUPDATE, SDL_IGNORE);
			for (int i = 0; i < SDL_NumSensors(); ++i) {
				SDL_SensorType const type = SDL_SensorGetDeviceType(i);
				if (type == SDL_SENSOR_ACCEL && found[MOTION_ACCEL] < 0) {
					found[MOTION_ACCEL] = i;
				} else if (type == SDL_SENSOR_GYRO && found[MOTION_GYRO] < 0) {
					found[MOTION_GYRO] = i;
				}
			}
		}
	}
#endif
	return found[sensor] >= 0;
}

// While the core reads a sensor, the screen stays turned as it is (Android's lockOrientation in the
// app's activity).
static void lockScreen(bool lock) {
#ifdef __ANDROID__
	JNIEnv* const env = SDL_AndroidGetJNIEnv();
	jclass const activity = env ? (*env)->FindClass(env, APP_JAVA_PATH "/" APP_JAVA_CLASS) : NULL;
	jmethodID const method = activity ? (*env)->GetStaticMethodID(env, activity, "lockOrientation", "(Z)V") : NULL;
	if (method) {
		(*env)->CallStaticVoidMethod(env, activity, method, (jboolean) lock);
	}
	if (env && (*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
	}
	if (activity) {
		(*env)->DeleteLocalRef(env, activity);
	}
#else
	(void) lock;
#endif
}

bool motionWant(int sensor, unsigned user, bool on) {
	bool const locked = (users[MOTION_ACCEL] | users[MOTION_GYRO]) & MOTION_FOR_CORE;
	users[sensor] = on ? users[sensor] | user : users[sensor] & ~user;
	if (locked != (bool) ((users[MOTION_ACCEL] | users[MOTION_GYRO]) & MOTION_FOR_CORE)) {
		lockScreen(!locked);
	}
	if (!users[sensor]) {
		if (sensors[sensor]) {
			SDL_SensorClose(sensors[sensor]);
			sensors[sensor] = NULL;
		}
		return true;
	}
	if (!sensors[sensor] && motionAvailable(sensor)) {
		sensors[sensor] = SDL_SensorOpen(found[sensor]);
	}
	return sensors[sensor] != NULL;
}

bool motionRead(int sensor, float out[3]) {
	float v[3];
	if (!sensors[sensor]) {
		return false;
	}
	SDL_SensorUpdate();
	if (SDL_SensorGetData(sensors[sensor], v, 3) != 0) {
		return false;
	}
	// The device's own axes are x right and y up as it's naturally held; the screen's turn with the
	// display (SDL names Android's rotations 0, 90, 180 and 270 portrait, landscape, portrait flipped,
	// landscape flipped).
	switch (SDL_GetDisplayOrientation(0)) {
	case SDL_ORIENTATION_LANDSCAPE:
		out[0] = -v[1], out[1] = v[0];
		break;
	case SDL_ORIENTATION_PORTRAIT_FLIPPED:
		out[0] = -v[0], out[1] = -v[1];
		break;
	case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
		out[0] = v[1], out[1] = -v[0];
		break;
	default:
		out[0] = v[0], out[1] = v[1];
		break;
	}
	out[2] = v[2];
	return true;
}

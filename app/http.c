/* Copyright (c) 2026 Magnominus
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Licensed under the GNU General Public License, version 2 or (at your option) any later version: see LICENSE. */

// Downloads to a file, for what a core's app fetches for the player (its hooks), apart from
// RetroAchievements' requests (cheevos.c): with the system's libcurl, loaded when first needed, or on
// Android with the app's Java helper (Http.download), the file written as it arrives. httpPrepare
// runs on the main thread; httpDownload on a worker thread of the caller's.

#include "frontend.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#ifdef __ANDROID__
#include <jni.h>
#endif

static char userAgent[128];

#ifndef __ANDROID__

// The few parts of libcurl's interface used here, as its curl.h numbers them.
enum {
	CURLOPT_WRITEDATA = 10001,
	CURLOPT_URL = 10002,
	CURLOPT_USERAGENT = 10018,
	CURLOPT_WRITEFUNCTION = 20011,
	CURLOPT_LOW_SPEED_LIMIT = 19,
	CURLOPT_LOW_SPEED_TIME = 20,
	CURLOPT_FOLLOWLOCATION = 52,
	CURLOPT_CONNECTTIMEOUT = 78,
	CURLOPT_NOSIGNAL = 99,
	CURLINFO_RESPONSE_CODE = 0x200002,
	CURL_GLOBAL_DEFAULT = 3,
};

static struct {
	void* library;
	int (*globalInit)(long flags);
	void* (*init)(void);
	int (*setopt)(void* curl, int option, ...);
	int (*perform)(void* curl);
	int (*getinfo)(void* curl, int info, ...);
	void (*cleanup)(void* curl);
} curl;

static bool load(void) {
	static bool tried;
	if (curl.library || tried) {
		return curl.library != NULL;
	}
	tried = true;
	static const char* const names[] = { "libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.so" };
	for (size_t i = 0; i < sizeof(names) / sizeof(*names) && !curl.library; ++i) {
		curl.library = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
	}
	if (!curl.library) {
		return false;
	}
	curl.globalInit = (int (*)(long)) dlsym(curl.library, "curl_global_init");
	curl.init = (void* (*)(void)) dlsym(curl.library, "curl_easy_init");
	curl.setopt = (int (*)(void*, int, ...)) dlsym(curl.library, "curl_easy_setopt");
	curl.perform = (int (*)(void*)) dlsym(curl.library, "curl_easy_perform");
	curl.getinfo = (int (*)(void*, int, ...)) dlsym(curl.library, "curl_easy_getinfo");
	curl.cleanup = (void (*)(void*)) dlsym(curl.library, "curl_easy_cleanup");
	if (!curl.globalInit || !curl.init || !curl.setopt || !curl.perform || !curl.getinfo || !curl.cleanup) {
		dlclose(curl.library);
		curl.library = NULL;
		return false;
	}
	curl.globalInit(CURL_GLOBAL_DEFAULT);
	return true;
}

static size_t written(char* data, size_t size, size_t count, void* file) {
	return fwrite(data, 1, size * count, file);
}

int httpDownload(const char* url, const char* path) {
	if (!curl.library) {
		return -1;
	}
	FILE* const f = fopen(path, "wb");
	if (!f) {
		return 0;
	}
	void* const handle = curl.init();
	long code = 0;
	int failed = 1;
	if (handle) {
		curl.setopt(handle, CURLOPT_URL, url);
		curl.setopt(handle, CURLOPT_USERAGENT, userAgent);
		curl.setopt(handle, CURLOPT_WRITEFUNCTION, written);
		curl.setopt(handle, CURLOPT_WRITEDATA, f);
		curl.setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
		curl.setopt(handle, CURLOPT_NOSIGNAL, 1L);
		curl.setopt(handle, CURLOPT_CONNECTTIMEOUT, 30L);
		// Given up on when under a byte a second for a minute
		curl.setopt(handle, CURLOPT_LOW_SPEED_LIMIT, 1L);
		curl.setopt(handle, CURLOPT_LOW_SPEED_TIME, 60L);
		failed = curl.perform(handle);
		curl.getinfo(handle, CURLINFO_RESPONSE_CODE, &code);
		curl.cleanup(handle);
	}
	bool const closed = !fclose(f);
	return !failed && closed ? (int) code : 0;
}

#else

// The app's Java helper (android/java/Http.java.in), found on the main thread, as a worker's thread
// can't find the app's classes by name.
static jclass http;
static jmethodID httpGet;

static bool load(void) {
	if (http) {
		return true;
	}
	JNIEnv* const env = SDL_AndroidGetJNIEnv();
	jclass const found = env ? (*env)->FindClass(env, APP_JAVA_PATH "/Http") : NULL;
	if (!found) {
		if (env && (*env)->ExceptionCheck(env)) {
			(*env)->ExceptionClear(env);
		}
		return false;
	}
	http = (*env)->NewGlobalRef(env, found);
	(*env)->DeleteLocalRef(env, found);
	httpGet = (*env)->GetStaticMethodID(env, http, "download", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[I)Z");
	if (!httpGet && (*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
	}
	return httpGet != NULL;
}

int httpDownload(const char* url, const char* path) {
	if (!httpGet) {
		return -1;
	}
	JNIEnv* const env = SDL_AndroidGetJNIEnv();
	if (!env) {
		return 0;
	}
	jstring const jurl = (*env)->NewStringUTF(env, url);
	jstring const jpath = (*env)->NewStringUTF(env, path);
	jstring const agent = (*env)->NewStringUTF(env, userAgent);
	jintArray const code = (*env)->NewIntArray(env, 1);
	jboolean ok = (*env)->CallStaticBooleanMethod(env, http, httpGet, jurl, jpath, agent, code);
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		ok = JNI_FALSE;
	}
	jint status = 0;
	(*env)->GetIntArrayRegion(env, code, 0, 1, &status);
	(*env)->DeleteLocalRef(env, jurl);
	(*env)->DeleteLocalRef(env, jpath);
	(*env)->DeleteLocalRef(env, agent);
	(*env)->DeleteLocalRef(env, code);
	return ok ? (int) status : 0;
}

#endif

bool httpPrepare(void) {
	if (!userAgent[0]) {
		struct retro_system_info info;
		retro_get_system_info(&info);
		snprintf(userAgent, sizeof(userAgent), APP_NAME "/%s", info.library_version);
	}
	return load();
}

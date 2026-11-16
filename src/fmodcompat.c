// FMOD Studio 1.10 -> 2.x compatibility shim for Celeste (arm64 macOS).
//
// Celeste.exe carries C# bindings for FMOD Studio 1.10.20, which was never
// released for Apple Silicon. The launcher maps the "fmodstudio" library to
// this shim. The shim re-exports the real FMOD Studio 2.x library that sits
// next to it, so every function whose name and signature are unchanged
// resolves straight through, and it overrides the few 1.10 entry points
// Celeste uses that were renamed, removed, or changed signature in 2.x.
//
// This file contains no FMOD code and needs no FMOD headers. The FMOD version
// to announce to FMOD_Studio_System_Create is read from fmod-version.txt next
// to the shim (written by the converter from the user's FMOD download).
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPORT __attribute__((visibility("default")))

typedef int FMOD_RESULT; // FMOD_OK == 0
typedef int FMOD_BOOL;
typedef struct { float x, y, z; } FMOD_VECTOR;
typedef struct { FMOD_VECTOR position, velocity, forward, up; } FMOD_3D_ATTRIBUTES;

static void *real_studio;
static char shim_dir[PATH_MAX];

static void locate(void) {
    if (shim_dir[0]) return;
    Dl_info info;
    char tmp[PATH_MAX];
    if (!dladdr((void *)&locate, &info) || !info.dli_fname) {
        fprintf(stderr, "[fmodcompat] cannot locate the shim on disk\n");
        abort();
    }
    snprintf(tmp, sizeof(tmp), "%s", info.dli_fname);
    snprintf(shim_dir, sizeof(shim_dir), "%s", dirname(tmp));
}

static void *real(const char *name) {
    if (!real_studio) {
        char path[PATH_MAX];
        locate();
        snprintf(path, sizeof(path), "%s/libfmodstudio.dylib", shim_dir);
        real_studio = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!real_studio) {
            fprintf(stderr, "[fmodcompat] cannot load %s: %s\n", path, dlerror());
            abort();
        }
    }
    return dlsym(real_studio, name);
}

static void *require(const char *name) {
    void *fn = real(name);
    if (!fn) {
        fprintf(stderr, "[fmodcompat] FMOD Studio library has no %s\n", name);
        abort();
    }
    return fn;
}

static unsigned int fmod_version(void) {
    char path[PATH_MAX], line[64];
    unsigned int version = 0;
    locate();
    snprintf(path, sizeof(path), "%s/fmod-version.txt", shim_dir);
    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(line, sizeof(line), f)) version = (unsigned int)strtoul(line, NULL, 16);
        fclose(f);
    }
    if (!version) {
        fprintf(stderr, "[fmodcompat] cannot read the FMOD version from %s\n", path);
        abort();
    }
    return version;
}

// 1.10 passes its own header version (0x00011014), which 2.x rejects.
EXPORT FMOD_RESULT FMOD_Studio_System_Create(void **system, unsigned int headerversion) {
    (void)headerversion;
    static FMOD_RESULT (*fn)(void **, unsigned int);
    if (!fn) fn = require("FMOD_Studio_System_Create");
    FMOD_RESULT result = fn(system, fmod_version());
    if (result != 0) {
        fprintf(stderr, "[fmodcompat] FMOD_Studio_System_Create failed: %d\n", result);
    }
    return result;
}

// Renamed to GetCoreSystem in 2.00.
EXPORT FMOD_RESULT FMOD_Studio_System_GetLowLevelSystem(void *system, void **core) {
    static FMOD_RESULT (*fn)(void *, void **);
    if (!fn) fn = require("FMOD_Studio_System_GetCoreSystem");
    return fn(system, core);
}

// 2.x added a fourth argument (attenuation position); NULL keeps 1.10 behaviour.
EXPORT FMOD_RESULT FMOD_Studio_System_SetListenerAttributes(void *system, int listener,
                                                            const FMOD_3D_ATTRIBUTES *attributes) {
    static FMOD_RESULT (*fn)(void *, int, const FMOD_3D_ATTRIBUTES *, const FMOD_VECTOR *);
    if (!fn) fn = require("FMOD_Studio_System_SetListenerAttributes");
    return fn(system, listener, attributes, NULL);
}

EXPORT FMOD_RESULT FMOD_Studio_System_GetListenerAttributes(void *system, int listener,
                                                            FMOD_3D_ATTRIBUTES *attributes) {
    static FMOD_RESULT (*fn)(void *, int, FMOD_3D_ATTRIBUTES *, FMOD_VECTOR *);
    if (!fn) fn = require("FMOD_Studio_System_GetListenerAttributes");
    return fn(system, listener, attributes, NULL);
}

// Parameter access by name was renamed and gained an "ignore seek speed" flag.
EXPORT FMOD_RESULT FMOD_Studio_EventInstance_SetParameterValue(void *event, const char *name,
                                                               float value) {
    static FMOD_RESULT (*fn)(void *, const char *, float, FMOD_BOOL);
    if (!fn) fn = require("FMOD_Studio_EventInstance_SetParameterByName");
    return fn(event, name, value, 0);
}

EXPORT FMOD_RESULT FMOD_Studio_EventInstance_GetParameterValue(void *event, const char *name,
                                                               float *value, float *finalvalue) {
    static FMOD_RESULT (*fn)(void *, const char *, float *, float *);
    if (!fn) fn = require("FMOD_Studio_EventInstance_GetParameterByName");
    return fn(event, name, value, finalvalue);
}

// Cues became sustain points / key off in 2.02; 2.00 and 2.01 still have TriggerCue.
EXPORT FMOD_RESULT FMOD_Studio_EventInstance_TriggerCue(void *event) {
    static FMOD_RESULT (*fn)(void *);
    if (!fn) fn = real("FMOD_Studio_EventInstance_KeyOff");
    if (!fn) fn = require("FMOD_Studio_EventInstance_TriggerCue");
    return fn(event);
}

EXPORT FMOD_RESULT FMOD_Studio_EventDescription_HasCue(void *description, FMOD_BOOL *cue) {
    static FMOD_RESULT (*fn)(void *, FMOD_BOOL *);
    if (!fn) fn = real("FMOD_Studio_EventDescription_HasSustainPoint");
    if (!fn) fn = real("FMOD_Studio_EventDescription_HasSustainPoints");
    if (!fn) fn = require("FMOD_Studio_EventDescription_HasCue");
    return fn(description, cue);
}

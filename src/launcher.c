// Native arm64 launcher for Celeste: hosts the Mono runtime bundled in
// Contents/MacOS/osx and runs Contents/Resources/Celeste.exe.
#include <limits.h>
#include <mach-o/dyld.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct _MonoDomain MonoDomain;
typedef struct _MonoAssembly MonoAssembly;
extern void mono_set_dirs(const char *assembly_dir, const char *config_dir);
extern void mono_set_assemblies_path(const char *path);
extern void mono_config_parse_memory(const char *buffer);
extern MonoDomain *mono_jit_init_version(const char *name, const char *version);
extern MonoAssembly *mono_domain_assembly_open(MonoDomain *domain, const char *name);
extern int mono_jit_exec(MonoDomain *domain, MonoAssembly *assembly, int argc, char *argv[]);
extern int mono_environment_exitcode_get(void);
extern void mono_jit_cleanup(MonoDomain *domain);

// Managed library name -> file in Contents/MacOS/osx.
static const char *const DLLMAP[][2] = {
    {"SDL3", "libSDL3.0.dylib"},
    {"FNA3D", "libFNA3D.0.dylib"},
    {"FAudio", "libFAudio.0.dylib"},
    {"libtheorafile", "libtheorafile.dylib"},
    {"fmod", "libfmod.dylib"},
    {"fmodstudio", "libfmodcompat.dylib"},
    {"steam_api", "libsteam_api.dylib"},
    {"MonoPosixHelper", "libMonoPosixHelper.dylib"},
    {"System.Native", "libmono-native.0.dylib"},
    {"System.Net.Security.Native", "libmono-native.0.dylib"},
    {"System.Security.Cryptography.Native.Apple", "libmono-native.0.dylib"},
};

int main(int argc, char *argv[]) {
    char raw[PATH_MAX], exe[PATH_MAX];
    uint32_t size = sizeof(raw);
    if (_NSGetExecutablePath(raw, &size) != 0 || !realpath(raw, exe)) {
        fprintf(stderr, "Celeste launcher: cannot resolve executable path\n");
        return 1;
    }
    char macos[PATH_MAX], libdir[PATH_MAX], resources[PATH_MAX], etc[PATH_MAX];
    snprintf(macos, sizeof(macos), "%s", dirname(exe));
    snprintf(libdir, sizeof(libdir), "%s/osx", macos);
    snprintf(resources, sizeof(resources), "%s/../Resources", macos);
    if (!realpath(resources, raw)) {
        perror("Celeste launcher: Resources folder");
        return 1;
    }
    snprintf(resources, sizeof(resources), "%s", raw);
    snprintf(etc, sizeof(etc), "%s/mono-etc", resources);

    if (chdir(resources) != 0) {
        perror("Celeste launcher: chdir to Resources");
        return 1;
    }

    // Native library mapping with absolute paths into the bundle.
    size_t cap = 4096 + sizeof(DLLMAP) / sizeof(DLLMAP[0]) * (PATH_MAX + 128);
    char *xml = malloc(cap);
    size_t off = (size_t)snprintf(xml, cap, "<configuration>\n");
    for (size_t i = 0; i < sizeof(DLLMAP) / sizeof(DLLMAP[0]); i++) {
        off += (size_t)snprintf(xml + off, cap - off,
                                "<dllmap dll=\"%s\" target=\"%s/%s\"/>\n",
                                DLLMAP[i][0], libdir, DLLMAP[i][1]);
    }
    snprintf(xml + off, cap - off,
             "<dllmap dll=\"libc\" target=\"libc.dylib\"/>\n"
             "<dllmap dll=\"i:msvcrt\" target=\"libc.dylib\"/>\n"
             "<dllmap dll=\"i:msvcrt.dll\" target=\"libc.dylib\"/>\n"
             "</configuration>\n");

    mono_set_dirs(libdir, etc);
    mono_set_assemblies_path(resources);
    mono_config_parse_memory(xml);
    free(xml);

    MonoDomain *domain = mono_jit_init_version("Celeste", "v4.0.30319");
    if (!domain) {
        fprintf(stderr, "Celeste launcher: failed to start the Mono runtime\n");
        return 1;
    }
    char game[PATH_MAX];
    snprintf(game, sizeof(game), "%s/Celeste.exe", resources);
    MonoAssembly *assembly = mono_domain_assembly_open(domain, game);
    if (!assembly) {
        fprintf(stderr, "Celeste launcher: cannot open %s\n", game);
        return 1;
    }

    char **margv = calloc((size_t)argc + 1, sizeof(char *));
    int n = 0;
    margv[n++] = game;
    for (int i = 1; i < argc; i++) {
        // Finder may pass -psn_... when launching by double click.
        if (strncmp(argv[i], "-psn_", 5) == 0) continue;
        margv[n++] = argv[i];
    }
    mono_jit_exec(domain, assembly, n, margv);
    int code = mono_environment_exitcode_get();
    mono_jit_cleanup(domain);
    return code;
}

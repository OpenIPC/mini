#include "sensor.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <dlfcn.h>
#include <inttypes.h>

int (*sensor_register_callback_fn)(void);
int (*sensor_unregister_callback_fn)(void);
void *libsns_so = NULL;

int tryLoadLibrary(const char *path) {
    printf("try to load: %s\n", path);
    libsns_so = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    printf("libsns_so 0x%016" PRIXPTR "\n", (uintptr_t)libsns_so);
    if (libsns_so == NULL) {
        printf("dlopen \"%s\" error: %s\n", path, dlerror());
        return 0;
    }
    return 1;
}
int LoadSensorLibrary(const char *libsns_name) {
    char path[250];
    sprintf(path, "%s", libsns_name);
    if (tryLoadLibrary(path) != 1) {
        sprintf(path, "./%s", libsns_name);
        if (tryLoadLibrary(path) != 1) {
            sprintf(path, "/usr/lib/%s", libsns_name);
            if (tryLoadLibrary(path) != 1) {
                return 0;
            }
        }
    }
    sensor_register_callback_fn = dlsym(libsns_so, "sensor_register_callback");
    sensor_unregister_callback_fn =
        dlsym(libsns_so, "sensor_unregister_callback");
    return 1;
}

void UnloadSensorLibrary() {
    dlclose(libsns_so);
    libsns_so = NULL;
}

int sensor_register_callback(void) { return sensor_register_callback_fn(); }
int sensor_unregister_callback(void) { return sensor_unregister_callback_fn(); }

#pragma once

extern int (*sensor_register_callback_fn)(void);
extern int (*sensor_unregister_callback_fn)(void);
#if HISILICON_SDK_GEN < 2
extern void (*sensor_init_fn)(void);
#endif
extern void *libsns_so;

int tryLoadLibrary(const char *path);
int LoadSensorLibrary(const char *libsns_name);
void UnloadSensorLibrary();

int sensor_register_callback(void);
int sensor_unregister_callback(void);

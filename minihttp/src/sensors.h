#pragma once

#include <hi_comm_isp.h>
#include <hi_mipi.h>

extern int (*sensor_register_callback_fn)(void);
extern int (*sensor_unregister_callback_fn)(void);
extern void *libsns_so;

int tryLoadLibrary(const char *path);
int LoadSensorLibrary(const char *libsns_name);
void UnloadSensorLibrary();

int sensor_register_callback(void);
int sensor_unregister_callback(void);


struct SensorConfig {
    char type[128];
    char so_path[256];
    WDR_MODE_E mode;
    input_mode_t input_mode;
    int dev_attr;

    raw_data_type_e data_type;

    int mask_num;
    int mask_0;
    int mask_1;

    ISP_BAYER_FORMAT_E isp_bayer;
    unsigned int    width;
    unsigned int    height;
    double          frame_rate;
};

int parse_sensor_config(const char *path, struct SensorConfig *config);

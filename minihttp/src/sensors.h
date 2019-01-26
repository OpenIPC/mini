#pragma once

#ifdef __cplusplus
extern "C" {
#endif
    #include <hi_comm_isp.h>
    #include <hi_mipi.h>
    #include <hi_comm_vi.h>
#ifdef __cplusplus
}
#endif

extern int (*sensor_register_callback_fn)(void);
extern int (*sensor_unregister_callback_fn)(void);
extern void *libsns_so;

int tryLoadLibrary(const char *path);
int LoadSensorLibrary(const char *libsns_name);
void UnloadSensorLibrary();

int sensor_register_callback(void);
int sensor_unregister_callback(void);

struct SensorMIPI {
    raw_data_type_e data_type;
    int lane_id[8];
};

struct SensorLVDS {
    int img_size_w;
    int img_size_h;
    wdr_mode_e wdr_mode;
    lvds_sync_mode_e sync_mode;
    raw_data_type_e raw_data_type;
    lvds_bit_endian data_endian;
    lvds_bit_endian sync_code_endian;
    int lane_id[8];
    int lvds_lane_num;
    int wdr_vc_num;
    int sync_code_num;
    int sync_code_0[16];
    int sync_code_1[16];
    int sync_code_2[16];
    int sync_code_3[16];
    int sync_code_4[16];
    int sync_code_5[16];
    int sync_code_6[16];
    int sync_code_7[16];
};

struct SensorVIDEV {
    VI_INPUT_MODE_E input_mod;
    VI_WORK_MODE_E work_mod;
    VI_COMBINE_MODE_E combine_mode;
    VI_COMP_MODE_E comp_mode;
    VI_CLK_EDGE_E clock_edge;
    unsigned int mask_num;
    unsigned int mask_0;
    unsigned int mask_1;
    VI_SCAN_MODE_E scan_mode;
    VI_DATA_YUV_SEQ_E data_seq;
    VI_VSYNC_E vsync;
    VI_VSYNC_NEG_E vsync_neg;
    VI_HSYNC_E hsync;
    VI_HSYNC_NEG_E hsync_neg;
    VI_VSYNC_VALID_E vsync_valid;
    VI_VSYNC_VALID_NEG_E vsync_valid_neg;
    unsigned int timing_blank_hsync_hfb;
    unsigned int timing_blank_hsync_act;
    unsigned int timing_blank_hsync_hbb;
    unsigned int timing_blank_vsync_vfb;
    unsigned int timing_blank_vsync_vact;
    unsigned int timing_blank_vsync_vbb;
    unsigned int timing_blank_vsync_vbfb;
    unsigned int timing_blank_vsync_vbact;
    unsigned int timing_blank_vsync_vbbb;

    BT656_FIXCODE_E fix_code;
    BT656_FIELD_POLAR_E field_polar;
    VI_DATA_PATH_E data_path;
    VI_DATA_TYPE_E input_data_type;
    int data_rev;
    int dev_rect_x;
    int dev_rect_y;
    unsigned int dev_rect_w;
    unsigned int dev_rect_h;
};

struct SensorConfig {
    // [sensor]
    char sensor_type[128];
    WDR_MODE_E mode;
    char dll_file[256];

    // [mode]
    input_mode_t input_mode;
    int dev_attr;

    // [mipi]
    struct SensorMIPI mipi;

    // [lvds]
    struct SensorLVDS lvds;

    // [isp_image]
    int isp_x;
    int isp_y;
    unsigned int isp_h;
    unsigned int isp_w;
    unsigned int isp_frame_rate;
    ISP_BAYER_FORMAT_E isp_bayer;

    // [vi_dev]
    struct SensorVIDEV videv;

    // [vi_chn]
    int cap_rect_x;
    int cap_rect_y;
    int cap_rect_width;
    int cap_rect_height;
    int dest_size_width;
    int dest_size_height;
    VI_CAPSEL_E cap_sel;
    PIXEL_FORMAT_E pix_format;
    COMPRESS_MODE_E compress_mode;
    int src_frame_rate;
    int frame_rate;
};

int parse_sensor_config(const char *path, struct SensorConfig *config);

#include "sensor_config.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools.h"

struct SensorConfig sensor_config;

enum ConfigError read_sensor_from_proc_cmdline(char *sensor_type) {
    FILE *file = fopen("/proc/cmdline", "r");
    if (!file)
        return CONFIG_CANT_OPEN_PROC_CMDLINE;
    char cmdline[5 * 1024];
    fread(cmdline, 1, 5 * 1024, file);
    fclose(file);

    regex_t regex;
    if (compile_regex(
            &regex, "[[:space:]]*sensor=([[:alnum:]_]+)[[:space:]]*") < 0) {
        printf("compile_regex error\n");
        return CONFIG_REGEX_ERROR;
    };
    size_t n_matches = 2; // We have 1 capturing group + the whole match group
    regmatch_t m[n_matches];
    int match = regexec(&regex, cmdline, n_matches, m, 0);
    if (match != 0) {
        regfree(&regex);
        printf("Can't parse sensor=xxxx from cmdline: \n'%s'\n", cmdline);
        return CONFIG_REGEX_ERROR;
    }
    sprintf(
        sensor_type, "%.*s", (int)(m[1].rm_eo - m[1].rm_so),
        cmdline + m[1].rm_so);
    return CONFIG_OK;
}

#if HISILICON_SDK_GEN >= 2
enum ConfigError parse_config_lvds(
    struct IniConfig *ini, const char *section, struct SensorLVDS *lvds) {
    enum ConfigError err;
    err = parse_int(
        ini, section, "img_size_w", INT_MIN, INT_MAX, &lvds->img_size_w);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "img_size_h", INT_MIN, INT_MAX, &lvds->img_size_h);
    if (err != CONFIG_OK)
        return err;
    {
        const char *possible_values[] = {
            "HI_WDR_MODE_NONE",  "HI_WDR_MODE_2F",     "HI_WDR_MODE_3F",
            "HI_WDR_MODE_4F",    "HI_WDR_MODE_DOL_2F", "HI_WDR_MODE_DOL_3F",
            "HI_WDR_MODE_DOL_4F"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "wdr_mode", (void *)&lvds->wdr_mode, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "LVDS_SYNC_MODE_SOL", "LVDS_SYNC_MODE_SAV"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "sync_mode", (void *)&lvds->sync_mode,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "RAW_DATA_8BIT", "RAW_DATA_10BIT", "RAW_DATA_12BIT",
            "RAW_DATA_14BIT"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "raw_data_type", (void *)&lvds->raw_data_type,
            possible_values, count, 1);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "LVDS_ENDIAN_LITTLE", "LVDS_ENDIAN_BIG"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "data_endian", (void *)&lvds->data_endian,
            possible_values, count, 1);
        if (err != CONFIG_OK)
            return err;
        err = parse_enum(
            ini, section, "sync_code_endian", (void *)&lvds->sync_code_endian,
            possible_values, count, 1);
        if (err != CONFIG_OK)
            return err;
    }
    err = parse_array(ini, section, "lane_id", lvds->lane_id, 8);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "lvds_lane_num", INT_MIN, INT_MAX, &lvds->lvds_lane_num);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "wdr_vc_num", INT_MIN, INT_MAX, &lvds->wdr_vc_num);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "sync_code_num", INT_MIN, INT_MAX, &lvds->sync_code_num);
    if (err != CONFIG_OK)
        return err;
    char syncname[16];
    for (int i = 0; i < 8; i++) {
        sprintf(syncname, "sync_code_%d", i);
        err = parse_array(ini, section, syncname, lvds->sync_code[i], 16);
        if (err != CONFIG_OK)
            return err;
    }
    return CONFIG_OK;
}
#endif

enum ConfigError parse_config_videv(
    struct IniConfig *ini, const char *section, struct SensorVIDEV *videv) {
    enum ConfigError err;
    {
        const char *possible_values[] = {
            "VI_INPUT_MODE_BT656",          "VI_INPUT_MODE_BT601",
            "VI_INPUT_MODE_DIGITAL_CAMERA", "VI_INPUT_MODE_INTERLEAVED",
            "VI_INPUT_MODE_MIPI",           "VI_INPUT_MODE_LVDS",
            "VI_INPUT_MODE_HISPI"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "Input_mod", (void *)&videv->input_mod,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_WORK_MODE_1Multiplex", "VI_WORK_MODE_2Multiplex",
            "VI_WORK_MODE_4Multiplex"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "Work_mod", (void *)&videv->work_mod, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_COMBINE_COMPOSITE", "VI_COMBINE_SEPARATE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "Combine_mode", (void *)&videv->combine_mode,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_COMP_MODE_SINGLE", "VI_COMP_MODE_DOUBLE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "Comp_mode", (void *)&videv->comp_mode,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_CLK_EDGE_SINGLE_UP", "VI_CLK_EDGE_SINGLE_DOWN",
            "VI_CLK_EDGE_DOUBLE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "Clock_edge", (void *)&videv->clock_edge,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    err = parse_uint32(ini, section, "Mask_num", 0, 2, &videv->mask_num);
    if (err != CONFIG_OK)
        return err;
    err = parse_uint32(ini, section, "Mask_0", 0, UINT_MAX, &videv->mask_0);
    if (err != CONFIG_OK)
        return err;
    err = parse_uint32(ini, section, "Mask_1", 0, UINT_MAX, &videv->mask_1);
    if (err != CONFIG_OK)
        return err;
    {
        const char *possible_values[] = {
            "VI_SCAN_INTERLACED", "VI_SCAN_PROGRESSIVE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "Scan_mode", (void *)&videv->scan_mode,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_INPUT_DATA_VUVU", "VI_INPUT_DATA_UVUV"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "Data_seq", (void *)&videv->data_seq, possible_values,
            count, 0);
        if (err != CONFIG_OK) {
            const char *possible_values[] = {
                "VI_INPUT_DATA_UYVY", "VI_INPUT_DATA_VYUY",
                "VI_INPUT_DATA_YUYV", "VI_INPUT_DATA_YVYU"};
            const int count = sizeof(possible_values) / sizeof(const char *);
            err = parse_enum(
                ini, section, "Data_seq", (void *)&videv->data_seq,
                possible_values, count, 0);
            if (err != CONFIG_OK)
                return err;
        }
    }
    {
        const char *possible_values[] = {"VI_VSYNC_FIELD", "VI_VSYNC_PULSE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "Vsync", (void *)&videv->vsync, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_VSYNC_NEG_HIGH", "VI_VSYNC_NEG_LOW"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "VsyncNeg", (void *)&videv->vsync_neg,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_HSYNC_VALID_SINGNAL", "VI_HSYNC_PULSE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "Hsync", (void *)&videv->hsync, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_HSYNC_NEG_HIGH", "VI_HSYNC_NEG_LOW"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "HsyncNeg", (void *)&videv->hsync_neg,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_VSYNC_NORM_PULSE", "VI_VSYNC_VALID_SINGAL"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "VsyncValid", (void *)&videv->vsync_valid,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_VSYNC_VALID_NEG_HIGH", "VI_VSYNC_VALID_NEG_LOW"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "VsyncValidNeg", (void *)&videv->vsync_valid_neg,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    err = parse_int(
        ini, section, "Timingblank_HsyncHfb", 0, INT_MAX,
        &videv->timing_blank_hsync_hfb);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "Timingblank_HsyncAct", 0, INT_MAX,
        &videv->timing_blank_hsync_act);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "Timingblank_HsyncHbb", 0, INT_MAX,
        &videv->timing_blank_hsync_hbb);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "Timingblank_VsyncVfb", 0, INT_MAX,
        &videv->timing_blank_vsync_vfb);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "Timingblank_VsyncVact", 0, INT_MAX,
        &videv->timing_blank_vsync_vact);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "Timingblank_VsyncVbb", 0, INT_MAX,
        &videv->timing_blank_vsync_vbb);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "Timingblank_VsyncVbfb", 0, INT_MAX,
        &videv->timing_blank_vsync_vbfb);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "Timingblank_VsyncVbact", 0, INT_MAX,
        &videv->timing_blank_vsync_vbact);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "Timingblank_VsyncVbbb", 0, INT_MAX,
        &videv->timing_blank_vsync_vbbb);
    if (err != CONFIG_OK)
        return err;
    {
        const char *possible_values[] = {"BT656_FIXCODE_1", "BT656_FIXCODE_0"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "FixCode", (void *)&videv->fix_code, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "BT656_FIELD_POLAR_STD", "BT656_FIELD_POLAR_NSTD"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "FieldPolar", (void *)&videv->field_polar,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_PATH_BYPASS", "VI_PATH_ISP", "VI_PATH_RAW"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "DataPath", (void *)&videv->data_path,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_DATA_TYPE_YUV", "VI_DATA_TYPE_RGB"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "InputDataType", &videv->input_data_type,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    err = parse_int(ini, section, "DataRev", 0, INT_MAX, &videv->data_rev);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, section, "DevRect_x", 0, INT_MAX, &videv->dev_rect_x);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, section, "DevRect_y", 0, INT_MAX, &videv->dev_rect_y);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, section, "DevRect_w", 0, INT_MAX, &videv->dev_rect_w);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, section, "DevRect_h", 0, INT_MAX, &videv->dev_rect_h);
    if (err != CONFIG_OK)
        return err;
    return CONFIG_OK;
}

int parse_config_vichn(
    struct IniConfig *ini, const char *section, struct SensorVIChn *vichn) {
    enum ConfigError err;
    err = parse_int(ini, section, "CapRect_X", 0, INT_MAX, &vichn->cap_rect_x);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, section, "CapRect_Y", 0, INT_MAX, &vichn->cap_rect_y);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "CapRect_Width", 0, INT_MAX, &vichn->cap_rect_width);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "CapRect_Height", 0, INT_MAX, &vichn->cap_rect_height);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "DestSize_Width", 0, INT_MAX, &vichn->dest_size_width);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "DestSize_Height", 0, INT_MAX, &vichn->dest_size_height);
    if (err != CONFIG_OK)
        return err;
    {
        const char *possible_values[] = {
            "VI_CAPSEL_TOP", "VI_CAPSEL_BOTTOM", "VI_CAPSEL_BOTH"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "CapSel", (void *)&vichn->cap_sel, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "PIXEL_FORMAT_RGB_1BPP",
            "PIXEL_FORMAT_RGB_2BPP",
            "PIXEL_FORMAT_RGB_4BPP",
            "PIXEL_FORMAT_RGB_8BPP",
            "PIXEL_FORMAT_RGB_444",
            "PIXEL_FORMAT_RGB_4444",
            "PIXEL_FORMAT_RGB_555",
            "PIXEL_FORMAT_RGB_565",
            "PIXEL_FORMAT_RGB_1555",
            "PIXEL_FORMAT_RGB_888",
            "PIXEL_FORMAT_RGB_8888",
            "PIXEL_FORMAT_RGB_PLANAR_888",
            "PIXEL_FORMAT_RGB_BAYER_8BPP",
            "PIXEL_FORMAT_RGB_BAYER_10BPP",
            "PIXEL_FORMAT_RGB_BAYER_12BPP",
            "PIXEL_FORMAT_RGB_BAYER_14BPP",
            "PIXEL_FORMAT_RGB_BAYER",
            "PIXEL_FORMAT_YUV_A422",
            "PIXEL_FORMAT_YUV_A444",
            "PIXEL_FORMAT_YUV_PLANAR_422",
            "PIXEL_FORMAT_YUV_PLANAR_420",
            "PIXEL_FORMAT_YUV_PLANAR_444",
            "PIXEL_FORMAT_YUV_SEMIPLANAR_422",
            "PIXEL_FORMAT_YUV_SEMIPLANAR_420",
            "PIXEL_FORMAT_YUV_SEMIPLANAR_444",
            "PIXEL_FORMAT_UYVY_PACKAGE_422",
            "PIXEL_FORMAT_YUYV_PACKAGE_422",
            "PIXEL_FORMAT_VYUY_PACKAGE_422",
            "PIXEL_FORMAT_YCbCr_PLANAR",
            "PIXEL_FORMAT_YUV_400"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "PixFormat", (void *)&vichn->pix_format,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
#if HISILICON_SDK_GEN >= 2
    {
        const char *possible_values[] = {
            "COMPRESS_MODE_NONE", "COMPRESS_MODE_SEG", "COMPRESS_MODE_SEG128",
            "COMPRESS_MODE_LINE", "COMPRESS_MODE_FRAME"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "CompressMode", (void *)&vichn->compress_mode,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
#endif
    err = parse_int(
        ini, section, "SrcFrameRate", INT_MIN, INT_MAX, &vichn->src_frame_rate);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "FrameRate", INT_MIN, INT_MAX, &vichn->frame_rate);
    if (err != CONFIG_OK)
        return err;
    return CONFIG_OK;
}

enum ConfigError parse_config_isp(
    struct IniConfig *ini, const char *section, struct SensorISP *isp) {
    enum ConfigError err;
    err = parse_int(ini, "isp_image", "Isp_x", 0, INT_MAX, &isp->isp_x);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, "isp_image", "Isp_y", 0, INT_MAX, &isp->isp_y);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, "isp_image", "Isp_W", 0, INT_MAX, &isp->isp_w);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, "isp_image", "Isp_H", 0, INT_MAX, &isp->isp_h);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, "isp_image", "Isp_FrameRate", 0, INT_MAX, &isp->isp_frame_rate);
    if (err != CONFIG_OK)
        return err;
    {
        const char *possible_values[] = {
            "BAYER_RGGB", "BAYER_GRBG", "BAYER_GBRG", "BAYER_BGGR"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, "isp_image", "isp_bayer", (void *)&isp->isp_bayer,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    return CONFIG_OK;
}

enum ConfigError parse_sensor_config(char *path, struct SensorConfig *config) {
    if (config == NULL)
        return -1;
    memset(config, 0, sizeof(struct SensorConfig));
    struct IniConfig ini;
    memset(&ini, 0, sizeof(struct IniConfig));
    enum ConfigError err;

    // load config file to string
    ini.str = NULL;
    {
        FILE *file = fopen(path, "rb");
        if (!file) {
            printf("Can't open file %s\n", path);
            return -1;
        }

        fseek(file, 0, SEEK_END);
        size_t length = (size_t)ftell(file);
        fseek(file, 0, SEEK_SET);

        ini.str = malloc(length + 1);
        if (!ini.str) {
            printf("Can't allocate buf in parse_sensor_config\n");
            fclose(file);
            return -1;
        }
        size_t n = fread(ini.str, 1, length, file);
        if (n != length) {
            printf("Can't read all file %s\n", path);
            fclose(file);
            free(ini.str);
            return -1;
        }
        fclose(file);
        ini.str[length] = 0;
    }

    find_sections(&ini);

    // [sensor]
    err = parse_param_value(&ini, "sensor", "sensor_type", config->sensor_type);
    if (err != CONFIG_OK)
        goto RET_ERR;
#if HISILICON_SDK_GEN >= 2
    {
        const char *possible_values[] = {
            "WDR_MODE_NONE",
            "WDR_MODE_BUILT_IN",
            "WDR_MODE_2To1_LINE",
            "WDR_MODE_2To1_FRAME",
            "WDR_MODE_2To1_FRAME_FULL_RATE",
            "WDR_MODE_3To1_LINE",
            "WDR_MODE_3To1_FRAME",
            "WDR_MODE_3To1_FRAME_FULL_RATE",
            "WDR_MODE_4To1_LINE",
            "WDR_MODE_4To1_FRAME",
            "WDR_MODE_4To1_FRAME_FULL_RATE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            &ini, "sensor", "mode", (void *)&config->mode, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }
#endif
    err = parse_param_value(&ini, "sensor", "DllFile", config->dll_file);
    if (err != CONFIG_OK)
        goto RET_ERR;

    // [mode]
#if HISILICON_SDK_GEN >= 2
    {
        const char *possible_values[] = {
            "INPUT_MODE_MIPI",   "INPUT_MODE_SUBLVDS",  "INPUT_MODE_LVDS",
            "INPUT_MODE_HISPI",  "INPUT_MODE_CMOS_18V", "INPUT_MODE_CMOS_33V",
            "INPUT_MODE_BT1120", "INPUT_MODE_BYPASS"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            &ini, "mode", "input_mode", (void *)&config->input_mode,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }

    if (config->input_mode == INPUT_MODE_MIPI) {
        // [mipi]
        {
            const char *possible_values[] = {// starts from 1 !!!!!
                                             "RAW_DATA_8BIT", "RAW_DATA_10BIT",
                                             "RAW_DATA_12BIT",
                                             "RAW_DATA_14BIT"};
            const int count = sizeof(possible_values) / sizeof(const char *);
            err = parse_enum(
                &ini, "mipi", "data_type", (void *)&config->mipi.data_type,
                possible_values, count, 1);
            if (err != CONFIG_OK)
                goto RET_ERR;
        }
        err = parse_array(&ini, "mipi", "lane_id", config->mipi.lane_id, 8);
        if (err != CONFIG_OK)
            goto RET_ERR;
    } else if (config->input_mode == INPUT_MODE_LVDS) {
        // [lvds]
        err = parse_config_lvds(&ini, "lvds", &config->lvds);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }
#endif

    // [isp_image]
    err = parse_config_isp(&ini, "ips_image", &config->isp);
    if (err != CONFIG_OK)
        goto RET_ERR;
    // [vi_dev]
    err = parse_config_videv(&ini, "vi_dev", &config->videv);
    if (err != CONFIG_OK)
        goto RET_ERR;
    // [vi_chn]
    err = parse_config_vichn(&ini, "vi_chn", &config->vichn);
    if (err != CONFIG_OK)
        goto RET_ERR;

    sensor_config = *config;
    free(ini.str);
    return CONFIG_OK;
RET_ERR:
    free(ini.str);
    return err;
}

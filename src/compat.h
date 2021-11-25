#ifndef COMPAT_H
#define COMPAT_H

#if HISILICON_SDK_CODE == 0x3518
#define HISILICON_SDK_GEN 1
#elif HISILICON_SDK_CODE == 0x3518E200
#define HISILICON_SDK_GEN 2
#elif HISILICON_SDK_CODE == 0x3516A
#define HISILICON_SDK_GEN 2
#elif HISILICON_SDK_CODE == 0x3516C300
#define HISILICON_SDK_GEN 3
#elif HISILICON_SDK_CODE == 0x3516E200
#define HISILICON_SDK_GEN 4
#elif HISILICON_SDK_CODE == 0x3516C500
#define HISILICON_SDK_GEN 4
#elif HISILICON_SDK_CODE == 0x7205200
#define HISILICON_SDK_GEN 4
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <hi_math.h>
#pragma GCC diagnostic pop

#ifndef ALIGN_UP
#define ALIGN_UP(x, a) ((((x) + ((a)-1)) / a) * a)
#endif

#include <hi_comm_isp.h>
#if HISILICON_SDK_GEN >= 2
#include <hi_mipi.h>
#endif
#include <hi_comm_vi.h>
#if HISILICON_SDK_GEN <= 3
#include <mpi_af.h>
#endif
#if HISILICON_SDK_GEN == 4
#include <hi_buffer.h>
#include <hi_comm_venc.h>
#include <hi_sns_ctrl.h>
#endif

#if HISILICON_SDK_GEN == 2 || HISILICON_SDK_GEN == 3
typedef raw_data_type_e data_type_t;

typedef wdr_mode_e wdr_mode_t;
typedef lvds_sync_mode_e lvds_sync_mode_t;
typedef lvds_bit_endian lvds_bit_endian_t;

typedef BT656_FIXCODE_E VI_BT656_FIXCODE_E;
typedef BT656_FIELD_POLAR_E VI_BT656_FIELD_POLAR_E;
#endif

#if HISILICON_SDK_GEN <= 2
#define MPEG_ATTR stAttrMjpeg
#define JPEG_ATTR stAttrJpeg
#elif HISILICON_SDK_GEN == 3
#define MPEG_ATTR stAttrMjpege
#define JPEG_ATTR stAttrJpege
#endif

#define MIPI_DEV "/dev/hi_mipi"

#endif /* COMPAT_H */

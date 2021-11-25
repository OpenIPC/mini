# - Try to find HiSiliconSDK
# Once done this will define
#  VENDOR_SDK_FOUND - System has HiSilicon SDK
#  VENDOR_SDK_INCLUDE_DIRS - The HiSilicon SDK include directories
#  VENDOR_SDK_LIBRARIES - The libraries needed to use HiSilicon SDK

find_path(VENDOR_SDK_INCLUDE_DIRS hi_common.h
    HINTS ${PLATFORM_SDK_DIR}
    PATH_SUFFIXES mpp2/include mpp/include include source/gmp/include
    REQUIRED
    NO_CMAKE_FIND_ROOT_PATH
)

execute_process(COMMAND ${CMAKE_CURRENT_LIST_DIR}/get_hisisdk_ver.py
    ${VENDOR_SDK_INCLUDE_DIRS}/..
    OUTPUT_VARIABLE RAW_SDK_VER
)
if(RAW_SDK_VER)
    list(GET RAW_SDK_VER 0 HISILICON_SDK_CODE)
    list(GET RAW_SDK_VER 1 HISILICON_SDK_FAMILY)
    list(GET RAW_SDK_VER 2 HISILICON_SDK_VERSION_MAJOR)
    list(GET RAW_SDK_VER 3 HISILICON_SDK_VERSION_MINOR)
    list(GET RAW_SDK_VER 4 HISILICON_SDK_VERSION_PATCH)
    list(GET RAW_SDK_VER 5 HISILICON_SDK_VERSION_TWEAK)
    list(GET RAW_SDK_VER 7 HISILICON_SDK_VERSION_DATE)
    list(SUBLIST RAW_SDK_VER 2 5 SDK_ONLY_VER)
    list(JOIN SDK_ONLY_VER "." HISILICON_SDK_VERSION)
    message("Detected ${HISILICON_SDK_FAMILY} SDK version ${HISILICON_SDK_VERSION}")
    set(VENDOR_SDK_FOUND 1)
else()
    message(FATAL_ERROR "No HiSilicon SDK detected")
endif()
add_compile_definitions(HISILICON_SDK_CODE=0x${HISILICON_SDK_CODE})
add_compile_definitions(HISILICON_SDK_FAMILY="${HISILICON_SDK_FAMILY}")
add_compile_definitions(HISILICON_SDK_VERSION="${HISILICON_SDK_VERSION}")

if(HISILICON_SDK_CODE STREQUAL "3516C500")
  add_compile_definitions(MAX_VIDEO_CHANNELS=3)
endif()

if(HISILICON_SDK_CODE STREQUAL "7205200")
  set(CORE_LIBS_NAMES
    hi_mpi
    hi_md           # Motion detection
    hi_isp          # Image signal processor
    hi_ive          # Intelligent video engine
    hi_ae
    hi_awb

    gk_ae
    gk_api
    gk_awb
    gk_awb_natura
    gk_bcd
    gk_cipher
    gk_isp
    gk_ive
    gk_ivp
    gk_md
    gk_qr
    gk_tde

    securec         # Secure C functions
    upvqe           # Up voice quality enhancement
    dnvqe           # Down voice quality enhancement
    voice_engine
    ldci
    dehaze          # Remove haze
    drc             # Dynamic range compression
    ir_auto         # IR Cut auto
  )
else()

  if(HISILICON_SDK_CODE STREQUAL "3518")
    set(SPECIFIC_LIBS
      resampler
      aec
      anr
      vqev2
    )
  endif()

  set(CORE_LIBS_NAMES
    mpi
    md              # Motion detection
    _hiae           # Automatic exposure
    isp             # Image signal processor
    ive             # Intelligent video engine
    _hidehaze       # Remove haze
    _hidefog        # Remove fog
    _hidrc          # Dynamic range compression
    _hildci         # LDCI/Sharpen
    _hiawb          # Automatic white balance
    _hiir_auto      # IR Cut auto
    _hiaf           # Automatic focus
    _hiacs
    _hicalcflicker  # Flicker calculations
    upvqe           # Up voice quality enhancement
    dnvqe           # Down voice quality enhancement
    securec         # Secure C functions
    VoiceEngine
    ${SPECIFIC_LIBS}
  )
endif()

foreach(LIB ${CORE_LIBS_NAMES})
        find_library(FOUND_LIB_${LIB} ${LIB}
            PATH_SUFFIXES mpp2/lib mpp/lib lib
              source/gmp/lib source/gmp/lib_log/static
            HINTS ${PLATFORM_SDK_DIR}
            NO_CMAKE_FIND_ROOT_PATH
        )
        if(FOUND_LIB_${LIB})
            list(APPEND CORE_LIBS ${FOUND_LIB_${LIB}})
        endif()
        message("Lib: ${LIB}")
        message("Found Lib: ${FOUND_LIB_${LIB}}")
endforeach(LIB)

set(VENDOR_SDK_LIBRARIES
    ${CORE_LIBS}
)

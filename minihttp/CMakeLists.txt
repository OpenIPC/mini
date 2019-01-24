cmake_minimum_required(VERSION 2.8)
project(minihttp C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "-std=c99")

include_directories(
    ./include   # HISDK headers
)


set(HILIBS
    isp         # ISP_AlgRegisterDehaze
    # sns_imx222  # sensor_register_callback / sensor_unregister_callback
    _hiae       # HI_MPI_AE_Register/HI_MPI_AE_UnRegister, HI_MPI_AE_SensorRegCallBack/HI_MPI_AE_SensorUnRegCallBack
    _hiawb      # HI_MPI_AWB_Register/HI_MPI_AWB_UnRegister, HI_MPI_AWB_SensorRegCallBack/HI_MPI_AWB_SensorUnRegCallBack
    _hiaf       # HI_MPI_AF_Register/HI_MPI_AF_UnRegister
    _hidefog    # ISP_AlgRegisterDehaze
    pthread m dl
    ive         # HI_MPI_IVE_DMA, HI_MPI_IVE_Query, ISP_AlgRegisterDehaze
    md          # ISP_AlgRegisterDehaze
    mpi
    tde         # ISP_AlgRegisterDehaze
    upvqe       # HI_UPVQE_Create/HI_UPVQE_Destroy, HI_UPVQE_ReadFrame/HI_UPVQE_WriteFrame, HI_UPVQE_GetVolume/HI_UPVQE_SetVolume, HI_UPVQE_GetConfig
    dnvqe       # ISP_AlgRegisterDehaze, HI_DNVQE_Create/HI_DNVQE_Destroy, HI_DNVQE_ReadFrame/HI_DNVQE_WriteFrame, HI_DNVQE_GetConfig
    VoiceEngine # ISP_AlgRegisterDehaze, HI_VOICE_EncodeFrame/HI_VOICE_DecodeFrame, HI_VOICE_EncReset/HI_VOICE_DecReset
    # slice_trans
)

# add_executable(minihttp src/main.c src/tools.c)
add_executable(minihttp
    src/hidemo.c
    src/tools.c
    src/stack.c
    src/sensors.c
    src/server.c
)
target_link_libraries(minihttp pthread ${HILIBS})
install(TARGETS minihttp RUNTIME DESTINATION /usr/bin/)

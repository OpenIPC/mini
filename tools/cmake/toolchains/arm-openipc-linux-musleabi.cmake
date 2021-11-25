set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross compiler.
set(CMAKE_C_COMPILER arm-openipc-linux-musleabi-gcc CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER arm-openipc-linux-musleabi-g++ CACHE FILEPATH "C++ compiler")
set(CMAKE_ASM_COMPILER arm-openipc-linux-musleabi-gcc CACHE FILEPATH "ASM compiler")

# Search libraries only under *target* paths.
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

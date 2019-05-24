# cmake 版本
cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -UNDEBUG")


add_library(H264MediaCodec SHARED
        src/main/cpp/H264MediaCodec.cpp
        )

#导入静态资源
add_library(hwcodec_3288 STATIC IMPORTED)
set_target_properties(hwcodec_3288 PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/src/main/cpp/libs/armeabi-v7a/libhwcodec_3288.so)

# Include libraries needed for hello-jni lib
target_link_libraries(H264MediaCodec
        hwcodec_3288
        android
        log
        mediandk
        OpenMAXAL)
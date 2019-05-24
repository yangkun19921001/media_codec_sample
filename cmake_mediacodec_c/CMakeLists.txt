cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -UNDEBUG")


# ADD_LIBRARY(libname [SHARED | STATIC | MODULE] [EXCLUDE_FROM_ALL] source1 source2 ... sourceN)
# 不需要写全 libhello.so,只需要填写hello即可,cmake系统会自动生成libhello.X的
# 类型有三种
# SHARED, 动态库
# STATIC, 静态库
# MODULE, 在使用dyld的系统有效,如果不支持dyld,则被当做SHARED对待,what is dyld?
# EXCLUDE_FROM_ALL 意思是这个库不会被默认构建,除非有其他的组件依赖或者手工构建


add_library(hwcodec_ndk_static STATIC
        src/main/cpp/GPU_msdk_codec.cpp
        src/main/cpp/image_scaler.cpp
        )

SET_TARGET_PROPERTIES(hwcodec_ndk_static PROPERTIES OUTPUT_NAME "hwcodec_ndk")

# Include libraries needed for native-codec-jni lib
target_link_libraries(hwcodec_ndk_static
        android
        log
        mediandk
        OpenMAXAL)
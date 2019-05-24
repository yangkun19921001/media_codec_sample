LOCAL_PATH :=$(call my-dir)
include $(CLEAR_VARS)

#打包的名字
LOCAL_MODULE :=libhwcodec_ndk

#本地资源实现类
LOCAL_SRC_FILES :=hw_codec.c

# 额外的C/C++编译头文件路径，用LOCAL_PATH表示本文件所在目录
LOCAL_C_INCLUDES :=$(LOCAL_PATH)/include

#为 C/C++编译器定义额外的标志 (如宏定义 )
#LOCAL_CFLAGS     := -Wno-multichar -O2

#LOCAL_CPPFLAGS   := -UNDEBUG -O2

#可链接动态库
#LOCAL_SHARED_LIBRARIES += libstagefright libmedia libutils libbinder libstagefright_foundation libcutils

# for native multimedia  为可执行程序或者库的编译指定额外的库，指定库以"-lxxx"格式
LOCAL_LDLIBS    += -lOpenMAXAL -lmediandk

# for logging
LOCAL_LDLIBS    += -llog

# for native windows
LOCAL_LDLIBS    += -landroid

#表示编译成动态库。
#include $(BUILD_SHARED_LIBRARY)

#表示编译成静态库
include $(BUILD_STATIC_LIBRARY)

#表示编译成可执行程序
#include $(BUILD_EXECUTABLE)
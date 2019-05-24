APP_PROJECT_PATH := $(call my-dir)/../
APP_BUILD_SCRIPT := $(call my-dir)/Android.mk

APP_STL := c++_static
APP_ABI := armeabi-v7a
APP_PLATFORM := android-21
APP_CPPFLAGS := -fexceptions -O2
APP_CFLAGS := -O2
LOCAL_ARM_NEON := true
APP_OPTIM := release

/////////////////////////////////////////////////////////////////////////////////////
#ifndef __IMAGE_SCALER_H__
#define __IMAGE_SCALER_H__

#include "../../../../../../../../Android/AndroidDeveloper-sdk/Android_NDK/android-ndk-r17b/sysroot/usr/include/stdint.h"

/////////////////////////////////////////////////////////////////////////////////////
int scaler_I420Scale(const uint8_t* src_y, int src_stride_y,
                     const uint8_t* src_u, int src_stride_u,
                     const uint8_t* src_v, int src_stride_v,
                     int src_width, int src_height,
                     uint8_t* dst_y, int dst_stride_y,
                     uint8_t* dst_u, int dst_stride_u,
                     uint8_t* dst_v, int dst_stride_v,
                     int dst_width, int dst_height);

/////////////////////////////////////////////////////////////////////////////////////
int scaler_I420Mirror(const uint8_t* src_y, int src_stride_y,
                      const uint8_t* src_u, int src_stride_u,
                      const uint8_t* src_v, int src_stride_v,
                      uint8_t* dst_y, int dst_stride_y,
                      uint8_t* dst_u, int dst_stride_u,
                      uint8_t* dst_v, int dst_stride_v,
                      int width, int height);

#endif  // End of __IMAGE_SCALER_H__

/////////////////////////////////////////////////////////////////////////////////////

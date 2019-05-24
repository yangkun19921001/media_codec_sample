/////////////////////////////////////////////////////////////////////////////////////

#include "image_scaler.h"

/////////////////////////////////////////////////////////////////////////////////////
static void MirrorPlane(const uint8_t* src_y, int src_stride_y,
                        uint8_t* dst_y, int dst_stride_y,
                        int width, int height)
{
    // Mirror plane
    for (int y = 0; y < height; ++y)
    {
        memcpy(dst_y, src_y, width);
        src_y += src_stride_y;
        dst_y += dst_stride_y;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
static void ScaleFilterRows_C(uint8_t* dst_ptr,
                              const uint8_t* src_ptr, ptrdiff_t src_stride,
                              int dst_width, int source_y_fraction)
{
    // Specialized case for 100% first row.  Helps avoid reading beyond last row.
    if (source_y_fraction == 0)
    {
        memcpy(dst_ptr, src_ptr, dst_width);
        dst_ptr[dst_width] = dst_ptr[dst_width - 1];
        return;
    }
    
    int y1_fraction = source_y_fraction;
    int y0_fraction = 256 - y1_fraction;
    const uint8_t* src_ptr1 = src_ptr + src_stride;
    
    // Blend 2 rows into 1 with filtering. N x 2 to N x 1
    for (int x = 0; x < dst_width - 1; x += 2)
    {
        dst_ptr[0] = (src_ptr[0] * y0_fraction + src_ptr1[0] * y1_fraction) >> 8;
        dst_ptr[1] = (src_ptr[1] * y0_fraction + src_ptr1[1] * y1_fraction) >> 8;
        src_ptr += 2;
        src_ptr1 += 2;
        dst_ptr += 2;
    }
    
    if (dst_width & 1)
    {
        dst_ptr[0] = (src_ptr[0] * y0_fraction + src_ptr1[0] * y1_fraction) >> 8;
        dst_ptr += 1;
    }
    
    dst_ptr[0] = dst_ptr[-1];
}

/////////////////////////////////////////////////////////////////////////////////////
static void ScaleFilterCols_C(uint8_t* dst_ptr, const uint8_t* src_ptr,
                              int dst_width, int x, int dx)
{
    // (1-f)a + fb can be replaced with a + f(b-a)
    #define BLENDER(a, b, f) (static_cast<int>(a) + \
    ((f) * (static_cast<int>(b) - static_cast<int>(a)) >> 16))
    
    for (int j = 0; j < dst_width - 1; j += 2)
    {
        int xi = x >> 16;
        int a = src_ptr[xi];
        int b = src_ptr[xi + 1];
        dst_ptr[0] = BLENDER(a, b, x & 0xffff);
        x += dx;
        xi = x >> 16;
        a = src_ptr[xi];
        b = src_ptr[xi + 1];
        dst_ptr[1] = BLENDER(a, b, x & 0xffff);
        x += dx;
        dst_ptr += 2;
    }
    
    if (dst_width & 1)
    {
        int xi = x >> 16;
        int a = src_ptr[xi];
        int b = src_ptr[xi + 1];
        dst_ptr[0] = BLENDER(a, b, x & 0xffff);
    }
}

/////////////////////////////////////////////////////////////////////////////////////
static void ScalePlaneBilinear(int src_width, int src_height,
                               int dst_width, int dst_height,
                               int src_stride, int dst_stride,
                               const uint8_t* src_ptr, uint8_t* dst_ptr)
{
    uint8_t row[2560 + 16];
    
    int dx = (src_width << 16) / dst_width;
    int dy = (src_height << 16) / dst_height;
    int x = (dx >= 65536) ? ((dx >> 1) - 32768) : (dx >> 1);
    int y = (dy >= 65536) ? ((dy >> 1) - 32768) : (dy >> 1);
    int maxy = (src_height > 1) ? ((src_height - 1) << 16) - 1 : 0;
    
    for (int j = 0; j < dst_height; ++j)
    {
        if (y > maxy)
        {
            y = maxy;
        }
        
        int yi = y >> 16;
        int yf = (y >> 8) & 255;
        const uint8_t* src = src_ptr + yi * src_stride;
        
        ScaleFilterRows_C(row, src, src_stride, src_width, yf);
        ScaleFilterCols_C(dst_ptr, row, dst_width, x, dx);
        
        dst_ptr += dst_stride;
        y += dy;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
static void ScalePlaneSimple(int src_width, int src_height,
                             int dst_width, int dst_height,
                             int src_stride, int dst_stride,
                             const uint8_t* src_ptr, uint8_t* dst_ptr)
{
    int dx = (src_width << 16) / dst_width;
    int dy = (src_height << 16) / dst_height;
    int y = (dy >= 65536) ? ((dy >> 1) - 32768) : (dy >> 1);
    
    for (int j = 0; j < dst_height; ++j)
    {
        int x = (dx >= 65536) ? ((dx >> 1) - 32768) : (dx >> 1);
        int yi = y >> 16;
        const uint8_t* src = src_ptr + yi * src_stride;
        uint8_t* dst = dst_ptr;
        
        for (int i = 0; i < dst_width; ++i)
        {
            *dst++ = src[x >> 16];
            x += dx;
        }
        
        dst_ptr += dst_stride;
        y += dy;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
int scaler_I420Scale(const uint8_t* src_y, int src_stride_y,
                     const uint8_t* src_u, int src_stride_u,
                     const uint8_t* src_v, int src_stride_v,
                     int src_width, int src_height,
                     uint8_t* dst_y, int dst_stride_y,
                     uint8_t* dst_u, int dst_stride_u,
                     uint8_t* dst_v, int dst_stride_v,
                     int dst_width, int dst_height)
{
    if (!src_y || !src_u || !src_v || src_width <= 0 || src_height == 0 ||
        !dst_y || !dst_u || !dst_v || dst_width <= 0 || dst_height <= 0)
    {
        return -1;
    }
    
    // Negative height means invert the image.
    if (src_height < 0)
    {
        src_height = -src_height;
        int halfheight = (src_height + 1) >> 1;
        src_y = src_y + (src_height - 1) * src_stride_y;
        src_u = src_u + (halfheight - 1) * src_stride_u;
        src_v = src_v + (halfheight - 1) * src_stride_v;
        src_stride_y = -src_stride_y;
        src_stride_u = -src_stride_u;
        src_stride_v = -src_stride_v;
    }
    
    int src_halfwidth  = (src_width + 1)  >> 1;
    int src_halfheight = (src_height + 1) >> 1;
    int dst_halfwidth  = (dst_width + 1)  >> 1;
    int dst_halfheight = (dst_height + 1) >> 1;
    
    ScalePlaneBilinear(src_width, src_height, dst_width, dst_height, 
                       src_stride_y, dst_stride_y, src_y, dst_y);
    
    ScalePlaneSimple(src_halfwidth, src_halfheight, dst_halfwidth, dst_halfheight, 
                     src_stride_u, dst_stride_u, src_u, dst_u);
    
    ScalePlaneSimple(src_halfwidth, src_halfheight, dst_halfwidth, dst_halfheight, 
                     src_stride_v, dst_stride_v, src_v, dst_v);
    
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
int scaler_I420Mirror(const uint8_t* src_y, int src_stride_y,
                      const uint8_t* src_u, int src_stride_u,
                      const uint8_t* src_v, int src_stride_v,
                      uint8_t* dst_y, int dst_stride_y,
                      uint8_t* dst_u, int dst_stride_u,
                      uint8_t* dst_v, int dst_stride_v,
                      int width, int height)
{
    if (!src_y || !src_u || !src_v || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0)
    {
        return -1;
    }
    
    // Negative height means invert the image.
    if (height < 0)
    {
        height = -height;
        int halfheight = (height + 1) >> 1;
        src_y = src_y + (height - 1) * src_stride_y;
        src_u = src_u + (halfheight - 1) * src_stride_u;
        src_v = src_v + (halfheight - 1) * src_stride_v;
        src_stride_y = -src_stride_y;
        src_stride_u = -src_stride_u;
        src_stride_v = -src_stride_v;
    }
    
    int halfwidth = (width + 1) >> 1;
    int halfheight = (height + 1) >> 1;
    
    MirrorPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
    MirrorPlane(src_u, src_stride_u, dst_u, dst_stride_u, halfwidth, halfheight);
    MirrorPlane(src_v, src_stride_v, dst_v, dst_stride_v, halfwidth, halfheight);
    
    return 0;
}

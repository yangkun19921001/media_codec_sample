/////////////////////////////////////////////////////////////////////////////////////

#include <media/NdkMediaCodec.h>
#include "hw_codec.h"
#include "fcntl.h"

#include "getopt.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/wait.h"
#include "termios.h"
#include "unistd.h"

#include "media/NdkMediaError.h"
#include "media/NdkMediaFormat.h"
#include "media/NdkMediaCodec.h"

/////////////////////////////////////////////////////////////////////////////////////
//the local control parameters for the MSDK encoder.
   AMediaCodec*           m_VideoEncoder;
   AMediaFormat*          m_VideoFormat;
   MSdkInputParam         m_InitParams;
   uint32_t               m_CodecInitFlag;
   uint32_t               m_ForDatashare;
   uint32_t               m_nFramesProcessed;
   uint32_t               m_SpsPpsLength;
   uint32_t               m_SpsPpsHeader[64];

   /*int BLENDER(int a, int b, int f)
   {
       return ((int)(a) + ((f) * (int)(b) - (int)(a)) >> 16);
   }*/

   #define BLENDER(a, b, f) ((int)a + (f * ((int)b - (int)a) >> 16))

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
    int x;
    int y1_fraction = source_y_fraction;
    int y0_fraction = 256 - y1_fraction;
    const uint8_t* src_ptr1 = src_ptr + src_stride;
    // Specialized case for 100% first row.  Helps avoid reading beyond last row.
    if (source_y_fraction == 0)
    {
        memcpy(dst_ptr, src_ptr, dst_width);
        dst_ptr[dst_width] = dst_ptr[dst_width - 1];
        return;
    }
    
    
    
    // Blend 2 rows into 1 with filtering. N x 2 to N x 1
    for (x = 0; x < dst_width - 1; x += 2)
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
    int j, xi, a, b;

    
    for (j = 0; j < dst_width - 1; j += 2)
    {
        xi = x >> 16;
        a = src_ptr[xi];
        b = src_ptr[xi + 1];
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
        xi = x >> 16;
        a = src_ptr[xi];
        b = src_ptr[xi + 1];
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
    int j, yi,yf;
    
    int dx = (src_width << 16) / dst_width;
    int dy = (src_height << 16) / dst_height;
    int x = (dx >= 65536) ? ((dx >> 1) - 32768) : (dx >> 1);
    int y = (dy >= 65536) ? ((dy >> 1) - 32768) : (dy >> 1);
    int maxy = (src_height > 1) ? ((src_height - 1) << 16) - 1 : 0;
    
    for (j = 0; j < dst_height; ++j)
    {
        if (y > maxy)
        {
            y = maxy;
        }
        
       yi = y >> 16;
       yf = (y >> 8) & 255;
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
    int i, j, x, yi ;
    const uint8_t * src;
    uint8_t * dst;
    
    for (j = 0; j < dst_height; ++j)
    {
        int x = (dx >= 65536) ? ((dx >> 1) - 32768) : (dx >> 1);
        int yi = y >> 16;
        src = src_ptr + yi * src_stride;
        dst = dst_ptr;
        
        for (i = 0; i < dst_width; ++i)
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
    int halfheight,src_halfwidth,src_halfheight,dst_halfwidth,dst_halfheight;
    if (!src_y || !src_u || !src_v || src_width <= 0 || src_height == 0 ||
        !dst_y || !dst_u || !dst_v || dst_width <= 0 || dst_height <= 0)
    {
        return -1;
    }
    
    // Negative height means invert the image.
    if (src_height < 0)
    {
        src_height = -src_height;
        halfheight = (src_height + 1) >> 1;
        src_y = src_y + (src_height - 1) * src_stride_y;
        src_u = src_u + (halfheight - 1) * src_stride_u;
        src_v = src_v + (halfheight - 1) * src_stride_v;
        src_stride_y = -src_stride_y;
        src_stride_u = -src_stride_u;
        src_stride_v = -src_stride_v;
    }
    
    src_halfwidth  = (src_width + 1)  >> 1;
    src_halfheight = (src_height + 1) >> 1;
    dst_halfwidth  = (dst_width + 1)  >> 1;
    dst_halfheight = (dst_height + 1) >> 1;
    
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
    int halfheight, halfwidth;
    if (!src_y || !src_u || !src_v || !dst_y || !dst_u || !dst_v || width <= 0 || height == 0)
    {
        return -1;
    }
    
    // Negative height means invert the image.
    if (height < 0)
    {
        height = -height;
        halfheight = (height + 1) >> 1;
        src_y = src_y + (height - 1) * src_stride_y;
        src_u = src_u + (halfheight - 1) * src_stride_u;
        src_v = src_v + (halfheight - 1) * src_stride_v;
        src_stride_y = -src_stride_y;
        src_stride_u = -src_stride_u;
        src_stride_v = -src_stride_v;
    }
    
    halfwidth = (width + 1) >> 1;
    halfheight = (height + 1) >> 1;
    
    MirrorPlane(src_y, src_stride_y, dst_y, dst_stride_y, width, height);
    MirrorPlane(src_u, src_stride_u, dst_u, dst_stride_u, halfwidth, halfheight);
    MirrorPlane(src_v, src_stride_v, dst_v, dst_stride_v, halfwidth, halfheight);
    
    return 0;
}


/////////////////////////////////////////////////////////////////////////////////////
int32_t OpenEncoder(MSdkInputParam *InputParam)
{
    media_status_t sts = AMEDIA_OK;
    
    //Only one instance of MSDK encoder could be created now.
    if (m_CodecInitFlag != 0)
    {
        return MCODEC_ERROR;
    }
    
    //Save the input MSDK encoder and VPP configure parameters.
    m_InitParams.InStreamType    = InputParam->InStreamType;
    m_InitParams.InFrameRate     = InputParam->InFrameRate;
    m_InitParams.InWidth         = InputParam->InWidth;
    m_InitParams.InHeight        = InputParam->InHeight;
    
    m_InitParams.nFrameRate      = InputParam->nFrameRate;
    m_InitParams.nWidth          = InputParam->nWidth;
    m_InitParams.nHeight         = InputParam->nHeight;
    m_InitParams.nTargetKbps     = InputParam->nTargetKbps;
    m_InitParams.nTemporalLayers = InputParam->nTemporalLayers;
    m_InitParams.nSpatialId      = InputParam->nSpatialId;
    m_InitParams.nMemType        = InputParam->nMemType;
    
    //create a mediacodec encoder instance.
    m_VideoEncoder = AMediaCodec_createEncoderByType("video/avc");
    if (m_VideoEncoder == NULL)
    {
        return MCODEC_ERROR;
    }
    
    //create the media format configure instance.
    if (m_VideoFormat == NULL)
    {
        m_VideoFormat = AMediaFormat_new();
        if (m_VideoFormat == NULL)
        {
            return MCODEC_ERROR;
        }
    }
    
    //update the encoder input and output format.
    AMediaFormat_setInt32(m_VideoFormat, "width", m_InitParams.nWidth);
    AMediaFormat_setInt32(m_VideoFormat, "height", m_InitParams.nHeight);
    AMediaFormat_setString(m_VideoFormat, "mime", "video/avc");
    AMediaFormat_setInt32(m_VideoFormat, "color-format", 21);
    AMediaFormat_setInt32(m_VideoFormat, "bitrate", m_InitParams.nTargetKbps * 1000);
    AMediaFormat_setFloat(m_VideoFormat, "frame-rate", m_InitParams.nFrameRate);
    AMediaFormat_setInt32(m_VideoFormat, "i-frame-interval", 5);
    
    //configure and initialize the encoder.
    uint32_t flags = AMEDIACODEC_CONFIGURE_FLAG_ENCODE;
    sts = AMediaCodec_configure(m_VideoEncoder, m_VideoFormat, NULL, NULL, flags);
    if (sts != AMEDIA_OK)
    {
        AMediaCodec_delete(m_VideoEncoder);
        m_VideoEncoder = NULL;
        return MCODEC_ERROR;
    }
    
    //start the android hardware video encoder device.
    sts = AMediaCodec_start(m_VideoEncoder);
    if (sts != AMEDIA_OK)
    {
        AMediaCodec_delete(m_VideoEncoder);
        m_VideoEncoder = NULL;
        return MCODEC_ERROR;
    }
    
    //Reset and initialize the local MSDK control parameters.
    memset(m_SpsPpsHeader, 0, sizeof(m_SpsPpsHeader));
    m_ForDatashare     = 0;
    m_nFramesProcessed = 0;
    m_SpsPpsLength     = 0;
    m_CodecInitFlag    = 1;
    
    //Succed to open the MSDK encoder, return the result.
    return MCODEC_SUCCEED;
}

/////////////////////////////////////////////////////////////////////////////////////
int32_t CloseEncoder()
{
    //if the MSDK device was not opened, do nothing and exit.
    if (m_CodecInitFlag == 0)
    {
        return MCODEC_ERROR;
    }
    
    //Update the MSDK initialize flag to close device.
    if (m_VideoEncoder != NULL)
    {
        AMediaCodec_stop(m_VideoEncoder);
        AMediaCodec_delete(m_VideoEncoder);
        m_VideoEncoder = NULL;
    }
    
    //delete the video format instance when close device.
    if (m_VideoFormat != NULL)
    {
        AMediaFormat_delete(m_VideoFormat);
        m_VideoFormat = NULL;
    }
    
    //Update the MSDK initialize flag to close device.
    m_CodecInitFlag = 0;
    
    //Succed to close the encoder, return the result.
    return MCODEC_SUCCEED;
}

/////////////////////////////////////////////////////////////////////////////////////
int32_t EncodeFrame(SSourcePicture* pSrcPic, SLayerBSInfo* pBsLayer)
{
    size_t BufSize = 0;
    
    //if the MSDK device was not opened, do nothing and exit.
    if (m_CodecInitFlag == 0)
    {
        return MCODEC_ERROR;
    }
    
    //the input both pointer should not be all NULL values.
    if ((pSrcPic == NULL) && (pBsLayer == NULL))
    {
        return MCODEC_ERROR;
    }
    
    //Get a memory block from buffer array to save YUV frame.
    ssize_t bufIndex = AMediaCodec_dequeueInputBuffer(m_VideoEncoder, -1ll);
    if (bufIndex < 0)
    {
        return MCODEC_ERROR;
    }
    
    //Get an input buffer, with the buffer index that previously obtained.
    uint8_t *inputBuffer = AMediaCodec_getInputBuffer(m_VideoEncoder, bufIndex, &BufSize);
    if (inputBuffer == NULL)
    {
        return MCODEC_ERROR;
    }
    
    //Get the YUV image parameters from the input frame.
    int32_t srcWidth  = pSrcPic->iPicWidth;
    int32_t srcHeight = pSrcPic->iPicHeight;
    int32_t dstWidth  = m_InitParams.nWidth;
    int32_t dstHeight = m_InitParams.nHeight;
    
    uint8_t *encPlaneY = inputBuffer;
    uint8_t *encPlaneU = encPlaneY + dstWidth * dstHeight;
    uint8_t *encPlaneV = encPlaneU + (dstWidth >> 1) * (dstHeight >> 1);
    
    //scale the input image to encoder size.
    if ((srcWidth != dstWidth) || (srcHeight != dstHeight))
    {
        scaler_I420Scale(pSrcPic->pData[0], pSrcPic->iStride[0],
                         pSrcPic->pData[1], pSrcPic->iStride[1],
                         pSrcPic->pData[2], pSrcPic->iStride[2],
                         srcWidth, srcHeight,
                         encPlaneY, dstWidth,
                         encPlaneU, (dstWidth >> 1),
                         encPlaneV, (dstWidth >> 1),
                         dstWidth, dstHeight);
    }
    
    //with the same frame size, copy YUV image to encoder buffer.
    else
    {
        scaler_I420Mirror(pSrcPic->pData[0], pSrcPic->iStride[0],
                          pSrcPic->pData[1], pSrcPic->iStride[1],
                          pSrcPic->pData[2], pSrcPic->iStride[2],
                          encPlaneY, dstWidth,
                          encPlaneU, (dstWidth >> 1),
                          encPlaneV, (dstWidth >> 1),
                          dstWidth, dstHeight);
    }
    
    //put the incoming frame to the encoding queue to encode.
    media_status_t sts = AMEDIA_OK;
    uint64_t time = pSrcPic->uiTimeStamp * 1000;
    
    sts = AMediaCodec_queueInputBuffer(m_VideoEncoder, bufIndex, 0, BufSize, time, 0);
    if (sts != AMEDIA_OK)
    {
        return MCODEC_ERROR;
    }
    
    //Update the total encoded frame counter for debug.
    m_nFramesProcessed++;
    
    //Succeed to start the MSDK encoder, return the results.
    return MCODEC_SUCCEED;
}

/////////////////////////////////////////////////////////////////////////////////////
int32_t GetBitstream(SLayerBSInfo* pBsLayer)
{
    size_t BufSize = 0;
    AMediaCodecBufferInfo BufInfo;
    
    //if the MSDK device was not opened, do nothing and exit.
    if (m_CodecInitFlag == 0)
    {
        return MCODEC_ERROR;
    }
    
    //Get bitstream buffer from android buffer array, with blocking mode.
    ssize_t bufIndex = AMediaCodec_dequeueOutputBuffer(m_VideoEncoder, &BufInfo, -1ll);
    if ((bufIndex < 0) && (bufIndex != AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED))
    {
        return MCODEC_ERROR;
    }
    
    //if the return value is "INFO_FORMAT_CHANGED", read buffer array again.
    if (bufIndex == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
    {
        bufIndex = AMediaCodec_dequeueOutputBuffer(m_VideoEncoder, &BufInfo, -1ll);
        if (bufIndex < 0)
        {
            return MCODEC_ERROR;
        }
    }
    
    //Get an output buffer, with the buffer index that previously obtained.
    uint8_t *outputBuffer = AMediaCodec_getOutputBuffer(m_VideoEncoder, bufIndex, &BufSize);
    if (outputBuffer == NULL)
    {
        return MCODEC_ERROR;
    }
    
    //for SPS/PPS unit, save them and read NAL unit again.
    if ((outputBuffer != NULL) && (BufInfo.size > 0))
    {
        uint8_t *src_buf = outputBuffer;
        int32_t nal_type = src_buf[4] & 0x1F;
        
        if (nal_type == 7)
        {
            //save SPS/PPS unit to local buffer, for repeat them every IDR frame.
            memcpy((uint8_t *)m_SpsPpsHeader, src_buf, BufInfo.size);
            m_SpsPpsLength = BufInfo.size;
            
            //release the output bitstream buffer, for the next encoding.
            AMediaCodec_releaseOutputBuffer(m_VideoEncoder, bufIndex, false);
            
            //lookup the buffer array again, to find out the encoded bitstream.
            bufIndex = AMediaCodec_dequeueOutputBuffer(m_VideoEncoder, &BufInfo, -1ll);
            if (bufIndex < 0)
            {
                return MCODEC_ERROR;
            }
            
            //Get an output buffer, with the buffer index that previously obtained.
            outputBuffer = AMediaCodec_getOutputBuffer(m_VideoEncoder, bufIndex, &BufSize);
            if (outputBuffer == NULL)
            {
                return MCODEC_ERROR;
            }
        }
    }
    
    //there is bitstream in the encoder buffer, save it to output buffer.
    if (BufInfo.size > 0)
    {
        uint8_t *src_buf = outputBuffer;
        uint8_t *dst_buf = pBsLayer->pBsBuf;
        uint8_t *sps_buf = (uint8_t *)m_SpsPpsHeader;
        int32_t layerId  = m_InitParams.nSpatialId;
        int32_t nal_type = src_buf[4] & 0x1F;
        
        //Define the default SVC prefix NAL unit data, for H264/AVC.
        uint8_t SvcPrefixCode[12] = {0x00, 0x00, 0x00, 0x01, 0x0E, 0x80, 0x80, 0x07, 0x20};
        int32_t SvcPrefixLen = 9;
        
        //Update the IDR_type and Spatial layerId in SVC prefix.
        if ((nal_type == 7) || (nal_type == 5))
        {
            SvcPrefixCode[4] |= 0x60;
            SvcPrefixCode[5] |= 0x40;
            SvcPrefixCode[6] |= ((layerId << 4) & 0x70);
        }
        else
        {
            SvcPrefixCode[4] |= 0x20;
            SvcPrefixCode[6] |= ((layerId << 4) & 0x70);
        }
        
        //For IDR frame without SPS/PPS, copy them and output NAL data.
        if (nal_type == 5)
        {
            int32_t layer_length = 0;
            int32_t sps_length = m_SpsPpsLength;
            
            //Save the encoded SPS/PPS and NAL data to output buffer.
            memcpy(dst_buf, sps_buf + 4, sps_length - 4);
            layer_length += sps_length - 4;
            
            memcpy(dst_buf + layer_length, SvcPrefixCode, SvcPrefixLen);
            layer_length += SvcPrefixLen;
            
            memcpy(dst_buf + layer_length, src_buf, BufInfo.size);
            layer_length += BufInfo.size;
            
            //Save the spatial layer encoded parameters to output buffer.
            pBsLayer->iNalCount = 2;
            pBsLayer->pNalLengthInByte[0] = sps_length - 4;
            pBsLayer->pNalLengthInByte[1] = BufInfo.size + SvcPrefixLen;
            pBsLayer->eFrameType = videoFrameTypeIDR;
            
            pBsLayer->uiTemporalId = (SvcPrefixCode[7] >> 5) & 0x07;
            pBsLayer->uiQualityId  = (SvcPrefixCode[6]) & 0x0F;
            pBsLayer->uiSpatialId  = layerId;
            pBsLayer->uiLayerType  = 1;
        }
        
        //Copy the P bitstream to output buffer without SPS/PPS.
        else
        {
            int32_t layer_length = 0;
            
            //Save the encoded NAL data to the output buffer.
            memcpy(dst_buf, SvcPrefixCode + 4, SvcPrefixLen - 4);
            layer_length += SvcPrefixLen - 4;
            
            memcpy(dst_buf + layer_length, src_buf, BufInfo.size);
            layer_length += BufInfo.size;
            
            //Save the spatial layer encoded parameters to output buffer.
            pBsLayer->iNalCount = 1;
            pBsLayer->pNalLengthInByte[0] = layer_length;
            pBsLayer->eFrameType = videoFrameTypeP;
            
            pBsLayer->uiTemporalId = (SvcPrefixCode[7] >> 5) & 0x07;
            pBsLayer->uiQualityId  = (SvcPrefixCode[6]) & 0x0F;
            pBsLayer->uiSpatialId  = layerId;
            pBsLayer->uiLayerType  = 1;
        }
        
        //release the output bitstream buffer, for the next encoding.
        AMediaCodec_releaseOutputBuffer(m_VideoEncoder, bufIndex, false);
        
        //succeed to output bitstream, return state code.
        return MCODEC_SUCCEED;
    }
    
    //release the output bitstream buffer, for the next encoding.
    AMediaCodec_releaseOutputBuffer(m_VideoEncoder, bufIndex, false);
    
    //succeed to encode this frame, but no bitstream to output.
    return MCODEC_ERROR;
}

/////////////////////////////////////////////////////////////////////////////////////
int32_t InsertKeyFrame(void)
{
    //if the MSDK device was not opened, do nothing and exit.
    if (m_CodecInitFlag == 0)
    {
        return MCODEC_ERROR;
    }
    
    //With API between 21~25, setParameters() do not supportted.
    if (MCODEC_SUCCEED != CloseEncoder())
    {
        return MCODEC_ERROR;
    }
    
    //Open the encoder again, which will only generate an IDR frame.
    if (MCODEC_SUCCEED != OpenEncoder(&m_InitParams))
    {
        return MCODEC_ERROR;
    }
    
    //Succeed to set IDR frame type, return the results.
    return MCODEC_SUCCEED;
}

/////////////////////////////////////////////////////////////////////////////////////
int32_t UpdateBitrate(uint32_t Bitrate, uint32_t Framerate)
{
    //if the MSDK device was not opened, do nothing and exit.
    if (m_CodecInitFlag == 0)
    {
        return MCODEC_ERROR;
    }
    
    //both the input parameters should not be zero.
    if ((Bitrate == 0) && (Framerate == 0))
    {
        return MCODEC_ERROR;
    }
    
    //With API between 21~25, setParameters() do not supportted.
    if (MCODEC_SUCCEED != CloseEncoder())
    {
        return MCODEC_ERROR;
    }
    
    //Update the target bitrate of video encoder, only for CBR.
    m_InitParams.nTargetKbps = Bitrate;
    m_InitParams.nFrameRate = Framerate;
    
    //Open the encoder again, which will generate an IDR frame.
    if (MCODEC_SUCCEED != OpenEncoder(&m_InitParams))
    {
        return MCODEC_ERROR;
    }
    
    //Succeed to update target bitrate, return the results.
    return MCODEC_SUCCEED;
}







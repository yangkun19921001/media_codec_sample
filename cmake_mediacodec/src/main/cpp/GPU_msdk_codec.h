/////////////////////////////////////////////////////////////////////////////////////
#ifndef __GPU_MSDK_CODEC_H__
#define __GPU_MSDK_CODEC_H__

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <new>

#include "media/NdkMediaError.h"
#include "media/NdkMediaFormat.h"
#include "media/NdkMediaCodec.h"

#include "include/GPU_codec_api.h"

/////////////////////////////////////////////////////////////////////////////////////
class CMSDKEncoder : public VM_MSDKEncoder
{
public:
    CMSDKEncoder(void);
    ~CMSDKEncoder(void);
    
    //Create the Intel MSDK encoder pipeline, and configure it.
    virtual int32_t OpenEncoder(MSdkInputParam *InputParam);
    
    //Delete the Intel MSDK encoder and release memory.
    virtual int32_t CloseEncoder(void);
    
    //Encode a frame asynchronously, without outputing bitstream.
    virtual int32_t EncodeFrame(SSourcePicture* pSrcPic, SLayerBSInfo* pBsLayer);
    
    //Synchronize the encoder and output bitstream data.
    virtual int32_t GetBitstream(SLayerBSInfo* pBsLayer);
    
    //Update the target bitrate online of specified pipeline.
    virtual int32_t UpdateBitrate(uint32_t Bitrate, uint32_t Framerate);
    
    //Request to encoder the current frame as IDR frame.
    virtual int32_t InsertKeyFrame(void);
    
private:
    
    //the local control parameters for the MSDK encoder.
    AMediaCodec*           m_VideoEncoder;
    AMediaFormat*          m_VideoFormat;
    MSdkInputParam         m_InitParams;
    uint32_t               m_CodecInitFlag;
    uint32_t               m_ForDatashare;
    uint32_t               m_nFramesProcessed;
    uint32_t               m_SpsPpsLength;
    uint32_t               m_SpsPpsHeader[64];
};

#endif  // End of __GPU_MSDK_CODEC_H__

/////////////////////////////////////////////////////////////////////////////////////

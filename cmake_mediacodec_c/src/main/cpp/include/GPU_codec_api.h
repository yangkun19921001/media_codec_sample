/////////////////////////////////////////////////////////////////////////////////////
#ifndef __GPU_CODEC_API_H__
#define __GPU_CODEC_API_H__

#include "stdio.h"
#include "stdint.h"
#include "codec_app_def.h"

#ifdef INTELHWCODEC_EXPORTS
#define INTELHWCODEC_DLLEXPORT _declspec(dllexport)
#elif INTELHWCODEC_DLL
#define INTELHWCODEC_DLLEXPORT _declspec(dllimport)
#else
#define INTELHWCODEC_DLLEXPORT
#endif

#define SYSTEM_MEMORY              0
#define D3D9_MEMORY                1
#define D3D11_MEMORY               2

#define MCODEC_SUCCEED             0
#define MCODEC_ERROR              -1

#define VIDEO_CODEC_TYPE_AVC       0
#define VIDEO_CODEC_TYPE_MJPEG     1
#define VIDEO_CODEC_TYPE_HEVC      2
#define VIDEO_CODEC_TYPE_VC1       3

//the Intel MSDK encoder single pipeline interface parameters.
typedef struct
{
    uint32_t  InStreamType;      // Input color format
    uint32_t  InFrameRate;       // Input Target encoding framerate
    uint32_t  InWidth;           // Input picture width
    uint32_t  InHeight;          // Input picture height
    
    uint32_t  nFrameRate;        // Target encoding framerate
    uint32_t  nWidth;            // Input picture width
    uint32_t  nHeight;           // Input picture height
    uint32_t  nTargetKbps;       // Target encoding bitrate
    uint32_t  nTemporalLayers;   // The number of temporal layers
    uint32_t  nSpatialId;        // the output spatial_id, 0~3.
    uint32_t  nMemType;          // the memory type for frame surface
    
    uint32_t  SpsLength;         // The incoming SPS nal_unit length
    uint32_t  PpsLength;         // The incoming PPS nal_unit length
    uint8_t   SpsNalUnit[200];   // The incoming SPS nal_unit data.
    uint8_t   PpsNalUnit[200];   // The incoming PPS nal_unit data.
    
}MSdkInputParam;

struct MSDKEncoder;


/////////////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
extern "C" {
#endif

INTELHWCODEC_DLLEXPORT struct MSDKEncoder *CreateEncoder(MSdkInputParam *InputParam);
INTELHWCODEC_DLLEXPORT void DeleteEncoder(struct MSDKEncoder *pMEncoder);
INTELHWCODEC_DLLEXPORT int32_t EncodeFrame(struct MSDKEncoder *pMEncoder, SSourcePicture* pSrcPic, SLayerBSInfo* pBsLayer);
INTELHWCODEC_DLLEXPORT int32_t GetBitstream(struct MSDKEncoder *pMEncoder, SLayerBSInfo* pBsLayer);
INTELHWCODEC_DLLEXPORT int32_t UpdateBitrate(struct MSDKEncoder *pMEncoder, uint32_t Bitrate, uint32_t Framerate);
INTELHWCODEC_DLLEXPORT int32_t InsertKeyFrame(struct MSDKEncoder *pMEncoder);
#ifdef __cplusplus
}
#endif



#endif  // End of __GPU_CODEC_API_H__

/////////////////////////////////////////////////////////////////////////////////////

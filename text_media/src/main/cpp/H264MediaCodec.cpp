#include <jni.h>
#include <string.h>
#include <stdio.h>


#include "com_yk_text_media_MainActivity.h"

#include "include/GPU_msdk_codec.h"
#include "include/codec_app_def.h"


unsigned char g_yuv_buffer[4096*1024];
unsigned char g_stream_buffer[4096*1024];
unsigned char g_encoder_buffer[2048*1024];

SSourcePicture _sSrcPic;
SLayerBSInfo   _sLayerData;

JNIEXPORT jstring JNICALL Java_com_yk_text_1media_MainActivity_getString
        (JNIEnv *env, jclass obj) {
    return env->NewStringUTF("123");
}


JNIEXPORT jint JNICALL Java_com_yk_text_1media_MainActivity_init
        (JNIEnv * ent, jclass obj){
    return 1;
}
/**
 **************************************************************************************
 * @file    mqtt_protocol.h
 * @brief   my mqtt
 * 
 * @author  Havens Tian
 * @version V1.0.0
 **************************************************************************************
 */

#ifndef __MQTT_PROTOCOL_H__
#define __MQTT_PROTOCOL_H__

#ifdef  __cplusplus
extern "C" {
#endif//__cplusplus

#include <stdint.h>

#define  FILE_NAME_MAX 128

typedef enum _MQTT_MSG_TYPE
{
	SYSTEM = 0x01,
	SONG,
} MQTT_MSG_TYPE_T;

typedef enum _MQTT_SONG_SUBTYPE
{
	SONG_PREVIEW = 0x01,
	SONG_NEXT,
	SONG_PLAY,
	SONG_PAUSE,
	SONG_STOP,
} MQTT_SONG_SUBTYPE_T;

typedef enum _MQTT_SYSTEM_SUBTYPE
{
	POWER_ON = 0x01,
	POWER_OFF,
	UPGRADE,
} MQTT_SYSTEM_SUBTYPE_T;

typedef struct _mqtt_msg_protocol
{
	uint8_t *device_sn;
	uint16_t msg_len;
	uint8_t type;
	uint8_t subtype;
	uint8_t *data;
} mqtt_msg_protocol_t;

typedef struct wav_header
{
    uint8_t   riff_type[4];    //4byte,资源交换文件标志:RIFF
    uint32_t  riff_size;       //4byte,从下个地址到文件结尾的总字节数
    uint8_t   wave_type[4];    //4byte,wave文件标志:WAVE
    uint8_t   format_type[4];  //4byte,波形文件标志:FMT

    uint32_t  format_size;     //4byte,音频属性(compressionCode,numChannels,sampleRate,bytesPerSecond,blockAlign,bitsPerSample)所占字节数
    uint16_t  compression_code;//2byte,编码格式(1-线性pcm-WAVE_FORMAT_PCM,WAVEFORMAT_ADPCM)
    uint16_t  num_channels;    //2byte,通道数
    uint32_t  sample_rate;     //4byte,采样率
    uint32_t  bytes_per_second; //4byte,传输速率
    uint16_t  block_align;     //2byte,数据块的对齐
    uint16_t  bits_per_sample;  //2byte,采样精度
    uint8_t   data_type[4];    //4byte,数据标志:data
    uint32_t  data_size;       //4byte,从下个地址到文件结尾的总字节数，即除了wav header以外的pcm data length
} wav_header_t;


#ifdef  __cplusplus
}
#endif//__cplusplus

#endif//__MQTT_PROTOCOL_H__

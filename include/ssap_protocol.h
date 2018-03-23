/**
 **************************************************************************************
 * @file    ssap_protocol.h
 * @brief   SPI Stream Audio Player Protocol
 * 
 * @author  Aissen Li
 * @version V1.0.0
 *
 * &copy; Shanghai Mountain View Silicon Technology Co.,Ltd. All rights reserved.
 **************************************************************************************
 */

#ifndef __SSAP_PROTOCOL_H__
#define __SSAP_PROTOCOL_H__

#ifdef  __cplusplus
extern "C" {
#endif//__cplusplus

#include <stdint.h>

#define SSAP_SEND_DATA_LENGTH       (512)
#define SSAP_SEND_DATA_BLOCKSIZE    (32)
#define SSAP_SYNC_WORD_LENGTH       (1)
#define SSAP_CRC_VALUE_LENGTH       (2)
#define SSAP_RESEND_IF_CRC_ERROR    (1)

void spim_test(void);
void web_downlaod_song_test(void);

#define  FILE_NAME_MAX 128
#define  SPIS_RESPONSE_TOKEN (0x55)
#define MAX_SUPPORT_FILE_TYPE_NUMBER 3

/**
 * @brief SPI Stream Audio Player Protocol Command Set
 *        1. For Reduced Instruction Set, see the context SSAPCmdContext when SSAP_SYNC_WORD_LENGTH equal 1.
 *        2. For Complex Instruction Set, see the context SSAPCmdContext when SSAP_SYNC_WORD_LENGTH equal 4.
 *        
 * @Note  For those commands with few content (less than 2 bytes) or without content, the content data
 *        just fill in the SSPPCmdContext.Content field.
 *        For those commands with large content (more than 2 bytes), such as command SSPP_CMD_DATA, first
 *        fill in the SSPPCmdContext.Content field with the length of content data, and then send a content
 *        data packet with the format as follow:
 *        SYNC_WORD + content_data + CRC
 */

#if 0
typedef enum {
	MONO = 1,
	STEREO,
	MAX_CHANNEL_NUMBER
} player_channels_t;

typedef enum {
	8_BIT,
	16_BIT,
	32_BIT
} player_resulotion_t;
#endif

typedef enum _audio_playing_state
{
	AUDIO_IDLE = 0x01,
	AUDIO_START,
	AUDIO_PLAY,
	AUDIO_PAUSE,
	AUDIO_STOP,
	AUDIO_FINISH,
} AUDIO_PLAYING_STATE_T;

typedef enum _sofa_command
{
	SOFA_CMD_STOP = 0,
	SOFA_CMD_HOME = 1,
	SOFA_CMD_OPEN = 2,
	SOFA_CMD_CLOSE = 4,
	SOFA_CMD_HEAD_UP = 8,
	SOFA_CMD_HEAD_DOWN = 16,
	SOFA_CMD_WAKEUP = 17,
	UNKNOW_SOFA_CMD = 18,
} SOFA_CMD;

typedef enum SSPP_CMD_SONG_PATH_MODE
{
	SONG_PATH_MODE_UNKNOW=0,
	SONG_PATH_MODE_USB,//1
	SONG_PATH_MODE_AUX,//2
	SONG_PATH_MODE_WIFI,//3
	SONG_PATH_MODE_BT,//4
} SONG_PATH_MODE;

typedef enum _DecoderType
{
    UNKOWN_DECODER               = 0,       /**< Unknown decoder                     */
    PURE_SOFTWARE_DECODERS       = 1,       /**< Pure software decoders as follows   */
    WAV_DECODER,                            /**< WAV  decoder */
    FLAC_DECODER,                           /**< FLAC decoder */
    AAC_DECODER,                            /**< AAC  decoder */
    AIF_DECODER,                            /**< AIFF and AIFC decoder */
    AMR_DECODER,                            /**< AMR  decoder */
	PCM_DECODER,
    WITH_HARDWARE_DECODERS       = 128,     /**< Decoders with hardware as follows   */
    MP3_DECODER,                            /**< MP3  decoder */
    WMA_DECODER,                            /**< WAM  decoder */
    SBC_DECODER,                            /**< SBC  decoder */
    MSBC_DECODER,                           /**< mSBC decoder */
} DecoderType;

typedef enum _SSPP_CMD
{
	SSAP_CMD_UNKOWN = 0,
	SSAP_CMD_START,//1
	SSAP_CMD_DATA,//2
	SSAP_CMD_STOP,
	SSAP_CMD_TOKEN,//4
	SSPP_CMD_SOFA_COMMAND, //5
	SSPP_CMD_SONG_PATH_MODE,
	SSPP_CMD_MAX,
} SSPP_CMD;

typedef enum __SSAP_CMD_RESPONSE
{
    SSAP_CMD_RESP_UNKOWN = 0,
/* sofa and song_path_mode command:*/
    SSAP_CMD_RESP_OKCMD,//1
    SSAP_CMD_RESP_ERCMD,//2
/* data and start command: */
    SSAP_CMD_RESP_OKSEND,//3
    SSAP_CMD_RESP_RESEND,//4
    SSAP_CMD_RESP_NEXTSEND,
/* token command: mode */
	SSAP_CMD_RESP_MODE_WIFI,
	SSAP_CMD_RESP_MODE_OTHER,
/* token command: song control */
	SSAP_CMD_RESP_SONG_PAUSE,
	SSAP_CMD_RESP_SONG_STOP,//3
	SSAP_CMD_RESP_SONG_PLAY,
	SSAP_CMD_RESP_SONG_LAST,
	SSAP_CMD_RESP_SONG_NEXT,
	SSAP_CMD_RESP_SONG_FF,
	SSAP_CMD_RESP_SONG_FB,
/* defaul */
    SSAP_CMD_RESP_TIME_OUT,//6
} SSAP_CMD_RESPONSE;

#if   (SSAP_SYNC_WORD_LENGTH == 1)

#define SSAP_SYNC_BYTE              'S'
#define SSAP_SYNC_WORD              'S'    //SPI Stream Player Protocol SyncWord
#define SSAP_SYNC_WORD_BE           'S'
#define SSAP_SYNC_WORD_LE           'S'

typedef struct _SSAPCmdContext
{
    uint8_t  sync_word;
    uint8_t  command;
    uint16_t content;
    uint16_t crc_value;
}SSAPCmdContext;

typedef struct _SSAPCmdResponseContext
{
    uint8_t  sync_word;
    uint8_t  command;
    uint16_t response;
    uint16_t crc_value;
}SSAPCmdResponseContext;

#elif (SSAP_SYNC_WORD_LENGTH == 4)

#define SSAP_SYNC_BYTE              'S'
#define SSAP_SYNC_WORD              'SSAP' //SPI Stream Player Protocol SyncWord
#define SSAP_SYNC_WORD_BE           'SSAP'
#define SSAP_SYNC_WORD_LE           'PASS'

typedef struct _SSAPCmdContext
{
    uint32_t sync_word;
    uint16_t command;
    uint16_t content;
    uint16_t reserved;
    uint16_t crc_value;
}SSAPCmdContext;

typedef struct _SSAPCmdResponseContext
{
    uint32_t sync_word;
    uint16_t command;
    uint16_t response;
    uint16_t reserved;
    uint16_t crc_value;
}SSAPCmdResponseContext;

#endif

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

typedef enum pcm_param
{
	CHANNELS_PARAM = 1,
	SAMPLE_RATE_PARAM,
	RESOLUTION_PARAM
} pcm_param_t;


/* protocol func */
void print_microsecond_time(uint32_t print_count);

SSAPCmdResponseContext spis_respones_command(void);

char *extract_str_right(char *dst,char *src, int n);

void spim_token_thread(void);

/* local play func */

uint32_t get_file_size(const char *path);

void *spim_stream_audio_player_process(void);

/* online play func */
void web_download_thread(uint8_t *audio_play_url);

void download_data_to_spim_process_thread(void);

void set_sofa_command_thread(void);

void change_play_song_mode_thread(void);

SSAPCmdResponseContext spim_send_command_data(uint8_t *data_buffer,
			uint32_t buffer_len, uint8_t data_command, uint32_t slice_len);

/* pcm2wave */
void send_pcm2wave_header(void);


#ifdef  __cplusplus
}
#endif//__cplusplus

#endif//__SSAP_PROTOCOL_H__

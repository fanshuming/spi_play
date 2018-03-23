#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#include "user_voice.h"
#include "ssap_protocol.h"
#include "log.h"
#include "spim.h"
#include "play_link.h"
#include "queue.h"

/********constants*********/
const char* loginParams 	 = "appid = 59929f37, work_dir = .";//登录参数,appid与msc库绑定,请勿随意改动
#define	USER_TOPQIZHI	loginParams

#define TTS_AUDIO_DATA_MAX_SIZE 48000

/*
* See "iFlytek MSC Reference Manual"
*/
const char* iatSessionBeginParams =
	"sub = iat, domain = iat, language = zh_cn, "
	"accent = mandarin, sample_rate = 16000, "
	"result_type = json, result_encoding = utf8, "
	"sch = 1,nlp_version = 3.0,scene=main ";
const char* ttsSessionBeginParams = 
	"voice_name = xiaoyan, text_encoding = utf8, sample_rate = 16000, "
	"speed = 50, volume = 50, pitch = 50, rdn = 0";
//const char* ttsSessionBeginParams = "voice_name = xiaoyan, text_encoding = utf8, sample_rate = 16000, speed = 50, volume = 50, pitch = 50, rdn = 2";
//const char* ttsSessionBeginParams = "voice_name = xiaomeng, text_encoding = utf8, sample_rate = 16000, speed = 50, volume = 50, pitch = 50, rdn = 0";

//const char* demoText1 = "亲爱的用户，您好，这是一个语音合成示例，感谢您对科大讯飞语音技术的支持！科大讯飞是亚太地区最大的语音上市公司，股票代码：002230"; //合成文本
//const char* demoText2 = "这套解决方案最大的特点是约60秒内可以测量出用户的心率、心率变异、血压趋势、血氧饱和度、心电图、光体积脉搏波图等六项生理数据。相比先前分散式的解决方案，MediaTek Sensio的硬件模组将前端信号处理、多种传感器等电、光学元件高度整合在一起，可直接嵌入到智能手机及配件等终端设备里。";

const char* ttsSessionID = NULL;
tts_audio_t ttsAudio;
tts_audio_t *p_ttsAudio = &ttsAudio;

sem_t sem_tts_audio_get;
sem_t sem_tts_audio_over;
sem_t sem_tts_audio_play;
pthread_t	thread_ttsAduioGet;

play_link_t *ttsLinkHead = NULL;
unsigned int ttsLinkDataSize = 0;

static sequeue_t *tts_audio_data_queue;

static pthread_mutex_t tts_play_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool quadratic_wakeup_flag = false;
static bool tts_audio_data_start_flag = false;
static bool tts_audio_data_end_flag = false;

static pthread_mutex_t quadratic_wakeup_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tts_audio_data_start_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tts_audio_data_end_mutex = PTHREAD_MUTEX_INITIALIZER;

/* add by fengtian */
static wav_header_t wave_header = { 
    .riff_type = "RIFF",
    .riff_size = 0x000804a4,
    .wave_type = "WAVE",
    .format_type = "fmt ",

    .format_size = 0x10,
    .compression_code = 1, //WAVE_FORMAT_PCM
    .num_channels = 1,
    .sample_rate = 16000,
    .bytes_per_second = 32000, //通道数×每秒数据位数×每样本的数据位数/8

    .block_align = 0x02,
    .bits_per_sample = 16, 
    .data_type = "data",
    .data_size = 0x00080480,
};


extern void tts_audio_init(void){
	p_ttsAudio->ttsAudioBuf = NULL;
	p_ttsAudio->ttsAudioBufLen = 0;
	p_ttsAudio->ttsSynthStatus = MSP_TTS_FLAG_DATA_END;	
}

/* add by fengtian begain */
static uint32_t tts_audio_start_command_send(uint32_t file_data_size)
{
	uint32_t ret = 0;
	uint32_t start_buffer_size = 0;
	uint8_t  start_command_buffer[256] = {0};
	uint8_t *tts_name = "demo.pcm";
	//uint32_t decoder_type = WAV_DECODER; /* change pcm to wav, ignore */
	uint32_t decoder_type = MP3_DECODER; /* mp3 */

	LOGD("enter tts_audio_start_command_send \n");

	/* fill pcm to wav */
	wave_header.data_size = file_data_size;
	wave_header.riff_size = file_data_size + sizeof(wav_header_t);

	/* fill start command */
	memcpy(&start_command_buffer, &decoder_type, 4);
	memcpy(&start_command_buffer[4], tts_name, strlen(tts_name));
	start_buffer_size = 4 + strlen(tts_name);

	/* send start command */
	spim_send_command_data(start_command_buffer, start_buffer_size, SSAP_CMD_START, SSAP_SEND_DATA_LENGTH);

	/* send pcm head */
	//spim_send_command_data((uint8_t *)&wave_header, sizeof(wav_header_t), SSAP_CMD_DATA, SSAP_SEND_DATA_LENGTH);

	return ret;
}
/* add by fengtian end */

void play_local_pcm(uint8_t *file_name)
{
	uint32_t play_audio_len = 64000;
	FILE *wakeup_pcm_fp;
	uint8_t read_pcm_frame[512] = {0};
#if 1
	LOGD("enter play_local_pcm, file_name=%s\n", file_name);
	tts_audio_start_command_send(play_audio_len);

	wakeup_pcm_fp = fopen(file_name, "r");
	if(wakeup_pcm_fp == NULL) {
		LOGE("open pcm file error\n");
	}
	while(1) {
		play_audio_len = fread(read_pcm_frame, 1, SSAP_SEND_DATA_LENGTH, wakeup_pcm_fp);
		if(play_audio_len <= 0) {
			LOGD("wakeup audio play end.\n");
			break;
		} else {
			//LOGD("play local pcm, audio len=%d\n", play_audio_len);
			spim_send_command_data(read_pcm_frame, play_audio_len, SSAP_CMD_DATA, SSAP_SEND_DATA_LENGTH);
		}
	}

	fclose(wakeup_pcm_fp);
#endif
}

void wakeup_audio_play(void)
{
	uint8_t *pcm_file_name = "/root/wakeup.pcm";

	play_local_pcm(pcm_file_name);
}

void sofa_response_audio_play(void)
{
	uint8_t *pcm_file_name = "/root/response.pcm";

	play_local_pcm(pcm_file_name);
}

void wrt_open_audio_play(void)
{
	//uint8_t *pcm_file_name = "/root/wrt_open.pcm";
	uint8_t *pcm_file_name = "/root/test.mp3";

	play_local_pcm(pcm_file_name);
}


void set_quadratic_wakeup_flag(bool flag)
{
	pthread_mutex_lock(&quadratic_wakeup_mutex);
	quadratic_wakeup_flag = flag;
	LOGD("quadratic_wakeup_flag=%d\n", quadratic_wakeup_flag);
	pthread_mutex_unlock(&quadratic_wakeup_mutex);
}


extern unsigned char* is_tts_have_data(void){
	if(p_ttsAudio->ttsAudioBuf)
		return p_ttsAudio->ttsAudioBuf;
	else
		return NULL;
}

extern int tts_get_audio_len(void){
	return p_ttsAudio->ttsAudioBufLen;
}

extern int tts_get_audio_status(void){
	return p_ttsAudio->ttsSynthStatus;
}

static int tts_audio_mem_free(void){
	if(p_ttsAudio->ttsAudioBuf){
		free(p_ttsAudio->ttsAudioBuf);
		p_ttsAudio->ttsAudioBuf = NULL;
	}
	return 0;
}

static int tts_audio_destroy(void){
	free(p_ttsAudio->ttsAudioBuf);
	p_ttsAudio->ttsAudioBuf = NULL;
	p_ttsAudio->ttsAudioBufLen = 0;
	p_ttsAudio->ttsSynthStatus = MSP_TTS_FLAG_DATA_END;

	return 0;
}

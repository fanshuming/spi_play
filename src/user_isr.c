/*
* 语音听写(iFly Auto Transform)技术能够实时地将语音转换成对应的文字。
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "user_voice.h"
#include "speech_recognizer.h"
#include "linuxrec.h"
#include "json.h"
#include "cJSON.h"
#include "log.h"
#include "ssap_protocol.h"


#define FRAME_LEN	640g
#define	BUFFER_SIZE	4096
#define JSON_DEBUG

/* 默认wav音频头部数据 */
wave_pcm_hdr default_wav_hdr =
{
	{ 'R', 'I', 'F', 'F' },
	0,
	{'W', 'A', 'V', 'E'},
	{'f', 'm', 't', ' '},
	16,
	1,
	1,
	16000,
	32000,
	2,
	16,
	{'d', 'a', 't', 'a'},
	0
};

/*
* See "iFlytek MSC Reference Manual"
*/
const char* isrSessionBeginParams =
	"sub = iat, domain = iat, language = zh_cn, "
	"accent = mandarin, sample_rate = 16000, "
	"result_type = json, result_encoding = utf8, "
	"vad_eos = 1000, "
	"sch = 1,nlp_version = 3.0,scene=main ";
const char *isrSessionID = NULL;

static uint8_t *demo_text = "说真话不应当是艰难的事情。我所谓真话不是指真理，也不是指正确的话。自己想什麽就讲什麽；自己怎麽想就怎麽说这就是说真话";

sem_t sem_mic_wakeup;
sem_t sem_put_next_tts_text;
sem_t sem_url_hint_tone;

pthread_t thread_isrRecord;

isrResult_t isrJsonResult;
isrResult_t *p_isrJsonResult = &isrJsonResult;
static bool audio_play_flag = false;

#define RECORDTIME	10

static void show_rec_result(char *string, char is_over)
{
	LOGD("\rResult: [ %s ]", string);
	if(is_over)
		putchar('\n');
}

static char *g_result = NULL;
static unsigned int g_buffersize = BUFFER_SIZE;

void on_rec_result(const char *result, char is_last)
{
	if (result) {
		size_t left = g_buffersize - 1 - strlen(g_result);
		size_t size = strlen(result);
		if (left < size) {
			g_result = (char*)realloc(g_result, g_buffersize + BUFFER_SIZE);
			if (g_result)
				g_buffersize += BUFFER_SIZE;
			else {
				LOGD("mem alloc failed\n");
				return;
			}
		}
		strncat(g_result, result, size);
		show_rec_result(g_result, is_last);
	}
}

void on_rec_begin()
{
	if (g_result)
	{
		free(g_result);
	}
	g_result = (char*)malloc(BUFFER_SIZE);
	g_buffersize = BUFFER_SIZE;
	memset(g_result, 0, g_buffersize);

	LOGD("Start Listening...\n");
}

void on_rec_end(int reason)
{
	if (reason == END_REASON_VAD_DETECT)
		LOGD("\nSpeaking done with VAD detect\n");
	else
		LOGD("\nRecognizer error %d\n", reason);
}

void post_wakeup(void)
{
	LOGD("[SEMAPHORE]:post wake up\n");
	sem_post(&sem_mic_wakeup);
}

void sem_wakeup_init(void)
{
	sem_init(&sem_mic_wakeup, 0, 0);
	sem_init(&sem_put_next_tts_text, 0, 0);
	sem_init(&sem_url_hint_tone, 0, 0);
}

void post_next_tts_sem(void)
{
	LOGD("[SEMAPHORE]:post next tts\n");
	sem_post(&sem_put_next_tts_text);
	if(audio_play_flag) {
		LOGD("[SEMAPHORE]:post url hint tone\n");
		sem_post(&sem_url_hint_tone);
	}
}

void post_url_hint_tone_sem(void)
{
	LOGD("[SEMAPHORE]:post next tts\n");
	sem_post(&sem_url_hint_tone);
}

#define SOFA_COMMAND_MAX_NUMBER 6
#define SOFA_COMMAND_MAX_INDEX 3
static uint8_t *sofa_command_compare_table[SOFA_COMMAND_MAX_NUMBER][SOFA_COMMAND_MAX_INDEX] = {
	{"停止", "停下来", "暂停"},
	{"复位"},
	{"打开沙发", "打开上方", "打开下吧"},
	{"关闭沙发", "打开下方"},
	{"头部打开"},
	{"头部关闭"},
};

static uint8_t *sofa_command_response_table[SOFA_COMMAND_MAX_NUMBER] = {
	"停止",
	"复位",
	"正在打开沙发",
	"正在关闭沙发",
	"正在头部打开",
	"正在头部关闭",
};

uint32_t handle_sofa_cmd(uint8_t *json_command)
{
	uint32_t ret = -1;
	uint8_t sofa_send_nuber = 0;
	uint8_t sofa_send_nuber_index = 0;
	bool sofa_send_nuber_flag = false;

#if 1
	/* hint audio */
	//sofa_response_audio_play();
	LOGD("handle_sofa_cmd=%s\n", json_command);
	for(sofa_send_nuber = 0; sofa_send_nuber < SOFA_COMMAND_MAX_NUMBER; sofa_send_nuber ++) {
		for(sofa_send_nuber_index = 0; sofa_send_nuber_index < SOFA_COMMAND_MAX_INDEX; sofa_send_nuber_index++) {
			if(NULL == sofa_command_compare_table[sofa_send_nuber][sofa_send_nuber_index]) {
				continue;
			}
			//LOGD("sofa_command_compare_table[%d][%d]=%s\n",
			//	sofa_send_nuber, sofa_send_nuber_index, sofa_command_compare_table[sofa_send_nuber][sofa_send_nuber_index]);
			if(strstr(json_command, sofa_command_compare_table[sofa_send_nuber][sofa_send_nuber_index])) {
				if(sofa_send_nuber) {
					sofa_send_nuber = (1<<(sofa_send_nuber-1));
				}
				LOGD("send sofa commad=%d\n", sofa_send_nuber);
				ret = sofa_send_nuber;
				spim_send_command_data(&sofa_send_nuber, 1, SSPP_CMD_SOFA_COMMAND, SSAP_SEND_DATA_LENGTH);
				sofa_send_nuber_flag = true;
				break;
			}
		}
		if(sofa_send_nuber_flag) {
			sofa_send_nuber_flag = false;
			LOGD("sofa commad=%d\n", sofa_send_nuber);
			break;
		}
	}
#endif
	return ret;
}

void handle_isr_result(struct speech_rec isr)
{
	int i = 0;
	uint32_t ret = 0;
	isrResult_t json_isrResult_tmp;
	isrResult_t *json_isrResult = &json_isrResult_tmp;

#if 1
		/* handle isr result */
		/* get json */
		isr_get_json_answer_txt(json_isrResult);
		//LOGD("json_isrResult isrType=%d, len=%d\n",
		//		json_isrResult->isrType, strlen(json_isrResult->isrResult));
		isr_get_json_data_result_url(json_isrResult);
		LOGD("json_isrResult->url=%s, len=%d\n", json_isrResult->url, strlen(json_isrResult->url));
		if(strlen(json_isrResult->url)) {
			audio_play_flag = true;
		}
		switch(json_isrResult->isrType) {
			case text:
				ret = handle_sofa_cmd(json_isrResult->isrResult);
				LOGD("sofa_send_nuber ret = %d\n", ret);
				if(ret != -1) {
#if 1
					sr_start_listening(&isr);
					LOGD("second time isr.ep_stat=%d\n", isr.ep_stat);
					while(isr.ep_stat != MSP_EP_AFTER_SPEECH) {
						i++;
						if(i > RECORDTIME) {
							LOGE("record time out ...\n");
							break;
						}
						sleep(1);
					}
					i = 0;
					sr_stop_listening(&isr);

					isr_get_json_answer_txt(json_isrResult);
					//LOGD("second time json_isrResult=%s, isrType=%d, len=%d\n",
					//		json_isrResult->isrResult, json_isrResult->isrType, strlen(json_isrResult->isrResult));

					ret = handle_sofa_cmd(json_isrResult->isrResult);
					LOGD("sofa_send_nuber ret = %d\n", ret);
#endif
				}
				/* wait for sem post */
				if(sem_wait(&sem_put_next_tts_text) != 0) {
					LOGE("sem_put_next_tts_text wait error!\n");
				}
				//LOGD("tts json=%s\n", json_isrResult->isrResult);
				/* upload , and post sem to tts */
				ret = tts_text_put(json_isrResult->isrResult);
				if(0 != ret) {
					LOGE("tts text put failed!\n");
					return -1;
				}
				if(strlen(json_isrResult->url)) {
					LOGD("wait sem_url_hint_tone\n");
					sem_wait(&sem_url_hint_tone);
					LOGD("set play url\n");
					set_online_play_url(json_isrResult->url);
					audio_play_flag = false;
				}
				break;
			case cmd:
				/* send to sofa through spi */
				LOGD("send cmd");
				//FIXME
				break;
			default:
				LOGE("no json command\n");
				break;
		}

		/* free */
		isr_free_result(json_isrResult);
#endif
}


void isr_voice_record_thread_proc(void)
{
	uint32_t ret = 0;
	int errCode;
	int i = 0;

	struct speech_rec isr;
    struct sigaction act, oact;

	struct speech_rec_notifier recNotifier = {
		on_rec_result,
		on_rec_begin,
		on_rec_end
	};


#if 1
	errCode = sr_init(&isr, isrSessionBeginParams, SR_MIC, &recNotifier);
	if(0 != errCode){
		dbg("speech recognizer init failed\n");
		return NULL;
	}
#endif

	/* first time notify upload isr result json */
	post_next_tts_sem();

	LOGD("run isr process ...\n");
	while(1) {
		/* mic wakeup */
		ret = sem_wait(&sem_mic_wakeup);
		if(ret != 0) {
			LOGE("sem_wait error, %d\n", ret);
			continue;
		}

		LOGD("tts_get_audio_status=%d\n", tts_get_audio_status());

		if(MSP_TTS_FLAG_STILL_HAVE_DATA == tts_get_audio_status()) {
			LOGD("set_quadratic_wakeup_flag here .....\n");
			set_quadratic_wakeup_flag(true);
			sleep(1);
		}

		//wakeup_audio_play();
		//sleep(1);

#if 1
		/* start isr */
		sr_start_listening(&isr);

		while(isr.ep_stat != MSP_EP_AFTER_SPEECH) {
			i++;
			if(i > RECORDTIME) {
				LOGE("record time out ...\n");
				break;
			}
			sleep(1);
		}
		i = 0;

		sr_stop_listening(&isr);
		/* end isr */
#endif

		/* handle isr */
		handle_isr_result(isr);
		/* enable mic wake now */
		set_mic_enable(true);
	}

	sr_uninit(&isr);
}

int json_get_result(isrResult_t *json_result,char* asr_result)
{
	uint8_t error;
	json_t*	root        = NULL;
	json_t*	resultJson  = NULL;
	json_t* subResult  = NULL;
	char* 	originalStr = NULL;

	json_result->isrResult = NULL;
	json_result->isrType = text;

	error = json_parse_document(&root, asr_result);
	if(JSON_OK != error)
	{
		dbg("json_parse_document failure\n");
		goto json_error;
	}
	resultJson = json_find_first_label(root, "service");
	//LOGD("after json_get_result resultJson=%d\n", resultJson);
	if(NULL != resultJson)
	{
		originalStr = resultJson->child->text;

		dbg("service = %s\n",originalStr);

		if(
			   !strcmp(originalStr,"weather")
			|| !strcmp(originalStr,"musicX")
			|| !strcmp(originalStr,"story")
			|| !strcmp(originalStr,"datetime")
			|| !strcmp(originalStr,"chat")
			|| !strcmp(originalStr,"joke")
			|| !strcmp(originalStr,"radio")
			|| !strcmp(originalStr,"cmd")
			|| !strcmp(originalStr,"mapU")
			|| !strcmp(originalStr,"calc")
			|| !strcmp(originalStr,"light_smartHome")
		)
		{
			if(!strcmp(originalStr,"light_smartHome")){
				json_result->isrType = cmd;
			}
			else{
				json_result->isrType = text;
			}

			resultJson = json_find_first_label(root, "answer");
			if(NULL != resultJson)
			{
				subResult = json_find_first_label(resultJson->child, "text");

				if(NULL != subResult)
				{
					originalStr = subResult->child->text;

					error = JSON_OK;
					goto json_exit;
				}
				else
					goto json_error;

			}
			else
				goto json_error;
		}
		else
		{
            dbg("no service surported!\n");
			goto json_error;
		}
	}
	else
	{
	LOGD("enter json_find_first_label\n");
		resultJson = json_find_first_label(root, "text");
		if(NULL != resultJson)
		{
			originalStr = resultJson->child->text;

            error = JSON_OK;
			goto json_exit;
		}
	}
json_error:
	originalStr = "抱歉，没有找到您想要的信息！";

json_exit:
	json_result->isrResult = (char*)malloc(strlen(originalStr)+1);
	if(NULL != json_result->isrResult)
	{
		strcpy(json_result->isrResult,originalStr);
	}
	else
		error = JSON_FALSE;

	if (root!=NULL)
	{
		json_free_value(&root);
		root = NULL;
	}

	return error;
}

int isr_get_json_answer_txt(isrResult_t *p_isrResult)
{
	int ret;

	//LOGD("enter isr_get_json_answer_txt\n");
	ret = json_get_result(p_isrResult, g_result);
	if(JSON_OK != ret)
		return -1;
	return 0;
}

int isr_get_json_data_result_url(isrResult_t *json_result)
{
	uint8_t error;
	cJSON *root        = NULL;
	cJSON *resultJson  = NULL;
	cJSON *subResult  = NULL;
	cJSON *sub2Result  = NULL;
	cJSON *sub3Result  = NULL;
	uint8_t url[1024] = {0};
	uint8_t play_url_key[10] = {0};
	cJSON *json_server = NULL;

	//LOGD("isr_get_json_data_result_url, g_result=%s\n", g_result);
	if(NULL != g_result) {
		//LOGD("enter1\n");
		root = cJSON_Parse(g_result);
	}

	if(NULL != root) {
		json_server = cJSON_GetObjectItem(root, "service");
		if(NULL != json_server) {
			//LOGD("json_server str=%s\n", json_server->valuestring);
			if(!strcmp(json_server->valuestring, "radio")) memcpy(play_url_key, "url", 3);
			if(!strcmp(json_server->valuestring, "joke")) memcpy(play_url_key, "mp3Url", 6);
			if(!strcmp(json_server->valuestring, "story")) memcpy(play_url_key, "playUrl", 7);
			//LOGD("play_url_key=%s\n", play_url_key);
		}
	}

	if(NULL != root) {
		//LOGD("enter2\n");
		//LOGD("%s\n", cJSON_Print(root));
		resultJson = cJSON_GetObjectItem(root, "data");
	}

	if(NULL != resultJson) {
		//LOGD("enter3\n");
		//LOGD("resultJson=%s\n", cJSON_Print(resultJson));
		subResult = cJSON_GetObjectItem(resultJson, "result");
	}
	if(NULL != subResult) {
		//LOGD("enter4\n");
		//LOGD("subResult=%s\n", cJSON_Print(subResult));
		sub2Result = cJSON_GetArrayItem(subResult, 0);
	}

	if(NULL != sub2Result) {
		//LOGD("enter5\n");
		//LOGD("playUrl=%s\n", cJSON_Print(sub2Result));
		if(NULL != play_url_key) {
			sub3Result = cJSON_GetObjectItem(sub2Result, play_url_key);
		}
	}

	if(NULL != sub3Result) {
		//LOGD("enter6\n");
		//LOGD("playUrl=%s\n", cJSON_Print(sub3Result));
		//LOGD("sub3Result->valuestring = %s\n", sub3Result->valuestring);
		//LOGD("sub3Result->valuestring strlen=%d\n", strlen(sub3Result->valuestring));
		memcpy(url, sub3Result->valuestring, strlen(sub3Result->valuestring));
		LOGD("%s=%s\n", play_url_key, url);
	}
	json_result->url = (char *)malloc(strlen(url));
	if((NULL != json_result->url) && (NULL != url)) {
		memcpy(json_result->url, url, strlen(url)+1);
	}

	if(NULL != root) {
		cJSON_Delete(root);
	}
}

extern void isr_free_result(isrResult_t *p_isrResult)
{
	free(p_isrResult->isrResult);
	p_isrResult->isrResult = NULL;
	p_isrResult->isrType = text;
}

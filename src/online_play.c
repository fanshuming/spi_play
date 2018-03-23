#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "crc.h"
#include "ssap_protocol.h"
#include "string_convert.h"
#include "spim.h"
#include "queue.h"
#include "log.h"

#define SONG_FILE_NAME "test.mp3"
#define PLAY_URL_ADDR_MAX_SIZE 100

struct MemoryStruct {
  char *memory;
  size_t size;
};

static bool spim_connected_spis_flag = false;
static sequeue_t *song_data_queue;
static bool download_finish_flag = false;
static uint8_t  long_file_name[FILE_NAME_MAX] = {0};
static uint8_t play_url_addr[PLAY_URL_ADDR_MAX_SIZE] = {0};
static uint8_t *default_play_url_addr = "https://www.topqizhi.com/chengdu.mp3";
static bool set_url_addr_flag = false;
static uint32_t   decoder_type = MP3_DECODER;
static bool pcm2wave_flag = false;
static pthread_mutex_t spi_send_data_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint8_t song_play_status = SSAP_CMD_RESP_UNKOWN;
static uint8_t audio_playing_state = AUDIO_IDLE;

static wav_header_t wave_header = {
    .riff_type = "RIFF",
    .riff_size = 0x000804a4,
    .wave_type = "WAVE",
    .format_type = "fmt ",

    .format_size = 0x10,
    .compression_code = 1, //WAVE_FORMAT_PCM
    .num_channels = 1,
    .sample_rate = 16000,
    .bytes_per_second = 0x7d000000, //通道数×每秒数据位数×每样本的数据位数/8

    .block_align = 0x02,
    .bits_per_sample = 16,
    .data_type = "data",
    .data_size = 0x00080480,
};

void set_audio_state(uint8_t state)
{
	audio_playing_state = state;
}

uint8_t geet_audio_state(void)
{
	return audio_playing_state;
}

void set_pcm_param(uint8_t param_type, uint16_t param)
{
    switch(param_type)
    {
        case CHANNELS_PARAM:
            wave_header.num_channels = param;
            break;
        case SAMPLE_RATE_PARAM:
            wave_header.sample_rate = param;
            break;
        case RESOLUTION_PARAM:
            wave_header.bits_per_sample = param;
            break;
        default:
            LOGW("no this type parameter\n");
    }

}

static pthread_t web_download_id;
static pthread_t download_data_to_spim_process_id;

static handle_response_count = 0;
uint32_t handle_spis_response(bool *frame_end_flag, bool *slice_end_flag,
					SSAPCmdResponseContext handle_response_context, bool data_buffer_flag)
{
	uint32_t ret = 0;

	switch(handle_response_context.command) {
		/* data and start command */
		case SSAP_CMD_DATA:
		case SSAP_CMD_STOP:
		case SSAP_CMD_START:
			if(handle_response_context.response == SSAP_CMD_RESP_OKSEND) {
				//LOGD("data_buffer_flag=%d\n", data_buffer_flag);
				if(data_buffer_flag) {
					//LOGD("end of one frame transfer\n");
					*frame_end_flag = true;
				}
			    //LOGD("[RESPONS] OKSEND\n");
			    *slice_end_flag = true;
			} else if(handle_response_context.response == SSAP_CMD_RESP_NEXTSEND) {
			    //LOGD("[RESPONS] FULL\n");
			} else if(handle_response_context.response == SSAP_CMD_RESP_RESEND) {
				handle_response_count++;
				if(handle_response_count > 200) {
					handle_response_count = 0;
					LOGE("[RESPONS] command = %d, TIMEOUT\n", handle_response_context.command);
					*frame_end_flag = true;
					spim_connected_spis_flag = false;
					*slice_end_flag = true;
				}
			} else {
				LOGE("data/start command error, command=%d\n", handle_response_context.command);
			}
			break;
		/* sofa command */
		case SSPP_CMD_SOFA_COMMAND:
		case SSPP_CMD_SONG_PATH_MODE:
			LOGD("sofa repose=%d\n", handle_response_context.response);
			if(handle_response_context.response == SSAP_CMD_RESP_OKCMD) {
				//LOGD("sofa or mode coammand ok\n");
				*frame_end_flag = true;
				*slice_end_flag = true;
			} else {
				LOGE("sofa command error\n");
			}
			break;
		/* token command */
		case SSAP_CMD_TOKEN:
			switch(handle_response_context.response) {
				case SSAP_CMD_RESP_SONG_PAUSE:
				case SSAP_CMD_RESP_SONG_STOP:
				case SSAP_CMD_RESP_SONG_PLAY:
				case SSAP_CMD_RESP_SONG_LAST:
				case SSAP_CMD_RESP_SONG_NEXT:
				case SSAP_CMD_RESP_SONG_FF:
				case SSAP_CMD_RESP_SONG_FB:
					song_play_status = handle_response_context.response;
				case SSAP_CMD_RESP_MODE_WIFI:
				case SSAP_CMD_RESP_MODE_OTHER:
					*frame_end_flag = true;
					*slice_end_flag = true;
					LOGE("token ok\n");
					spim_connected_spis_flag = true;
					break;
				default:
					//LOGE("token error, response=%d\n", handle_response_context.response);
					handle_response_count ++;
					if(ret > 200) {
						LOGE("token error, spi cannot connect!!!\n");
						spim_connected_spis_flag = false;
						*frame_end_flag = true;
						*slice_end_flag = true;
						handle_response_count = 0;
					}
					break;
			}
			break;
		default:
			handle_response_count ++;
			if(handle_response_count > 200) {
				LOGE("command error, command = %d, response=%d\n",
						handle_response_context.command, handle_response_context.response);
				handle_response_count = 0;
				*frame_end_flag = true;
				*slice_end_flag = true;
				spim_connected_spis_flag = false;
			}
	}
}

uint32_t send_command_data_count = 0;
SSAPCmdResponseContext spim_send_command_data(uint8_t *data_buffer, uint32_t buffer_len, uint8_t data_command, uint32_t slice_len)
{
	pthread_mutex_lock(&spi_send_data_mutex);
    uint32_t ret = 0;
    uint16_t total = 0;
    uint16_t remain = 0;
    uint16_t point_location = 0;
    uint16_t data_buffer_point_loca = 0;
    uint32_t frame_buffer_count = 0;
	bool     data_buffer_end_flag = false;
	bool     one_frame_end_flag = false;
	bool     one_slice_end_flag = false;
	uint32_t actual_tranfer_len = 0;
	uint8_t SPIM_SEND_BUFFER[sizeof(SSAPCmdContext) + SSAP_SYNC_WORD_LENGTH + SSAP_SEND_DATA_LENGTH + SSAP_CRC_VALUE_LENGTH];
    uint8_t  *tx_buffer_point = (uint8_t*)&SPIM_SEND_BUFFER[0];
    uint8_t  *spim_send_buffer   = (uint8_t*)&SPIM_SEND_BUFFER[sizeof(SSAPCmdContext) + SSAP_SYNC_WORD_LENGTH];
    uint16_t *crc_value_buf;
    SSAPCmdContext*         spim_song_command_context  = (SSAPCmdContext*)SPIM_SEND_BUFFER;
    SSAPCmdResponseContext  spis_song_response_context;

	if(slice_len > SSAP_SEND_DATA_LENGTH) {
		LOGE("slice_len must <= SSAP_SEND_DATA_LENGTH\n");
		pthread_mutex_unlock(&spi_send_data_mutex);
		LOGD("[UNLOCK********send_command_data_count=%d, data_command=%d\n\n", send_command_data_count, data_command);
		return ;
	}
	if((data_command < SSAP_CMD_UNKOWN) || (data_command > SSPP_CMD_MAX)) {
		LOGE("not support this data command(%d)\n", data_command);
		pthread_mutex_unlock(&spi_send_data_mutex);
		return ;
	}

	send_command_data_count++;
	//LOGD("[LOCK]  send_command_data_count=%d, data_command=%d\n", send_command_data_count, data_command);

	//LOGD("buffer_len=%d, slice_len=%d\n", buffer_len, slice_len);
    while(!one_frame_end_flag) { /* one frame while start */
        if(buffer_len > slice_len) {
            frame_buffer_count ++;
			if(NULL != data_buffer) {
				memcpy(spim_send_buffer, data_buffer + data_buffer_point_loca, slice_len);
			}
            data_buffer_point_loca += slice_len;
            buffer_len -= slice_len;
        } else {
			data_buffer_end_flag = true;
			if(NULL != data_buffer) {
				memcpy(spim_send_buffer, data_buffer + data_buffer_point_loca, buffer_len);
			}
        }

		actual_tranfer_len = data_buffer_end_flag ? buffer_len : slice_len;
		//LOGD("/******************one slice actual_tranfer_len = %d********************data_buffer_end_flag=%d/\n",
		//		actual_tranfer_len, data_buffer_end_flag);
        /* data crc value */
		crc_value_buf = (uint16_t*)&SPIM_SEND_BUFFER[sizeof(SSAPCmdContext) + SSAP_SYNC_WORD_LENGTH + actual_tranfer_len];
        *crc_value_buf = get_crc16_nbs(spim_send_buffer, actual_tranfer_len);
        while(!one_slice_end_flag) { /* send data while(1) start */
            /* 1. set command */
            spim_song_command_context->sync_word = SSAP_SYNC_WORD_LE;
            spim_song_command_context->command  = data_command;
            spim_song_command_context->content  = actual_tranfer_len;
            spim_song_command_context->crc_value = get_crc16_nbs((uint8_t*)spim_song_command_context,
                                                    sizeof(SSAPCmdContext) - SSAP_CRC_VALUE_LENGTH);
            SPIM_SEND_BUFFER[6] = 'S';

            /* 2. send data */
#if 0
			LOGD("command: ");
			for(ret = 0;ret < 7; ret ++){
				printf("0x%02x ", tx_buffer_point[ret]);
			}
			printf("\n");
#endif

            total = sizeof(SSAPCmdContext) + SSAP_SYNC_WORD_LENGTH + actual_tranfer_len + SSAP_CRC_VALUE_LENGTH;
            remain = total % SSAP_SEND_DATA_BLOCKSIZE;
            for(ret = 0, point_location = 0; ret < total/SSAP_SEND_DATA_BLOCKSIZE; ret ++) {
				//LOGE("[MUTEX]1:data thread spi \n");
                spi_master_send_data(tx_buffer_point+point_location, 32);
                point_location += SSAP_SEND_DATA_BLOCKSIZE;
            }
            if(remain>0) {
				//LOGE("[MUTEX]2:data thread spi \n");
                spi_master_send_data(tx_buffer_point+point_location, remain);
            }
#if 1
            /* 3. receive response */
            spis_song_response_context = spis_respones_command();
			//LOGD("command=%d, response=%d\n", spis_song_response_context.command, spis_song_response_context.response);
			/* handle response */
			handle_spis_response(&one_frame_end_flag, &one_slice_end_flag,
									spis_song_response_context, data_buffer_end_flag);
			//LOGD("one_frame_end_flag=%d, &one_slice_end_flag=%d\n", one_frame_end_flag, one_slice_end_flag);
#endif
        } /* send buffer while(1) end */
		one_slice_end_flag = false;
    } /* one frame while(1) end */


	//LOGD("[UNLOCK]send_command_data_count=%d, data_command=%d\n\n", send_command_data_count, data_command);
	pthread_mutex_unlock(&spi_send_data_mutex);

	return spis_song_response_context;
}


/**
 * @brief web downlaod mp3 transfer to spis
 * @param[in]  NONE
 * @param[out] NONE
 */
uint32_t enq_count = 0;
uint32_t deq_count = 0;
static int *fp_mp3 = NULL;


void set_sofa_command_thread()
{
	uint32_t ret = 0;
	uint8_t sofa_command = UNKNOW_SOFA_CMD;
#if 0
	/* get sofa command */
	LOGD("enter sofa\n");
	while(!spim_connected_spis_flag);
	LOGD("sofa command\n");

	/* set sofa command to slave */
	while(1) {
		for(ret = 0; ret < 7; ret++) {
			spim_send_command_data(&sofa_command, 1, SSPP_CMD_SOFA_COMMAND, SSAP_SEND_DATA_LENGTH);
			sleep(1);
		}
	}
#endif
}

/**
 * @brief spi master send token to keep touch with spi slave
 * @param[in]  NONE
 * @param[out] NONE
 */
#define SPIM_TOKEN_COMMAND_CONTEXT_SIZE 7
void spim_token_thread(void)
{
    uint32_t ret = 0;
	SSAPCmdResponseContext token_response;

	LOGD("enter token\n");

#if 1
    while(1) {
#if 1
		//spi_master_send_data(token_command_buffer, SPIM_TOKEN_COMMAND_CONTEXT_SIZE);
		token_response = spim_send_command_data(NULL, 0, SSAP_CMD_TOKEN, 0);
		//token_response = spis_respones_command();
		LOGD("token_response.command=%d, token_response.response=%d\n",
							token_response.command, token_response.response);
		printf("\n");

		if(!spim_connected_spis_flag) {
			LOGE("token connet error, stop spi\n");
			//break;
		}
		//LOGD("spim_connected_spis_flag=%d\n", spim_connected_spis_flag);
		token_response.response = 0;
		token_response.command = 0;
#endif
		sleep(2);
    }
#endif
}


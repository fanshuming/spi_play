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
#include <curl/curl.h>

#include "crc.h"
#include "ssap_protocol.h"
#include "string_convert.h"
#include "spim.h"
#include "queue.h"
#include "log.h"

#define FILE_PATH_NAME "/tmp/test.mp3"
#define SONG_FILE_NAME "test.mp3"

//#define FILE_PATH_NAME "/tmp/test_wav.wav"
//#define SONG_FILE_NAME "test_wav.wav"

//#define FILE_PATH_NAME "/tmp/demo_file.pcm"
//#define SONG_FILE_NAME "demo_file.pcm"


static uint8_t SPIM_SEND_BUFFER[sizeof(SSAPCmdContext) + SSAP_SYNC_WORD_LENGTH + SSAP_SEND_DATA_LENGTH + SSAP_CRC_VALUE_LENGTH];

static uint8_t  long_file_name[FILE_NAME_MAX] = {0};
static bool spim_connected_spis = false;
static uint32_t   decoder_type = MP3_DECODER;
static uint8_t file_path_name[100] = {0};

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

#if 0
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
			LOGE("no this type parameter\n");
	}

}
#endif

/**
 * @brief get mp3 file total length
 * @param[in]  file path
 * @param[out] file size
 */
static bool pcm2wave_flag = false;
void handle_file_name(uint8_t *file_name)
{
	uint32_t ret = 0;
	char file_type[4] = {0};
	uint8_t type_number = 0;
	char file_path[100];

	/* to fill file name */
	if(file_name != NULL) {
		memcpy(long_file_name, file_name, strlen(file_name));
	} else {
		exit(EXIT_FAILURE);
	}
	/* to fill decoder_type */
	extract_str_right(file_type, file_name, 3);

	decoder_type = select_decoder_type(file_type);
	if(decoder_type == PCM_DECODER) {
		decoder_type = WAV_DECODER;
		pcm2wave_flag = true;
	}

	LOGD("file_name is %s, decoder_type=%d\n", long_file_name, decoder_type);
    getcwd(file_path_name, sizeof(file_path));
	strcat(file_path_name, "/");
	strcat(file_path_name, file_name);
    LOGD("file directory is: %s\n", file_path_name);
}



/**
 * @brief get mp3 file total length
 * @param[in]  file path
 * @param[out] file size
 */
uint32_t get_file_size(const char *path)
{
    uint32_t filesize = 0;
    struct stat statbuff;
    if(stat(path, &statbuff) < 0){
        LOGE("stat error\n");
        return filesize;
    }else{
        filesize = statbuff.st_size;
    }
    return filesize;
}

/**
 * @brief SPI stream audio player process
 * @param[in]  NONE
 * @param[out] NONE
 */
void *spim_stream_audio_player_process(void) {
    uint32_t  ret = 0;
    uint32_t file_copy_buffer_count = 0;
    uint32_t file_send_count = 0;
    uint8_t   *tx_buffer_point = (uint8_t*)&SPIM_SEND_BUFFER[0];
    FILE      *file_handle;
    bool      file_start_flag = true;
    bool file_end_flag   = false;
    uint16_t  total = 0;
    uint16_t  remain = 0;
    uint16_t  point_location = 0;
    int32_t   file_len = 0;
	uint8_t   *spim_send_buffer   = (uint8_t*)&SPIM_SEND_BUFFER[sizeof(SSAPCmdContext) + SSAP_SYNC_WORD_LENGTH];
    uint16_t  *crc_value_buf = (uint16_t*)&SPIM_SEND_BUFFER[sizeof(SSAPCmdContext) +
                                                    SSAP_SYNC_WORD_LENGTH + SSAP_SEND_DATA_LENGTH];

    SSAPCmdContext*         spim_command_context  = (SSAPCmdContext*)SPIM_SEND_BUFFER;
    SSAPCmdResponseContext  spis_response_context;//(SSAPCmdResponseContext*)SPIM_SEND_BUFFER;
    if((file_handle = fopen(file_path_name, "rb+")) == NULL) {
        LOGE("fopen error\n");
        exit(1);
    }
	if(file_path_name != NULL) {
		file_len = get_file_size(file_path_name);
	}
    LOGD("file len = %d\n", file_len);
#if 1
    //memcpy(long_file_name, SONG_FILE_NAME, FILE_NAME_MAX);

    LOGD("[INFO]: Begin of file\n");
    LOGD("[SCMD]: START\n");

    /* set start command struct */
    spim_command_context->sync_word = SSAP_SYNC_WORD_LE;
    spim_command_context->command  = SSAP_CMD_START;
    spim_command_context->content  = 4 + strlen((const char*)long_file_name);
    spim_command_context->crc_value = get_crc16_nbs((uint8_t*)spim_command_context,
                                            sizeof(SSAPCmdContext) - SSAP_CRC_VALUE_LENGTH);
    SPIM_SEND_BUFFER[6] = 'S';

    *(int32_t*)spim_send_buffer = decoder_type;

    memcpy(spim_send_buffer + 4, long_file_name, strlen((const char*)long_file_name));

    *(uint16_t*)(spim_send_buffer + spim_command_context->content) = get_crc16_nbs(
                                                    spim_send_buffer, spim_command_context->content);

    while(file_start_flag) {
        /* Wait SPIS handshake signal */
        //while(!spim_connected_spis);
        LOGD("hand shake success\n");

        /* Send START command to SPIS */
        LOGD("send start command: ");
        for(ret=0;ret<20;ret++){
            LOGD("0x%x ", SPIM_SEND_BUFFER[ret]);
        }
        LOGD("\n");
        ret = spi_master_send_data((uint8_t*)spim_command_context, sizeof(SSAPCmdContext) +
            SSAP_SYNC_WORD_LENGTH + spim_command_context->content + SSAP_CRC_VALUE_LENGTH);
        if(ret == SPIM_ERR)
            LOGE("send start command error\n");

        /* Receive response */
        spis_response_context = spis_respones_command();
        LOGD("send start response command=0x%x\n",  spis_response_context.command);
        LOGD("send start response response=0x%x\n", spis_response_context.response);
        if(spis_response_context.response == SSAP_CMD_RESP_OKSEND) {
            file_start_flag = false;
        } else {
            spim_command_context->sync_word = SSAP_SYNC_WORD_LE;
            spim_command_context->command  = SSAP_CMD_START;
            spim_command_context->content  = 4 + strlen((const char*)long_file_name);
            spim_command_context->crc_value = get_crc16_nbs((uint8_t*)spim_command_context,
                                                    sizeof(SSAPCmdContext) - SSAP_CRC_VALUE_LENGTH);
        }
    } /* end of while(file_start_flag) */
    LOGD("[INFO]: Send info done.\n");
#endif

#if 1
	/* send pcm header */
	if(pcm2wave_flag) {
		LOGD("send pcm header\n");
		pcm2wave_flag = false;
		memcpy(spim_send_buffer, &wave_header, sizeof(wav_header_t));
		//memcpy(spim_send_buffer, 0, SSAP_SEND_DATA_LENGTH - sizeof(wav_header_t));
        /* data crc value */
        *crc_value_buf = get_crc16_nbs(spim_send_buffer, SSAP_SEND_DATA_LENGTH);
		while(1) { /* send data while(1) start */
            /* 1. Set DATA command */
            spim_command_context->sync_word = SSAP_SYNC_WORD_LE;
            spim_command_context->command  = SSAP_CMD_DATA;
            spim_command_context->content  = SSAP_SEND_DATA_LENGTH;
            spim_command_context->crc_value = get_crc16_nbs((uint8_t*)spim_command_context,
                                                    sizeof(SSAPCmdContext) - SSAP_CRC_VALUE_LENGTH);
            SPIM_SEND_BUFFER[6] = 'S';

            /* 2. Send data */
            total = sizeof(SPIM_SEND_BUFFER);
            remain = total % SSAP_SEND_DATA_BLOCKSIZE;
            ret = total / SSAP_SEND_DATA_BLOCKSIZE;
            //LOGD("send one frame data total=%d, remain=%d, quotient=%d\n", total, remain, ret);
            //print_microsecond_time(1);
            LOGD("command: ");
            for(ret = 0; ret < 7; ret++){
              LOGD("0x%02x ", SPIM_SEND_BUFFER[ret]);
            }
            //for(ret = 7; ret < total; ret++){
            //  if(((ret-7)%16)==0){
            //      LOGD("\n");
            //  }
            //  LOGD("0x%02x ", SPIM_SEND_BUFFER[ret]);
            //}
            LOGD("\n");
            file_send_count ++;
            for(ret = 0, point_location = 0; ret < total/SSAP_SEND_DATA_BLOCKSIZE; ret ++) {
                LOGD("send one buffer, point_location=%d\n", point_location);
                spi_master_send_data(tx_buffer_point+point_location, 32);
                point_location += SSAP_SEND_DATA_BLOCKSIZE;
            }
            if(remain>0) {
                LOGD("send remainde buffer, file_send_count=%d\n", file_send_count);
                spi_master_send_data(tx_buffer_point+point_location, remain);
            }
//          usleep(1000);
        //  print_microsecond_time(2);
#if 1
            /* 3. Receive response */
            spis_response_context = spis_respones_command();
            //print_microsecond_time(3);
            //LOGD("send data response command=0x%x response=0x%x\n",
            //              spis_response_context.command, spis_response_context.response);
            if(spis_response_context.response == SSAP_CMD_RESP_TIME_OUT) {
                LOGD("spis respone time out++++++++++++++++++++++++++++++++++++++++%d\n", file_copy_buffer_count);
                //pthread_exit(0);
            }
            if(spis_response_context.command == SSAP_CMD_DATA) {
                if(spis_response_context.response == SSAP_CMD_RESP_OKSEND) {
                    LOGD("[INFO]resp ok send\n");
                    break;
                }
                else if(spis_response_context.response == SSAP_CMD_RESP_NEXTSEND) {
                    LOGD("[INFO]: FULL\n");
                } else if(spis_response_context.response == SSAP_CMD_RESP_RESEND) {
                    LOGE("[INFO]: CRC ERROR, file_copy_buffer_count=%d\n", file_copy_buffer_count);
                    LOGE("[INFO]: CRC ERROR, file_send_count=%d\n", file_send_count);
                    //pthread_exit(0);
                    //break;
                    //if(!SSAP_RESEND_IF_CRC_ERROR){
                    //    break;
                    //}
                }
#if 0
                else if(spis_response_context.response == SSAP_CMD_RESP_TIME_OUT) {
                    LOGE("send data spis respone time out\n");
                    pthread_exit(0);
                }
#endif
            } else if(SSAP_CMD_STOP == spis_response_context.command) {


            }
#endif
        } /* send buffer while(1) end */
	}

#endif


    while(1) { /* send file while(1) start */
        file_len -= SSAP_SEND_DATA_LENGTH;

        file_end_flag = file_len <= 0 ? true : false;

        if(!file_end_flag) {
            /* copy file to spim_send_buffer */
            file_copy_buffer_count++;
            //LOGD("/*********file_copy_buffer_count[%d], file_len=%d***********/\n", file_copy_buffer_count, file_len);
            ret = fread(spim_send_buffer, 1, SSAP_SEND_DATA_LENGTH, file_handle);
            if(ret <= 0) {
                LOGE("fread error\n");
             }
            /* data crc value */
            *crc_value_buf = get_crc16_nbs(spim_send_buffer, SSAP_SEND_DATA_LENGTH);
        } else {
            LOGD("[INFO]: End of file\n");
            LOGD("[SCMD]: STOP\n");

            spim_command_context->command  = SSAP_CMD_STOP;
            /* command crc value */
            spim_command_context->crc_value = get_crc16_nbs((uint8_t*)spim_command_context,
                                                    sizeof(SSAPCmdContext) - SSAP_CRC_VALUE_LENGTH);

            /* Send STOP command to SPIS */
            spi_master_send_data((uint8_t*)spim_command_context, sizeof(SSAPCmdContext));

            break;
        }

        while(1) { /* send data while(1) start */
            /* 1. Set DATA command */
            spim_command_context->sync_word = SSAP_SYNC_WORD_LE;
            spim_command_context->command  = SSAP_CMD_DATA;
            spim_command_context->content  = SSAP_SEND_DATA_LENGTH;
            spim_command_context->crc_value = get_crc16_nbs((uint8_t*)spim_command_context,
                                                    sizeof(SSAPCmdContext) - SSAP_CRC_VALUE_LENGTH);
            SPIM_SEND_BUFFER[6] = 'S';

            /* 2. Send data */
            total = sizeof(SPIM_SEND_BUFFER);
            remain = total % SSAP_SEND_DATA_BLOCKSIZE;
            ret = total / SSAP_SEND_DATA_BLOCKSIZE;
            //LOGD("send one frame data total=%d, remain=%d, quotient=%d\n", total, remain, ret);
            //print_microsecond_time(1);
            //LOGD("command: ");
            //for(ret = 0; ret < 7; ret++){
            //  LOGD("0x%02x ", SPIM_SEND_BUFFER[ret]);
            //}
            //for(ret = 7; ret < total; ret++){
            //  if(((ret-7)%16)==0){
            //      LOGD("\n");
            //  }
            //  LOGD("0x%02x ", SPIM_SEND_BUFFER[ret]);
            //}
            LOGD("\n");
            file_send_count ++;
            for(ret = 0, point_location = 0; ret < total/SSAP_SEND_DATA_BLOCKSIZE; ret ++) {
                //LOGD("send one buffer, point_location=%d\n", point_location);
                spi_master_send_data(tx_buffer_point+point_location, 32);
                point_location += SSAP_SEND_DATA_BLOCKSIZE;
            }
            if(remain>0) {
                //LOGD("send remainde buffer, file_send_count=%d\n", file_send_count);
                spi_master_send_data(tx_buffer_point+point_location, remain);
            }
//          usleep(1000);
        //  print_microsecond_time(2);
#if 1
            /* 3. Receive response */
            spis_response_context = spis_respones_command();
            //print_microsecond_time(3);
            //LOGD("send data response command=0x%x response=0x%x\n",
            //              spis_response_context.command, spis_response_context.response);
            if(spis_response_context.response == SSAP_CMD_RESP_TIME_OUT) {
                LOGD("spis respone time out++++++++++++++++++++++++++++++++++++++++%d\n", file_copy_buffer_count);
                //pthread_exit(0);
            }
            if(spis_response_context.command == SSAP_CMD_DATA) {
                if(spis_response_context.response == SSAP_CMD_RESP_OKSEND) {
                    LOGD("[INFO]resp ok send\n");
                    break;
                }
                else if(spis_response_context.response == SSAP_CMD_RESP_NEXTSEND) {
                    LOGE("[INFO]: FULL\n");
                } else if(spis_response_context.response == SSAP_CMD_RESP_RESEND) {
                    LOGE("[INFO]: CRC ERROR, file_copy_buffer_count=%d\n", file_copy_buffer_count);
                    LOGE("[INFO]: CRC ERROR, file_send_count=%d\n", file_send_count);
                    //pthread_exit(0);
                    //break;
                    //if(!SSAP_RESEND_IF_CRC_ERROR){
                    //    break;
                    //}
                }
#if 0
                else if(spis_response_context.response == SSAP_CMD_RESP_TIME_OUT) {
                    LOGE("send data spis respone time out\n");
                    pthread_exit(0);
                }
#endif
            } else if(SSAP_CMD_STOP == spis_response_context.command) {


            }
#endif
        } /* send buffer while(1) end */
    } /* send file while(1) end */

    /* close file */
    fclose(file_handle);
}


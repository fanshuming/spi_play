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



uint32_t select_decoder_type(uint8_t *file_type)
{
	uint32_t decoder_type = MP3_DECODER;
	uint8_t type_number = 0;
	uint8_t *file_type_table[MAX_SUPPORT_FILE_TYPE_NUMBER] = {"mp3", "wav", "pcm"};

    for(type_number = 0; type_number < MAX_SUPPORT_FILE_TYPE_NUMBER; type_number ++) {
        if(strcmp(file_type_table[type_number], file_type) == 0) {
            break;
        }
    }
    LOGD("type_number=%d\n", type_number);
    switch(type_number) {
        case 0:
            decoder_type = MP3_DECODER;
            break;
        case 1:
            decoder_type = WAV_DECODER;
            break;
        case 2:
            decoder_type = PCM_DECODER;
            break;
        default:
            LOGE("not support this type!\n");
    }

	return decoder_type;
}

/**
 * @brief printf local microsecond time
 * @param[in]  print count
 * @param[out] NONE
 */
void print_microsecond_time(uint32_t print_count) {
    struct timeval tv; 
    gettimeofday(&tv, NULL);
    LOGD("print count = %d, time:%ld:%ld\n", print_count, tv.tv_sec, tv.tv_usec);
}

/**
 * @brief get mp3 file total length
 * @param[in]  file path
 * @param[out] file size
 */
char *extract_str_right(char *dst,char *src, int n)
{
    char *p = src;
    char *q = dst;
    int len = strlen(src);
    if(n>len) n = len;
    p += (len-n);
    while(*(q++) = *(p++));
    return dst;
}

/**
 * @brief SPIS response command
 * @param[in]  NONE
 * @param[out] response command
 */
#define SSAP_CMD_RESPONSE_CONTEXT_SIZE (sizeof(SSAPCmdResponseContext))
SSAPCmdResponseContext spis_respones_command() {
    uint32_t ret = SSAP_CMD_RESP_UNKOWN;
    uint32_t time_out = 0;
    uint8_t spis_respones_buffer[SSAP_CMD_RESPONSE_CONTEXT_SIZE] = {0};
    SSAPCmdResponseContext *response_context = (SSAPCmdResponseContext*)spis_respones_buffer;

#if 1
	//LOGD("%s, waite for response\n", __func__);
    while(1) { /* start receive spis respones while(1) */
        /* Recv SYNC Byte */
        spis_respones_buffer[0] = spi_master_recv_byte();

        if(spis_respones_buffer[0] == SSAP_SYNC_BYTE) {
            spi_master_recv_data((uint8_t*)&response_context->command, (sizeof(SSAPCmdContext) - SSAP_SYNC_WORD_LENGTH));
#if 0
            LOGD("spis respones: ");
			for(ret = 0; ret < SSAP_CMD_RESPONSE_CONTEXT_SIZE; ret ++)
				printf("0x%02x ", spis_respones_buffer[ret]);
			printf("\n");
#endif
            break;
        }

#if 1
        if(time_out++ > 10000) {
            response_context->response = SSAP_CMD_RESP_TIME_OUT;
            break;
        }
#endif

    } /* end receive spis respones while(1) */
#endif
    return *response_context;
}


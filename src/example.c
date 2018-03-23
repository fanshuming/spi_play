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

#include "curl/curl.h"
#include "queue.h"
#include "log.h"

#define EXAMPLE_FILE_PATH_NAME "/root/test.mp3"


void sequeue_test(void)
{
	uint32_t ret = 0;

	//sequeue_t test_queue = create_empty_sequeue();

}


uint32_t write_data_count = 0;
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	uint32_t ret = 0;
	uint32_t count = 0;
	uint32_t count_time = 0;

	uint8_t p[CURL_MAX_WRITE_SIZE];
	memcpy(p, (uint8_t*)stream, nmemb);
	write_data_count++;
	size_t written = 0;
	LOGD("==stream[1]=0x%x==============write data count=%d\n", p[0], write_data_count);
	written = fwrite(ptr, size, nmemb, (FILE *)stream);
#if 1
	for(ret = 0; ret < CURL_MAX_WRITE_SIZE; ret++) {
		count_time++;
		if((ret%16) == 0)
			LOGD("\n NO.%d: ", count_time);
		LOGD("0x%02x ", p[ret]);
	}
#endif
	LOGD("writen=%d, size=%d, nmemb=%d\n", written, size, nmemb);

	return nmemb;
}

struct MemoryStruct {
  char *memory;
  size_t size;
};
static uint32_t mem_count = 0;
static sequeue_t *song_data_queue;
static uint32_t queue_element_count = 0;
static uint8_t buffer_test[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
static uint32_t test_size = 5;
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{

	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;
	uint32_t ret = 0;
	uint8_t p[CURL_MAX_WRITE_SIZE];
	uint32_t count_time = 0;
	uint32_t full_flag = 0;

#if 0
	mem_count++;
	LOGD("*******mem_count=%d, nmemb=%d, realsize=%d\n", mem_count, nmemb, realsize);
	//memcpy(p, (uint8_t*)contents, realsize);

	/* create queue first time */
	if(song_data_queue == NULL) {
		song_data_queue = create_empty_sequeue(realsize);
		LOGD("create song data queue success\n");
	}

	full_flag = full_sequeue(song_data_queue);
	while(full_flag);
	queue_element_count ++;
	enqueue(song_data_queue, contents, realsize);

	LOGD("song_data_queue->data[%d].element[0]=%x\n", song_data_queue->rear, song_data_queue->data[song_data_queue->rear].element[0]);

	LOGD("=========================================================queue_element_count=%d\n", queue_element_count);
	LOGD("\n \n");
#endif

#if 0
	for(ret = 0; ret < realsize; ret++) {
		count_time++;
		if((ret%16) == 0)
			LOGD("\n NO.%d: ", count_time);
		LOGD("0x%02x ", p[ret]);
	}
#endif
#if 0
	LOGD("realloc size=%d\n", mem->size + realsize + 1);
	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL) {
	g* out of memory! */
		LOGD("not enough memory (realloc returned NULL)\n");
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	LOGD("last mem->size=%d\n", mem->size);
	mem->memory[mem->size] = 0;
#endif

	return realsize;
}


/* download song play */
void web_downlaod_song_test(void)
{
#if 1
	CURL *curl_handle;
	CURLcode res;

	struct MemoryStruct chunk;

	chunk.memory = malloc(1); /* will be grown as needed by the realloc above */
	chunk.size = 0;   /* no data at this point */

	curl_global_init(CURL_GLOBAL_ALL);
/* init the curl session */
	curl_handle = curl_easy_init();
/* specify URL to get */
	curl_easy_setopt(curl_handle, CURLOPT_URL, "https://www.topqizhi.com/Panama.mp3");
/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
/* some servers don't like requests that are made without a user-agent
	   field, so we provide one */
//	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
/* get it! */
	res = curl_easy_perform(curl_handle);
	LOGD("after curl_easy_perform\n");
/* check for errors */
	if(res != CURLE_OK) {
	  LOGD("curl_easy_perform() failed: %s\n");
	}
	else {
	 /*
	   * Now, our chunk.memory points to a memory block that is chunk.size
	   * bytes big and contains the remote file.
	   *
	   * Do something nice with it!
	   */

	  LOGD("%lu bytes retrieved\n", (long)chunk.size);
	}
/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);
	free(chunk.memory);
/* we're done with libcurl, so clean it up */
	curl_global_cleanup();

#endif

#if 0/* url2file */
	CURL *curl_handle;
	static const char *downlaod_filename = "download.mp3";
	FILE *downlaod_file;
	char *website_addr = "https://www.topqizhi.com/Panama.mp3";

	curl_global_init(CURL_GLOBAL_ALL);
/* init the curl session */
   curl_handle = curl_easy_init();
/* set URL to get here */
   curl_easy_setopt(curl_handle, CURLOPT_URL, website_addr);
/* Switch on full protocol/debug output while testing */
   curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
/* disable progress meter, set to 0L to enable and disable debug output */
   curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
/* send all data to this function  */
   curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
/* open the file */
	downlaod_file = fopen(downlaod_filename, "wb");
	if(downlaod_file) {
	/* write the page body to this file handle */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, downlaod_file);
	/* get it! */
		curl_easy_perform(curl_handle);
	/* close the header file */
		fclose(downlaod_file);
	}
/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);
#endif
}

/* spim test */
#define TX_BUFFER_LEN 32
#define RX_BUFFER_LEN 4
void spim_test() {
    uint8_t spim_tx_buffer[TX_BUFFER_LEN] = {0x02, 0x02, 0x03, 0x04, 0x05,
                                             0x10, 0x20, 0x30, 0x40, 0x50,
                                             0x10, 0x20
                                            };
	uint8_t spim_rx_buffer[RX_BUFFER_LEN] = {0};
    uint8_t ret = 0x55;
    uint8_t i = 0;
    uint8_t test_buffer[256] = {0};

	for(ret=0;ret < TX_BUFFER_LEN; ret++) {
		spim_tx_buffer[ret] = ret;
	}

    memset(test_buffer, 0x55, 256);

    spi_master_init();
    while(1) {
        ret = spi_master_send_data(spim_tx_buffer , TX_BUFFER_LEN);
        if(ret == SPIM_ERR) {
            LOGD("in %s, spi send data error\n", __func__);
        }
    }

#if 0
    while(1) {
        ret = spi_master_send_data(spim_tx_buffer, TX_BUFFER_LEN);
        if(ret == SPIM_ERR) {
            LOGD("in %s, spi send data error\n", __func__);
        }
        usleep(100);

/*
        ret = 0;
        ret = SpiMasterRecvByte();
        LOGD("recv byte = 0x%x\n", ret);
        ret = spi_master_recv_data(spim_rx_buffer, RX_BUFFER_LEN);
        if(ret != 0) {
            LOGD("recv data: ");
            for(i = 0; i < RX_BUFFER_LEN; i++) {
                LOGD("0x%x ", spim_rx_buffer[i]);
            }
            LOGD("\n");
        } else {
			//LOGD("rev data error\n");
        }
*/
    }
#endif
    spi_master_deinit();
}

/* operate file test */
void file_test() {
    uint32_t file_len = get_file_size(EXAMPLE_FILE_PATH_NAME);
    uint32_t ret = 0;
    FILE     *file_handle;
    uint8_t  pBuffer[10] = {0};
    bool     file_end_flag   = true;

    LOGD("file size = %d\n", file_len);
    if((file_handle = fopen(EXAMPLE_FILE_PATH_NAME, "rb+")) == NULL) {
        LOGD("fopen error\n");
        exit(EXIT_FAILURE);
    }

    while(1) { /* send file while(1) start */
        file_len -= 2;

        file_end_flag = file_len <= 0;

        if(!file_end_flag) {
			/* copy file to pBuffer */
            ret = fread(pBuffer, 1, 10, file_handle);
            if(ret <= 0) {
                LOGD("fread error\n");
                break;
            }
            for(ret = 0; ret<10; ret++)
                LOGD("%x",pBuffer[ret]);
            LOGD("\n");
        } else {
            LOGD("file end\n");
            break;
        }
    }

    fclose(file_handle);
}

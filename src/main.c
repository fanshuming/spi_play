/**
 **************************************************************************************
 * @file    main.c
 * @brief   SPI Stream Audio Player (Master)
 *
 * @author  Tian Feng
 * @email   fengtian@topqizhi
 * @version V1.0.0
 *
 * &copy; Shanghai Mountain View Silicon Technology Co.,Ltd. All rights reserved.
 **************************************************************************************
 */

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
#include <getopt.h>

#include "crc.h"
#include "ssap_protocol.h"
#include "string_convert.h"
#include "spim.h"
#include "queue.h"
#include "log.h"
#include "microphone.h"
#include "user_tts.h"

#define  VERSION "V1.0.0"

unsigned long get_fileSize(const char *path)  
{  
    unsigned long filesize = -1;  
    FILE *fp;  
    fp = fopen(path, "r");  
    if(fp == NULL)  
        return filesize;  
    fseek(fp, 0L, SEEK_END);  
    filesize = ftell(fp);  
    fclose(fp);  
    return filesize;  
} 

int main(int argc, char * argv[])
{
	//	wrt_open_audio_play();

	uint8_t *pcm_file_name = "/tmp/test.mp3";
	
	unsigned long fileSize = -1;

	char s[] = "0\n";
        FILE *fid = fopen("/etc/config/mic_wake","w");
        fprintf(fid,"%s",s);
        fclose(fid);
	

	if(argc >= 2)
	{
		pcm_file_name = argv[1];
	}

	fileSize = get_fileSize(pcm_file_name);
	if(fileSize > 100)
	{
        	spi_master_init();
		play_local_pcm(pcm_file_name);
	}
/*
        int ret;
        FILE * pFile;
        char read[512];

        spi_master_init();

        pFile = fopen("test.mp3" , "rb");

        fread(read , 1 , sizeof(read) , pFile);
        printf("read: %s\n", read);

        spi_master_deinit();

        return ret;
*/

	return 0;
}



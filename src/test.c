#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char * argv[])
{   
        int ret;
        FILE * pFile;
        char read[512];
    
        spi_master_init();

        pFile = fopen("test.mp3" , "rb");

        fread(read , 1 , sizeof(read) , pFile);
	printf("read: %s\n", read);

        spi_master_deinit();

        return ret;
}


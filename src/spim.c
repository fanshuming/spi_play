/**
  *****************************************************************************
  * @file:			spim.c
  * @author			Tian Feng
  * @email          fengtian@topqizhi.com
  * @version		V1.0.0
  * @data
  * @Brief			SPI Master driver header file.
  ******************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <pthread.h>

#include "spim.h"
#include "log.h"

#define SPI_DEVICES_NAME "/dev/spidev32766.1"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


static uint16_t fd = 0;
static pthread_mutex_t spim_mutex;
/**
 * @brief	SPIM
 * @param	Mode
 *			0 - CPOL = 0 & CPHA = 0, 1 - CPOL = 0 & CPHA = 1,
 *			2 - CPOL = 1 & CPHA = 0, 3 - CPOL = 1 & CPHA = 1,
 * @param
 *
 * @return
 * @note@
 */
void spi_master_init()
{
	uint8_t ret = 0;
	uint32_t mode = 0;
	uint8_t  bits = 8;
	uint32_t speed = 24000000;

	pthread_mutex_init(&spim_mutex, NULL);

	fd = open(SPI_DEVICES_NAME, O_RDWR);
	if(fd < 0) {
		LOGE("spi open error\n");
		exit(EXIT_FAILURE);
	}
#if 1
    /*
     * spi mode
     */
    ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode);
    if (ret == -1)
        pabort("can't set spi mode");

    ret = ioctl(fd, SPI_IOC_RD_MODE32, &mode);
    if (ret == -1)
        pabort("can't get spi mode");

    /*
     * bits per word
     */
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
        pabort("can't set bits per word");

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1)
        pabort("can't get bits per word");

    /*
     * max speed hz
     */
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        pabort("can't set max speed hz");

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        pabort("can't get max speed hz");

    LOGD("spi mode: 0x%x\n", mode);
    LOGD("bits per word: %d\n", bits);
    LOGD("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
#endif

}

/**
 * @brief Deinit SPIM
 * @param
 * @return
 */
void spi_master_deinit()
{
	LOGD("spi deinit\n");
	if(fd > 0) {
		close(fd);
	}
}

/**
 * @brief
 * @param
 * @return
 */
void spi_master_send_byte(uint8_t val)
{
	pthread_mutex_lock(&spim_mutex);
	if (write(fd, &val, 1) <= 0) {
		LOGE("spi send byte error\n");
		exit(EXIT_FAILURE);
	}
	pthread_mutex_unlock(&spim_mutex);
}

/**
 * @brief
 * @param
 * @return
 */
uint8_t spi_master_recv_byte(void)
{
	uint8_t recv_buffer = 0;
	uint8_t ret = 0;

	pthread_mutex_lock(&spim_mutex);
	ret = read(fd, &recv_buffer, 1);
	if(ret < 0) {
		LOGE("spi recv  byte error\n");
		ret = SPIM_ERR;
	}
	pthread_mutex_unlock(&spim_mutex);

	return recv_buffer;
}

/**
 * @brief
 * @param
 * @param
 * @return
 * @note@
 */
SPI_MASTER_ERR_CODE spi_master_send_data(uint8_t* SendBuf, uint32_t Length)
{
	int ret = SPIM_NONE_ERR;

	if(Length > 36) {
		LOGE("spim send data too long\n");
	}
	pthread_mutex_lock(&spim_mutex);
	if (write(fd, SendBuf, Length) <= 0) {
		LOGE("spi send data error\n");
		ret = SPIM_ERR;
	}
	pthread_mutex_unlock(&spim_mutex);

	return ret;
}

/**
 * @brief
 * @param
 * @param
 * @return
 * @note@
 */
SPI_MASTER_ERR_CODE spi_master_recv_data(uint8_t* RecvBuf, uint32_t Length)
{
	int ret = SPIM_NONE_ERR;

	pthread_mutex_lock(&spim_mutex);
	ret = read(fd, RecvBuf, Length);
	if(ret < 0) {
		LOGE("recv data error\n");
		ret = SPIM_ERR;
	}
	pthread_mutex_unlock(&spim_mutex);

	return ret;

}

/**
 *****************************************************************************
 * @file:          spim.h
 * @author         Tian Feng
 * @email          fengtian@topqizhi.com
 * @version        V1.0.0
 * @data
 * @Brief          SPI Master driver header file.
 ******************************************************************************
*/

#ifndef __SPIM_H__
#define __SPIM_H__

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

#include <stdint.h>
#include <stdbool.h>

#define	NONE_FLASH  0
#define	FLASH_GD	1
#define	FLASH_SST	2

/**
 * ������
 */
typedef enum _SPI_MASTER_ERR_CODE
{
    //ERR_SPIM_TIME_OUT = -255,			/**<function execute time out*/
    //ERR_SPIM_DATALEN_OUT_OF_RANGE,		/**<data len is out of range < 0*/
    SPIM_NONE_ERR = 0,
    SPIM_ERR = 1,
} SPI_MASTER_ERR_CODE;


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
void spi_master_init();
void spi_master_deInit();
void spi_transfer_message();

/**
 * @brief
 * @param
 * @return
 */
void spi_master_send_byte(uint8_t val);

/**
 * @brief
 * @param
 * @return
 */
uint8_t spi_master_recv_byte(void);

/**
 * @brief	SPIM��������
 * @param	SendBuf	��������Buf���׵�ַ
 * @param	Length 	�������ݵĳ��ȣ��ڲ�ʹ��DMA�������ݣ���󳤶�65535�ֽڣ�
 * @return	������
 * @note@
 */
SPI_MASTER_ERR_CODE spi_master_send_data(uint8_t* SendBuf, uint32_t Length);

/**
 * @brief
 * @param
 * @param
 * @return
 * @note@
 */
SPI_MASTER_ERR_CODE spi_master_recv_data(uint8_t* RecvBuf, uint32_t Length);

/**
 * @brief
 *
 * @param
 * @return
 * @note@
 */
bool SpiMasterGetDmaDone(void);

/**
 * @brief
 *
 * @param
 * @return
 * @note@
 */
void SpiMasterIntClr(void);

#ifdef __cplusplus
}
#endif//__cplusplus

#endif

/**
 * @}
 * @}
 */


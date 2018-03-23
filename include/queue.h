/**
 *****************************************************************************
 * @file:          queue.h
 * @author         Tian Feng
 * @email          fengtian@topqizhi.com
 * @version        V1.0.0
 * @data
 * @Brief          sequeue header file.
 ******************************************************************************
*/

#ifndef __SEQUEUE_H__
#define __SEQUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif/* __cplusplus */

#define MAX_QUEUE_ELEMENT_NUM 32
#define curl_max_write_size_test 5

typedef struct {
    //uint8_t element[curl_max_write_size_test]; /* CURL_MAX_WRITE_SIZE define in curl.h file, default 16K[16384] */
    uint8_t *element; /* CURL_MAX_WRITE_SIZE define in curl.h file, default 16K[16384] */
	uint32_t element_size;
} data_t;

typedef struct {
    data_t data[MAX_QUEUE_ELEMENT_NUM];
    uint32_t front;
    uint32_t rear;
} sequeue_t;

sequeue_t *create_empty_sequeue(void);

void destroy_sequeue(sequeue_t *queue);

uint32_t empty_sequeue(sequeue_t *queue);

uint32_t full_sequeue(sequeue_t *queue);

uint32_t enqueue(sequeue_t *queue, uint8_t *x, uint32_t realsize);

uint32_t dequeue(sequeue_t *queue, uint8_t *x);

#ifdef __cplusplus
}
#endif/* __cplusplus */

#endif /* __SEQUEUE_H__ */

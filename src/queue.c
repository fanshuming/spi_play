/**
  *****************************************************************************
  * @file:    equeue.c
  * @author   Tian Feng
  * @email    fengtian@topqizhi.com
  * @version  V1.0.0
  * @data	  2017.12.28
  * @Brief    This sequeue module write for transfer downloading song data
              to spi fifo buffer. Enqueue data from web, dequeue data to SPI.
  ******************************************************************************
*/
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "queue.h"
#include "log.h"

sequeue_t *create_empty_sequeue()
{
	uint32_t ret = 0;
	sequeue_t *queue;

	queue = (sequeue_t *)malloc(sizeof(sequeue_t) + sizeof(data_t));
#if 0
	for(ret = 0; ret < MAX_QUEUE_ELEMENT_NUM; ret ++) {
		queue->data[ret].element = (uint8_t *)malloc(element_size);
	}
#endif

	if(NULL == queue) 
		return NULL;

//	queue->element_size = element_size;
	queue->front = 0;
	queue->rear = 0;

	return queue;
}

void destroy_sequeue(sequeue_t *queue)
{
	if(NULL != queue) {
		free(queue);
	}
}

uint32_t empty_sequeue(sequeue_t *queue)
{
	if(NULL == queue)
	    return -1;
	
	return (queue->front == queue->rear ? 1 : 0);
}

uint32_t full_sequeue(sequeue_t *queue)
{
	if(NULL == queue)
		return -1;

	return ((queue->rear + 1) % MAX_QUEUE_ELEMENT_NUM == queue->front ? 1 : 0);
}

void clear_sequeue(sequeue_t *queue)
{
	if(NULL == queue)
		return;
	
	queue->front = queue->rear = 0;
	
	return;
}

uint32_t enqueue(sequeue_t *queue, uint8_t *x, uint32_t element_size)
{
	if(NULL == queue)
		return - 1;

	if(1 == full_sequeue(queue)) {
		LOGE("%s, FULL !!!\n");
		return -1; /* full */
	}

	queue->rear = (queue->rear + 1) % MAX_QUEUE_ELEMENT_NUM;

	queue->data[queue->rear].element = (uint8_t *)malloc(element_size);
	queue->data[queue->rear].element_size = element_size;
	memcpy(queue->data[queue->rear].element, x, element_size);
	
	return 0;
}

uint32_t dequeue(sequeue_t *queue, uint8_t *x)
{
	uint32_t ret = 0;
	if(NULL == queue)
		return -1;
	
	if(1 == empty_sequeue(queue)) {
		LOGE("empty sequeue\n");
		return -1; /* empty */
	}
	
	queue->front = (queue->front + 1) % MAX_QUEUE_ELEMENT_NUM;
	
	if(NULL != x) {
	    memcpy(x, queue->data[queue->front].element, queue->data[queue->front].element_size);
		ret = queue->data[queue->front].element_size;
		free(queue->data[queue->front].element);
	}

	return ret;
}

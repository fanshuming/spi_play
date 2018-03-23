//#include <stdio.h>
#include <stdlib.h>
#include "play_link.h"

play_link_t *play_link_create(void){
	play_link_t * ptrPlayLinkHead = NULL;

	ptrPlayLinkHead = (play_link_t*)malloc(sizeof(play_link_t));
	if(NULL == ptrPlayLinkHead)
		return NULL;
	return ptrPlayLinkHead;
}

int play_link_add(play_link_t **ptrPlayLinkHead, play_link_t *ptrNewPlayDataNode){
	play_link_t *ptrCurDataNode = *ptrPlayLinkHead;

	if(NULL == ptrNewPlayDataNode)
		return -1;

	if(NULL == *ptrPlayLinkHead){
		*ptrPlayLinkHead = ptrNewPlayDataNode;
		return 0;
	}

	while(NULL != ptrCurDataNode->ptrNext){
		ptrCurDataNode = ptrCurDataNode->ptrNext;
	}
	ptrCurDataNode->ptrNext = ptrNewPlayDataNode;
	//ptrNewPlayDataNode->ptrNext = NULL;
	return 0;
}

int play_link_free(play_link_t **ptrPlayLinkHead){
	play_link_t *ptrCurDataNode = NULL;
	play_link_t *ptrPrevDataNode = NULL;

	if(NULL == *ptrPlayLinkHead)
		return -1;

	ptrCurDataNode = *ptrPlayLinkHead;

	while(NULL != ptrCurDataNode){
		ptrPrevDataNode = ptrCurDataNode;
		ptrCurDataNode = ptrCurDataNode->ptrNext;
		free(ptrPrevDataNode->ptrPlayDataNode);
		free(ptrPrevDataNode);
	}
	*ptrPlayLinkHead = NULL;
	return 0;
}
#ifndef	PLAY_LINK_H
#define	PLAY_LINK_H

struct play_link{
	char *ptrPlayDataNode;
	unsigned int uiPlayDataLen; 
	struct play_link *ptrNext;
};
typedef struct play_link play_link_t;

/*function prototype*/
play_link_t* play_link_create(void);
int play_link_add(play_link_t **ptrPlayLinkHead,play_link_t *ptrNewPlayDataNode);
int play_link_free(play_link_t **ptrPlayLinkHead);
#endif
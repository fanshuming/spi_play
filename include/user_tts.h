#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#include "user_voice.h"
#include "ssap_protocol.h"
#include "log.h"
#include "spim.h"
#include "play_link.h"
#include "queue.h"



extern void play_local_pcm(uint8_t *file_name);

extern void wakeup_audio_play(void);

extern void sofa_response_audio_play(void);

extern void wrt_open_audio_play(void);


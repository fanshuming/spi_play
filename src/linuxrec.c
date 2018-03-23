/*
@file
@brief  record demo for linux

@author		taozhang9
@date		2016/05/27
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <pthread.h>
#include "formats.h"
#include "linuxrec.h"
#include "log.h"


static pthread_mutex_t record_mutex = PTHREAD_MUTEX_INITIALIZER;
static int show_xrun = 1;

static int start_record_internal(snd_pcm_t *pcm)
{
	LOGD("snd_pcm_start\n");
	return snd_pcm_start(pcm);
}

static int stop_record_internal(snd_pcm_t *pcm)
{
	LOGD("snd_pcm_drop\n");
	return snd_pcm_drop(pcm);
}


static int is_stopped_internal(struct recorder *rec)
{
	snd_pcm_state_t state;

	state =  snd_pcm_state((snd_pcm_t *)rec->wavein_hdl);
	switch (state) {
	case SND_PCM_STATE_RUNNING:
	case SND_PCM_STATE_DRAINING:
		return 0;					//not stopped
	default: break;
	}
	return 1;						//is stopped
	
}

static int format_ms_to_alsa(const WAVEFORMATEX * wavfmt, snd_pcm_format_t * format)
{
	snd_pcm_format_t tmp;
	tmp = snd_pcm_build_linear_format(wavfmt->wBitsPerSample, 
			wavfmt->wBitsPerSample, wavfmt->wBitsPerSample == 8 ? 1 : 0, 0);
	if ( tmp == SND_PCM_FORMAT_UNKNOWN )
		return -EINVAL;
	*format = tmp;
	return 0;
}

/* set hardware and software params */
static int set_hwparams(struct recorder * rec,  const WAVEFORMATEX *wavfmt,
			unsigned int buffertime, unsigned int periodtime)
{
	snd_pcm_hw_params_t *params;
	int err;
	unsigned int rate;
	snd_pcm_format_t format;
	snd_pcm_uframes_t size;
	snd_pcm_t *handle = (snd_pcm_t *)rec->wavein_hdl;

	rec->buffer_time = buffertime;
	rec->period_time = periodtime;

	snd_pcm_hw_params_alloca(&params);
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		dbg("Broken configuration for this PCM");
		return err;
	}
	err = snd_pcm_hw_params_set_access(handle, params,
					   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		dbg("Access type not available");
		return err;
	}
	err = format_ms_to_alsa(wavfmt, &format);
	if (err) {
		dbg("Invalid format");
		return - EINVAL;
	}
	err = snd_pcm_hw_params_set_format(handle, params, format);
	if (err < 0) {
		dbg("Sample format non available");
		return err;
	}
	err = snd_pcm_hw_params_set_channels(handle, params, wavfmt->nChannels);
	if (err < 0) {
		dbg("Channels count non available");
		return err;
	}

	rate = wavfmt->nSamplesPerSec;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
	if (err < 0) {
		dbg("Set rate failed");
		return err;
	}
	if(rate != wavfmt->nSamplesPerSec) {
		dbg("Rate mismatch");
		return -EINVAL;
	}
	if (rec->buffer_time == 0 || rec->period_time == 0) {
		err = snd_pcm_hw_params_get_buffer_time_max(params,
						    &rec->buffer_time, 0);
		assert(err >= 0);
		if (rec->buffer_time > 500000)
			rec->buffer_time = 500000;
		rec->period_time = rec->buffer_time / 4;
	}
	err = snd_pcm_hw_params_set_period_time_near(handle, params,
					     &rec->period_time, 0);
	if (err < 0) {
		dbg("set period time fail");
		return err;
	}
	err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
					     &rec->buffer_time, 0);
	if (err < 0) {
		dbg("set buffer time failed");
		return err;
	}
	err = snd_pcm_hw_params_get_period_size(params, &size, 0);
	if (err < 0) {
		dbg("get period size fail");
		return err;
	}
	rec->period_frames = size; 
	err = snd_pcm_hw_params_get_buffer_size(params, &size);
	if (size == rec->period_frames) {
		dbg("Can't use period equal to buffer size (%lu == %lu)",
				      size, rec->period_frames);
		return -EINVAL;
	}
	rec->buffer_frames = size;
	rec->bits_per_frame = wavfmt->wBitsPerSample;

	/* set to driver */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		dbg("Unable to install hw params:");
		return err;
	}
	return 0;
}

static int set_swparams(struct recorder * rec)
{
	int err;
	snd_pcm_sw_params_t *swparams;
	snd_pcm_t * handle = (snd_pcm_t*)(rec->wavein_hdl);
	/* sw para */
	snd_pcm_sw_params_alloca(&swparams);
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		dbg("get current sw para fail");
		return err;
	}

	err = snd_pcm_sw_params_set_avail_min(handle, swparams, 
						rec->period_frames);
	if (err < 0) {
		dbg("set avail min failed");
		return err;
	}
	/* set a value bigger than the buffer frames to prevent the auto start.
	 * we use the snd_pcm_start to explicit start the pcm */
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 
			rec->buffer_frames * 2);
	if (err < 0) {
		dbg("set start threshold fail");
		return err;
	}

	if ( (err = snd_pcm_sw_params(handle, swparams)) < 0) {
		dbg("unable to install sw params:");
		return err;
	}
	return 0;
}

static int set_params(struct recorder *rec, WAVEFORMATEX *fmt,
		unsigned int buffertime, unsigned int periodtime)
{
	int err;
	WAVEFORMATEX defmt = DEFAULT_FORMAT;
	
	if (fmt == NULL) {
		fmt = &defmt;
	}
	
	err = set_hwparams(rec, fmt, buffertime, periodtime);
	if (err)
		return err;
	err = set_swparams(rec);
	if (err)
		return err;
#if 0
				err = snd_pcm_start((snd_pcm_t*)rec->wavein_hdl);
				if(err){		
					dbg("snd_pcm_start error code = %d\n",err);
					return err;
				}
				else
					dbg("snd_pcm_start error code = %d\n",err);
#endif		
					
	return 0;
}

/*
 *   Underrun and suspend recovery
 */
 
static int xrun_recovery(snd_pcm_t *handle, int err)
{
	if (err == -EPIPE) {	/* over-run */
		if (show_xrun)
			LOGD("!!!!!!overrun happend!!!!!!\n");

		err = snd_pcm_prepare(handle);
		if (err < 0) {
			if (show_xrun)
				LOGE("Can't recovery from overrun,"
				"prepare failed: %s\n", snd_strerror(err));
			return err;
		}
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			usleep(200000);	/* wait until the suspend flag is released */
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0) {
				if (show_xrun)
					printf("Can't recovery from suspend,"
					"prepare failed: %s\n", snd_strerror(err));
				return err;
			}
		}
		return 0;
	}
	return err;
}

static ssize_t pcm_read(struct recorder *rec, size_t rcount)
{
	ssize_t r;
	size_t count = rcount;
	char *data;
	snd_pcm_t *handle = (snd_pcm_t *)rec->wavein_hdl;
	if(!handle) {
		LOGE("handle -EINVAL\n");
		return -EINVAL;
	}

	data = rec->audiobuf;
	while (count > 0) {
		r = snd_pcm_readi(handle, data, count);
		//LOGD("EBADFD[%d]/EPIPE[%d]/ESTRPIPE[%d], pcm read ret=%d\n", EBADFD, EPIPE, ESTRPIPE, r);
		if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
			snd_pcm_wait(handle, 100);
		} else if (r < 0) {
			if(xrun_recovery(handle, r) < 0) {
				LOGE("can not xrun_recovery\n");
				return -1;
			}
		} 

		if (r > 0) {
			count -= r;
			data += r * rec->bits_per_frame / 8;
		}
	}
	return rcount;
}

static sem_t sem_start_record;
void post_start_record(void)
{
	LOGD("sem start record\n");
	sem_post(&sem_start_record);
}

static void * record_thread_proc(void * para)
{
	struct recorder * rec = (struct recorder *) para;
	size_t frames, bytes;
	sigset_t mask, oldmask;

	sem_init(&sem_start_record, 0, 0);

#if 0
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &mask, &oldmask);
#endif

	LOGD("enter record_thread_proc\n");
	while(1) {
		sem_wait(&sem_start_record);
		LOGD("record begain ...\n");
		while(1) {
			frames = rec->period_frames;
			bytes = frames * rec->bits_per_frame / 8;

			//pthread_mutex_lock(&record_mutex);
			if(rec->state == RECORD_STATE_RECORDING) {
				if (pcm_read(rec, frames) != frames) {
					LOGE("pcm read error\n");
					pthread_mutex_unlock(&record_mutex);
					return NULL;
				}
				if (rec->on_data_ind) {
					//LOGD("upload data\n");
					rec->on_data_ind(rec->audiobuf, bytes, rec->user_cb_para);
				}
			} else {
				LOGD("record end ...\n");
				break;
			}
			//pthread_mutex_unlock(&record_mutex);
		}
	}
	return rec;

}
int create_record_thread(void * para, pthread_t * tidp)
{
	int err;
	err = pthread_create(tidp, NULL, record_thread_proc, (void *)para);
	if (err != 0) {
		LOGE("rec thread create\n");
		return err;
	}

	return 0;
}

#if 0 /* don't use it now... cuz only one buffer supported */
static void free_rec_buffer(struct recorder * rec)
{
	if (rec->bufheader) {
		unsigned int i;
		struct bufinfo *info = (struct bufinfo *) rec->bufheader;

		assert(rec->bufcount > 0);
		for (i = 0; i < rec->bufcount; ++i) {
			if (info->data) {
				free(info->data);
				info->data = NULL;
				info->bufsize = 0;
				info->audio_bytes = 0;
			}
			info++;
		}
		free(rec->bufheader);
		rec->bufheader = NULL;
	}
	rec->bufcount = 0;
}

static int prepare_rec_buffer(struct recorder * rec )
{
	struct bufinfo *buffers;
	unsigned int i;
	int err;
	size_t sz;

	/* the read and QISRWrite is blocked, currently only support one buffer,
	 * if overrun too much, need more buffer and another new thread
	 * to write the audio to network */
	rec->bufcount = 1;
	sz = sizeof(struct bufinfo)*rec->bufcount;
	buffers=(struct bufinfo*)malloc(sz);
	if (!buffers) {
		rec->bufcount = 0;
		goto fail;
	}
	memset(buffers, 0, sz);
	rec->bufheader = buffers;

	for (i = 0; i < rec->bufcount; ++i) {
		buffers[i].bufsize = 
			(rec->period_frames * rec->bits_per_frame / 8);
		buffers[i].data = (char *)malloc(buffers[i].bufsize);
		if (!buffers[i].data) {
			buffers[i].bufsize = 0;
			goto fail;
		}
		buffers[i].audio_bytes = 0;
	}
	return 0;
fail:
	free_rec_buffer(rec);
	return -ENOMEM;
}
#else
static void free_rec_buffer(struct recorder * rec)
{
	if (rec->audiobuf) {
		free(rec->audiobuf);
		rec->audiobuf = NULL;
	}
}

static int prepare_rec_buffer(struct recorder * rec )
{
	/* the read and QISRWrite is blocked, currently only support one buffer,
	 * if overrun too much, need more buffer and another new thread
	 * to write the audio to network */
	size_t sz = (rec->period_frames * rec->bits_per_frame / 8);
	rec->audiobuf = (char *)malloc(sz);
	if(!rec->audiobuf)
		return -ENOMEM;
	return 0;
}
#endif

static int open_recorder_internal(struct recorder * rec, 
		record_dev_id dev, WAVEFORMATEX * fmt)
{
	int err = 0;

	LOGD("pcm open\n");
	err = snd_pcm_open((snd_pcm_t **)&rec->wavein_hdl, dev.u.name, 
			SND_PCM_STREAM_CAPTURE, 0); // 0 意味着标准配置
	if(err < 0)
		goto fail;

	err = set_params(rec, fmt, DEF_BUFF_TIME, DEF_PERIOD_TIME);
	LOGD("set  err=%d\n", err);
	if(err)
		goto fail;
	assert(rec->bufheader == NULL);
	err = prepare_rec_buffer(rec);
	if(err)
		goto fail;

	return 0;
fail:
	if(rec->wavein_hdl) {
		LOGD("pcm close2\n");
		snd_pcm_close((snd_pcm_t *) rec->wavein_hdl);
	}
	rec->wavein_hdl = NULL;
	free_rec_buffer(rec);
	return err;
}

static void close_recorder_internal(struct recorder *rec)
{
	LOGD(" close_recorder_internal\n");
	snd_pcm_t * handle;

	handle = (snd_pcm_t *) rec->wavein_hdl;

	/* may be the thread is blocked at read, cancel it */
//	pthread_cancel(rec->rec_thread);
//	
//	/* wait for the pcm thread quit first */
//	pthread_join(rec->rec_thread, NULL);

	if(handle) {
		LOGD("pcm close\n");
		snd_pcm_close(handle);
		rec->wavein_hdl = NULL;
	}
	LOGD("free_rec_buffer\n");
	free_rec_buffer(rec);
}
/* return the count of pcm device */
/* list all cards */
static int get_pcm_device_cnt(snd_pcm_stream_t stream)
{
	void **hints, **n;
	char *io, *filter, *name;
	int cnt = 0;

	if (snd_device_name_hint(-1, "pcm", &hints) < 0)
		return 0;
	n = hints;
	filter = stream == SND_PCM_STREAM_CAPTURE ? "Input" : "Output";
	while (*n != NULL) {
		io = snd_device_name_get_hint(*n, "IOID");
		name = snd_device_name_get_hint(*n, "NAME");
		//printf("name = %s\n",name);
		if (name && (io == NULL || strcmp(io, filter) == 0))
			cnt ++;
		if (io != NULL)
			free(io);
		if (name != NULL)
			free(name);
		n++;
	}
	snd_device_name_free_hint(hints);
	return cnt;
}

static void free_name_desc(char **name_or_desc) 
{
	char **ss;
	ss = name_or_desc;
	if(NULL == name_or_desc)
		return;
	while(*name_or_desc) {
		free(*name_or_desc);
		*name_or_desc = NULL;
		name_or_desc++;
	}
	free(ss);
}
/* return success: total count, need free the name and desc buffer 
 * fail: -1 , *name_out and *desc_out will be NULL */
static int list_pcm(snd_pcm_stream_t stream, char**name_out, 
						char ** desc_out)
{
	void **hints, **n;
	char **name, **descr;
	char *io;
	const char *filter;
	int cnt = 0;
	int i = 0;

	if (snd_device_name_hint(-1, "pcm", &hints) < 0)
		return 0;
	n = hints;
	cnt = get_pcm_device_cnt(stream);
	if(!cnt) {
		goto fail; 
	}

	//*name_out = calloc(sizeof(char *) , (1+cnt));
	*name_out = calloc((1+cnt) , sizeof(char *));		//modified by qdli
	if (*name_out == NULL)
		goto fail;
	//*desc_out = calloc(sizeof(char *) , (1 + cnt));
	*desc_out = calloc((1+cnt) , sizeof(char *));		//modified by qdli
	if (*desc_out == NULL)
		goto fail;

	/* the last one is a flag, NULL */
	name_out[cnt] = NULL;
	desc_out[cnt] = NULL;
	name = name_out;
	descr = desc_out;

	filter = stream == SND_PCM_STREAM_CAPTURE ? "Input" : "Output";
	while (*n != NULL && i < cnt) {
		*name = snd_device_name_get_hint(*n, "NAME");
		*descr = snd_device_name_get_hint(*n, "DESC");
		io = snd_device_name_get_hint(*n, "IOID");
		if (name == NULL || 
			(io != NULL && strcmp(io, filter) != 0) ){
			if (*name) free(*name);
			if (*descr) free(*descr);
		} else {
			if (*descr == NULL) {
				*descr = malloc(4);
				memset(*descr, 0, 4);
			}
			name++;
			descr++;
			i++;
		}
		if (io != NULL)
			free(io);
		n++;
	}
	snd_device_name_free_hint(hints);
	return cnt;
fail:
	free_name_desc(name_out);
	free_name_desc(desc_out);
	snd_device_name_free_hint(hints);
	return -1;
}
/* -------------------------------------
 * Interfaces 
 --------------------------------------*/ 
/* the device id is a pcm string name in linux */
record_dev_id  get_default_input_dev()
{
	record_dev_id id; 
	//id.u.name = "default";
	id.u.name = "plughw:1,0";
	return id;
}

record_dev_id * list_input_device() 
{
	// TODO: unimplemented
	return NULL;
}

int get_input_dev_num()
{
	return get_pcm_device_cnt(SND_PCM_STREAM_CAPTURE);
}


/* callback will be run on a new thread */
int create_recorder(struct recorder ** out_rec, 
				void (*on_data_ind)(char *data, unsigned long len, void *user_cb_para), 
				void* user_cb_para)
{
	struct recorder * myrec;
	myrec = (struct recorder *)malloc(sizeof(struct recorder));
	if(!myrec)
		return -RECORD_ERR_MEMFAIL;

	memset(myrec, 0, sizeof(struct recorder));
	myrec->on_data_ind = on_data_ind;
	myrec->user_cb_para = user_cb_para;
	myrec->state = RECORD_STATE_CREATED;

	*out_rec = myrec;
	LOGD("create_recorder, rec->state=%d\n", myrec->state);
	return 0;
}

void destroy_recorder(struct recorder *rec)
{
	LOGD("destroy_recorder enter\n");
	if(!rec)
		return;

	LOGD("destroy_recorder, free rec\n");
	free(rec);
}

int open_recorder(struct recorder * rec, record_dev_id dev, WAVEFORMATEX * fmt)
{
	int ret = 0;
	if(!rec )
		return -RECORD_ERR_INVAL;
	if(rec->state >= RECORD_STATE_READY)
		return 0;

	ret = open_recorder_internal(rec, dev, fmt);
	LOGD("1:snd_pcm_state=%d\n", snd_pcm_state((snd_pcm_t *)rec->wavein_hdl));
	if(ret == 0) {
		LOGD("RECORD_STATE_READY\n");
		rec->state = RECORD_STATE_READY;
	}

	return 0;

}

void close_recorder(struct recorder *rec)
{
	if(rec == NULL || rec->state < RECORD_STATE_READY)
		return;
	if(rec->state == RECORD_STATE_RECORDING)
		stop_record(rec);

	rec->state = RECORD_STATE_CLOSING;

	close_recorder_internal(rec);

	rec->state = RECORD_STATE_CREATED;	
}

int start_record(struct recorder * rec)
{
	int ret=0;
	if(rec == NULL)
		return -RECORD_ERR_INVAL;
	if( rec->state < RECORD_STATE_READY)
		return -RECORD_ERR_NOT_READY;
	LOGD("enter start_record, rec->state=%d\n", rec->state);
	//if( rec->state == RECORD_STATE_RECORDING)
	//	return 0;

	//if(rec->wavein_hdl) {
	//	LOGD("snd_pcm_state=%d\n", snd_pcm_state((snd_pcm_t *)rec->wavein_hdl));
	//} else {
	//	LOGE("rec->wavein_hdl null\n");
	//}
	//ret = start_record_internal((snd_pcm_t *)rec->wavein_hdl);
	//LOGD("snd_pcm_state=%d\n", snd_pcm_state((snd_pcm_t *)rec->wavein_hdl));

	//LOGD("pcm start ret=%d\n", ret);
	//LOGD("pcm error coder EPIPE[%d]/ESTRPIPE[%d]/EBADFD[%d]/ENOTTY[%d]/ENODEV[%d]\n",
	//						EPIPE, ESTRPIPE, EBADFD, ENOTTY, ENODEV);
	//if(ret == 0)
	rec->state = RECORD_STATE_RECORDING;
	return ret;
}

int stop_record(struct recorder * rec)
{
	int ret=0;
	if(rec == NULL)
		return -RECORD_ERR_INVAL;
	if( rec->state < RECORD_STATE_RECORDING)
		return 0;

	LOGD("stop rec \n");
	rec->state = RECORD_STATE_STOPPING;
	//ret = stop_record_internal((snd_pcm_t *)rec->wavein_hdl);
	//if(ret == 0) {		
	//	rec->state = RECORD_STATE_READY;
	//}
	return ret;
}

int is_record_stopped(struct recorder *rec)
{
	if(rec->state == RECORD_STATE_RECORDING)
		return 0;

	return is_stopped_internal(rec);
}

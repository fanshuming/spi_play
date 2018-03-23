#ifndef	USER_VOICE_H
#define	USER_VOICE_H

#include "msp_cmn.h"
#include "msp_errors.h"
#include "msp_types.h"
#include "qtts.h"
#include "qisr.h"
#include "play_link.h"


#define dbg	printf

/*tts*/
typedef struct{
	unsigned char *ttsAudioBuf;
	unsigned int ttsAudioBufLen;
	int ttsSynthStatus;
}tts_audio_t;

extern void tts_audio_init(void);
extern int tts_init(void);
//extern int tts_text_put(void);
extern int tts_text_put(unsigned char *text);
extern int tts_audio_get(void);
extern int tts_audio_link_play(play_link_t *pLinkHead,unsigned int size);
extern unsigned char* is_tts_have_data(void);
extern int tts_get_audio_len(void);
extern int tts_get_audio_status(void);
//extern int tts_audio_mem_free(void);
void tts_audio_get_thread_proc(void);
void tts_audio_play_thread(void);

/*asr*/
/* wav音频头部格式 */
typedef struct _wave_pcm_hdr
{
	char            riff[4];                // = "RIFF"
	int				size_8;                 // = FileSize - 8
	char            wave[4];                // = "WAVE"
	char            fmt[4];                 // = "fmt "
	int				fmt_size;		// = 下一个结构体的大小 : 16

	short int       format_tag;             // = PCM : 1
	short int       channels;               // = 通道数 : 1
	int				samples_per_sec;        // = 采样率 : 8000 | 6000 | 11025 | 16000
	int				avg_bytes_per_sec;      // = 每秒字节数 : samples_per_sec * bits_per_sample / 8
	short int       block_align;            // = 每采样点字节数 : wBitsPerSample / 8
	short int       bits_per_sample;        // = 量化比特数: 8 | 16

	char            data[4];                // = "data";
	int				data_size;              // = 纯数据长度 : FileSize - 44 
} wave_pcm_hdr;

typedef enum{
	text,
	cmd
}isrType_t;

typedef	struct{
	unsigned char *isrResult;
	isrType_t isrType;
	unsigned char *url;
}isrResult_t;

extern int isr_init(void);


/*voice common service*/
extern int voice_init(void);

void post_wakeup(void);

void isr_voice_record_thread_proc(void);

int isr_get_result(isrResult_t *p_isrResult);

#endif

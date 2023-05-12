#include "SDL_config.h"

/* Allow access to a raw mixing buffer */

#include <stdio.h>	/* For perror() */
#include <string.h>	/* For strerror() */
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>

#define DEBUG_AUDIO

#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../SDL_audiomem.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"
#include "SDL_miaoaudio.h"

#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <linux/input.h>
#include <SDL/SDL.h>

#ifdef HAVE_LIBMSETTINGS
#include <msettings.h> // TODO: remove me
#endif

/* The tag name used by MI_AO audio */
#define MIAO_DRIVER_NAME         "miao"

/* Audio driver functions */
static int MIAO_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void MIAO_WaitAudio(_THIS);
static void MIAO_PlayAudio(_THIS);
static Uint8* MIAO_GetAudioBuf(_THIS);
static void MIAO_CloseAudio(_THIS);

static int Audio_Available(void)
{
    MI_AUDIO_Attr_t    attr;
    memset(&attr, 0, sizeof(attr));
    attr.eSamplerate = 48000;
    attr.eSoundmode = 1;
    attr.u32ChnCnt = 2;
    attr.u32PtNumPerFrm = 256;
    if (MI_AO_SetPubAttr(0,&attr)) return(0);
    return(1);
}

static void Audio_DeleteDevice(SDL_AudioDevice *device)
{
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_AudioDevice *Audio_CreateDevice(int devindex)
{
	fprintf(stdout, "create miao audio device...\n");
	
	SDL_AudioDevice *this;

	/* Initialize all variables that we clean on shutdown */
	this = (SDL_AudioDevice *)SDL_malloc(sizeof(SDL_AudioDevice));
	if ( this ) {
		SDL_memset(this, 0, (sizeof *this));
		this->hidden = (struct SDL_PrivateAudioData *)
				SDL_malloc((sizeof *this->hidden));
	}
	if ( (this == NULL) || (this->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( this ) {
			SDL_free(this);
		}
		return(0);
	}
	SDL_memset(this->hidden, 0, (sizeof *this->hidden));

	/* Set the function pointers */
	this->OpenAudio = MIAO_OpenAudio;
	this->WaitAudio = MIAO_WaitAudio;
	this->PlayAudio = MIAO_PlayAudio;
	this->GetAudioBuf = MIAO_GetAudioBuf;
	this->CloseAudio = MIAO_CloseAudio;

	this->free = Audio_DeleteDevice;

	fprintf(stdout, "created miao audio device!\n");
	
	return this;
}

AudioBootStrap MIAO_bootstrap = {
	MIAO_DRIVER_NAME, "MI AO audio for Miyoo Mini",
	Audio_Available, Audio_CreateDevice
};

// based on AOSample_rev7

// #define	YIELD_WAIT	// Flag to wait with sched_yield() when the wait time is less than 10.5ms
			// ( callback function will be called at more precise timing,
			//   but may cause blocking/slowdown of low-priority daemons/threads execution )
#ifdef	YIELD_WAIT
#include <sched.h>
#ifndef	SCHED_IDLE
#define SCHED_IDLE	5
#endif
#endif

static struct timeval tod;
static int usleepclock;
static uint64_t startclock;
static uint64_t targetclock;
static uint64_t clock_freqframes;
static uint32_t framecounter;
static uint32_t num_frames;
#ifdef	YIELD_WAIT
static int policy;
static struct sched_param scprm;
#endif

/* This function waits until it is possible to write a full sound buffer */
static void MIAO_WaitAudio(_THIS)
{
	framecounter++;
	if (framecounter == (uint32_t)this->spec.freq) {
		framecounter = 0;
		startclock += clock_freqframes;
	}
	targetclock = framecounter * clock_freqframes / this->spec.freq + startclock;
	gettimeofday(&tod, NULL);
	usleepclock = targetclock - (tod.tv_usec + tod.tv_sec * 1000000);
	// check 300ms under/overrun (1frame max = 256ms at 8kHz/2048samples)
	if ((usleepclock < -300000)||(usleepclock > 300000)) {
		// reset buffer
		MI_AO_ClearChnBuf(0,0);
		memset(mixbuf, 0, mixlen);
		for (uint32_t i=num_frames-1; i>0; i--) MI_AO_SendFrame(0, 0, frame, 0);
		framecounter = 0;
		gettimeofday(&tod, NULL);
		startclock = tod.tv_usec + tod.tv_sec * 1000000;
	} else if (usleepclock > 0) {
#ifdef	YIELD_WAIT
		// wait process for miyoomini with 10ms sleep precision
		if (usleepclock > 10500) usleep(usleepclock - 10500);	// 0.5ms margin
		// wait for less than 10.5ms with sched_yield()
		sched_setscheduler(0, SCHED_IDLE, &scprm);
		do { sched_yield(); gettimeofday(&tod, NULL);
		} while (targetclock > (tod.tv_usec + tod.tv_sec * 1000000));
		sched_setscheduler(0, policy, &scprm);
#else
		usleep(usleepclock);
#endif
	}
}

static void MIAO_PlayAudio(_THIS)
{
	// MI_S32 ret = MI_AO_SendFrame(0, 0, frame, 0);
	// if (ret) fprintf(stdout, "MI_Error: err:0x%x\n", ret);
	
	// NOTE: this is not the cause of the crash/hang
	// or at least the response to it evaluating to true isn't
	if (MI_AO_SendFrame(0, 0, frame, 0))
	{
		perror("Audio write");
		this->enabled = 0;
	}

#ifdef DEBUG_AUDIO
	// fprintf(stdout, "Wrote %d bytes of audio data\n", mixlen);
#endif
}

static Uint8 *MIAO_GetAudioBuf(_THIS)
{
	return(mixbuf);
}

static void MIAO_CloseAudio(_THIS)
{
	fprintf(stdout, "close miao audio\n");

	MI_AO_ClearChnBuf(0,0);
	MI_AO_DisableChn(0,0);
	MI_AO_Disable(0);

#ifdef HAVE_LIBMSETTINGS
	QuitSettings();
#endif

	if ( mixbuf != NULL ) {
		SDL_FreeAudioMem(mixbuf);
		mixbuf = NULL;
	}

	if ( frame != NULL) {
		SDL_FreeAudioMem(frame);
		frame = NULL;
	}
}

//		signed 16bit only
//		sampling rate: 8/11.025/12/16/22.05/24/32/44.1/48kHz
//		samples should be 2048 or less

static int MIAO_OpenAudio(_THIS, SDL_AudioSpec *spec)
{
	fprintf(stdout, "open miao audio\n");
	
	MI_AUDIO_Attr_t	attr;
	const int freqtable[] = { 8000,11025,12000,16000,22050,24000,32000,44100,48000 };
	uint32_t	i;

	spec->format = AUDIO_S16SYS;
	for (i=0; i<(sizeof(freqtable)/sizeof(int)); i++) {
		if (spec->freq <= freqtable[i]) { spec->freq = freqtable[i]; break; }
	} if (spec->freq > 48000) spec->freq = 48000;
	if (spec->samples > 2048) spec->samples = 2048;
	else if (spec->samples < 8) spec->samples = 8;
	spec->size = spec->samples * spec->channels * 2;

	memset(&attr, 0, sizeof(attr));
	attr.eSamplerate = (MI_AUDIO_SampleRate_e)spec->freq;
	attr.eSoundmode = (MI_AUDIO_SoundMode_e)(spec->channels - 1);
	attr.u32ChnCnt = spec->channels;
	attr.u32PtNumPerFrm = spec->samples;

	if (MI_AO_SetPubAttr(0,&attr)) return -1;
	if (MI_AO_Enable(0)) return -1;
	if (MI_AO_EnableChn(0,0)) return -1;
	if (MI_AO_SetMute(0,FALSE)) return -1;
	//if (MI_AO_SetVolume(0,0)) return -1;

#ifdef HAVE_LIBMSETTINGS
	InitSettings(); // restores volume
#endif

	mixlen = spec->size;
	mixbuf = (Uint8 *)SDL_AllocAudioMem(mixlen);
	if ( mixbuf == NULL ) {
		MIAO_CloseAudio(this);
		return(-1);
	}
	memset(mixbuf, 0, mixlen);
	
#ifdef	YIELD_WAIT
	policy = sched_getscheduler(0);
	scprm.sched_priority = 0;
#endif
	
	frame = (MI_AUDIO_Frame_t*)SDL_AllocAudioMem(sizeof(MI_AUDIO_Frame_t));
	if ( frame == NULL ) {
		MIAO_CloseAudio(this);
		return(-1);
	}
	memset(frame, 0, sizeof(MI_AUDIO_Frame_t));
	frame->apVirAddr[0] = mixbuf;
	frame->u32Len = mixlen;
	
	fprintf(stdout, "buffer size: %d\n", mixlen);
	
	// Buffer initial frames (calculate at least 20ms)
	num_frames = (uint32_t)((spec->freq-1) / (50*spec->samples)) +1;
	if (num_frames < 2) num_frames = 2;
	MI_AO_ClearChnBuf(0,0);
	for (i=num_frames; i>0; i--) MI_AO_SendFrame(0, 0, frame, 0);
	
	clock_freqframes = spec->samples * 1000000;
	framecounter = 0;
	gettimeofday(&tod, NULL);
	startclock = tod.tv_usec + tod.tv_sec * 1000000;
	
	/* Get the parent process id (we're the parent of the audio thread) */
	parent = getpid();
	
	return(0);
}

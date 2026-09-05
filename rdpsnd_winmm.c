/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rdesktop.h"
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rdpsnd.h"
#include "rdpsnd_winmm_logic.h"

typedef struct { WAVEHDR hdr; unsigned char *data; unsigned int bytes; } WINMM_BUFFER;
static struct audio_driver driver;
static HWAVEOUT output;
static WAVEFORMATEX active;
static WINMM_BUFFER buffer;
static unsigned long queued_bytes, max_queued_bytes, submitted, drops, buffer_waits;
static unsigned long physical_opens, physical_closes, logical_closes, format_reuses;
static RD_BOOL cleanup_registered, trace_initialized, trace_enabled;

static void play(void);

static RD_BOOL trace_on(void)
{
	if (!trace_initialized) { const char *v=getenv("RDESKTOP_WIN32_AUDIO_DEBUG"); trace_enabled=v&&v[0]&&strcmp(v,"0"); trace_initialized=True; }
	return trace_enabled;
}
static void reap(void)
{
	if (!buffer.data || !(buffer.hdr.dwFlags & WHDR_DONE)) return;
	waveOutUnprepareHeader(output,&buffer.hdr,sizeof(buffer.hdr));
	queued_bytes=queued_bytes>=buffer.bytes?queued_bytes-buffer.bytes:0;
	free(buffer.data); memset(&buffer,0,sizeof(buffer));
}
static RD_BOOL open_device(void) { return waveOutGetNumDevs()>0; }
static void hard_close(void)
{
	if (!output) return;
	waveOutReset(output);
	if (buffer.data) { waveOutUnprepareHeader(output,&buffer.hdr,sizeof(buffer.hdr)); free(buffer.data); memset(&buffer,0,sizeof(buffer)); }
	waveOutClose(output); output=NULL; queued_bytes=0; physical_closes++;
}
static void cleanup(void)
{
	unsigned long final_tail_ms;

	reap();
	final_tail_ms = winmm_audio_duration_us(queued_bytes,
						 active.nAvgBytesPerSec) / 1000;
	hard_close();
	if (trace_on()) fprintf(stderr,"TRACE RDPSND WINMM SUMMARY submitted=%lu drops=%lu max_queue_ms=%lu final_tail_ms=%lu buffer_waits=%lu physical_opens=%lu physical_closes=%lu logical_closes=%lu format_reuses=%lu\n",submitted,drops,winmm_audio_duration_us(max_queued_bytes,active.nAvgBytesPerSec)/1000,final_tail_ms,buffer_waits,physical_opens,physical_closes,logical_closes,format_reuses);
}
static void logical_close(void)
{
	logical_closes++;
	/* Windows servers may put SNDC_WAVE and SNDC_CLOSE in the same channel
	 * delivery. Submit the queued wave before the core marks the device closed. */
	play();
	reap();
}
static RD_BOOL format_supported(RD_WAVEFORMATEX *f)
{
	WAVEFORMATEX w;
	if (!winmm_pcm_format_valid(f->wFormatTag,f->nChannels,f->nSamplesPerSec,f->nAvgBytesPerSec,f->nBlockAlign,f->wBitsPerSample)) return False;
	memset(&w,0,sizeof(w)); w.wFormatTag=WAVE_FORMAT_PCM; w.nChannels=f->nChannels; w.nSamplesPerSec=f->nSamplesPerSec; w.nAvgBytesPerSec=f->nAvgBytesPerSec; w.nBlockAlign=f->nBlockAlign; w.wBitsPerSample=f->wBitsPerSample;
	return waveOutOpen(NULL,WAVE_MAPPER,&w,0,0,WAVE_FORMAT_QUERY)==MMSYSERR_NOERROR;
}
static RD_BOOL set_format(RD_WAVEFORMATEX *f)
{
	if (!format_supported(f)) return False;
	if (output && active.nChannels==f->nChannels && active.nSamplesPerSec==f->nSamplesPerSec && active.nAvgBytesPerSec==f->nAvgBytesPerSec && active.nBlockAlign==f->nBlockAlign && active.wBitsPerSample==f->wBitsPerSample) { format_reuses++; reap(); return True; }
	hard_close(); memset(&active,0,sizeof(active)); active.wFormatTag=WAVE_FORMAT_PCM; active.nChannels=f->nChannels; active.nSamplesPerSec=f->nSamplesPerSec; active.nAvgBytesPerSec=f->nAvgBytesPerSec; active.nBlockAlign=f->nBlockAlign; active.wBitsPerSample=f->wBitsPerSample;
	if (waveOutOpen(&output,WAVE_MAPPER,&active,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR) { output=NULL; return False; }
	physical_opens++; return True;
}
static void volume(uint16 l,uint16 r) { if(output) waveOutSetVolume(output,((DWORD)r<<16)|l); }
static void play(void)
{
	while (output && !rdpsnd_queue_empty()) {
		struct audio_packet *p; STREAM s; unsigned int size; MMRESULT rc;
		reap(); if (buffer.data) { buffer_waits++; return; }
		p=rdpsnd_queue_current_packet(); s=p->s; size=s_remaining(s);
		buffer.data=(unsigned char*)malloc(size?size:1); if(!buffer.data) return;
		buffer.bytes=size; if(size) memcpy(buffer.data,s->p,size);
		memset(&buffer.hdr,0,sizeof(buffer.hdr)); buffer.hdr.lpData=(LPSTR)buffer.data; buffer.hdr.dwBufferLength=size;
		rc=waveOutPrepareHeader(output,&buffer.hdr,sizeof(buffer.hdr)); if(rc==MMSYSERR_NOERROR) rc=waveOutWrite(output,&buffer.hdr,sizeof(buffer.hdr));
		if(rc!=MMSYSERR_NOERROR) { if(buffer.hdr.dwFlags&WHDR_PREPARED) waveOutUnprepareHeader(output,&buffer.hdr,sizeof(buffer.hdr)); free(buffer.data); memset(&buffer,0,sizeof(buffer)); drops++; rdpsnd_queue_next(0); continue; }
		queued_bytes+=size; if(queued_bytes>max_queued_bytes) max_queued_bytes=queued_bytes; submitted++;
		rdpsnd_queue_next(winmm_audio_duration_us(queued_bytes,active.nAvgBytesPerSec));
	}
}
static void add_fds(int *n,fd_set *r,fd_set *w,struct timeval *t){UNUSED(n);UNUSED(r);UNUSED(w);UNUSED(t);}
static void check_fds(fd_set *r,fd_set *w){UNUSED(r);UNUSED(w);reap();if(output&&!rdpsnd_queue_empty()&&!buffer.data)play();}
struct audio_driver *winmm_register(char *options)
{
	UNUSED(options); memset(&driver,0,sizeof(driver)); if(!cleanup_registered){atexit(cleanup);cleanup_registered=True;}
	driver.name="winmm"; driver.description="Windows waveOut PCM output driver"; driver.add_fds=add_fds; driver.check_fds=check_fds; driver.wave_out_open=open_device; driver.wave_out_close=logical_close; driver.wave_out_format_supported=format_supported; driver.wave_out_set_format=set_format; driver.wave_out_volume=volume; driver.wave_out_drain_after_close=True; return &driver;
}

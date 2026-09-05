#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <stdio.h>
#include "rdpsnd_winmm_logic.h"
int main(void)
{
	int ok = 1;
	ok &= winmm_pcm_format_valid(WAVE_FORMAT_PCM,2,44100,176400,4,16);
	ok &= winmm_pcm_format_valid(WAVE_FORMAT_PCM,1,22050,22050,1,8);
	ok &= !winmm_pcm_format_valid(WAVE_FORMAT_ADPCM,2,44100,176400,4,16);
	ok &= !winmm_pcm_format_valid(WAVE_FORMAT_PCM,3,44100,264600,6,16);
	ok &= !winmm_pcm_format_valid(WAVE_FORMAT_PCM,2,44100,176400,2,16);
	ok &= winmm_audio_duration_us(17640,176400) == 100000;
	ok &= winmm_audio_duration_us(100,0) == 0;
	printf("WinMM RDPSND logic: %s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}

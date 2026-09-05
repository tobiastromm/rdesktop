#ifndef RDESKTOP_RDPSND_WINMM_LOGIC_H
#define RDESKTOP_RDPSND_WINMM_LOGIC_H
static int winmm_pcm_format_valid(unsigned int tag, unsigned int channels,
	unsigned int rate, unsigned int avg, unsigned int align, unsigned int bits)
{
	unsigned int expected;
	if (tag != WAVE_FORMAT_PCM || (channels != 1 && channels != 2) ||
	    (bits != 8 && bits != 16) || rate < 4000 || rate > 192000)
		return 0;
	expected = channels * (bits / 8);
	return align == expected && avg == rate * expected;
}
static unsigned long winmm_audio_duration_us(unsigned long bytes, unsigned long avg)
{
	return avg ? (unsigned long)(((unsigned long long)bytes * 1000000ULL) / avg) : 0;
}
#endif

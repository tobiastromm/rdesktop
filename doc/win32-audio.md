# Win32 remote audio

The Win32 client reuses the upstream `rdpsnd.c` protocol, negotiation, queue,
and WaveConfirm scheduling. The `winmm` driver implements the existing audio
driver interface with asynchronous `waveOut` output.

Use `-r sound:local:winmm` for local playback, `-r sound:off` to disable sound,
or `-r sound:remote` to leave sound at the server. The shared configuration
model represents disabled and local-output modes; the existing upstream CLI
parser remains authoritative.

The backend accepts PCM formats dynamically after validating the channel count,
sample width, rate, block alignment, and average byte rate, then queries WinMM
with `WAVE_FORMAT_QUERY`. It does not hard-code the format observed in any one
application.

One `WAVEHDR` may be outstanding. A complete RDPSND block is copied and
submitted without fragmentation. While that header is busy, the existing core
queue provides backpressure. Completion is reaped from the normal main-loop
callback; there is no sleep, spin, or playback wait on the network/UI thread.
The queue advances with the estimated queued playback duration, preserving the
upstream timed WaveConfirm policy. Logical `SNDC_CLOSE` retains the device;
reset and physical close occur only for a real format change or process exit.
The WinMM driver remains eligible for main-loop completion callbacks after a
logical close so multi-block effects drain fully; other upstream drivers retain
their original lifecycle.

`RDESKTOP_WIN32_AUDIO_DEBUG=1` enables a single aggregate summary at process
exit. It reports submissions, failed submissions, maximum queued duration,
final queued tail,
buffer backpressure, physical opens/closes, logical closes, and same-format
reuse. It does not log PCM data or individual blocks.

Capture/microphone redirection and non-PCM codecs are outside this backend.

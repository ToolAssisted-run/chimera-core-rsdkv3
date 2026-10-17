/* theora-stubs.c - the video decoder the engine links against, answering
 * that no video can be decoded.
 *
 * The engine plays its .ogv videos (Sonic CD's opening and endings) with
 * theoraplay, which decodes on a thread of its own; the core has no threads,
 * and does not play videos yet (docs/PLAN.md). The engine never gets this
 * far in any case: it opens a video's file first, and the core has none to
 * give (rsdk-files.c). If it did, a decoder that does not start makes the
 * engine skip the video, as it does when the file is missing.
 */
#include <stddef.h>

#include "theoraplay.h"

THEORAPLAY_Decoder *THEORAPLAY_startDecode(THEORAPLAY_Io *io, const unsigned int maxframes, THEORAPLAY_VideoFormat vidfmt, unsigned int audio_bitstream)
{
	(void)maxframes; (void)vidfmt; (void)audio_bitstream;
	if (io && io->close) io->close(io);
	return NULL;
}
void THEORAPLAY_stopDecode(THEORAPLAY_Decoder *decoder) { (void)decoder; }
int THEORAPLAY_isDecoding(THEORAPLAY_Decoder *decoder) { (void)decoder; return 0; }
const THEORAPLAY_AudioPacket *THEORAPLAY_getAudio(THEORAPLAY_Decoder *decoder) { (void)decoder; return NULL; }
void THEORAPLAY_freeAudio(const THEORAPLAY_AudioPacket *item) { (void)item; }
const THEORAPLAY_VideoFrame *THEORAPLAY_getVideo(THEORAPLAY_Decoder *decoder) { (void)decoder; return NULL; }
void THEORAPLAY_freeVideo(const THEORAPLAY_VideoFrame *item) { (void)item; }

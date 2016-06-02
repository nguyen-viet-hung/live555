#ifndef VIDEO_DECODER_H__
#define VIDEO_DECODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>

#ifdef __cplusplus
}
#endif

class VideoDecoder
{
protected:
	AVCodecContext* avctx;
	AVFrame* avframe;
public:
	VideoDecoder(void);
	virtual ~VideoDecoder(void);

	int openCodec(int codecID);
	AVFrame* decode(const unsigned char* buffer, int buffer_size, int& consumed);
};

#endif//VIDEO_DECODER_H__
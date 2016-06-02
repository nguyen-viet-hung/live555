#include "videodecoder.h"

#if defined(_WIN32) || defined(WIN32)
#pragma warning(disable:4996)
#endif

VideoDecoder::VideoDecoder(void)
	: avctx(NULL)
	, avframe(NULL)
{
}


VideoDecoder::~VideoDecoder(void)
{
	if (avctx) {
		avcodec_close(avctx);
		avctx = NULL;
	}

	if (avframe) {
		av_frame_free(&avframe);
		avframe = NULL;
	}
}


int VideoDecoder::openCodec(int codecID)
{
	AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!codec)
		return -1;

	avctx = avcodec_alloc_context3(codec);
	if (!avctx)
		return -1;

	if(codec->capabilities & CODEC_CAP_TRUNCATED)
		avctx->flags |= CODEC_FLAG_TRUNCATED; /* We may send incomplete frames */
	if(codec->capabilities & CODEC_FLAG2_CHUNKS)
		avctx->flags |= CODEC_FLAG2_CHUNKS;

	if (avcodec_open2(avctx, codec, NULL) < 0)
		return -1;

	avframe = av_frame_alloc();
	return 0;
}

AVFrame* VideoDecoder::decode(const unsigned char* buffer, int buffer_size, int& consumed)
{
	AVPacket pkt;// = {0};

	av_init_packet(&pkt);
	pkt.data = (uint8_t*)buffer;
	pkt.size = buffer_size;

	int got_frame;
	int len;

	consumed = 0;
	while (pkt.size) {
		len = avcodec_decode_video2(avctx, avframe, &got_frame, &pkt);
		if (len < 0) {
			consumed = pkt.size;
			return NULL;
		}

		if (got_frame) {
			consumed += len;
			return avframe;
		}

		pkt.data += len;
		pkt.size -= len;
		consumed += len;
	}

	return NULL;
}
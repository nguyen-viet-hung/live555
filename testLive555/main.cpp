#include "live555client.h"
#include <iostream>
#include <conio.h>
#include "videodecoder.h"

#if defined(_WIN32) || defined(WIN32)
#pragma warning(disable:4996)
#endif

class myClient : public Live555Client
{
protected:
	VideoDecoder* decoder;
public:
	myClient() : decoder(NULL) {}
	virtual ~myClient() {}

	virtual void onData(LiveTrack* track, uint8_t* p_buffer, int i_size, int i_truncated_bytes, int64_t pts, int64_t dts)
	{
		//std::cout << "Got Data. size = " << i_size << "; truncated bytes = " << i_truncated_bytes << "; pts = " << pts << "; dts = " << dts << std::endl;
		//std::cout << "Got Data. size = " << i_size << "; pts = " << pts << std::endl;
		int consumed;

		if (track->getFormat().i_codec != VLC_CODEC_H264)
			return;

		//std::cout << "Got H264 Data. size = " << i_size << "; truncated bytes = " << i_truncated_bytes << "; NAL type = " << (int)(p_buffer[4] & 0x1f) << std::endl;

		if (!decoder) {
			decoder = new VideoDecoder();
			decoder->openCodec(0);
			if (track->getFormat().p_extra) {
				decoder->decode(track->getFormat().p_extra, track->getFormat().i_extra, consumed);
			}
		}

		uint8_t* tmp = p_buffer;
		int left = i_size;

		while(left) {
			AVFrame* ret = decoder->decode(tmp, left, consumed);
			if (ret)
				std::cout << "Got frame!!!" << std::endl;

			tmp += consumed;
			left -= consumed;
		}
	}
};

int main(int argc, char** argv) {

	avcodec_register_all();
	myClient* client;
	//const char* url = "rtsp://192.168.61.80/axis-media/media.amp";
	const char* url = "rtsp://rtsp-v3-spbtv.msk.spbtv.com/spbtv_v3_1/118_350.sdp";
	client = new myClient();
	client->setRTPPortBegin(6868);
	int ret = client->open(url);
	if ( ret == 401) //authentication failed
	{
		client->setUser("root", "admin");
		ret = client->open(url);
	}

	if (!ret)
	{
		client->play();
		char c;
		do 
		{
			c = getch();

			if (c == 'p' || c== 'P')
				client->togglePause();
		}
		while ((c != (char)27) && (!client->isNeedStop()));
	}

	client->stop();

	delete client;

	return 0;
}
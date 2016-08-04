#include "live555client.h"
#include <iostream>
#include "videodecoder.h"

#if defined(_WIN32) || defined(WIN32)
#include <conio.h>
#pragma warning(disable:4996)
#else

#include <unistd.h>
#include <termios.h>
#include <stdio.h>

static struct termios _old, _new;

/* Initialize new terminal i/o settings */
void initTermios(int echo)
{
  tcgetattr(0, &_old); /* grab old terminal i/o settings */
  _new = _old; /* make new settings same as old settings */
  _new.c_lflag &= ~ICANON; /* disable buffered i/o */
  _new.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
  tcsetattr(0, TCSANOW, &_new); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void)
{
  tcsetattr(0, TCSANOW, &_old);
}

/* Read 1 character - echo defines echo mode */
char getch_(int echo)
{
  char ch;
  initTermios(echo);
  ch = getchar();
  resetTermios();
  return ch;
}

/* Read 1 character without echo */
char getch(void)
{
  return getch_(0);
}

#endif

class myClient: public Live555Client {
protected:
	VideoDecoder* decoder;
public:
	myClient() :
			decoder(NULL) {
	}
	virtual ~myClient() {
	}

	virtual void onData(LiveTrack* track, uint8_t* p_buffer, int i_size,
			int i_truncated_bytes, int64_t pts, int64_t dts) {
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
				decoder->decode(track->getFormat().p_extra,
						track->getFormat().i_extra, consumed);
			}
		}

		uint8_t* tmp = p_buffer;
		int left = i_size;

		while (left) {
			AVFrame* ret = decoder->decode(tmp, left, consumed);
			if (ret) {
				av_frame_unref(ret);
				std::cout << "Got frame!!!" << std::endl;
			}

			tmp += consumed;
			left -= consumed;
		}
	}
};

int main(int argc, char** argv) {

	avcodec_register_all();
	myClient* client;
	const char* url = "rtsp://192.168.61.90/axis-media/media.amp";
	//const char* url = "rtsp://rtsp-v3-spbtv.msk.spbtv.com/spbtv_v3_1/118_350.sdp";
	//const char* url = argv[1];
	client = new myClient();
	client->setRTPPortBegin(6868);
	int ret = client->open(url);
	if (ret == 401) //authentication failed
			{
		client->setUser("root", "admin");
		ret = client->open(url);
	}

	if (!ret) {
		client->play();
		char c;
		do {
			c = getch();

			if (c == 'p' || c == 'P')
				client->togglePause();
			else if (c == 'r' || c == 'R') {
				client->stop();
				if (!client->open(url))
					client->play();
			}
		} while ((c != (char) 27) && (!client->isNeedStop()));
	}

	client->stop();

	delete client;

	return 0;
}

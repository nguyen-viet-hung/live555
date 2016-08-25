#include "live555client.h"
#include <iostream>
#include "videodecoder.h"
#include <mutex>

#if defined(_WIN32) || defined(WIN32)
#include <conio.h>
#pragma warning(disable:4996)
#else

#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <sys/select.h>
#include <stropts.h>

static struct termios _old, _new;

/* Initialize new terminal i/o settings */
void initTermios(int echo) {
	tcgetattr(0, &_old); /* grab old terminal i/o settings */
	_new = _old; /* make new settings same as old settings */
	_new.c_lflag &= ~ICANON; /* disable buffered i/o */
	_new.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
	tcsetattr(0, TCSANOW, &_new); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void) {
	tcsetattr(0, TCSANOW, &_old);
}

/* Read 1 character - echo defines echo mode */
char getch_(int echo) {
	char ch;
	initTermios(echo);
	ch = getchar();
	resetTermios();
	return ch;
}

/* Read 1 character without echo */
char getch(void) {
	return getch_(0);
}

/**
Linux (POSIX) implementation of _kbhit().
Morgan McGuire, morgan@cs.brown.edu
*/

int _kbhit() {
	static const int STDIN = 0;
	static bool initialized = false;

	if (! initialized) {
		// Use termios to turn off line buffering
		termios term;
		tcgetattr(STDIN, &term);
		term.c_lflag &= ~ICANON;
		tcsetattr(STDIN, TCSANOW, &term);
		setbuf(stdin, NULL);
		initialized = true;
	}

	int bytesWaiting;
	ioctl(STDIN, FIONREAD, &bytesWaiting);
	return bytesWaiting;
}

#endif

// for libavcodec
static int ff_lockmgr(void **mutex, enum AVLockOp op)
{
	std::recursive_mutex** pmutex = (std::recursive_mutex**) mutex;
	switch (op) {
	case AV_LOCK_CREATE:
		*pmutex = new std::recursive_mutex();
		break;
	case AV_LOCK_OBTAIN:
		(*pmutex)->lock();
		break;
	case AV_LOCK_RELEASE:
		(*pmutex)->unlock();
		break;
	case AV_LOCK_DESTROY:
		delete (*pmutex);
		break;
	}
	return 0;
}

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
					std::cout << "client " << this << " got frame!!!\n";
				}

				tmp += consumed;
				left -= consumed;
			}
	}
};

int main(int argc, char** argv) {

	avcodec_register_all();
	av_log_set_level(AV_LOG_PANIC);
	av_lockmgr_register(ff_lockmgr);

	myClient* client;
	//const char* url = "rtsp://mpv.cdn3.bigCDN.com/bigCDN/_definst_/mp4:bigbuckbunnyiphone_400.mp4";
	//const char* url = "rtsp://192.168.61.90/axis-media/media.amp";
	//const char* url = "rtsp://rtsp-v3-spbtv.msk.spbtv.com/spbtv_v3_1/118_350.sdp";
	//const char* url = argv[1];
	//const char* url = "rtsp://nvr:nvr@192.168.6.245:8554/11100608?t1=20160812-140000&t2=20160812-140245&speed=1";
	const char* url = "rtsp://root:admin@192.168.61.80/axis-media/media.amp";
	client = new myClient();
	client->setRTPPortBegin(6868);
	int ret = client->open(url);
	if (ret == 401) //authentication failed
	{
		client->setUser("root", "admin");
		ret = client->open(url);
	}

	myClient* client1 = new myClient();
	client1->setRTPPortBegin(5000);
	client1->open("rtsp://root:elcom@192.168.61.60/axis-media/media.amp");

	if (!ret) {
		client->play();
		client1->play();
		std::cout << "\n\n";
		char c;
		do {

			if (_kbhit())
			{
				c = getch();

				if (c == 'p' || c == 'P')
					client->togglePause();
				else if (c == 'r' || c == 'R') {
					client->stop();
					if (!client->open(url))
						client->play();
				}
				else if (c == 'b' || c == 'B') {
					int64_t cur = client->getCurrentTime();
					//seek back 5 second
					cur -= 5000000;
					if (client->seek((double)cur / 1000000.0) < 0)
						std::cout <<"\nUnsupported seek" << std::endl;
				}
				else if (c == 'f' || c == 'F') {
					int64_t cur = client->getCurrentTime();
					//seek forward 5 second
					cur += 5000000;
					if (client->seek((double)cur / 1000000.0) < 0)
						std::cout <<"\nUnsupported seek" << std::endl;
				}
				else if (c == 27) {
					break;
				}
			}
			else
			{
				_sleep(1);
			}

			//std::cout << "Current time = " << client->getCurrentTime() / 1000000 << "\r";
		} while (!client->isNeedStop());
	}

	client->stop();

	delete client;

	client1->stop();
	delete client1;

	return 0;
}

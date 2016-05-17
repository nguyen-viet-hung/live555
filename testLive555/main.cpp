#include "live555client.h"
#include <iostream>
#include <conio.h>

#if defined(_WIN32) || defined(WIN32)
#pragma warning(disable:4996)
#endif

class myClient : public Live555Client
{
public:
	myClient() {}
	virtual ~myClient() {}

	virtual void onData(LiveTrack* track, uint8_t* p_buffer, int i_size, int i_truncated_bytes, int64_t pts, int64_t dts)
	{
		//std::cout << "Got Data. size = " << i_size << "; truncated bytes = " << i_truncated_bytes << "; pts = " << pts << "; dts = " << dts << std::endl;
		std::cout << "Got Data. size = " << i_size << "; pts = " << pts << std::endl;
	}
};

int main(int argc, char** argv) {
	myClient* client;
	const char* url = "rtsp://192.168.61.80/axis-media/media.amp";

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
		}
		while (c != 27);
	}

	client->stop();

	delete client;

	return 0;
}
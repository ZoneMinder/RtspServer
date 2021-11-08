// RTSP Pusher

#include "xop/RtspPusher.h"
#include "xop/DigestAuthenticator.h"
#include "net/Timer.h"
#include <thread>
#include <memory>
#include <iostream>
#include <string>

#include "md5/md5.hpp"

// #define PUSH_TEST "rtsp://10.11.165.203:554/test"

class H264File
{
public:
	H264File(int buf_size=500000);
	~H264File();

	bool Open(const char *path);
	void Close();

	bool IsOpened() const
	{ return (m_file != NULL); }

	int ReadFrame(char* in_buf, int in_buf_size, bool* end);

private:
	FILE *m_file = NULL;
	char *m_buf = NULL;
	int  m_buf_size = 0;
	int  m_bytes_used = 0;
	int  m_count = 0;
};

void sendFrameThread(xop::RtspPusher* rtspPusher, H264File* h264_file);

int main(int argc, char **argv)
{
	if(argc != 3) {
		printf("Usage: %s <h264file.h264> <rtspaddr> \n", argv[0]);
		return 0;
	}

	H264File h264_file;
	if(!h264_file.Open(argv[1])) {
		printf("Open %s failed.\n", argv[1]);
		return 0;
	}

	std::shared_ptr<xop::EventLoop> event_loop(new xop::EventLoop());
	std::shared_ptr<xop::RtspPusher> rtsp_pusher = xop::RtspPusher::Create(event_loop.get());

	xop::MediaSession *session = xop::MediaSession::CreateNew();
	session->AddSource(xop::channel_0, xop::H264Source::CreateNew());
	// session->AddSource(xop::channel_1, xop::AACSource::CreateNew(44100, 2, false));
	rtsp_pusher->AddSession(session);

/*
    std::string serverIP("127.0.0.1");
    uint16_t port = 8009;
    std::string channel("ar4-stream");
*/
    std::string username("");
    std::string password("");

    std::string rtspaddr(argv[2]);
    size_t it = rtspaddr.find("rtsp://") + strlen("rtsp://");
    std::string tstr = rtspaddr.substr(it,40); // should be within the first 40 chars after rtsp://
    // std::cout << tstr << std::endl;
    size_t it2 = tstr.find("@");
    if(it2 != tstr.npos) {
        size_t it2 = rtspaddr.find(":",it);
        std::cout << "==== user: " << rtspaddr.substr(it, it2-it) << std::endl;
        username = rtspaddr.substr(it, it2-it);
        size_t it3 = rtspaddr.find("@",it2);
        std::cout << "==== pass: " << rtspaddr.substr(it2+1, it3-it2-1) << std::endl;
        password = rtspaddr.substr(it2+1, it3-it2-1);
        it = it3+1;
    }
    it2 = rtspaddr.find(":", it);
    std::cout << "==== server: " << rtspaddr.substr(it, it2-it) << std::endl;
    std::string serverIP = rtspaddr.substr(it, it2-it);
    size_t it3 = rtspaddr.find("/",it2+1);
    std::cout << "==== port: " << rtspaddr.substr(it2+1, it3-it2-1) << std::endl;
    uint16_t port = atoi(rtspaddr.substr(it2+1, it3-it2-1).c_str());
    std::string channel(rtspaddr.substr(it3+1));
    std::cout << "==== channel: " << channel << std::endl;
    
/*
    std::string serverIP("192.168.0.32");
    uint16_t port = 8554;
    std::string channel("live");
    std::string username("");
    std::string password("");
   */
	if(username.size() && password.size())
    {
        std::shared_ptr<xop::DigestAuthenticator> auth = std::shared_ptr<xop::DigestAuthenticator>(new xop::DigestAuthenticator("-_-", username, password)); // "visocon.rtsp"
        rtsp_pusher->SetAuthenticator(auth);
    }

	std::string connectString = "rtsp://" + serverIP + ":" + std::to_string(port) + "/" + channel;
	if (rtsp_pusher->OpenUrl(connectString.c_str(), 3000) < 0) {
		std::cout << "Open " << connectString.c_str() << " failed." << std::endl;
		getchar();
		return 0;
	}

	std::cout << "Push stream to " << connectString.c_str() << " ..." << std::endl;

	std::thread thread(sendFrameThread, rtsp_pusher.get(),&h264_file);
	thread.detach();

	while (1) {
		xop::Timer::Sleep(100);
	}

	getchar();
	return 0;
}

void sendFrameThread(xop::RtspPusher* rtsp_pusher, H264File* h264_file)
{
	while(rtsp_pusher->IsConnected())
	{
		int buf_size = 2000000;
		std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);

		bool end_of_frame = false;
		int frame_size = h264_file->ReadFrame((char*)frame_buf.get(), buf_size, &end_of_frame);
		if(frame_size > 0) {
			xop::AVFrame videoFrame;
			videoFrame.type = 0;
            auto time_point = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());
            int64_t tp = (int64_t)((time_point.time_since_epoch().count() + 500) / 1000 * 90 );
            videoFrame.timestamp = xop::H264Source::GetTimestamp(); // tp;
			//videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
			//memcpy(videoFrame.buffer.get(), frame_buf.get(), videoFrame.size);
			videoFrame.buffer.resize(frame_size);
			memcpy(videoFrame.buffer.data(), frame_buf.get(), frame_size);
			rtsp_pusher->PushFrame(xop::channel_0, videoFrame);
		}
		else {
			break;
		}

		{
			/*
				//获取一帧 AAC, 打包
				xop::AVFrame audioFrame = {0};
				//audioFrame.size = audio frame size;  // 音频帧大小
				audioFrame.timestamp = xop::AACSource::GetTimestamp(44100); // 时间戳
				audioFrame.buffer.reset(new uint8_t[audioFrame.size]);
				//memcpy(audioFrame.buffer.get(), audio frame data, audioFrame.size);

				rtsp_pusher->PushFrame(xop::channel_1, audioFrame); //推流到服务器, 接口线程安全
			*/
		}

		xop::Timer::Sleep(40);
	}
}

H264File::H264File(int buf_size)
    : m_buf_size(buf_size)
{
	m_buf = new char[m_buf_size];
}

H264File::~H264File()
{
	delete m_buf;
}

bool H264File::Open(const char *path)
{
#if WIN32
	errno_t err = fopen_s(&m_file, path, "rb");
#else
	m_file = fopen(path, "rb");
#endif
	if(m_file == NULL) {
		return false;
	}

	return true;
}

void H264File::Close()
{
	if(m_file) {
		fclose(m_file);
		m_file = NULL;
		m_count = 0;
		m_bytes_used = 0;
	}
}

int H264File::ReadFrame(char* in_buf, int in_buf_size, bool* end)
{
	if(m_file == NULL) {
		return -1;
	}

	int bytes_read = (int)fread(m_buf, 1, m_buf_size, m_file);
	if(bytes_read == 0) {
		fseek(m_file, 0, SEEK_SET);
		m_count = 0;
		m_bytes_used = 0;
		bytes_read = (int)fread(m_buf, 1, m_buf_size, m_file);
		if(bytes_read == 0)         {
			this->Close();
			return -1;
		}
	}

	bool is_find_start = false, is_find_end = false;
	int i = 0, start_code = 3;
	*end = false;

	for (i=0; i<bytes_read-5; i++) {
		if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 1) {
			start_code = 3;
		}
		else if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 0 && m_buf[i+3] == 1) {
			start_code = 4;
		}
		else  {
			continue;
		}

		if (((m_buf[i+start_code]&0x1F) == 0x5 || (m_buf[i+start_code]&0x1F) == 0x1)
			&& ((m_buf[i+start_code+1]&0x80) == 0x80)) {
			is_find_start = true;
			i += 4;
			break;
		}
	}

	for (; i<bytes_read-5; i++) {
		if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 1)
		{
			start_code = 3;
		}
		else if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 0 && m_buf[i+3] == 1) {
			start_code = 4;
		}
		else   {
			continue;
		}

		if (((m_buf[i+start_code]&0x1F) == 0x7) || ((m_buf[i+start_code]&0x1F) == 0x8)
			|| ((m_buf[i+start_code]&0x1F) == 0x6)|| (((m_buf[i+start_code]&0x1F) == 0x5
			|| (m_buf[i+start_code]&0x1F) == 0x1) &&((m_buf[i+start_code+1]&0x80) == 0x80)))  {
			is_find_end = true;
			break;
		}
	}

	bool flag = false;
	if(is_find_start && !is_find_end && m_count>0) {
		flag = is_find_end = true;
		i = bytes_read;
		*end = true;
	}

	if(!is_find_start || !is_find_end) {
		this->Close();
		return -1;
	}

	int size = (i<=in_buf_size ? i : in_buf_size);
	memcpy(in_buf, m_buf, size);

	if(!flag) {
		m_count += 1;
		m_bytes_used += i;
	}
	else {
		m_count = 0;
		m_bytes_used = 0;
	}

	fseek(m_file, m_bytes_used, SEEK_SET);
	return size;
}

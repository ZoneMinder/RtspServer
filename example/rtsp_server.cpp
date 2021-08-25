// RTSP Server

#include "xop/RtspServer.h"
#include "net/Timer.h"
#include <thread>
#include <memory>
#include <iostream>
#include <string>

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

void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, H264File* file, int& clients);

int main(int argc, char **argv)
{
    if(argc != 2) {
        printf("Usage: %s test.h264 \n", argv[0]);
        return 0;
    }
    
	int clients = 0;
	std::string ip = "0.0.0.0";
	std::string rtsp_url = "rtsp://127.0.0.1:8554/live";

    H264File h264_file;
    if(!h264_file.Open(argv[1])) {
        printf("Open %s failed.\n", argv[1]);
        return 0;
    }
    
	std::shared_ptr<xop::EventLoop> event_loop(new xop::EventLoop());  
	std::shared_ptr<xop::RtspServer> server = xop::RtspServer::Create(event_loop.get());
	if (!server->Start(ip, 8554)) {
		return -1;
	}
	
#ifdef AUTH_CONFIG
	server->SetAuthConfig("-_-", "admin", "12345");
#endif
	 
	xop::MediaSession *session = xop::MediaSession::CreateNew("live"); // url: rtsp://ip/live
	session->AddSource(xop::channel_0, xop::H264Source::CreateNew()); 
	// session->AddSource(xop::channel_1, xop::AACSource::CreateNew(44100,2));
	// session->startMulticast(); /* 开启组播(ip,端口随机生成), 默认使用 RTP_OVER_UDP, RTP_OVER_RTSP */

    session->AddNotifyConnectedCallback([&clients] (xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port){
		printf("RTSP client connect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
        clients++;
	});
   
    session->AddNotifyDisconnectedCallback([&clients](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port) {
		printf("RTSP client disconnect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
        clients--;
	});

	std::cout << "URL: " << rtsp_url << std::endl;
        
	xop::MediaSessionId session_id = server->AddSession(session); 
	//server->removeMeidaSession(session_id); /* 取消会话, 接口线程安全 */
         
	std::thread thread(SendFrameThread, server.get(), session_id, &h264_file, std::ref(clients));
	thread.detach();

	while(1) {
		xop::Timer::Sleep(100);
	}

	getchar();
	return 0;
}

void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, H264File* h264_file, int& clients)
{
    int buf_size = 2000000;
    std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);

	while(1)
	{
		if(clients > 0) /* 会话有客户端在线, 发送音视频数据 */
		{
            {
                bool end_of_frame = false;
                int frame_size = h264_file->ReadFrame((char*)frame_buf.get(), buf_size, &end_of_frame);
                if(frame_size > 0) {
                    xop::AVFrame videoFrame;
                    videoFrame.type = 0;
                    videoFrame.timestamp = xop::H264Source::GetTimestamp();
                    //videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
                    //memcpy(videoFrame.buffer.get(), frame_buf.get(), videoFrame.size);
                    videoFrame.buffer.resize(frame_size);
                    memcpy(videoFrame.buffer.data(), frame_buf.get(), frame_size);
                    rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame);
                }
                else {
                    break;
                }

                xop::Timer::Sleep(40);
            }
			{     
				/*
				//获取一帧 H264, 打包
				xop::AVFrame videoFrame = {0};
				videoFrame.type = 0; // 建议确定帧类型。I帧(xop::VIDEO_FRAME_I) P帧(xop::VIDEO_FRAME_P)
				videoFrame.size = video frame size;  // 视频帧大小 
				videoFrame.timestamp = xop::H264Source::GetTimestamp(); // 时间戳, 建议使用编码器提供的时间戳
				videoFrame.buffer.reset(new uint8_t[videoFrame.size]);                    
				memcpy(videoFrame.buffer.get(), video frame data, videoFrame.size);					
                   
				rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame); //送到服务器进行转发, 接口线程安全
				*/
			}
                    
			{				
				/*
				//获取一帧 AAC, 打包
				xop::AVFrame audioFrame = {0};
				audioFrame.type = xop::AUDIO_FRAME;
				audioFrame.size = audio frame size;  /* 音频帧大小 
				audioFrame.timestamp = xop::AACSource::GetTimestamp(44100); // 时间戳
				audioFrame.buffer.reset(new uint8_t[audioFrame.size]);                    
				memcpy(audioFrame.buffer.get(), audio frame data, audioFrame.size);

				rtsp_server->PushFrame(session_id, xop::channel_1, audioFrame); // 送到服务器进行转发, 接口线程安全
				*/
			}		
		}

		xop::Timer::Sleep(1); /* 实际使用需要根据帧率计算延时! */
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
    m_file = fopen(path, "rb");
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

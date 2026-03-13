// RTSP Server

#include "xop/RtspServer.h"
#include "net/Timer.h"
#include <thread>
#include <memory>
#include <iostream>
#include <string>

void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, xop::MediaSession* session);

int main(int argc, char **argv)
{	
	std::string ip = "0.0.0.0";
	std::string rtsp_url = "rtsp://127.0.0.1:554/live";

	std::shared_ptr<xop::EventLoop> event_loop(new xop::EventLoop());  
	std::shared_ptr<xop::RtspServer> server = xop::RtspServer::Create(event_loop.get());
	if (!server->Start(ip, 554)) {
		return -1;
	}
	
#ifdef AUTH_CONFIG
	server->SetAuthConfig("-_-", "admin", "12345");
#endif
	 
	xop::MediaSession *session = xop::MediaSession::CreateNew("live"); // url: rtsp://ip/live
	session->AddSource(xop::channel_0, xop::H264Source::CreateNew()); 
	session->AddSource(xop::channel_1, xop::AACSource::CreateNew(44100,2));
	// session->StartMulticast(); /* Enable multicast (IP and port auto-generated), default RTP_OVER_UDP */

	session->AddNotifyConnectedCallback([] (xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port){
		printf("RTSP client connect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
	});
   
	session->AddNotifyDisconnectedCallback([](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port) {
		printf("RTSP client disconnect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
	});

	std::cout << "URL: " << rtsp_url << std::endl;
	std::cout << "Max concurrent clients: " << xop::MAX_CLIENTS << std::endl;
        
	xop::MediaSessionId session_id = server->AddSession(session); 
	//server->RemoveSession(session_id); /* Remove session (thread-safe API) */
         
	std::thread thread(SendFrameThread, server.get(), session_id, session);
	thread.detach();

	while(1) {
		xop::Timer::Sleep(100);
	}

	getchar();
	return 0;
}

void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, xop::MediaSession* session)
{       
	while(1)
	{
		/* Only send audio/video frames if there are connected clients */
		if(session->GetNumClient() > 0)
		{
			{     
				/*
				// Acquire H.264 frame and package it
				xop::AVFrame videoFrame = {0};
				videoFrame.type = xop::VIDEO_FRAME_I; // Recommend setting frame type explicitly: I-frame (xop::VIDEO_FRAME_I) or P-frame (xop::VIDEO_FRAME_P)
				videoFrame.buffer.assign(video_frame_data, video_frame_data + frame_size);
				videoFrame.timestamp = xop::H264Source::GetTimestamp(); // Timestamp, recommend using encoder's timestamp
                   
				rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame); // Send to server for forwarding (thread-safe API)
				*/
			}
                    
			{				
				/*
				// Acquire AAC frame and package it
				xop::AVFrame audioFrame = {0};
				audioFrame.type = xop::AUDIO_FRAME;
				audioFrame.buffer.assign(audio_frame_data, audio_frame_data + frame_size);
				audioFrame.timestamp = xop::AACSource::GetTimestamp(44100); // Timestamp

				rtsp_server->PushFrame(session_id, xop::channel_1, audioFrame); // Send to server for forwarding (thread-safe API)
				*/
			}
		}

		xop::Timer::Sleep(1); /* In production, calculate delay based on frame rate! */
	}
}

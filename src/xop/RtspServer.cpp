#include "RtspServer.h"
#include "RtspConnection.h"
#include "net/SocketUtil.h"
#include "net/Logger.h"

using namespace xop;
using namespace std;

#define RTSP_DEBUG 0

RtspServer::RtspServer(EventLoop* loop)
	: TcpServer(loop)
{
#if RTSP_DEBUG
	#if defined(ANDROID)
	    __android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "RtspServer Constructor");
	#else
	    std::cout << "RtspServer Constructor" <<  std::endl;
	#endif
#endif
}

RtspServer::~RtspServer()
{
#if RTSP_DEBUG
	#if defined(ANDROID)
	    __android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "RtspServer Destructor");
	#else
	    std::cout << "RtspServer Destructor" <<  std::endl;
	#endif
#endif
}

std::shared_ptr<RtspServer> RtspServer::Create(xop::EventLoop* loop)
{
#if RTSP_DEBUG
	#if defined(ANDROID)
	    __android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "RtspServer Create");
	#else
	    std::cout << "RtspServer Create" <<  std::endl;
	#endif
#endif
	std::shared_ptr<RtspServer> server(new RtspServer(loop));
	return server;
}

MediaSessionId RtspServer::AddSession(MediaSession* session)
{
#if RTSP_DEBUG
	#if defined(ANDROID)
	    __android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "RtspServer AddSession");
	#else
	    std::cout << "RtspServer AddSession" <<  std::endl;
	#endif
#endif
    std::lock_guard<std::mutex> locker(mutex_);

    if (rtsp_suffix_map_.find(session->GetRtspUrlSuffix()) != rtsp_suffix_map_.end()) {
        return 0;
    }

    std::shared_ptr<MediaSession> media_session(session);
    MediaSessionId sessionId = media_session->GetMediaSessionId();
	rtsp_suffix_map_.emplace(std::move(media_session->GetRtspUrlSuffix()), sessionId);
	media_sessions_.emplace(sessionId, std::move(media_session));

    return sessionId;
}

void RtspServer::RemoveSession(MediaSessionId sessionId)
{
#if RTSP_DEBUG
	#if defined(ANDROID)
    __android_log_print(ANDROID_LOG_VERBOSE,  MODULE_NAME, "RtspServer: RemoveSession");
	#else
		std::cout << "RtspServer: RemoveSession" <<  std::endl;
	#endif
#endif
    std::lock_guard<std::mutex> locker(mutex_);

    auto iter = media_sessions_.find(sessionId);
    if(iter != media_sessions_.end()) {
        rtsp_suffix_map_.erase(iter->second->GetRtspUrlSuffix());
        media_sessions_.erase(sessionId);
    }
}

MediaSession::Ptr RtspServer::LookMediaSession(const std::string& suffix)
{
#if RTSP_DEBUG
	#if defined(ANDROID)
    __android_log_print(ANDROID_LOG_VERBOSE,  MODULE_NAME, "RtspServer: LookMediaSession by suffix...");
	#else
		std::cout << "RtspServer: LookMediaSession by suffix..." <<  std::endl;
	#endif
#endif
    std::lock_guard<std::mutex> locker(mutex_);

    auto iter = rtsp_suffix_map_.find(suffix);
    if(iter != rtsp_suffix_map_.end()) {
        MediaSessionId id = iter->second;
        return media_sessions_[id];
    }

    return nullptr;
}

MediaSession::Ptr RtspServer::LookMediaSession(MediaSessionId session_Id)
{
#if RTSP_DEBUG
	#if defined(ANDROID)
    __android_log_print(ANDROID_LOG_VERBOSE,  MODULE_NAME, "RtspServer: LookMediaSession...");
	#else
		std::cout << "RtspServer: LookMediaSession..." <<  std::endl;
	#endif
#endif
    std::lock_guard<std::mutex> locker(mutex_);

    auto iter = media_sessions_.find(session_Id);
    if(iter != media_sessions_.end()) {
        return iter->second;
    }

    return nullptr;
}

bool RtspServer::PushFrame(MediaSessionId session_id, MediaChannelId channel_id, AVFrame frame)
{
#if RTSP_DEBUG
	#if defined(ANDROID)
	    __android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "RtspServer PushFrame");
	#else
	    std::cout << "RtspServer PushFrame" <<  std::endl;
	#endif
#endif
    std::shared_ptr<MediaSession> sessionPtr = nullptr;
    {
        std::lock_guard<std::mutex> locker(mutex_);
        auto iter = media_sessions_.find(session_id);
        if (iter != media_sessions_.end()) {
            sessionPtr = iter->second;
        }
        else {
            return false;
        }
    }

    if (sessionPtr!=nullptr && sessionPtr->GetNumClient()!=0) {
        return sessionPtr->HandleFrame(channel_id, frame);
    }

    return false;
}

TcpConnection::Ptr RtspServer::OnConnect(SOCKET sockfd)
{
#if RTSP_DEBUG
	#if defined(ANDROID)
    __android_log_print(ANDROID_LOG_VERBOSE,  MODULE_NAME, "RtspServer: received connect request...");
	#else
		std::cout << "RtspServer: received connect request..." <<  std::endl;
	#endif
#endif
	return std::make_shared<RtspConnection>(shared_from_this(), event_loop_->GetTaskScheduler().get(), sockfd);
}

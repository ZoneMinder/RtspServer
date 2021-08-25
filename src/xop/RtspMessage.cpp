// PHZ
// 2018-5-16

#if defined(WIN32) || defined(_WIN32)
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "RtspMessage.h"
#include "DigestAuthenticator.h"
#include "media.h"

#include <iostream>

using namespace std;
using namespace xop;

bool RtspRequest::ParseRequest(BufferReader *buffer)
{
	if(buffer->Peek()[0] == '$') {
		method_ = RTCP;
		return true;
	}

	bool ret = true;
	while(1) {
		if(state_ == kParseRequestLine) {
			const char* firstCrlf = buffer->FindFirstCrlf();
			if(firstCrlf != nullptr)
			{
				ret = ParseRequestLine(buffer->Peek(), firstCrlf);
				buffer->RetrieveUntil(firstCrlf + 2);
			}

			if (state_ == kParseHeadersLine) {
				continue;
			}
			else {
				break;
			}
		}
		else if(state_ == kParseHeadersLine) {
			const char* lastCrlf = buffer->FindLastCrlf();
			if(lastCrlf != nullptr) {
				ret = ParseHeadersLine(buffer->Peek(), lastCrlf);
				buffer->RetrieveUntil(lastCrlf + 2);
			}
			break;
		}
		else if(state_ == kGotAll) {
			buffer->RetrieveAll();
			return true;
		}
	}

	return ret;
}

bool RtspRequest::ParseRequestLine(const char* begin, const char* end)
{
	string message(begin, end);
	char method[64] = {0};
	char url[512] = {0};
	char version[64] = {0};

	if(sscanf(message.c_str(), "%s %s %s", method, url, version) != 3) {
		return true;
	}

	string method_str(method);
	if(method_str == "OPTIONS") {
		method_ = OPTIONS;
	}
	else if(method_str == "DESCRIBE") {
		method_ = DESCRIBE;
	}
	else if(method_str == "SETUP") {
		method_ = SETUP;
	}
	else if(method_str == "PLAY") {
		method_ = PLAY;
	}
	else if(method_str == "TEARDOWN") {
		method_ = TEARDOWN;
	}
	else if(method_str == "GET_PARAMETER") {
		method_ = GET_PARAMETER;
	}
	else {
		method_ = NONE;
		return false;
	}

	if(strncmp(url, "rtsp://", 7) != 0) {
		return false;
	}

	// parse url
	uint16_t port = 0;
	char ip[64] = {0};
	char suffix[256] = {0};

	if(sscanf(url+7, "%[^:]:%hu/%s", ip, &port, suffix) == 3) {

	}
	else if(sscanf(url+7, "%[^/]/%s", ip, suffix) == 2) {
		port = 554;
	}
	else {
		return false;
	}

	request_line_param_.emplace("url", make_pair(string(url), 0));
	request_line_param_.emplace("url_ip", make_pair(string(ip), 0));
	request_line_param_.emplace("url_port", make_pair("", (uint32_t)port));
	request_line_param_.emplace("url_suffix", make_pair(string(suffix), 0));
	request_line_param_.emplace("version", make_pair(string(version), 0));
	request_line_param_.emplace("method", make_pair(move(method_str), 0));

	state_ = kParseHeadersLine;
	return true;
}

bool RtspRequest::ParseHeadersLine(const char* begin, const char* end)
{
	string message(begin, end);
	if(!ParseCSeq(message)) {
		if (header_line_param_.find("cseq") == header_line_param_.end()) {
			return false;
		}
	}

	if (method_ == DESCRIBE || method_ == SETUP || method_ == PLAY) {
		ParseAuthorization(message);
	}

	if(method_ == OPTIONS) {
		state_ = kGotAll;
		return true;
	}

	if(method_ == DESCRIBE) {
		if(ParseAccept(message)) {
			state_ = kGotAll;
		}
		return true;
	}

	if(method_ == SETUP) {
		if(ParseTransport(message)) {
			ParseMediaChannel(message);
			state_ = kGotAll;
		}

		return true;
	}

	if(method_ == PLAY) {
		if(ParseSessionId(message)) {
			state_ = kGotAll;
		}
		return true;
	}

	if(method_ == TEARDOWN) {
		state_ = kGotAll;
		return true;
	}

	if(method_ == GET_PARAMETER) {
		state_ = kGotAll;
		return true;
	}

	return true;
}

bool RtspRequest::ParseCSeq(std::string& message)
{
	std::size_t pos = message.find("CSeq");
	if (pos != std::string::npos) {
		uint32_t cseq = 0;
		sscanf(message.c_str()+pos, "%*[^:]: %u", &cseq);
		header_line_param_.emplace("cseq", make_pair("", cseq));
		return true;
	}

	return false;
}

bool RtspRequest::ParseAccept(std::string& message)
{
	if ((message.rfind("Accept")==std::string::npos)
		|| (message.rfind("sdp")==std::string::npos)) {
		return false;
	}

	return true;
}

bool RtspRequest::ParseTransport(std::string& message)
{
	std::size_t pos = message.find("Transport");
	if(pos != std::string::npos) {
		if((pos=message.find("RTP/AVP/TCP")) != std::string::npos) {
			transport_ = RTP_OVER_TCP;
			uint16_t rtpChannel = 0, rtcpChannel = 0;
			if (sscanf(message.c_str() + pos, "%*[^;];%*[^;];%*[^=]=%hu-%hu", &rtpChannel, &rtcpChannel) != 2) {
				return false;
			}
			header_line_param_.emplace("rtp_channel", make_pair("", rtpChannel));
			header_line_param_.emplace("rtcp_channel", make_pair("", rtcpChannel));
		}
		else if((pos=message.find("RTP/AVP")) != std::string::npos) {
			uint16_t rtp_port = 0, rtcpPort = 0;
			if(((message.find("unicast", pos)) != std::string::npos)) {
				transport_ = RTP_OVER_UDP;
				if(sscanf(message.c_str()+pos, "%*[^;];%*[^;];%*[^=]=%hu-%hu",
						&rtp_port, &rtcpPort) != 2)
				{
					return false;
				}

			}
			else if((message.find("multicast", pos)) != std::string::npos) {
				transport_ = RTP_OVER_MULTICAST;
			}
			else {
				return false;
			}

			header_line_param_.emplace("rtp_port", make_pair("", rtp_port));
			header_line_param_.emplace("rtcp_port", make_pair("", rtcpPort));
		}
		else {
			return false;
		}

		return true;
	}

	return false;
}

bool RtspRequest::ParseSessionId(std::string& message)
{
	std::size_t pos = message.find("Session");
	if (pos != std::string::npos) {
		uint32_t session_id = 0;
		if (sscanf(message.c_str() + pos, "%*[^:]: %u", &session_id) != 1) {
			return false;
		}
		return true;
	}

	return false;
}

bool RtspRequest::ParseMediaChannel(std::string& message)
{
	channel_id_ = channel_0;

	auto iter = request_line_param_.find("url");
	if(iter != request_line_param_.end()) {
		std::size_t pos = iter->second.first.find("track1");
		if (pos != std::string::npos) {
			channel_id_ = channel_1;
		}
	}

	return true;
}

bool RtspRequest::ParseAuthorization(std::string& message)
{
	std::size_t pos = message.find("Authorization");
	if (pos != std::string::npos) {
		if ((pos = message.find("response=")) != std::string::npos) {
			auth_response_ = message.substr(pos + 10, 32);
			if (auth_response_.size() == 32) {
				return true;
			}
		}
	}

	auth_response_.clear();
	return false;
}

uint32_t RtspRequest::GetCSeq() const
{
	uint32_t cseq = 0;
	auto iter = header_line_param_.find("cseq");
	if(iter != header_line_param_.end()) {
		cseq = iter->second.second;
	}

	return cseq;
}

std::string RtspRequest::GetIp() const
{
	auto iter = request_line_param_.find("url_ip");
	if(iter != request_line_param_.end()) {
		return iter->second.first;
	}

	return "";
}

std::string RtspRequest::GetRtspUrl() const
{
	auto iter = request_line_param_.find("url");
	if(iter != request_line_param_.end()) {
		return iter->second.first;
	}

	return "";
}

std::string RtspRequest::GetRtspUrlSuffix() const
{
	auto iter = request_line_param_.find("url_suffix");
	if(iter != request_line_param_.end()) {
		return iter->second.first;
	}

	return "";
}

std::string RtspRequest::GetRtspUrlSession() const
{
	auto iter = request_line_param_.find("url_suffix");
	if(iter != request_line_param_.end()) {
    int found = iter->second.first.find('?');
    if (found != std::string::npos)
      return iter->second.first.substr(0, found);
		return iter->second.first;
	}

	return "";
}

std::string RtspRequest::GetRtspUrlQueryString() const
{
	auto iter = request_line_param_.find("url_suffix");
	if(iter != request_line_param_.end()) {
    int found = iter->second.first.find('?');
    if (found != std::string::npos)
      return iter->second.first.substr(found+1, std::string::npos);
	}

	return "";
}

std::string RtspRequest::GetAuthResponse() const
{
	return auth_response_;
}

uint8_t RtspRequest::GetRtpChannel() const
{
	auto iter = header_line_param_.find("rtp_channel");
	if(iter != header_line_param_.end()) {
		return iter->second.second;
	}

	return 0;
}

uint8_t RtspRequest::GetRtcpChannel() const
{
	auto iter = header_line_param_.find("rtcp_channel");
	if(iter != header_line_param_.end()) {
		return iter->second.second;
	}

	return 0;
}

uint16_t RtspRequest::GetRtpPort() const
{
	auto iter = header_line_param_.find("rtp_port");
	if(iter != header_line_param_.end()) {
		return iter->second.second;
	}

	return 0;
}

uint16_t RtspRequest::GetRtcpPort() const
{
	auto iter = header_line_param_.find("rtcp_port");
	if(iter != header_line_param_.end()) {
		return iter->second.second;
	}

	return 0;
}

int RtspRequest::BuildOptionRes(const char* buf, int buf_size)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %u\r\n"
			"Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY\r\n"
			"\r\n",
			this->GetCSeq());

	return (int)strlen(buf);
}

/*
int RtspRequest::BuildTeardownMSG(const char* buf, int buf_size, uint32_t session_id)
{
    memset((void*)buf, 0, buf_size);
    snprintf((char*)buf, buf_size,
            "TEARDOWN rtsp://127.0.0.1:8554/live RTSP/1.0\r\n"
            "CSeq: %u\r\n"
            "Session: %llu\r\n"
            "\r\n",
            this->GetCSeq(),
            session_id);

    method_ = TEARDOWN;

    return (int)strlen(buf);
}
*/

int RtspRequest::BuildDescribeRes(const char* buf, int buf_size, const char* sdp)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %u\r\n"
			"Content-Length: %d\r\n"
			"Content-Type: application/sdp\r\n"
			"\r\n"
			"%s",
			this->GetCSeq(),
			(int)strlen(sdp),
			sdp);

	return (int)strlen(buf);
}

int RtspRequest::BuildSetupMulticastRes(const char* buf, int buf_size, const char* multicast_ip, uint16_t port, uint64_t session_id)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %u\r\n"
			"Transport: RTP/AVP;multicast;destination=%s;source=%s;port=%u-0;ttl=255\r\n"
			"Session: %llu\r\n"
			"\r\n",
			this->GetCSeq(),
			multicast_ip,
			this->GetIp().c_str(),
			port,
			session_id);

	return (int)strlen(buf);
}

int RtspRequest::BuildSetupUdpRes(const char* buf, int buf_size, uint16_t rtp_chn, uint16_t rtcp_chn, uint64_t session_id)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %u\r\n"
			"Transport: RTP/AVP;unicast;client_port=%hu-%hu;server_port=%hu-%hu\r\n"
			"Session: %llu\r\n"
			"\r\n",
			this->GetCSeq(),
			this->GetRtpPort(),
			this->GetRtcpPort(),
			rtp_chn,
			rtcp_chn,
			session_id);

	return (int)strlen(buf);
}

int RtspRequest::BuildSetupTcpRes(const char* buf, int buf_size, uint16_t rtp_chn, uint16_t rtcp_chn, uint64_t session_id)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %u\r\n"
			"Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n"
			"Session: %llu\r\n"
			"\r\n",
			this->GetCSeq(),
			rtp_chn, rtcp_chn,
			session_id);

	return (int)strlen(buf);
}

int RtspRequest::BuildPlayRes(const char* buf, int buf_size, const char* rtpInfo, uint64_t session_id)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Range: npt=0.000-\r\n"
			"Session: %llu; timeout=60\r\n",
			this->GetCSeq(),
			session_id);

	if (rtpInfo != nullptr) {
		snprintf((char*)buf + strlen(buf), buf_size - strlen(buf), "%s\r\n", rtpInfo);
	}

	snprintf((char*)buf + strlen(buf), buf_size - strlen(buf), "\r\n");
	return (int)strlen(buf);
}

int RtspRequest::BuildTeardownRes(const char* buf, int buf_size, uint64_t session_id)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Session: %llu\r\n"
			"\r\n",
			this->GetCSeq(),
			session_id);

	return (int)strlen(buf);
}

int RtspRequest::BuildGetParamterRes(const char* buf, int buf_size, uint64_t session_id)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Session: %llu\r\n"
			"\r\n",
			this->GetCSeq(),
			session_id);

	return (int)strlen(buf);
}

int RtspRequest::BuildNotFoundRes(const char* buf, int buf_size)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 404 Stream Not Found\r\n"
			"CSeq: %u\r\n"
			"\r\n",
			this->GetCSeq());

	return (int)strlen(buf);
}

int RtspRequest::BuildServerErrorRes(const char* buf, int buf_size)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 500 Internal Server Error\r\n"
			"CSeq: %u\r\n"
			"\r\n",
			this->GetCSeq());

	return (int)strlen(buf);
}

int RtspRequest::BuildUnsupportedRes(const char* buf, int buf_size)
{
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 461 Unsupported transport\r\n"
			"CSeq: %d\r\n"
			"\r\n",
			this->GetCSeq());

	return (int)strlen(buf);
}

size_t RtspRequest::BuildUnauthorizedRes(const char *buf, size_t buf_size)
{
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 401 Unauthorized\r\n"
			"CSeq: %d\r\n"
			"\r\n",
			this->GetCSeq()
			);

	return strlen(buf);
}

size_t RtspRequest::BuildUnauthorizedRes(const char* buf, size_t buf_size, const char* realm, const char* nonce)
{
#ifdef DEBUG
	#if defined(ANDROID)
			__android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "RtspRequest BuildUnauthorizedRes");
	#else
			std::cout << "RtspRequest BuildUnauthorizedRes" <<  std::endl;
	#endif
#endif
	memset((void*)buf, 0, buf_size);
	snprintf((char*)buf, buf_size,
			"RTSP/1.0 401 Unauthorized\r\n"
			"CSeq: %d\r\n"
			"WWW-Authenticate: Digest realm=\"%s\", nonce=\"%s\"\r\n"
			"\r\n",
			this->GetCSeq(),
			realm,
			nonce);

	return strlen(buf);
}

uint32_t gen_random(const int len) {

    string tmp_s;
    static const char num[] =
        "0123456789";

    srand( (unsigned) time(NULL) * getpid());

    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i)
        tmp_s += num[rand() % (sizeof(num) - 1)];


    return std::stoi(tmp_s);

}

string gen_randomString(const int len) {

    string tmp_s;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    srand( (unsigned) time(NULL) * getpid());

    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i)
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];


    return tmp_s;

}

RtspResponse::RtspResponse()
{
    session_ = gen_random(8);
}

bool RtspResponse::ParseResponse(xop::BufferReader *buffer)
{
	if (strstr(buffer->Peek(), "\r\n\r\n") != NULL) {
		if (strstr(buffer->Peek(), "OK") == NULL) {
			return false;
		}

		char* ptr = strstr(buffer->Peek(), "Session");
		if (ptr != NULL) {
			char session_id[50] = {0};
			if (sscanf(ptr, "%*[^:]: %s", session_id) == 1)
				try
				{
					session_ = std::stoull(session_id);
				}
				catch (const std::out_of_range& oor)
				{
					return false;
				}
		}

		cseq_++;
		buffer->RetrieveUntil("\r\n\r\n");
	}

	return true;
}

int RtspResponse::BuildOptionReq(const char* buf, int buf_size, std::string& nonce, Authenticator* auth)
{
    if(auth != nullptr && nonce.size() > 0)
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                "OPTIONS %s RTSP/1.0\r\n"
                "CSeq: %u\r\n"
                "User-Agent: %s\r\n"
                "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"
                "\r\n",
                rtsp_url_.c_str(),
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                auth->GetUsername().c_str(), auth->GetRealm().c_str(), nonce.c_str(), rtsp_url_.c_str(),
                auth->GetResponse(nonce, "OPTIONS", rtsp_url_.c_str()).c_str());
    }
    else
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                "OPTIONS %s RTSP/1.0\r\n"
                "CSeq: %u\r\n"
                "User-Agent: %s\r\n"
                "\r\n",
                rtsp_url_.c_str(),
                this->GetCSeq() + 1,
                user_agent_.c_str());
    }
	method_ = OPTIONS;
	return (int)strlen(buf);
}

int RtspResponse::BuildTeardownReq(const char* buf, int buf_size, std::string& nonce, Authenticator* auth)
{
    if(auth != nullptr && nonce.size() > 0)
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                "TEARDOWN %s RTSP/1.0\r\n"
                "CSeq: %u\r\n"
                "User-Agent: %s\r\n"
                "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"
                "Session: %llu\r\n"
                "\r\n",
                rtsp_url_.c_str(),
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                auth->GetUsername().c_str(), auth->GetRealm().c_str(), nonce.c_str(), rtsp_url_.c_str(),
                auth->GetResponse(nonce, "TEARDOWN", rtsp_url_.c_str()).c_str(),
                this->GetSession());
    }
    else
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                "TEARDOWN %s RTSP/1.0\r\n"
                "CSeq: %u\r\n"
                "User-Agent: %s\r\n"
                "Session: %llu\r\n"
                "\r\n",
                rtsp_url_.c_str(),
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                this->GetSession());
    }
    method_ = TEARDOWN;
    return (int)strlen(buf);
}

int RtspResponse::BuildAnnounceReq(const char* buf, int buf_size, const char *sdp, std::string& nonce, Authenticator* auth)
{
    if(auth != nullptr && nonce.size() > 0)
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                "ANNOUNCE %s RTSP/1.0\r\n"
                "Content-Type: application/sdp\r\n"
                "CSeq: %u\r\n"
                "User-Agent: %s\r\n"
                "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"
                "Session: %llu\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                rtsp_url_.c_str(),
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                auth->GetUsername().c_str(), auth->GetRealm().c_str(), nonce.c_str(), rtsp_url_.c_str(),
                auth->GetResponse(nonce, "ANNOUNCE", rtsp_url_.c_str()).c_str(),
                this->GetSession(),
                (int)strlen(sdp),
                sdp);
    }
    else
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                "ANNOUNCE %s RTSP/1.0\r\n"
                "Content-Type: application/sdp\r\n"
                "CSeq: %u\r\n"
                "User-Agent: %s\r\n"
                "Session: %llu\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
                rtsp_url_.c_str(),
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                this->GetSession(),
                (int)strlen(sdp),
                sdp);
    }
	method_ = ANNOUNCE;
	return (int)strlen(buf);
}

int RtspResponse::BuildDescribeReq(const char* buf, int buf_size, std::string& nonce, Authenticator* auth)
{
    if(auth != nullptr && nonce.size() > 0)
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                "DESCRIBE %s RTSP/1.0\r\n"
                "CSeq: %u\r\n"
                "Accept: application/sdp\r\n"
                "User-Agent: %s\r\n"
                "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"
                "Session: %llu\r\n"
                "\r\n",
                rtsp_url_.c_str(),
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                auth->GetUsername().c_str(), auth->GetRealm().c_str(), nonce.c_str(), rtsp_url_.c_str(),
                auth->GetResponse(nonce, "DESCRIBE", rtsp_url_.c_str()).c_str(),
                this->GetSession());
    }
    else
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                "DESCRIBE %s RTSP/1.0\r\n"
                "CSeq: %u\r\n"
                "Accept: application/sdp\r\n"
                "User-Agent: %s\r\n"
                "Session: %llu\r\n"
                "\r\n",
                rtsp_url_.c_str(),
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                this->GetSession());
    }
	method_ = DESCRIBE;
	return (int)strlen(buf);
}

int RtspResponse::BuildSetupTcpReq(const char* buf, int buf_size, int trackId, std::string& nonce, Authenticator* auth)
{
	int interleaved[2] = { 0, 1 };
	if (trackId == 1) {
		interleaved[0] = 2;
		interleaved[1] = 3;
	}
    if(auth != nullptr && nonce.size() > 0)
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                // "SETUP %s/track%d RTSP/1.0\r\n"
                "SETUP %s/streamid=%d RTSP/1.0\r\n"
                "Transport: RTP/AVP/TCP;unicast;mode=record;interleaved=%d-%d\r\n"
                "CSeq: %u\r\n"
                "User-Agent: %s\r\n"
                "Session: %llu\r\n"
                "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s/streamid=%d\", response=\"%s\"\r\n"
                "\r\n",
                rtsp_url_.c_str(),
                trackId,
                interleaved[0],
                interleaved[1],
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                this->GetSession(),
                auth->GetUsername().c_str(), auth->GetRealm().c_str(), nonce.c_str(), rtsp_url_.c_str(), trackId,
                auth->GetResponse(nonce, "SETUP", (rtsp_url_ + "/streamid=" + std::to_string(trackId)).c_str()).c_str());
    }
    else
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                // "SETUP %s/track%d RTSP/1.0\r\n"
                "SETUP %s/streamid=%d RTSP/1.0\r\n"
                "Transport: RTP/AVP/TCP;unicast;mode=record;interleaved=%d-%d\r\n"
                "CSeq: %u\r\n"
                "User-Agent: %s\r\n"
                "Session: %llu\r\n"
                "\r\n",
                rtsp_url_.c_str(),
                trackId,
                interleaved[0],
                interleaved[1],
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                this->GetSession());
    }
	method_ = SETUP;
	return (int)strlen(buf);
}

int RtspResponse::BuildRecordReq(const char* buf, int buf_size, std::string& nonce, Authenticator* auth)
{
    if(auth != nullptr && nonce.size() > 0)
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                "RECORD %s RTSP/1.0\r\n"
                "Range: npt=0.000-\r\n"
                "CSeq: %u\r\n"
                "User-Agent: %s\r\n"
                "Session: %llu\r\n"
                "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"
                "\r\n",
                rtsp_url_.c_str(),
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                this->GetSession(),
                auth->GetUsername().c_str(), auth->GetRealm().c_str(), nonce.c_str(), rtsp_url_.c_str(),
                auth->GetResponse(nonce, "RECORD", rtsp_url_.c_str()).c_str());
    }
    else
    {
        memset((void*)buf, 0, buf_size);
        snprintf((char*)buf, buf_size,
                "RECORD %s RTSP/1.0\r\n"
                "Range: npt=0.000-\r\n"
                "CSeq: %u\r\n"
                "User-Agent: %s\r\n"
                "Session: %llu\r\n"
                "\r\n",
                rtsp_url_.c_str(),
                this->GetCSeq() + 1,
                user_agent_.c_str(),
                this->GetSession());
    }
	method_ = RECORD;
	return (int)strlen(buf);
}

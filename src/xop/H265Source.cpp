// PHZ
// 2018-6-7

#if defined(WIN32) || defined(_WIN32) 
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "H265Source.h"
#include <cstdio>
#include <chrono>
#if defined(__linux) || defined(__linux__) 
#include <sys/time.h>
#endif

using namespace xop;
using namespace std;

H265Source::H265Source(uint32_t framerate)
	: framerate_(framerate)
{
	payload_    = 96;
	media_type_ = H265;
	clock_rate_ = 90000;
}

H265Source* H265Source::CreateNew(uint32_t framerate)
{
	return new H265Source(framerate);
}

H265Source::~H265Source()
{

}

std::string H265Source::Base64Encode(const uint8_t* data, size_t size)
{
	static const char base64_chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string result;
	result.reserve(((size + 2) / 3) * 4);

	for (size_t i = 0; i < size; i += 3) {
		uint32_t n = (data[i] << 16);
		if (i + 1 < size) n |= (data[i + 1] << 8);
		if (i + 2 < size) n |= data[i + 2];

		result += base64_chars[(n >> 18) & 0x3F];
		result += base64_chars[(n >> 12) & 0x3F];
		result += (i + 1 < size) ? base64_chars[(n >> 6) & 0x3F] : '=';
		result += (i + 2 < size) ? base64_chars[n & 0x3F] : '=';
	}

	return result;
}

string H265Source::GetMediaDescription(uint16_t port)
{
	char buf[100] = {0};
	sprintf(buf, "m=video %hu RTP/AVP 96", port);
	return string(buf);
}
	
string H265Source::GetAttribute()
{
	string attr = "a=rtpmap:96 H265/90000";

	// Add fmtp line with sprop-vps, sprop-sps, sprop-pps if available
	if (!vps_.empty() && !sps_.empty() && !pps_.empty()) {
		std::string vps_b64 = Base64Encode(vps_.data(), vps_.size());
		std::string sps_b64 = Base64Encode(sps_.data(), sps_.size());
		std::string pps_b64 = Base64Encode(pps_.data(), pps_.size());

		char buf[1024];
		sprintf(buf, "\r\na=fmtp:96 sprop-vps=%s;sprop-sps=%s;sprop-pps=%s",
				vps_b64.c_str(), sps_b64.c_str(), pps_b64.c_str());
		attr += buf;
	}

	if (width_ > 0 && height_ > 0) {
		char buf[64];
		sprintf(buf, "\r\na=x-dimensions:%u,%u", width_, height_);
		attr += buf;
	}
	return attr;
}

bool H265Source::HandleFrame(MediaChannelId channelId, AVFrame frame)
{
	uint8_t *frame_buf  = frame.buffer.data();
	uint32_t frame_size = frame.buffer.size();

	if (frame.timestamp == 0) {
		frame.timestamp = GetTimestamp();
	}
        
	if (frame_size <= MAX_RTP_PAYLOAD_SIZE) {
		RtpPacket rtp_pkt;
		rtp_pkt.type = frame.type;
		rtp_pkt.timestamp = frame.timestamp;
		rtp_pkt.size = frame_size + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
		rtp_pkt.last = frame.last;  // Use caller-provided value for RTP marker bit

		memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE, frame_buf, frame_size);
        
		if (send_frame_callback_) {
			if (!send_frame_callback_(channelId, rtp_pkt)) {
				return false;
			}          
		}
	}	
	else {	
		char FU[3] = {0};	
		char nalUnitType = (frame_buf[0] & 0x7E) >> 1; 
		FU[0] = (frame_buf[0] & 0x81) | (49<<1); 
		FU[1] = frame_buf[1]; 
		FU[2] = (0x80 | nalUnitType); 
        
		frame_buf  += 2;
		frame_size -= 2;
        
		while (frame_size + 3 > MAX_RTP_PAYLOAD_SIZE) {
			RtpPacket rtp_pkt;
			rtp_pkt.type = frame.type;
			rtp_pkt.timestamp = frame.timestamp;
			rtp_pkt.size = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;
			rtp_pkt.last = 0;

			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = FU[0];
			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = FU[1];
			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2] = FU[2];
			memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+3, frame_buf, MAX_RTP_PAYLOAD_SIZE-3);
            
			if (send_frame_callback_) {
				if (!send_frame_callback_(channelId, rtp_pkt)) {
					return false;
				}                
			}
            
			frame_buf  += (MAX_RTP_PAYLOAD_SIZE - 3);
			frame_size -= (MAX_RTP_PAYLOAD_SIZE - 3);
        
			FU[2] &= ~0x80;						
		}
        
		{
			RtpPacket rtp_pkt;
			rtp_pkt.type = frame.type;
			rtp_pkt.timestamp = frame.timestamp;
			rtp_pkt.size = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 3 + frame_size;
			rtp_pkt.last = 1;

			FU[2] |= 0x40;
			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = FU[0];
			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = FU[1];
			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2] = FU[2];
			memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+3, frame_buf, frame_size);
            
			if (send_frame_callback_) {
				if (!send_frame_callback_(channelId, rtp_pkt)) {
					return false;
				}               
			}
		}            
	}

	return true;
}

int64_t H265Source::GetTimestamp()
{
/* #if defined(__linux) || defined(__linux__) 
	struct timeval tv = {0};
	gettimeofday(&tv, NULL);
	uint32_t ts = ((tv.tv_sec*1000)+((tv.tv_usec+500)/1000))*90; // 90: _clockRate/1000;
	return ts;
#else */
	//auto time_point = chrono::time_point_cast<chrono::milliseconds>(chrono::system_clock::now());
	//auto time_point = chrono::time_point_cast<chrono::milliseconds>(chrono::steady_clock::now());
	auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
	return (int64_t)((time_point.time_since_epoch().count() + 500) / 1000 * 90);
//#endif 
}

#if defined(WIN32) || defined(_WIN32) 
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif
#include "G711USource.h"
#include <cstdio>
#include <chrono>
#if defined(__linux) || defined(__linux__) 
#include <sys/time.h>
#endif

using namespace xop;
using namespace std;

G711USource::G711USource()
{
	payload_    = 0;
	media_type_ = PCMU;
	clock_rate_ = 8000;
}

G711USource* G711USource::CreateNew()
{
    return new G711USource();
}

G711USource::~G711USource()
{
	
}

string G711USource::GetMediaDescription(uint16_t port)
{
	char buf[100] = {0};
	sprintf(buf, "m=audio %hu RTP/AVP 0", port);
	return string(buf);
}
	
string G711USource::GetAttribute()
{
    return string("a=rtpmap:0 PCMU/8000/1");
}

bool G711USource::HandleFrame(MediaChannelId channel_id, AVFrame frame)
{
	if (frame.buffer.size() > MAX_RTP_PAYLOAD_SIZE) {
		return false;
	}

	uint8_t *frame_buf  = frame.buffer.data();
	uint32_t frame_size = frame.buffer.size();

	RtpPacket rtp_pkt;
	rtp_pkt.type = frame.type;
	rtp_pkt.timestamp = frame.timestamp;
	rtp_pkt.size = frame_size + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
	rtp_pkt.last = 1;

	memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE, frame_buf, frame_size);

	if (send_frame_callback_) {
		send_frame_callback_(channel_id, rtp_pkt);
	}

	return true;
}

int64_t G711USource::GetTimestamp()
{
	auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
	return (int64_t)((time_point.time_since_epoch().count()+500)/1000*8);
}

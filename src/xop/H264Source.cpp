// PHZ
// 2018-5-16

#if defined(WIN32) || defined(_WIN32)
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "H264Source.h"
#include <cstdio>
#include <chrono>
#if defined(__linux) || defined(__linux__)
#include <sys/time.h>
#endif

using namespace xop;
using namespace std;

H264Source::H264Source(uint32_t framerate)
	: framerate_(framerate)
{
    payload_    = 96; 
    media_type_ = H264;
    clock_rate_ = 90000;
}

H264Source* H264Source::CreateNew(uint32_t framerate)
{
    return new H264Source(framerate);
}

H264Source::~H264Source()
{

}

std::string H264Source::Base64Encode(const uint8_t* data, size_t size)
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

string H264Source::GetMediaDescription(uint16_t port)
{
    char buf[100] = {0};
    sprintf(buf, "m=video %hu RTP/AVP 96", port); // \r\nb=AS:2000
    return string(buf);
}

string H264Source::GetAttribute()
{
    string attr = "a=rtpmap:96 H264/90000";

    // Add fmtp line with profile-level-id and sprop-parameter-sets if SPS/PPS available
    if (!sps_.empty() && !pps_.empty()) {
        // profile-level-id is bytes 1,2,3 of SPS (after NAL header)
        uint32_t profile_level_id = 0;
        if (sps_.size() >= 4) {
            profile_level_id = (sps_[1] << 16) | (sps_[2] << 8) | sps_[3];
        }

        std::string sps_b64 = Base64Encode(sps_.data(), sps_.size());
        std::string pps_b64 = Base64Encode(pps_.data(), pps_.size());

        char buf[512];
        sprintf(buf, "\r\na=fmtp:96 packetization-mode=1;profile-level-id=%06X;sprop-parameter-sets=%s,%s",
                profile_level_id, sps_b64.c_str(), pps_b64.c_str());
        attr += buf;
    }

    if (width_ > 0 && height_ > 0) {
        char buf[64];
        sprintf(buf, "\r\na=x-dimensions:%u,%u", width_, height_);
        attr += buf;
    }
    return attr;
}

bool H264Source::HandleFrame(MediaChannelId channel_id, AVFrame frame)
{
    uint8_t* frame_buf  = frame.buffer.data();
    uint32_t frame_size = frame.buffer.size();

    if (frame.timestamp == 0) {
	    frame.timestamp = GetTimestamp();
    }    

    if (frame_size <= MAX_RTP_PAYLOAD_SIZE) {
      RtpPacket rtp_pkt;
      rtp_pkt.type = frame.type;
      rtp_pkt.timestamp = frame.timestamp;
      rtp_pkt.size = frame_size + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
      rtp_pkt.last = 1;
      memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE, frame_buf, frame_size);

      if (send_frame_callback_) {
        if (!send_frame_callback_(channel_id, rtp_pkt)) {
          return false;
        }               
      }
    } else {
        char FU_A[2] = {0};

        FU_A[0] = (frame_buf[0] & 0xE0) | 28;
        FU_A[1] = 0x80 | (frame_buf[0] & 0x1f);

        frame_buf  += 1;
        frame_size -= 1;

        while (frame_size + 2 > MAX_RTP_PAYLOAD_SIZE) {
            RtpPacket rtp_pkt;
            rtp_pkt.type = frame.type;
            rtp_pkt.timestamp = frame.timestamp;
            rtp_pkt.size = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;
            rtp_pkt.last = 0;

            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = FU_A[0];
            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = FU_A[1];
            memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+2, frame_buf, MAX_RTP_PAYLOAD_SIZE-2);

            if (send_frame_callback_) {
                if (!send_frame_callback_(channel_id, rtp_pkt))
                    return false;
            }

            frame_buf  += MAX_RTP_PAYLOAD_SIZE - 2;
            frame_size -= MAX_RTP_PAYLOAD_SIZE - 2;

            FU_A[1] &= ~0x80;
        }

        {
            RtpPacket rtp_pkt;
            rtp_pkt.type = frame.type;
            rtp_pkt.timestamp = frame.timestamp;
            rtp_pkt.size = 4 + RTP_HEADER_SIZE + 2 + frame_size;
            rtp_pkt.last = 1;

            FU_A[1] |= 0x40;
            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = FU_A[0];
            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = FU_A[1];
            memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+2, frame_buf, frame_size);

            if (send_frame_callback_) {
			    if (!send_frame_callback_(channel_id, rtp_pkt)) {
				    return false;
			    }              
            }
        }
    }

    return true;
}

int64_t H264Source::GetTimestamp()
{
/* #if defined(__linux) || defined(__linux__)
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    uint32_t ts = ((tv.tv_sec*1000)+((tv.tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    return ts;
#else  */
    auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
    return (int64_t)((time_point.time_since_epoch().count() + 500) / 1000 * 90 );
//#endif
}
 

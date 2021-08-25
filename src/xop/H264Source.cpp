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
#if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(ANDROID)
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
    
    nalUnitChecked_ = false;
    decodeNAL_ = false;
}

H264Source* H264Source::CreateNew(uint32_t framerate)
{
    return new H264Source(framerate);
}

H264Source::~H264Source()
{

}

string H264Source::GetMediaDescription(uint16_t port)
{
    char buf[100] = {0};
    sprintf(buf, "m=video %hu RTP/AVP 96", port); // \r\nb=AS:2000
    return string(buf);
}

string H264Source::GetAttribute()
{
    return string("a=rtpmap:96 H264/90000");
}


//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                          RTP Header                           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |STAP-B NAL HDR | DON                           | NALU 1 Size   |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | NALU 1 Size   | NALU 1 HDR    | NALU 1 Data                   |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
//  :                                                               :
//  +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |               | NALU 2 Size                   | NALU 2 HDR    |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                       NALU 2 Data                             |
//  :                                                               :
//  |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                               :...OPTIONAL RTP padding        |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
const string naluTypesStrings2[] =
{
    "0: Unspecified (non-VCL)",
    "1: Coded slice of a non-IDR picture (VCL)",    // P frame
    "2: Coded slice data partition A (VCL)",
    "3: Coded slice data partition B (VCL)",
    "4: Coded slice data partition C (VCL)",
    "5: Coded slice of an IDR picture (VCL)",      // I frame
    "6: Supplemental enhancement information (SEI) (non-VCL)",
    "7: Sequence parameter set (non-VCL)",         // SPS parameter
    "8: Picture parameter set (non-VCL)",          // PPS parameter
    "9: Access unit delimiter (non-VCL)",
    "10: End of sequence (non-VCL)",
    "11: End of stream (non-VCL)",
    "12: Filler data (non-VCL)",
    "13: Sequence parameter set extension (non-VCL)",
    "14: Prefix NAL unit (non-VCL)",
    "15: Subset sequence parameter set (non-VCL)",
    "16: Reserved (non-VCL)",
    "17: Reserved (non-VCL)",
    "18: Reserved (non-VCL)",
    "19: Coded slice of an auxiliary coded picture without partitioning (non-VCL)",
    "20: Coded slice extension (non-VCL)",
    "21: Coded slice extension for depth view components (non-VCL)",
    "22: Reserved (non-VCL)",
    "23: Reserved (non-VCL)",
    "24: STAP-A Single-time aggregation packet (non-VCL)",
    "25: STAP-B Single-time aggregation packet (non-VCL)",
    "26: MTAP16 Multi-time aggregation packet (non-VCL)",
    "27: MTAP24 Multi-time aggregation packet (non-VCL)",
    "28: FU-A Fragmentation unit (non-VCL)",
    "29: FU-B Fragmentation unit (non-VCL)",
    "30: Unspecified (non-VCL)",
    "31: Unspecified (non-VCL)",
};

bool H264Source::HandleFrame(MediaChannelId channel_id, AVFrame frame)
{
    uint8_t* frame_buf  = frame.buffer.data();
    uint32_t frame_size = frame.buffer.size();
    
    if(!nalUnitChecked_)
    {
        if(frame_buf[0] == 0 && frame_buf[1] == 0 && frame_buf[2] == 1) {
            decodeNAL_ = true;
        }
        else if(frame_buf[0] == 0 && frame_buf[1] == 0 && frame_buf[2] == 0 && frame_buf[3] == 1) {
            decodeNAL_ = true;
        }
    }
    if(decodeNAL_)
    {
        if (frame.timestamp == 0) {
            frame.timestamp = GetTimestamp();
        }
        
        // we received the raw byte stream - we need to packetize it into individual single NALU units (essentially
        // just have RTP header + data of NAL
        // parse full buffer
        int nalutypes[32];
        int nalustart[32];
        int naluend[32];
        int cnt = 0;
        for (int i = 0; i < frame_size-1; i++) {
            if (i >= 3) {
                if(frame_buf[i] == 0x01 && frame_buf[i-1] == 0x00 && frame_buf[i-2] == 0x00)
                {
                    int nalu_type = ((uint8_t)frame_buf[i+1] & 0x1F);
#if DEBUG
#if ANDROID
                    __android_log_print(ANDROID_LOG_VERBOSE,  MODULE_NAME,"=== FOUND NAL UNIT \"%@\" FOUND at %i", naluTypesStrings2[nalu_type], i);
#else
                    printf("=== FOUND NAL UNIT \"%@\" FOUND at %i", naluTypesStrings2[nalu_type], i);
#endif
#endif
                    nalutypes[cnt] = nalu_type;
                    nalustart[cnt] = i+1; // first element
                    if(cnt > 0)
                        naluend[cnt-1] = i-3; // last element that counts!
                    cnt++;
                }
                else if (frame_buf[i] == 0x01 && frame_buf[i-1] == 0x00 && frame_buf[i-2] == 0x00 && frame_buf[i-3] == 0x00)
                {
                    int nalu_type = ((uint8_t)frame_buf[i+1] & 0x1F);
#if DEBUG
#if ANDROID
                    __android_log_print(ANDROID_LOG_VERBOSE,  MODULE_NAME,"=== FOUND NAL UNIT \"%@\" FOUND at %i", naluTypesStrings2[nalu_type], i);
#else
                    printf("=== FOUND NAL UNIT \"%@\" FOUND at %i", naluTypesStrings2[nalu_type], i);
#endif
#endif
                    nalutypes[cnt] = nalu_type;
                    nalustart[cnt] = i+1; // first element
                    if(cnt > 0)
                        naluend[cnt-1] = i-4; // last element that counts!
                    cnt++;
                }
            }
        }
        naluend[cnt-1] = frame_size-1;
        
        // now we have the individual NAL units - lets send them individually...
        for(int i = 0; i < cnt; i++)
        {
            int this_size = naluend[i] - nalustart[i] + 1;
            uint8_t* this_buf = frame_buf + nalustart[i];
            
            if (this_size <= MAX_RTP_PAYLOAD_SIZE) {
              RtpPacket rtp_pkt;
              rtp_pkt.type = frame.type;
              rtp_pkt.timestamp = frame.timestamp;
              rtp_pkt.size = this_size + 4 + RTP_HEADER_SIZE;
              rtp_pkt.last = 1;
              memcpy(rtp_pkt.data.get()+4+RTP_HEADER_SIZE, this_buf, this_size);

              if (send_frame_callback_) {
                if (!send_frame_callback_(channel_id, rtp_pkt)) {
                  return false;
                }
              }
            } else {
                char FU_A[2] = {0};

                FU_A[0] = (this_buf[0] & 0xE0) | 28;
                FU_A[1] = 0x80 | (this_buf[0] & 0x1f);

                this_buf  += 1;
                this_size -= 1;

                while (this_size + 2 > MAX_RTP_PAYLOAD_SIZE) {
                    RtpPacket rtp_pkt;
                    rtp_pkt.type = frame.type;
                    rtp_pkt.timestamp = frame.timestamp;
                    rtp_pkt.size = 4 + RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;
                    rtp_pkt.last = 0;

                    rtp_pkt.data.get()[RTP_HEADER_SIZE+4] = FU_A[0];
                    rtp_pkt.data.get()[RTP_HEADER_SIZE+5] = FU_A[1];
                    memcpy(rtp_pkt.data.get()+4+RTP_HEADER_SIZE+2, this_buf, MAX_RTP_PAYLOAD_SIZE-2);

                    if (send_frame_callback_) {
                        if (!send_frame_callback_(channel_id, rtp_pkt))
                            return false;
                    }

                    this_buf  += MAX_RTP_PAYLOAD_SIZE - 2;
                    this_size -= MAX_RTP_PAYLOAD_SIZE - 2;

                    FU_A[1] &= ~0x80;
                }

                {
                    RtpPacket rtp_pkt;
                    rtp_pkt.type = frame.type;
                    rtp_pkt.timestamp = frame.timestamp;
                    rtp_pkt.size = 4 + RTP_HEADER_SIZE + 2 + this_size;
                    rtp_pkt.last = 1;

                    FU_A[1] |= 0x40;
                    rtp_pkt.data.get()[RTP_HEADER_SIZE+4] = FU_A[0];
                    rtp_pkt.data.get()[RTP_HEADER_SIZE+5] = FU_A[1];
                    memcpy(rtp_pkt.data.get()+4+RTP_HEADER_SIZE+2, this_buf, this_size);

                    if (send_frame_callback_) {
                        if (!send_frame_callback_(channel_id, rtp_pkt)) {
                            return false;
                        }
                    }
                }
            }
        }
    }
    else // WE RECEIVED THE BUFS ALREADY - JUST SEND NALUS AS BEFORE
    {
        if (frame.timestamp == 0) {
            frame.timestamp = GetTimestamp();
        }

        if (frame_size <= MAX_RTP_PAYLOAD_SIZE) {
          RtpPacket rtp_pkt;
          rtp_pkt.type = frame.type;
          rtp_pkt.timestamp = frame.timestamp;
          rtp_pkt.size = frame_size + 4 + RTP_HEADER_SIZE;
          rtp_pkt.last = 1;
          memcpy(rtp_pkt.data.get()+4+RTP_HEADER_SIZE, frame_buf, frame_size);

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
                rtp_pkt.size = 4 + RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;
                rtp_pkt.last = 0;

                rtp_pkt.data.get()[RTP_HEADER_SIZE+4] = FU_A[0];
                rtp_pkt.data.get()[RTP_HEADER_SIZE+5] = FU_A[1];
                memcpy(rtp_pkt.data.get()+4+RTP_HEADER_SIZE+2, frame_buf, MAX_RTP_PAYLOAD_SIZE-2);

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
                rtp_pkt.data.get()[RTP_HEADER_SIZE+4] = FU_A[0];
                rtp_pkt.data.get()[RTP_HEADER_SIZE+5] = FU_A[1];
                memcpy(rtp_pkt.data.get()+4+RTP_HEADER_SIZE+2, frame_buf, frame_size);

                if (send_frame_callback_) {
                    if (!send_frame_callback_(channel_id, rtp_pkt)) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

int64_t H264Source::GetTimestamp()
{
/* #if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(ANDROID)
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    uint32_t ts = ((tv.tv_sec*1000)+((tv.tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    return ts;
#else  */
    auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
    return (int64_t)((time_point.time_since_epoch().count() + 500) / 1000 * 90 );
//#endif
}

// AV1 RTSP Source
// Based on RFC 9164 - RTP Payload Format for AV1

#if defined(WIN32) || defined(_WIN32)
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "AV1Source.h"
#include <cstdio>
#include <chrono>
#if defined(__linux) || defined(__linux__)
#include <sys/time.h>
#endif

using namespace xop;
using namespace std;

AV1Source::AV1Source(uint32_t framerate)
	: framerate_(framerate)
{
	payload_    = 98;  // Dynamic payload type for AV1
	media_type_ = AV1;
	clock_rate_ = 90000;
}

AV1Source* AV1Source::CreateNew(uint32_t framerate)
{
	return new AV1Source(framerate);
}

AV1Source::~AV1Source()
{

}

std::string AV1Source::Base64Encode(const uint8_t* data, size_t size)
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

string AV1Source::GetMediaDescription(uint16_t port)
{
	char buf[100] = {0};
	sprintf(buf, "m=video %hu RTP/AVP 98", port);
	return string(buf);
}

string AV1Source::GetAttribute()
{
	// RFC 9164: a=rtpmap:98 AV1/90000
	string attr = "a=rtpmap:98 AV1/90000";

	// Add fmtp line with profile and level-idx if sequence header available
	// RFC 9164 defines: profile, level-idx, tier
	if (!sequence_header_.empty() && sequence_header_.size() >= 3) {
		// Parse sequence header OBU to extract profile and level
		// Sequence header structure (after OBU header):
		// seq_profile (3 bits), still_picture (1 bit), reduced_still_picture_header (1 bit)
		// The profile is in the first 3 bits of the payload
		uint8_t seq_profile = (sequence_header_[0] >> 5) & 0x07;

		// Level is more complex to extract - use a reasonable default
		// For now, just include the profile
		char buf[256];
		sprintf(buf, "\r\na=fmtp:98 profile=%u", seq_profile);
		attr += buf;
	}

	if (width_ > 0 && height_ > 0) {
		char buf[64];
		sprintf(buf, "\r\na=x-dimensions:%u,%u", width_, height_);
		attr += buf;
	}
	return attr;
}

bool AV1Source::HandleFrame(MediaChannelId channelId, AVFrame frame)
{
	uint8_t *frame_buf  = frame.buffer.data();
	uint32_t frame_size = frame.buffer.size();

	if (frame.timestamp == 0) {
		frame.timestamp = GetTimestamp();
	}

	// RFC 9164 AV1 RTP packetization
	// AV1 aggregation header (1 byte):
	// - Z (1 bit): Set to 1 if first OBU element is a continuation of previous packet
	// - Y (1 bit): Set to 1 if last OBU element will continue in next packet
	// - W (2 bits): Number of OBU elements minus 1 (0-2), or 3 for arbitrary number
	// - N (1 bit): Set to 1 if this is the start of a new coded video sequence
	// - Reserved (3 bits)

	if (frame_size + 1 <= MAX_RTP_PAYLOAD_SIZE) {
		// Frame fits in a single packet
		RtpPacket rtp_pkt;
		rtp_pkt.type = frame.type;
		rtp_pkt.timestamp = frame.timestamp;
		rtp_pkt.size = frame_size + 1 + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
		rtp_pkt.last = 1;

		// AV1 aggregation header: Z=0, Y=0, W=0 (1 OBU), N=0
		rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE] = 0x00;
		memcpy(rtp_pkt.data.get() + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1, frame_buf, frame_size);

		if (send_frame_callback_) {
			if (!send_frame_callback_(channelId, rtp_pkt)) {
				return false;
			}
		}
	} else {
		// Frame needs fragmentation
		// First fragment: Z=0 (not continuation), Y=1 (will continue), W=0, N=0
		// Middle fragments: Z=1 (continuation), Y=1 (will continue), W=0, N=0
		// Last fragment: Z=1 (continuation), Y=0 (end), W=0, N=0

		uint32_t offset = 0;
		bool first = true;

		while (offset < frame_size) {
			uint32_t chunk_size = MAX_RTP_PAYLOAD_SIZE - 1;  // -1 for aggregation header
			bool last_chunk = false;

			if (offset + chunk_size >= frame_size) {
				chunk_size = frame_size - offset;
				last_chunk = true;
			}

			RtpPacket rtp_pkt;
			rtp_pkt.type = frame.type;
			rtp_pkt.timestamp = frame.timestamp;
			rtp_pkt.size = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1 + chunk_size;
			rtp_pkt.last = last_chunk ? 1 : 0;

			// Build aggregation header
			uint8_t agg_header = 0;
			if (!first) {
				agg_header |= 0x80;  // Z=1: continuation
			}
			if (!last_chunk) {
				agg_header |= 0x40;  // Y=1: will continue
			}
			// W=0 (00): single OBU element in this packet

			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE] = agg_header;
			memcpy(rtp_pkt.data.get() + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1,
			       frame_buf + offset, chunk_size);

			if (send_frame_callback_) {
				if (!send_frame_callback_(channelId, rtp_pkt)) {
					return false;
				}
			}

			offset += chunk_size;
			first = false;
		}
	}

	return true;
}

int64_t AV1Source::GetTimestamp()
{
	auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
	return (int64_t)((time_point.time_since_epoch().count() + 500) / 1000 * 90);
}

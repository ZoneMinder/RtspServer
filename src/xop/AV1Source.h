// AV1 RTSP Source
// Based on RFC 9164 - RTP Payload Format for AV1

#ifndef XOP_AV1_SOURCE_H
#define XOP_AV1_SOURCE_H

#include "MediaSource.h"
#include "rtp.h"
#include <vector>

namespace xop
{

class AV1Source : public MediaSource
{
public:
	static AV1Source* CreateNew(uint32_t framerate=25);
	~AV1Source();

	void SetFramerate(uint32_t framerate)
	{ framerate_ = framerate; }

	uint32_t GetFramerate() const
	{ return framerate_; }

	void SetResolution(uint32_t width, uint32_t height)
	{ width_ = width; height_ = height; }

	uint32_t GetWidth() const { return width_; }
	uint32_t GetHeight() const { return height_; }

	// Set sequence header OBU for SDP
	void SetSequenceHeader(const uint8_t* data, size_t size)
	{ sequence_header_.assign(data, data + size); }

	// Passthrough mode: the caller (ZM, via ffmpeg's RTP muxer) has already
	// packetized the AV1 payload, so HandleFrame must NOT re-packetize — it just
	// copies the payload into an RTP packet. media_desc_fmt is a printf format
	// with a single %hu for the port (e.g. "m=video %hu RTP/AVP 96"); attribute
	// is the a= block from av_sdp_create. payload_type must match the SDP.
	void SetPassthrough(const std::string& media_desc_fmt,
	                    const std::string& attribute, uint32_t payload_type)
	{
		passthrough_ = true;
		passthrough_media_desc_ = media_desc_fmt;
		passthrough_attribute_ = attribute;
		payload_ = payload_type;
	}

	virtual std::string GetMediaDescription(uint16_t port=0);

	virtual std::string GetAttribute();

	virtual bool HandleFrame(MediaChannelId channelId, AVFrame frame);

	static int64_t GetTimestamp();

private:
	AV1Source(uint32_t framerate);
	static std::string Base64Encode(const uint8_t* data, size_t size);

	uint32_t framerate_ = 25;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	std::vector<uint8_t> sequence_header_;

	bool passthrough_ = false;
	std::string passthrough_media_desc_;
	std::string passthrough_attribute_;
};

}

#endif

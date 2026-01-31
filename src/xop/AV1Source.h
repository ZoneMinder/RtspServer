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
};

}

#endif

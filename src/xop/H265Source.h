// PHZ
// 2018-5-16

#ifndef XOP_H265_SOURCE_H
#define XOP_H265_SOURCE_H

#include "MediaSource.h"
#include "rtp.h"
#include <vector>

namespace xop
{

class H265Source : public MediaSource
{
public:
	static H265Source* CreateNew(uint32_t framerate=25);
	~H265Source();

	void Setframerate(uint32_t framerate)
	{ framerate_ = framerate; }

	uint32_t GetFramerate() const
	{ return framerate_; }

	void SetResolution(uint32_t width, uint32_t height)
	{ width_ = width; height_ = height; }

	uint32_t GetWidth() const { return width_; }
	uint32_t GetHeight() const { return height_; }

	// Set VPS/SPS/PPS for sprop parameters in SDP
	void SetVPS(const uint8_t* data, size_t size)
	{ vps_.assign(data, data + size); }

	void SetSPS(const uint8_t* data, size_t size)
	{ sps_.assign(data, data + size); }

	void SetPPS(const uint8_t* data, size_t size)
	{ pps_.assign(data, data + size); }

	virtual std::string GetMediaDescription(uint16_t port=0);

	virtual std::string GetAttribute();

	virtual bool HandleFrame(MediaChannelId channelId, AVFrame frame);

	static int64_t GetTimestamp();

private:
	H265Source(uint32_t framerate);
	static std::string Base64Encode(const uint8_t* data, size_t size);

	uint32_t framerate_ = 25;
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	std::vector<uint8_t> vps_;
	std::vector<uint8_t> sps_;
	std::vector<uint8_t> pps_;
};
	
}

#endif

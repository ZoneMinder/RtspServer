#ifndef XOP_G711U_SOURCE_H
#define XOP_G711U_SOURCE_H

#include "MediaSource.h"
#include "rtp.h"

namespace xop
{

class G711USource : public MediaSource
{
public:
	static G711USource* CreateNew();
	virtual ~G711USource();

	uint32_t GetSampleRate() const
	{ return samplerate_; }

	uint32_t GetChannels() const
	{ return channels_; }

	virtual std::string GetMediaDescription(uint16_t port=0);

	virtual std::string GetAttribute();

	bool HandleFrame(MediaChannelId channel_id, AVFrame frame);

	static int64_t GetTimestamp();

private:
	G711USource();

	uint32_t samplerate_ = 8000;
	uint32_t channels_ = 1;
};

}

#endif

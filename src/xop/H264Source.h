// PHZ
// 2018-5-16

#ifndef XOP_H264_SOURCE_H
#define XOP_H264_SOURCE_H

#include "MediaSource.h"
#include "rtp.h"

namespace xop
{ 

class DLL_API H264Source : public MediaSource
{
public:
	static H264Source* CreateNew(uint32_t framerate=25);
	~H264Source();

	void SetFramerate(uint32_t framerate)
	{ framerate_ = framerate; }

	uint32_t GetFramerate() const 
	{ return framerate_; }

	virtual std::string GetMediaDescription(uint16_t port); 

	virtual std::string GetAttribute(); 

	bool HandleFrame(MediaChannelId channel_id, AVFrame frame);

	static int64_t GetTimestamp();
	
private:
	H264Source(uint32_t framerate);

    bool nalUnitChecked_;
    bool decodeNAL_;
    
	uint32_t framerate_ = 25;
};
	
}

#endif




// PHZ
// 2018-5-16

#ifndef XOP_MEDIA_H
#define XOP_MEDIA_H

#include <cstdint>
#include <vector>

namespace xop
{

/* RTSP服务支持的媒体类型 */
enum MediaType
{
	PCMU = 0,
	PCMA = 8,
	H264 = 96,
	AAC  = 37,
	H265 = 265,   
	NONE
};	

enum FrameType
{
	VIDEO_FRAME_I = 0x01,	  
	VIDEO_FRAME_P = 0x02,
	VIDEO_FRAME_B = 0x03,    
	AUDIO_FRAME   = 0x11,   
};

struct AVFrame
{
	AVFrame() : type(0), timestamp(0) {}
	AVFrame(const uint8_t *data, std::size_t size) : AVFrame()
	{
		buffer.reserve(size);
		buffer.assign(data, data + size);
	}

	std::vector<uint8_t> buffer;     /* 帧数据 */
	uint8_t  type;				     /* 帧类型 */
	int64_t timestamp;		  	     /* 时间戳 */
};

static const int MAX_MEDIA_CHANNEL = 2;

enum MediaChannelId
{
	channel_0,
	channel_1
};

typedef uint32_t MediaSessionId;

}

#endif


#ifndef XOP_MEMMORY_MANAGER_H
#define XOP_MEMMORY_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mutex>
#include <memory>
#include <queue>
#include <vector>

namespace xop
{

void* Alloc(uint32_t size);
void Free(void *ptr);

std::shared_ptr<uint8_t> AllocRtpPacket();
void FreeRtpPacket(std::shared_ptr<uint8_t> buffer);

class MemoryPool;
class RtpPacketPool;

struct MemoryBlock
{
	uint32_t block_id = 0;
	MemoryPool *pool = nullptr;
	MemoryBlock *next = nullptr;
};

class MemoryPool
{
public:
	MemoryPool();
	virtual ~MemoryPool();

	void  Init(uint32_t size, uint32_t n);
	void* Alloc(uint32_t size);
	void  Free(void* ptr);

	size_t BolckSize() const
	{ return block_size_; }

//private:
	char* memory_ = nullptr;
	uint32_t block_size_ = 0;
	uint32_t num_blocks_ = 0;
	MemoryBlock* head_ = nullptr;
	std::mutex mutex_;
};

struct RtpPacketBuffer
{
	uint8_t data[1600];
	RtpPacketBuffer() { }
};

class RtpPacketPool
{
public:
	RtpPacketPool();
	virtual ~RtpPacketPool();
	
	void Init(uint32_t capacity);
	std::shared_ptr<uint8_t> AllocPacket();
	void FreePacket(std::shared_ptr<uint8_t> buffer);
	
	uint32_t GetAvailable() const
	{ return available_.size(); }
	
private:
	std::queue<std::shared_ptr<uint8_t>> available_;
	std::mutex mutex_;
	uint32_t capacity_ = 0;
	std::vector<std::shared_ptr<RtpPacketBuffer>> buffers_;
};

class MemoryManager
{
public:
	static MemoryManager& Instance();
	~MemoryManager();

	void* Alloc(uint32_t size);
	void  Free(void* ptr);
	
	std::shared_ptr<uint8_t> AllocRtpPacket();
	void FreeRtpPacket(std::shared_ptr<uint8_t> buffer);

private:
	MemoryManager();

	static const int kMaxMemoryPool = 3;
	MemoryPool memory_pools_[kMaxMemoryPool];
	RtpPacketPool rtp_packet_pool_;
};

}
#endif

// PHZ
// 2018-9-30

#include "RtpConnection.h"
#include "RtspConnection.h"
#include "net/SocketUtil.h"

using namespace std;
using namespace xop;

RtpConnection::RtpConnection(std::weak_ptr<TcpConnection> rtsp_connection)
    : rtsp_connection_(rtsp_connection)
{
	std::random_device rd;

	for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
		rtpfd_[chn] = 0;
		rtcpfd_[chn] = 0;
		memset(&media_channel_info_[chn], 0, sizeof(media_channel_info_[chn]));
		media_channel_info_[chn].rtp_header.version = RTP_VERSION;
		media_channel_info_[chn].packet_seq = rd()&0xffff;
		media_channel_info_[chn].rtp_header.seq = 0; //htons(1);
		media_channel_info_[chn].rtp_header.ts = htonl(rd());
		media_channel_info_[chn].rtp_header.ssrc = htonl(rd());
	}

	auto conn = rtsp_connection_.lock();
	rtsp_ip_ = conn->GetIp();
	rtsp_port_ = conn->GetPort();
}

RtpConnection::~RtpConnection()
{
	for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
		if(rtpfd_[chn] > 0) {
			SocketUtil::Close(rtpfd_[chn]);
		}

		if(rtcpfd_[chn] > 0) {
			SocketUtil::Close(rtcpfd_[chn]);
		}
	}
}

int RtpConnection::GetId() const
{
	auto conn = rtsp_connection_.lock();
	if (!conn) {
		return -1;
	}
	RtspConnection *rtspConn = (RtspConnection *)conn.get();
	return rtspConn->GetId();
}

bool RtpConnection::SetupRtpOverTcp(MediaChannelId channel_id, uint16_t rtp_channel, uint16_t rtcp_channel)
{
	auto conn = rtsp_connection_.lock();
	if (!conn) {
		return false;
	}

	media_channel_info_[channel_id].rtp_channel = rtp_channel;
	media_channel_info_[channel_id].rtcp_channel = rtcp_channel;
	rtpfd_[channel_id] = conn->GetSocket();
	rtcpfd_[channel_id] = conn->GetSocket();
	media_channel_info_[channel_id].is_setup = true;
	transport_mode_ = RTP_OVER_TCP;

	return true;
}

bool RtpConnection::SetupRtpOverUdp(MediaChannelId channel_id, uint16_t rtp_port, uint16_t rtcp_port)
{
	auto conn = rtsp_connection_.lock();
	if (!conn) {
		return false;
	}

	if(SocketUtil::GetPeerAddr(conn->GetSocket(), &peer_addr_) < 0) {
		return false;
	}

	media_channel_info_[channel_id].rtp_port = rtp_port;
	media_channel_info_[channel_id].rtcp_port = rtcp_port;

	std::random_device rd;
	for (int n = 0; n <= 10; n++) {
		if (n == 10) {
			return false;
		}

		local_rtp_port_[channel_id] = rd() & 0xfffe;
		local_rtcp_port_[channel_id] =local_rtp_port_[channel_id] + 1;

		rtpfd_[channel_id] = ::socket(AF_INET, SOCK_DGRAM, 0);
		if(!SocketUtil::Bind(rtpfd_[channel_id], "0.0.0.0",  local_rtp_port_[channel_id])) {
			SocketUtil::Close(rtpfd_[channel_id]);
			continue;
		}

		rtcpfd_[channel_id] = ::socket(AF_INET, SOCK_DGRAM, 0);
		if(!SocketUtil::Bind(rtcpfd_[channel_id], "0.0.0.0", local_rtcp_port_[channel_id])) {
			SocketUtil::Close(rtpfd_[channel_id]);
			SocketUtil::Close(rtcpfd_[channel_id]);
			continue;
		}

		break;
	}

	SocketUtil::SetSendBufSize(rtpfd_[channel_id], 200*1024);
#ifdef DEBUG
#if defined(ANDROID)
  __android_log_print(ANDROID_LOG_VERBOSE,  MODULE_NAME, "SocketUtil: SendBufSize is %d",SocketUtil::GetSendBufSize(rtpfd_[channel_id]));
#else
  std::cerr << "SendBufSize is " << SocketUtil::GetSendBufSize(rtpfd_[channel_id]) << std::endl;
#endif
#endif
	peer_rtp_addr_[channel_id].sin_family = AF_INET;
	peer_rtp_addr_[channel_id].sin_addr.s_addr = peer_addr_.sin_addr.s_addr;
	peer_rtp_addr_[channel_id].sin_port = htons(media_channel_info_[channel_id].rtp_port);

	peer_rtcp_sddr_[channel_id].sin_family = AF_INET;
	peer_rtcp_sddr_[channel_id].sin_addr.s_addr = peer_addr_.sin_addr.s_addr;
	peer_rtcp_sddr_[channel_id].sin_port = htons(media_channel_info_[channel_id].rtcp_port);

	media_channel_info_[channel_id].is_setup = true;
	transport_mode_ = RTP_OVER_UDP;

	return true;
}

bool RtpConnection::SetupRtpOverMulticast(MediaChannelId channel_id, std::string ip, uint16_t port)
{
    std::random_device rd;
    for (int n = 0; n <= 10; n++) {
		if (n == 10) {
			return false;
		}

		local_rtp_port_[channel_id] = rd() & 0xfffe;
		rtpfd_[channel_id] = ::socket(AF_INET, SOCK_DGRAM, 0);
		if (!SocketUtil::Bind(rtpfd_[channel_id], "0.0.0.0", local_rtp_port_[channel_id])) {
			SocketUtil::Close(rtpfd_[channel_id]);
			continue;
		}

		break;
    }

	media_channel_info_[channel_id].rtp_port = port;

	peer_rtp_addr_[channel_id].sin_family = AF_INET;
	peer_rtp_addr_[channel_id].sin_addr.s_addr = inet_addr(ip.c_str());
	peer_rtp_addr_[channel_id].sin_port = htons(port);

	media_channel_info_[channel_id].is_setup = true;
	transport_mode_ = RTP_OVER_MULTICAST;
	is_multicast_ = true;
	return true;
}

void RtpConnection::Play()
{
	for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
		if (media_channel_info_[chn].is_setup) {
			media_channel_info_[chn].is_play = true;
		}
	}
}

const uint32_t NTP_EPOCH_OFFSET = 2208992400u;
timeval NTP2Timeval(uint32_t msw, uint32_t lsw)
{
    struct timeval t;
    t.tv_sec = msw - NTP_EPOCH_OFFSET;
    t.tv_usec = (uint32_t)((((double)lsw) * 1000000.0) / ((uint32_t)(~0)));
    return t;
}

void RtpConnection::AssembleRTCPMessage()
{
// RTP:
//        0                   1                   2                   3
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |V=2|P|X|  CC   |M|     PT      |       sequence number         |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |                           timestamp                           |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |           synchronization source (SSRC) identifier            |
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//       |            contributing source (CSRC) identifiers             |
//       |                             ....                              |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


// RTCP:
//          0                   1                   2                   3
//          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// header  |V=2|P|    RC   |   PT=SR=200   |             length            |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |                         SSRC of sender                        |
//         +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// sender  |              NTP timestamp, most significant word             |
// info    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |             NTP timestamp, least significant word             |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |                         RTP timestamp                         |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |                     sender's packet count                     |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |                      sender's octet count                     |

    RtpPacket rpkt;
    rtcp_packet *pkt = reinterpret_cast<xop::rtcp_packet*>(rpkt.data.get() + 4);
    pkt->header.version = 2;
    pkt->header.p = 0;
    pkt->header.count = 0;
    pkt->header.pt = 200;
    pkt->header.length = htons(6);
    pkt->ssrc = media_channel_info_[0].rtp_header.ssrc;
    //
    int32_t* blk = (int32_t*)(rpkt.data.get() + 12); // + 12);

    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    uint32_t secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    uint64_t nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() - (1e9 * secs);

    auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
    uint32_t rtptm = (int64_t)((time_point.time_since_epoch().count() + 500) / 1000 * 90 );

    // usecs = (uint32)((((double)lsw) * 1000000.0) / ((uint32)(~0)))
    // NTP time stamp is seconds since 1970 as MSW
    blk[0] = htonl(secs + NTP_EPOCH_OFFSET);
    blk[1] = htonl((uint32_t)((nsecs * 4294967295) / 1000000000));
    blk[2] = htonl(rtptm); // is based on 10 microsecond intervals
    blk[3] = htonl(0x00);
    blk[4] = htonl(0x00);

    rpkt.last = 1;
    rpkt.size = 32;

    SendRtpPacket(xop::channel_0, rpkt, true);
}

void RtpConnection::Record()
{
	for (int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
		if (media_channel_info_[chn].is_setup) {
			media_channel_info_[chn].is_record = true;
			media_channel_info_[chn].is_play = true;
		}
    }

    AssembleRTCPMessage();
}

void RtpConnection::Teardown()
{
	if(!is_closed_) {
		is_closed_ = true;
		for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
			media_channel_info_[chn].is_play = false;
			media_channel_info_[chn].is_record = false;
		}
	}
}

string RtpConnection::GetMulticastIp(MediaChannelId channel_id) const
{
	return std::string(inet_ntoa(peer_rtp_addr_[channel_id].sin_addr));
}

string RtpConnection::GetRtpInfo(const std::string& rtsp_url)
{
	char buf[2048] = { 0 };
	snprintf(buf, 1024, "RTP-Info: ");

	int num_channel = 0;

	auto time_point = chrono::time_point_cast<chrono::milliseconds>(chrono::steady_clock::now());
	auto ts = time_point.time_since_epoch().count();
	for (int chn = 0; chn<MAX_MEDIA_CHANNEL; chn++) {
		uint32_t rtpTime = (uint32_t)(ts*media_channel_info_[chn].clock_rate / 1000);
		if (media_channel_info_[chn].is_setup) {
			if (num_channel != 0) {
				snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",");
			}

			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
					"url=%s/track%d;seq=0;rtptime=%u",
					rtsp_url.c_str(), chn, rtpTime);
			num_channel++;
		}
	}

	return std::string(buf);
}

void RtpConnection::SetFrameType(uint8_t frame_type)
{
	frame_type_ = frame_type;
	if(!has_key_frame_ && (frame_type == 0 || frame_type == VIDEO_FRAME_I)) {
		has_key_frame_ = true;
	}
}

void RtpConnection::SetRtpHeader(MediaChannelId channel_id, RtpPacket pkt)
{
	if((media_channel_info_[channel_id].is_play || media_channel_info_[channel_id].is_record) && has_key_frame_) {
		media_channel_info_[channel_id].rtp_header.marker = pkt.last;
		media_channel_info_[channel_id].rtp_header.ts = htonl(pkt.timestamp);
		media_channel_info_[channel_id].rtp_header.seq = htons(media_channel_info_[channel_id].packet_seq++);
		memcpy(pkt.data.get()+4, &media_channel_info_[channel_id].rtp_header, RTP_HEADER_SIZE);
	}
}

int RtpConnection::SendRtpPacket(MediaChannelId channel_id, RtpPacket pkt, bool isRtcp)
{
	if (is_closed_) {
		return -1;
	}

	auto conn = rtsp_connection_.lock();
	if (!conn) {
		return -1;
	}
	RtspConnection *rtsp_conn = (RtspConnection *)conn.get();
	bool ret = rtsp_conn->task_scheduler_->AddTriggerEvent([this, channel_id, pkt, isRtcp] {
        if(!isRtcp)
        {
            this->SetFrameType(pkt.type);
            this->SetRtpHeader(channel_id, pkt);
        }
		if((media_channel_info_[channel_id].is_play || media_channel_info_[channel_id].is_record) && (has_key_frame_ || isRtcp) ) {
			if(transport_mode_ == RTP_OVER_TCP) {
				SendRtpOverTcp(channel_id, pkt, isRtcp);
			}
			else {
				SendRtpOverUdp(channel_id, pkt);
			}

			//media_channel_info_[channel_id].octetCount  += pkt.size;
			//media_channel_info_[channel_id].packetCount += 1;
		}
	});

	return ret ? 0 : -1;
}

int RtpConnection::SendRtpOverTcp(MediaChannelId channel_id, RtpPacket pkt, bool isRtcp)
{
	auto conn = rtsp_connection_.lock();
	if (!conn) {
		return -1;
	}

	uint8_t* rtpPktPtr = pkt.data.get();
	rtpPktPtr[0] = '$';
    if(isRtcp)
        rtpPktPtr[1] = (char)media_channel_info_[channel_id].rtcp_channel;
    else
        rtpPktPtr[1] = (char)media_channel_info_[channel_id].rtp_channel;
	rtpPktPtr[2] = (char)(((pkt.size-4)&0xFF00)>>8);
	rtpPktPtr[3] = (char)((pkt.size -4)&0xFF);

	conn->Send((char*)rtpPktPtr, pkt.size);
	return pkt.size;
}

int RtpConnection::SendRtpOverUdp(MediaChannelId channel_id, RtpPacket pkt)
{
	int ret = sendto(rtpfd_[channel_id], (const char*)pkt.data.get()+4, pkt.size-4, 0,
					(struct sockaddr *)&(peer_rtp_addr_[channel_id]), sizeof(struct sockaddr_in));

	if(ret < 0) {
		Teardown();
		return -1;
	}

	return ret;
}

#include "media/MediaSession.h"
#include "media/RtpSession.h"
#include "media/RtpSource.h"

#include "util/Log.h"
#include "util/SafeMem.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>

int MediaSession::kMaxChannels = 2;
std::atomic<MediaSessionId> MediaSession::IdGen(0);

MediaSession::MediaSession(std::string name)
    : sess_name_(std::string(name)), sess_id_(IdGen.fetch_add(1)),
      conn_cb_(nullptr), discon_cb_(nullptr), rtp_packet_buf_(nullptr) {
  source_vct_.resize(kMaxChannels);
  for (auto &s : source_vct_) {
    s = nullptr;
  }
}

MediaSession::~MediaSession() {
  for (auto &source : source_vct_) {
    SafeDelete(source);
  }
}

int MediaSession::UserNum() {
  std::lock_guard<std::mutex> lg(session_map_mutex_);
  return socket_session_map_.size();
}

void MediaSession::AddRtpSource(int channel_id, RtpSource *source) {
  if (!source) {
    LOG(ERROR) << "add rtp source is null";
    return;
  }
  if (channel_id > kMaxChannels) {
    LOG(ERROR) << "add rtp source is null";
    return;
  }

  if (source_vct_[channel_id]) {
    delete source_vct_[channel_id];
  }

  source->SetSendRtpPacketCb([this](char *data, int len, bool key_frame) {
    std::lock_guard<std::mutex> lg(session_map_mutex_);
    // LOG(INFO) << "send rtp packet ...";
    for (auto &[k, v] : socket_session_map_) {
      v->SendRtpPacket(data, len, key_frame);
    }
  });
  source_vct_[channel_id] = source;
}

void MediaSession::AddRtpSession(int socket_fd, RtpSessionPtr &rtp) {
  if (conn_cb_) {
    conn_cb_();
  }
  LOG(INFO) << "add rtp_session into  media_session";
  std::lock_guard<std::mutex> lg(session_map_mutex_);
  socket_session_map_.emplace(socket_fd, rtp);
}

void MediaSession::RemoveRtpSession(int socket_fd) {
  std::lock_guard<std::mutex> lg(session_map_mutex_);
  auto iter = socket_session_map_.find(socket_fd);
  if (iter == socket_session_map_.end()) {
    return;
  }
  socket_session_map_.erase(iter);
}

void MediaSession::PushFrame(int channel_id, char *frame, int frame_size,
                             bool key_frame) {
  if (channel_id > kMaxChannels) {
    return;
  }
  if (!source_vct_[channel_id]) {
    return;
  }

  auto &source = source_vct_[channel_id];
  if (UserNum()) {
    source->SendFrameByRtpTcp(channel_id, frame, frame_size, key_frame);
  }
}

std::string MediaSession::GetSDP(std::string ip) {
  std::string res;
  char buf[2048] = {0};

  // sdp ???
  snprintf(buf, sizeof(buf),
           "v=0\r\n"
           "o=- 9%ld 1 IN IP4 %s\r\n"
           "t=0 0\r\n"
           "a=control:*\r\n",
           (long)std::time(NULL), ip.c_str());

  for (uint32_t chn = 0; chn < source_vct_.size(); chn++) {
    if (source_vct_[chn]) {
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s\r\n",
               source_vct_[chn]->GetMediaDescription(0).c_str());
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s\r\n",
               source_vct_[chn]->GetAttribute().c_str());

      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
               "a=control:track%d\r\n", chn);
    }
  }
  res = buf;
  return res;
}

// if (!rtp_packet_buf_) {
//   rtp_packet_buf_ = (char *)malloc(RTP_MAX_PKT_SIZE);
// } else {
//   memset(rtp_packet_buf_, 0, RTP_MAX_PKT_SIZE);
// }

// RtpPacketTcp *rtp_packet_tcp = (RtpPacketTcp *)rtp_packet_buf_;
// rtpHeaderInit(rtp_packet_tcp, 0, 0, 0, RTP_VESION, RTP_PAYLOAD_TYPE_H264,
// 0,
//               0, 0, 0x88923423);

// // RtpPacketTcp *rtp_pkt =
// //     (RtpPacketTcp *)malloc(sizeof(fg)) uint8_t naluType; //
// nalu??????????????? int sendBytes = 0;
// // int ret;
// char naluType = frame[0];
// if (frame_size <= RTP_MAX_PKT_SIZE) //
// nalu?????????????????????????????????NALU????????????
// {
//   /*
//    *   0 1 2 3 4 5 6 7 8 9
//    *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    *  |F|NRI|  Type   | a single NAL unit ... |
//    *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    */
//   memcpy(rtp_packet_tcp->payload, frame, frame_size);

//   int packet_size = frame_size + RTP_HEADER_SIZE;
//   rtp_packet_tcp->header[0] = '$';
//   rtp_packet_tcp->header[1] = 0;
//   rtp_packet_tcp->header[2] = (packet_size & 0xFF00) >> 8;
//   rtp_packet_tcp->header[3] = (packet_size)&0xFF;

//   int send_size = packet_size + 4;

//   if(socket_session_map_.size()){
//     auto it=socket_session_map_.begin();
//     it.
//   }

//   rtp_packet_tcp->rtpHeader.seq = htons(rtp_packet_tcp->rtpHeader.seq);
//   rtp_packet_tcp->rtpHeader.timestamp =
//       htonl(rtp_packet_tcp->rtpHeader.timestamp);
//   rtp_packet_tcp->rtpHeader.ssrc = htonl(rtp_packet_tcp->rtpHeader.ssrc);

// }
//   ret = rtpSendPacket(socket, rtpChannel, rtpPacket, frameSize);
//   if (ret < 0)
//     return -1;

//   rtpPacket->rtpHeader.seq++;
//   sendBytes += ret;
//   if ((naluType & 0x1F) == 7 ||
//       (naluType & 0x1F) == 8) // ?????????SPS???PPS????????????????????????
//     goto out;
// } else // nalu???????????????????????????????????????
// {
//   /*
//    *  0                   1                   2
//    *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
//    * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    * | FU indicator  |   FU header   |   FU payload   ...  |
//    * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    */

//   /*
//    *     FU Indicator
//    *    0 1 2 3 4 5 6 7
//    *   +-+-+-+-+-+-+-+-+
//    *   |F|NRI|  Type   |
//    *   +---------------+
//    */

//   /*
//    *      FU Header
//    *    0 1 2 3 4 5 6 7
//    *   +-+-+-+-+-+-+-+-+
//    *   |S|E|R|  Type   |
//    *   +---------------+
//    */

//   int pktNum = frame_size / RTP_MAX_PKT_SIZE;        // ?????????????????????
//   int remainPktSize = frame_size % RTP_MAX_PKT_SIZE; //
//   ??????????????????????????? int i, pos = 1;

//   /* ?????????????????? */
//   for (i = 0; i < pktNum; i++) {
//     rtpPacket->payload[0] = (naluType & 0x60) | 28;
//     rtpPacket->payload[1] = naluType & 0x1F;

//     if (i == 0)                                     //???????????????
//       rtpPacket->payload[1] |= 0x80;                // start
//     else if (remainPktSize == 0 && i == pktNum - 1) //??????????????????
//       rtpPacket->payload[1] |= 0x40;                // end

//     memcpy(rtpPacket->payload + 2, frame + pos, RTP_MAX_PKT_SIZE);
//     ret = rtpSendPacket(socket, rtpChannel, rtpPacket, RTP_MAX_PKT_SIZE +
//     2); if (ret < 0)
//       return -1;

//     rtpPacket->rtpHeader.seq++;
//     sendBytes += ret;
//     pos += RTP_MAX_PKT_SIZE;
//   }

//   /* ????????????????????? */
//   if (remainPktSize > 0) {
//     rtpPacket->payload[0] = (naluType & 0x60) | 28;
//     rtpPacket->payload[1] = naluType & 0x1F;
//     rtpPacket->payload[1] |= 0x40; // end

//     memcpy(rtpPacket->payload + 2, frame + pos, remainPktSize + 2);
//     ret = rtpSendPacket(socket, rtpChannel, rtpPacket, remainPktSize +
//     2); if (ret < 0)
//       return -1;

//     rtpPacket->rtpHeader.seq++;
//     sendBytes += ret;
//   }
// }
// }

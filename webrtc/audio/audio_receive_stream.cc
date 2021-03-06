/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/audio_receive_stream.h"

#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {
std::string AudioReceiveStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << '}';
  return ss.str();
}

std::string AudioReceiveStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{rtp: " << rtp.ToString();
  ss << ", voe_channel_id: " << voe_channel_id;
  if (!sync_group.empty())
    ss << ", sync_group: " << sync_group;
  ss << '}';
  return ss.str();
}

namespace internal {
AudioReceiveStream::AudioReceiveStream(
      RemoteBitrateEstimator* remote_bitrate_estimator,
      const webrtc::AudioReceiveStream::Config& config)
    : remote_bitrate_estimator_(remote_bitrate_estimator),
      config_(config),
      rtp_header_parser_(RtpHeaderParser::Create()) {
  LOG(LS_INFO) << "AudioReceiveStream: " << config_.ToString();
  RTC_DCHECK(config.voe_channel_id != -1);
  RTC_DCHECK(remote_bitrate_estimator_ != nullptr);
  RTC_DCHECK(rtp_header_parser_ != nullptr);
  for (const auto& ext : config.rtp.extensions) {
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    RTC_DCHECK_GE(ext.id, 1);
    RTC_DCHECK_LE(ext.id, 14);
    if (ext.name == RtpExtension::kAudioLevel) {
      RTC_CHECK(rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAudioLevel, ext.id));
    } else if (ext.name == RtpExtension::kAbsSendTime) {
      RTC_CHECK(rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAbsoluteSendTime, ext.id));
    } else if (ext.name == RtpExtension::kTransportSequenceNumber) {
      RTC_CHECK(rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionTransportSequenceNumber, ext.id));
    } else {
      RTC_NOTREACHED() << "Unsupported RTP extension.";
    }
  }
}

AudioReceiveStream::~AudioReceiveStream() {
  LOG(LS_INFO) << "~AudioReceiveStream: " << config_.ToString();
}

webrtc::AudioReceiveStream::Stats AudioReceiveStream::GetStats() const {
  return webrtc::AudioReceiveStream::Stats();
}

const webrtc::AudioReceiveStream::Config& AudioReceiveStream::config() const {
  return config_;
}

void AudioReceiveStream::Start() {
}

void AudioReceiveStream::Stop() {
}

void AudioReceiveStream::SignalNetworkState(NetworkState state) {
}

bool AudioReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return false;
}

bool AudioReceiveStream::DeliverRtp(const uint8_t* packet,
                                    size_t length,
                                    const PacketTime& packet_time) {
  RTPHeader header;

  if (!rtp_header_parser_->Parse(packet, length, &header)) {
    return false;
  }

  // Only forward if the parsed header has absolute sender time. RTP timestamps
  // may have different rates for audio and video and shouldn't be mixed.
  if (config_.combined_audio_video_bwe &&
      header.extension.hasAbsoluteSendTime) {
    int64_t arrival_time_ms = TickTime::MillisecondTimestamp();
    if (packet_time.timestamp >= 0)
      arrival_time_ms = (packet_time.timestamp + 500) / 1000;
    size_t payload_size = length - header.headerLength;
    remote_bitrate_estimator_->IncomingPacket(arrival_time_ms, payload_size,
                                              header, false);
  }
  return true;
}
}  // namespace internal
}  // namespace webrtc

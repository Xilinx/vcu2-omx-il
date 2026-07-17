// SPDX-FileCopyrightText: © 2026 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#pragma once

extern "C"
{
#include "lib_common/I_Communication.h"
}

#include <vector>

#include "SyncIp.hpp"

struct DummyDriver : public AL_ICommunication
{
  explicit DummyDriver(std::string device);
  virtual ~DummyDriver() = default;

  bool Open(void* handle, char const* device);
  void Close(void* handle);
  AL_ECommunicationError PostMessage(void* handle, AL_ECommunicationMessageID eMessageID, void* data, bool isBlocking);
  void Destroy(void);

  /* mock-up interface */

  void FinalizeBuffer(int32_t chanId, int32_t fb_id = -1);
  void SignalSyncError(int32_t chanId);
  void SignalWatchdogError(int32_t chanId);
  void SignalLumaDiffError(int32_t chanId);
  void SignalChromaDiffError(int32_t chanId);
  bool encode = true;
  int32_t numChan = 4;
  std::vector<ChannelStatus> channelStatuses {};
  std::string const device;
};

bool AL_InitDummyDriver(DummyDriver* driver, bool encode, int32_t numChan);
DummyDriver* AL_GetDummyDriver(std::string);

// SPDX-FileCopyrightText: © 2026 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "device_dec_interface.hpp"
#include <string>

extern "C"
{
#include <lib_common/I_Communication.h>
}

struct DecDeviceHardwareMcu final : DecDeviceInterface
{
  DecDeviceHardwareMcu(std::string device);
  ~DecDeviceHardwareMcu() override;

  AL_IDecScheduler* Init() override;
  void Deinit() override;

  BufferContiguities GetBufferContiguities() const override;
  BufferBytesAlignments GetBufferBytesAlignments() const override;
  void* GetDeviceContext() override;

private:
  std::string const device;
  AL_IDecScheduler* scheduler {};
  AL_ICommunication* communication {};
};

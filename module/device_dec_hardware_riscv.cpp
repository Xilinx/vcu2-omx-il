// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include <stdexcept>
#include "device_dec_hardware_riscv.h"

using namespace std;

DecDeviceHardwareRiscV::DecDeviceHardwareRiscV(string device) : device{device}
{
  AL_Lib_Decoder_Init(AL_LIB_DECODER_ARCH_RISCV);
  riscv_ctx = AL_Riscv_Decode_CreateCtx(device.c_str());

  if(!riscv_ctx)
    throw std::runtime_error("Failed to create context (trying to use " + device + ")");
}

DecDeviceHardwareRiscV::~DecDeviceHardwareRiscV()
{
  if(riscv_ctx)
    AL_Riscv_Decode_DestroyCtx(riscv_ctx);
}

AL_IDecScheduler* DecDeviceHardwareRiscV::Init()
{
  return nullptr;
}

void DecDeviceHardwareRiscV::Deinit()
{
}

BufferContiguities DecDeviceHardwareRiscV::GetBufferContiguities() const
{
  BufferContiguities bufferContiguities;
  bufferContiguities.input = false;
  bufferContiguities.output = true;
  return bufferContiguities;
}

BufferBytesAlignments DecDeviceHardwareRiscV::GetBufferBytesAlignments() const
{
  BufferBytesAlignments bufferBytesAlignments;
  bufferBytesAlignments.input = 0;
  bufferBytesAlignments.output = 32;
  return bufferBytesAlignments;
}

void* DecDeviceHardwareRiscV::GetDeviceContext()
{
  return riscv_ctx;
}

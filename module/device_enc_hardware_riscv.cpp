// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include <stdexcept>
#include "device_enc_hardware_riscv.h"

EncDeviceHardwareRiscV::EncDeviceHardwareRiscV(std::string device) :  device{device}
{
  AL_Lib_Encoder_Init(AL_LIB_ENCODER_ARCH_RISCV);
  riscv_ctx = AL_Riscv_Encode_CreateCtx(device.c_str());

  if(!riscv_ctx)
    throw std::runtime_error("Failed to create context (trying to use " + device + ")");
}

EncDeviceHardwareRiscV::~EncDeviceHardwareRiscV()
{
  if(riscv_ctx)
    AL_Riscv_Encode_DestroyCtx(riscv_ctx);
}

AL_IEncScheduler* EncDeviceHardwareRiscV::Init()
{
  if(!riscv_ctx)
    riscv_ctx = AL_Riscv_Encode_CreateCtx(device.c_str());
  return nullptr;
}

void EncDeviceHardwareRiscV::Deinit()
{
}

BufferContiguities EncDeviceHardwareRiscV::GetBufferContiguities() const
{
  BufferContiguities bufferContiguities;
  bufferContiguities.input = true;
  bufferContiguities.output = true;
  return bufferContiguities;
}

BufferBytesAlignments EncDeviceHardwareRiscV::GetBufferBytesAlignments() const
{
  BufferBytesAlignments bufferBytesAlignments;
  bufferBytesAlignments.input = 64;
  bufferBytesAlignments.output = 64;
  return bufferBytesAlignments;
}

void* EncDeviceHardwareRiscV::GetDeviceContext()
{
  return riscv_ctx;
}

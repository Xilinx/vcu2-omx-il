// SPDX-FileCopyrightText: © 2026 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include <stdexcept>
#include "device_enc_hardware_riscv.hpp"

EncDeviceHardwareRiscV::EncDeviceHardwareRiscV(std::string device) :  device{device}
{
  if(!AL_IS_SUCCESS_CODE(AL_Lib_Encoder_Init(AL_LIB_ENCODER_ARCH_RISCV)))
    throw std::runtime_error("Failed to initialized encoder RISC-V architecture");

  riscv_ctx = AL_Riscv_Encode_CreateCtx(device.c_str());

  if(!riscv_ctx)
    throw std::runtime_error("Failed to create context (trying to use " + device + ")");
}

EncDeviceHardwareRiscV::~EncDeviceHardwareRiscV()
{
  if(riscv_ctx)
    AL_Riscv_Encode_DestroyCtx(riscv_ctx);

  AL_Lib_Encoder_DeInit();
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

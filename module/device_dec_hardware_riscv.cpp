// SPDX-FileCopyrightText: © 2026 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include <stdexcept>
#include "device_dec_hardware_riscv.hpp"

using namespace std;

DecDeviceHardwareRiscV::DecDeviceHardwareRiscV(string device) : device{device}
{
  if(!AL_IS_SUCCESS_CODE(AL_Lib_Decoder_Init(AL_LIB_DECODER_ARCH_RISCV)))
    throw std::runtime_error("Failed to initialized decoder RISCV-V architecture");

  riscv_ctx = AL_Riscv_Decode_CreateCtx(device.c_str());

  if(!riscv_ctx)
    throw std::runtime_error("Failed to create context (trying to use " + device + ")");
}

DecDeviceHardwareRiscV::~DecDeviceHardwareRiscV()
{
  if(riscv_ctx)
    AL_Riscv_Decode_DestroyCtx(riscv_ctx);

  AL_Lib_Decoder_DeInit();
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

// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include "base/omx_component/omx_component_dec.h"
#include "base/omx_component/omx_expertise_hevc.h"
#include "module/settings_dec_hevc.h"
#include "module/module_dec.h"

#if AL_ENABLE_RISCV
#include "module/device_dec_hardware_riscv.h"
#endif

#include <utility>
#include <cstring>
#include <memory>
#include <functional>
#include <stdexcept>

using namespace std;

extern "C"
{
#include <lib_fpga/DmaAlloc.h>
}

#if AL_ENABLE_RISCV
static char const* RISCV_DEVICE_DEC_NAME()
{
  if(getenv("ALLEGRO_RISCV_DEC_DEVICE_PATH"))
    return getenv("ALLEGRO_RISCV_DEC_DEVICE_PATH");
  return "/dev/al_d3xx";
}

#endif

#include "base/omx_component/omx_expertise_avc.h"
#include "module/settings_dec_avc.h"

#if AL_ENABLE_RISCV
static DecComponent* GenerateAvcComponentRiscV(OMX_HANDLETYPE hComponent, OMX_STRING cComponentName, OMX_STRING cRole, OMX_ALG_COREINDEXTYPE nCoreParamIndex, OMX_PTR pSettings)
{
  (void)nCoreParamIndex;
  (void)pSettings;
  string deviceName = RISCV_DEVICE_DEC_NAME();

  if(nCoreParamIndex == OMX_ALG_CoreIndexDevice)
    deviceName = ((OMX_ALG_CORE_DEVICE*)pSettings)->cDevice;

  shared_ptr<DecDeviceHardwareRiscV> device {
    new DecDeviceHardwareRiscV {
      deviceName
    }
  };

  shared_ptr<AL_TAllocator> allocator = std::shared_ptr<AL_TAllocator> {
    AL_Riscv_Decode_DmaAlloc_Create(device->GetDeviceContext()), [](AL_TAllocator* allocator) {
      AL_Allocator_Destroy(allocator);
    }
  };

  shared_ptr<DecSettingsAVC> media {
    new DecSettingsAVC {
      device->GetBufferContiguities(), device->GetBufferBytesAlignments(), STRIDE_ALIGNMENTS_HARDWARE
    }
  };

  unique_ptr<DecModule> module {
    new DecModule {
      media, device, allocator
    }
  };
  unique_ptr<ExpertiseAVC> expertise {
    new ExpertiseAVC {}
  };

  return new DecComponent {
           hComponent, media, std::move(module), cComponentName, cRole, std::move(expertise)
  };
}

#endif

#include "module/settings_dec_mjpeg.h"

#if AL_ENABLE_RISCV
static DecComponent* GenerateJpegComponentRiscV(OMX_HANDLETYPE hComponent, OMX_STRING cComponentName, OMX_STRING cRole, OMX_ALG_COREINDEXTYPE nCoreParamIndex, OMX_PTR pSettings)
{
  (void)nCoreParamIndex;
  (void)pSettings;
  string deviceName = RISCV_DEVICE_DEC_NAME();

  if(nCoreParamIndex == OMX_ALG_CoreIndexDevice)
    deviceName = ((OMX_ALG_CORE_DEVICE*)pSettings)->cDevice;

  shared_ptr<DecDeviceHardwareRiscV> device {
    new DecDeviceHardwareRiscV {
      deviceName
    }
  };

  shared_ptr<AL_TAllocator> allocator = std::shared_ptr<AL_TAllocator> {
    AL_Riscv_Decode_DmaAlloc_Create(device->GetDeviceContext()), [](AL_TAllocator* allocator) {
      AL_Allocator_Destroy(allocator);
    }
  };

  shared_ptr<DecSettingsJPEG> media {
    new DecSettingsJPEG {
      device->GetBufferContiguities(), device->GetBufferBytesAlignments(), STRIDE_ALIGNMENTS_HARDWARE
    }
  };

  unique_ptr<DecModule> module {
    new DecModule {
      media, device, allocator
    }
  };

  return new DecComponent {
           hComponent, media, std::move(module), cComponentName, cRole, NULL
  };
}

#endif

#if AL_ENABLE_RISCV
static DecComponent* GenerateHevcComponentRiscV(OMX_HANDLETYPE hComponent, OMX_STRING cComponentName, OMX_STRING cRole, OMX_ALG_COREINDEXTYPE nCoreParamIndex, OMX_PTR pSettings)
{
  (void)nCoreParamIndex;
  (void)pSettings;
  string deviceName = RISCV_DEVICE_DEC_NAME();

  if(nCoreParamIndex == OMX_ALG_CoreIndexDevice)
    deviceName = ((OMX_ALG_CORE_DEVICE*)pSettings)->cDevice;

  shared_ptr<DecDeviceHardwareRiscV> device {
    new DecDeviceHardwareRiscV {
      deviceName
    }
  };

  shared_ptr<AL_TAllocator> allocator = std::shared_ptr<AL_TAllocator> {
    AL_Riscv_Decode_DmaAlloc_Create(device->GetDeviceContext()), [](AL_TAllocator* allocator) {
      AL_Allocator_Destroy(allocator);
    }
  };

  shared_ptr<DecSettingsHEVC> media {
    new DecSettingsHEVC {
      device->GetBufferContiguities(), device->GetBufferBytesAlignments(), STRIDE_ALIGNMENTS_HARDWARE
    }
  };

  unique_ptr<DecModule> module {
    new DecModule {
      media, device, allocator
    }
  };
  unique_ptr<ExpertiseHEVC> expertise {
    new ExpertiseHEVC {}
  };
  return new DecComponent {
           hComponent, media, std::move(module), cComponentName, cRole, std::move(expertise)
  };
}

#endif

static OMX_PTR GenerateDefaultComponent(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_STRING cComponentName, OMX_IN OMX_STRING cRole, OMX_IN OMX_ALG_COREINDEXTYPE nCoreParamIndex, OMX_IN OMX_PTR pSettings)
{
#if AL_ENABLE_RISCV

  if(!strncmp(cComponentName, "OMX.allegro.h265.riscv.decoder", strlen(cComponentName)))
    return GenerateHevcComponentRiscV(hComponent, cComponentName, cRole, nCoreParamIndex, pSettings);
#endif
#if AL_ENABLE_RISCV

  if(!strncmp(cComponentName, "OMX.allegro.h264.riscv.decoder", strlen(cComponentName)))
    return GenerateAvcComponentRiscV(hComponent, cComponentName, cRole, nCoreParamIndex, pSettings);
#endif
#if AL_ENABLE_RISCV

  if(!strncmp(cComponentName, "OMX.allegro.mjpeg.riscv.decoder", strlen(cComponentName)))
    return GenerateJpegComponentRiscV(hComponent, cComponentName, cRole, nCoreParamIndex, pSettings);
#endif
  return nullptr;
}

OMX_PTR CreateDecComponentPrivate(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_STRING cComponentName, OMX_IN OMX_STRING cRole, OMX_IN OMX_ALG_COREINDEXTYPE nCoreParamIndex, OMX_IN OMX_PTR pSettings)
{
  return GenerateDefaultComponent(hComponent, cComponentName, cRole, nCoreParamIndex, pSettings);
}

void DestroyDecComponentPrivate(OMX_IN OMX_PTR pComponentPrivate)
{
  delete static_cast<DecComponent*>(pComponentPrivate);
}

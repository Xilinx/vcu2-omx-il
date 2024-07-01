// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include "module/settings_enc_hevc.h"
#include "module/module_enc.h"
#include "module/cpp_memory.h"

#include "base/omx_component/omx_component_enc.h"
#include "base/omx_component/omx_expertise_hevc.h"

#if AL_ENABLE_DMA_COPY_ENC
#include "module/dma_memory.h"
#endif

#include "module/device_enc_hardware_riscv.h"

#include <cstring>
#include <memory>
#include <functional>
#include <stdexcept>
#include <utility>
#include <unistd.h>

using namespace std;

extern "C"
{
#include <lib_fpga/DmaAlloc.h>
}

static int constexpr HORIZONTAL_STRIDE_ALIGNMENTS = 64;
static int constexpr VERCTICAL_STRIDE_ALIGNMENTS_HEVC = 32;

static StrideAlignments constexpr STRIDE_ALIGNMENTS_HEVC {
  HORIZONTAL_STRIDE_ALIGNMENTS, VERCTICAL_STRIDE_ALIGNMENTS_HEVC
};

static int constexpr VERCTICAL_STRIDE_ALIGNMENTS_AVC = 16;

static StrideAlignments constexpr STRIDE_ALIGNMENTS_AVC {
  HORIZONTAL_STRIDE_ALIGNMENTS, VERCTICAL_STRIDE_ALIGNMENTS_AVC
};

#if defined(ANDROID) || defined(__ANDROID_API__)
static bool constexpr IS_SEPARATE_CONFIGURATION_FROM_DATA_ENABLED = true;
#else
static bool constexpr IS_SEPARATE_CONFIGURATION_FROM_DATA_ENABLED = false;
#endif

static MemoryInterface* createMemory()
{
#if AL_ENABLE_DMA_COPY_ENC
  char const* device = "/dev/dmaproxy";
  bool exist = access( device, F_OK ) == 0;
  if( exist )
    return new DMAMemory(device);
#endif
  return new CPPMemory();
}

static char const* RISCV_DEVICE_ENC_NAME()
{
  if(getenv("ALLEGRO_RISCV_ENC_DEVICE_PATH"))
    return getenv("ALLEGRO_RISCV_ENC_DEVICE_PATH");
  return "/dev/al_e2xx";
}

#include "base/omx_component/omx_expertise_avc.h"
#include "module/settings_enc_avc.h"

static EncComponent* GenerateAvcComponentRiscV(OMX_HANDLETYPE hComponent, OMX_STRING cComponentName, OMX_STRING cRole, OMX_ALG_COREINDEXTYPE nCoreParamIndex, OMX_PTR pSettings)
{
  (void)nCoreParamIndex;
  (void)pSettings;
  string deviceName = RISCV_DEVICE_ENC_NAME();

  if(nCoreParamIndex == OMX_ALG_CoreIndexDevice)
    deviceName = ((OMX_ALG_CORE_DEVICE*)pSettings)->cDevice;

  shared_ptr<EncDeviceHardwareRiscV> device {
    new EncDeviceHardwareRiscV {
      string(deviceName),
    }
  };

  shared_ptr<AL_TAllocator> allocator = std::shared_ptr<AL_TAllocator> {
    AL_Riscv_Encode_DmaAlloc_Create(device->GetDeviceContext()), [](AL_TAllocator* allocator) {
      AL_Allocator_Destroy(allocator);
    }
  };

  shared_ptr<EncSettingsAVC> media {
    new EncSettingsAVC {
      device->GetBufferContiguities(), device->GetBufferBytesAlignments(), STRIDE_ALIGNMENTS_AVC, IS_SEPARATE_CONFIGURATION_FROM_DATA_ENABLED, allocator
    }
  };

  shared_ptr<MemoryInterface> memory {
    createMemory()
  };
  unique_ptr<EncModule> module {
    new EncModule {
      media, device, allocator, memory
    }
  };
  unique_ptr<ExpertiseAVC> expertise {
    new ExpertiseAVC {}
  };
  return new EncComponent {
           hComponent, media, std::move(module), cComponentName, cRole, std::move(expertise)
  };
}

static EncComponent* GenerateHevcComponentRiscV(OMX_HANDLETYPE hComponent, OMX_STRING cComponentName, OMX_STRING cRole, OMX_ALG_COREINDEXTYPE nCoreParamIndex, OMX_PTR pSettings)
{
  (void)nCoreParamIndex;
  (void)pSettings;
  string deviceName = RISCV_DEVICE_ENC_NAME();

  if(nCoreParamIndex == OMX_ALG_CoreIndexDevice)
    deviceName = ((OMX_ALG_CORE_DEVICE*)pSettings)->cDevice;

  shared_ptr<EncDeviceHardwareRiscV> device {
    new EncDeviceHardwareRiscV {
      string(deviceName),
    }
  };

  shared_ptr<AL_TAllocator> allocator = std::shared_ptr<AL_TAllocator> {
    AL_Riscv_Encode_DmaAlloc_Create(device->GetDeviceContext()), [](AL_TAllocator* allocator) {
      AL_Allocator_Destroy(allocator);
    }
  };

  shared_ptr<EncSettingsHEVC> media {
    new EncSettingsHEVC {
      device->GetBufferContiguities(), device->GetBufferBytesAlignments(), STRIDE_ALIGNMENTS_HEVC, IS_SEPARATE_CONFIGURATION_FROM_DATA_ENABLED, allocator
    }
  };

  shared_ptr<MemoryInterface> memory {
    createMemory()
  };
  unique_ptr<EncModule> module {
    new EncModule {
      media, device, allocator, memory
    }
  };
  unique_ptr<ExpertiseHEVC> expertise {
    new ExpertiseHEVC {}
  };
  return new EncComponent {
           hComponent, media, std::move(module), cComponentName, cRole, std::move(expertise)
  };
}

static OMX_PTR GenerateDefaultComponent(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_STRING cComponentName, OMX_IN OMX_STRING cRole, OMX_IN OMX_ALG_COREINDEXTYPE nCoreParamIndex, OMX_IN OMX_PTR pSettings)
{

  if(!strncmp(cComponentName, "OMX.allegro.h265.riscv.encoder", strlen(cComponentName)))
    return GenerateHevcComponentRiscV(hComponent, cComponentName, cRole, nCoreParamIndex, pSettings);

  if(!strncmp(cComponentName, "OMX.allegro.h264.riscv.encoder", strlen(cComponentName)))
    return GenerateAvcComponentRiscV(hComponent, cComponentName, cRole, nCoreParamIndex, pSettings);
  return nullptr;
}

OMX_PTR CreateEncComponentPrivate(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_STRING cComponentName, OMX_IN OMX_STRING cRole, OMX_IN OMX_ALG_COREINDEXTYPE nCoreParamIndex, OMX_IN OMX_PTR pSettings)
{
  return GenerateDefaultComponent(hComponent, cComponentName, cRole, nCoreParamIndex, pSettings);
}

void DestroyEncComponentPrivate(OMX_IN OMX_PTR pComponentPrivate)
{
  delete static_cast<EncComponent*>(pComponentPrivate);
}

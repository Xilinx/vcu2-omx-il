// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <OMX_ComponentAlg.h> // buffer mode

#include "omx_component.h"
#include "module/module_dec.h"
#include <list>

struct DecComponent final : Component
{
  DecComponent(OMX_HANDLETYPE component, std::shared_ptr<SettingsInterface>, std::unique_ptr<DecModule>&& module, OMX_STRING name, OMX_STRING role, std::unique_ptr<ExpertiseInterface>&& expertise);
  ~DecComponent() override;
  OMX_ERRORTYPE AllocateBuffer(OMX_INOUT OMX_BUFFERHEADERTYPE** header, OMX_IN OMX_U32 index, OMX_IN OMX_PTR app, OMX_IN OMX_U32 size) override;
  OMX_ERRORTYPE FreeBuffer(OMX_IN OMX_U32 index, OMX_IN OMX_BUFFERHEADERTYPE* header) override;

private:
  struct PropagatedData
  {
    PropagatedData(OMX_HANDLETYPE hMarkTargetComponent, OMX_PTR pMarkData, OMX_U32 nTickCount, OMX_TICKS nTimeStamp, OMX_U32 nFlags) :
      hMarkTargetComponent{hMarkTargetComponent},
      pMarkData{pMarkData},
      nTickCount{nTickCount},
      nTimeStamp{nTimeStamp},
      nFlags{nFlags}
    {
    };
    OMX_HANDLETYPE const hMarkTargetComponent;
    OMX_PTR const pMarkData;
    OMX_U32 const nTickCount;
    OMX_TICKS const nTimeStamp;
    OMX_U32 const nFlags;
  };
  void EmptyThisBufferCallBack(BufferHandleInterface* handle) override;
  void AssociateCallBack(BufferHandleInterface* empty, BufferHandleInterface* fill) override;
  void FillThisBufferCallBack(BufferHandleInterface* filled) override;
  void EventCallBack(Callbacks::Event type, void* data) override;
  void FlushComponent() override;

  void TreatEmptyBufferCommand(Task* task) override;
  std::list<PropagatedData> transmit;
  std::mutex mutex;
  OMX_TICKS oldTimeStamp;
  bool dataHasBeenPropagated;
};

// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include "omx_component_enc.h"

#include <OMX_VideoAlg.h>
#include <OMX_ComponentAlg.h>
#include <OMX_IVCommonAlg.h>
#include <OMX_CoreAlg.h>

#include <cmath>
#include <utility>

#include "omx_component_getset.h"

#include "base/omx_checker/omx_checker.h"

using namespace std;

static EncModule& ToEncModule(ModuleInterface& module)
{
  return dynamic_cast<EncModule &>(module);
}

static BufferHandleType GetBufferHandlePort(shared_ptr<SettingsInterface> media, OMX_IN OMX_U32 index)
{
  BufferHandles handles;
  media->Get(SETTINGS_INDEX_BUFFER_HANDLES, &handles);
  auto bufferHandlePort = IsInputPort(index) ? handles.input : handles.output;
  return bufferHandlePort;
}

EncComponent::EncComponent(OMX_HANDLETYPE component, shared_ptr<SettingsInterface> media, std::unique_ptr<EncModule>&& module, OMX_STRING name, OMX_STRING role, std::unique_ptr<ExpertiseInterface>&& expertise) :
  Component{component, media, std::move(module), std::move(expertise), name, role}
{
}

EncComponent::~EncComponent() = default;

void EncComponent::EmptyThisBufferCallBack(BufferHandleInterface* handle)
{
  assert(handle);
  auto header = ((OMXBufferHandle*)(handle))->header;
  delete handle;

  if(roiMap.Exist(header))
  {
    auto roiBuffer = roiMap.Pop(header);
    roiFreeBuffers.push(roiBuffer);
  }

  ReturnEmptiedBuffer(header);
}

static void AddEncoderFlags(OMX_BUFFERHEADERTYPE* header, shared_ptr<SettingsInterface> media, EncModule& module)
{
  Flags flags;
  auto success = module.GetDynamic(DYNAMIC_INDEX_STREAM_FLAGS, &flags);
  assert(success == ModuleInterface::SUCCESS);

  if(flags.isEndOfFrame)
    header->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

  if(flags.isEndOfSlice)
    header->nFlags |= OMX_BUFFERFLAG_ENDOFSUBFRAME;

  bool isSeparateConfigurationFromDataEnabled;
  auto ret = media->Get(SETTINGS_INDEX_SEPARATE_CONFIGURATION_FROM_DATA, &isSeparateConfigurationFromDataEnabled);
  assert(ret == SettingsInterface::SUCCESS);

  if(flags.isConfig && isSeparateConfigurationFromDataEnabled)
  {
    /* Remove previous added flags. They are not needed for codec config. Only sync may be used */
    header->nFlags = OMX_BUFFERFLAG_CODECCONFIG;
  }

  if(flags.isSync)
    header->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;

  if(flags.isCorrupt)
    header->nFlags |= OMX_BUFFERFLAG_DATACORRUPT;
}

void EncComponent::AssociateCallBack(BufferHandleInterface* empty_, BufferHandleInterface* fill_)
{
  auto empty = (OMXBufferHandle*)(empty_);
  auto fill = (OMXBufferHandle*)(fill_);
  auto emptyHeader = empty->header;
  auto fillHeader = fill->header;

  PropagateHeaderData(*emptyHeader, *fillHeader);

  AddEncoderFlags(fillHeader, media, ToEncModule(*module));

  /* backward datacorrupt to source buffer */
  if(fillHeader->nFlags & OMX_BUFFERFLAG_DATACORRUPT)
    emptyHeader->nFlags |= OMX_BUFFERFLAG_DATACORRUPT;

  if(seisMap.Exist(empty_) && ((fillHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG) == 0))
  {
    auto seis = seisMap.Pop(empty_);

    for(auto sei : seis)
    {
      Sei moduleSei {};
      moduleSei.type = sei.configSei.nType;
      moduleSei.data = sei.configSei.pBuffer + sei.configSei.nOffset;
      moduleSei.payload = sei.configSei.nFilledLen;
      sei.isPrefix ? module->SetDynamic(DYNAMIC_INDEX_INSERT_PREFIX_SEI, &moduleSei) : module->SetDynamic(DYNAMIC_INDEX_INSERT_SUFFIX_SEI, &moduleSei);
      delete[]sei.configSei.pBuffer;
    }
  }

  if(IsEOSDetected(emptyHeader->nFlags))
    callbacks.EventHandler(component, app, OMX_EventBufferFlag, output.index, emptyHeader->nFlags, nullptr);

  if(IsCompMarked(emptyHeader->hMarkTargetComponent, component))
    callbacks.EventHandler(component, app, OMX_EventMark, 0, 0, emptyHeader->pMarkData);
}

void EncComponent::FillThisBufferCallBack(BufferHandleInterface* filled)
{
  if(!filled)
  {
    if(eosHandles.input && eosHandles.output)
      AssociateCallBack(eosHandles.input, eosHandles.output);

    if(eosHandles.input)
      EmptyThisBufferCallBack(eosHandles.input);

    if(eosHandles.output)
      FillThisBufferCallBack(eosHandles.output);
    eosHandles.input = nullptr;
    eosHandles.output = nullptr;
    return;
  }

  assert(filled);
  auto header = (OMX_BUFFERHEADERTYPE*)(((OMXBufferHandle*)(filled))->header);
  auto offset = ((OMXBufferHandle*)filled)->offset;
  auto payload = ((OMXBufferHandle*)filled)->payload;
  bool isSkipped;
  auto err = module->GetDynamic(DYNAMIC_INDEX_SKIP_PICTURE, &isSkipped);
  assert(err == ModuleInterface::SUCCESS);

  if(isSkipped)
    header->nFlags |= OMX_BUFFERFLAG_SKIPFRAME;

  delete filled;

  ReturnFilledBuffer(header, offset, payload);
}

static OMX_BUFFERHEADERTYPE* AllocateHeader(OMX_PTR app, int size, OMX_U8* buffer, bool isBufferAllocatedByModule, int index)
{
  auto header = new OMX_BUFFERHEADERTYPE;
  OMXChecker::SetHeaderVersion(*header);
  header->pBuffer = buffer;
  header->nAllocLen = size;
  header->pAppPrivate = app;
  header->pInputPortPrivate = new bool(isBufferAllocatedByModule);
  header->pOutputPortPrivate = new bool(isBufferAllocatedByModule);
  auto& p = IsInputPort(index) ? header->nInputPortIndex : header->nOutputPortIndex;
  p = index;

  return header;
}

static inline bool isBufferAllocatedByModule(OMX_BUFFERHEADERTYPE const* header)
{
  if(!header->pInputPortPrivate || !header->pOutputPortPrivate)
    return false;

  auto isInputAllocated = *(static_cast<bool*>(header->pInputPortPrivate));
  auto isOutputAllocated = *(static_cast<bool*>(header->pOutputPortPrivate));

  return isInputAllocated || isOutputAllocated;
}

static void DeleteHeader(OMX_BUFFERHEADERTYPE* header)
{
  delete static_cast<bool*>(header->pInputPortPrivate);
  delete static_cast<bool*>(header->pOutputPortPrivate);
  delete header;
}

uint8_t* EncComponent::AllocateROIBuffer()
{
  int roiSize;
  module->GetDynamic(DYNAMIC_INDEX_REGION_OF_INTEREST_QUALITY_BUFFER_SIZE, &roiSize);
  return static_cast<uint8_t*>(calloc(roiSize, sizeof(uint8_t)));
}

OMX_ERRORTYPE EncComponent::UseBuffer(OMX_OUT OMX_BUFFERHEADERTYPE** header, OMX_IN OMX_U32 index, OMX_IN OMX_PTR app, OMX_IN OMX_U32 size, OMX_IN OMX_U8* buffer)
{
  OMX_TRY();
  OMXChecker::CheckNotNull(header);
  OMXChecker::CheckNotNull(size);

  CheckPortIndex(index);
  auto port = GetPort(index);

  if(transientState != TransientState::LoadedToIdle && !(port->isTransientToEnable))
    throw OMX_ErrorIncorrectStateOperation;

  *header = AllocateHeader(app, size, buffer, false, index);
  assert(*header);
  port->Add(*header);

  if(IsInputPort(index))
  {
    auto roiBuffer = AllocateROIBuffer();
    roiFreeBuffers.push(roiBuffer);
    roiDestroyMap.Add(*header, roiBuffer);
  }

  return OMX_ErrorNone;
  OMX_CATCH_L([&](OMX_ERRORTYPE& e)
  {
    if(e != OMX_ErrorBadPortIndex)
      GetPort(index)->ErrorOccurred();
  });
}

OMX_ERRORTYPE EncComponent::AllocateBuffer(OMX_INOUT OMX_BUFFERHEADERTYPE** header, OMX_IN OMX_U32 index, OMX_IN OMX_PTR app, OMX_IN OMX_U32 size)
{
  OMX_TRY();
  OMXChecker::CheckNotNull(header);
  OMXChecker::CheckNotNull(size);
  CheckPortIndex(index);

  auto port = GetPort(index);

  if(transientState != TransientState::LoadedToIdle && !(port->isTransientToEnable))
    throw OMX_ErrorIncorrectStateOperation;

  auto bufferHandlePort = GetBufferHandlePort(media, index);
  bool dmaOnPort = (bufferHandlePort == BufferHandleType::BUFFER_HANDLE_FD);
  auto buffer = dmaOnPort ? reinterpret_cast<OMX_U8*>(ToEncModule(*module).AllocateDMA(size * sizeof(OMX_U8))) : static_cast<OMX_U8*>(module->Allocate(size * sizeof(OMX_U8)));

  if(dmaOnPort ? (static_cast<int>((intptr_t)buffer) < 0) : !buffer)
    throw OMX_ErrorInsufficientResources;

  *header = AllocateHeader(app, size, buffer, true, index);
  assert(*header);
  port->Add(*header);

  if(IsInputPort(index))
  {
    auto roiBuffer = AllocateROIBuffer();
    roiFreeBuffers.push(roiBuffer);
    roiDestroyMap.Add(*header, roiBuffer);
  }

  return OMX_ErrorNone;
  OMX_CATCH_L([&](OMX_ERRORTYPE& e)
  {
    if(e != OMX_ErrorBadPortIndex)
      GetPort(index)->ErrorOccurred();
  });
}

void EncComponent::DestroyROIBuffer(uint8_t* roiBuffer)
{
  free(roiBuffer);
}

OMX_ERRORTYPE EncComponent::FreeBuffer(OMX_IN OMX_U32 index, OMX_IN OMX_BUFFERHEADERTYPE* header)
{
  OMX_TRY();
  OMXChecker::CheckNotNull(header);

  CheckPortIndex(index);
  auto port = GetPort(index);

  if((transientState != TransientState::IdleToLoaded) && (!port->isTransientToDisable))
    callbacks.EventHandler(component, app, OMX_EventError, OMX_ErrorPortUnpopulated, 0, nullptr);

  if(isBufferAllocatedByModule(header))
  {
    auto bufferHandlePort = GetBufferHandlePort(media, index);
    bool dmaOnPort = (bufferHandlePort == BufferHandleType::BUFFER_HANDLE_FD);
    dmaOnPort ? ToEncModule(*module).FreeDMA(static_cast<int>((intptr_t)header->pBuffer)) : module->Free(header->pBuffer);
  }

  if(IsInputPort(index))
  {
    if(roiDestroyMap.Exist(header))
    {
      auto roiBuffer = roiDestroyMap.Pop(header);
      DestroyROIBuffer(roiBuffer);
    }
  }

  port->Remove(header);
  DeleteHeader(header);

  return OMX_ErrorNone;
  OMX_CATCH();
}

void EncComponent::TreatEmptyBufferCommand(Task* task)
{
  assert(task);
  assert(task->cmd == Command::EmptyBuffer);
  assert(static_cast<int>((intptr_t)task->data) == input.index);
  auto header = static_cast<OMX_BUFFERHEADERTYPE*>(task->opt.get());
  assert(header);

  if(state == OMX_StateInvalid)
  {
    callbacks.EmptyBufferDone(component, app, header);
    return;
  }

  AttachMark(header);

  if(header->nFilledLen == 0)
  {
    if(header->nFlags & OMX_BUFFERFLAG_EOS)
    {
      auto handle = new OMXBufferHandle(header);
      eosHandles.input = handle;
      auto success = module->Empty(handle);
      assert(success);
      return;
    }
    callbacks.EmptyBufferDone(component, app, header);
    return;
  }

  if(shouldPushROI && header->nFilledLen)
  {
    auto roiBuffer = roiFreeBuffers.pop();
    module->GetDynamic(DYNAMIC_INDEX_REGION_OF_INTEREST_QUALITY_BUFFER_FILL, roiBuffer);
    module->SetDynamic(DYNAMIC_INDEX_REGION_OF_INTEREST_QUALITY_BUFFER_EMPTY, roiBuffer);
    roiMap.Add(header, roiBuffer);
  }

  auto handle = new OMXBufferHandle(header);

  seisMap.Add(handle, std::move(tmpSeis));
  auto success = module->Empty(handle);
  assert(success);

  if(header->nFlags & OMX_BUFFERFLAG_EOS)
  {
    success = module->Empty(nullptr);
    assert(success);
    return;
  }

  shouldClearROI = true;
}

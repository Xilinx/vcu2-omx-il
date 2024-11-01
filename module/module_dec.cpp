// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include "module_dec.h"
#include <cmath>
#include <cassert>
#include <algorithm>
#include <utility/round.h>
#include <utility/logger.h>
#include <utility/scope_exit.h>
#include "convert_module_soft.h"

extern "C"
{
#include <lib_common/BufferPixMapMeta.h>
#include <lib_common/BufferStreamMeta.h>
#include <lib_common/BufferHandleMeta.h>
#include <lib_common/Error.h>
#include "lib_common/PicFormat.h"

#include <lib_common_dec/HDRMeta.h>
#include <lib_common_dec/IpDecFourCC.h>

#include <lib_fpga/DmaAllocLinux.h>
}

using namespace std;

DecModule::DecModule(shared_ptr<DecSettingsInterface> media, shared_ptr<DecDeviceInterface> device, shared_ptr<AL_TAllocator> allocator) :
  media{media},
  device{device},
  allocator{allocator},
  decoder{nullptr},
  resolutionFoundHasBeenCalled{false},
  initialDimension{-1, -1}
{
  assert(this->media);
  assert(this->device);
  assert(this->allocator);
  currentDisplayPictureInfo.type = -1;
  currentDisplayPictureInfo.concealed = false;
  ResetHDR();
  media->Reset();
}

void DecModule::ResetHDR()
{
  currentTransferCharacteristics = TransferCharacteristicsType::TRANSFER_CHARACTERISTICS_UNSPECIFIED;
  currentColourMatrix = ColourMatrixType::COLOUR_MATRIX_UNSPECIFIED;
  currentColorPrimaries = ColorPrimariesType::COLOR_PRIMARIES_UNSPECIFIED;
  currentHDRSEIs.mdcv.enabled = false;
  currentHDRSEIs.cll.enabled = false;
  currentHDRSEIs.st2094_10.enabled = false;
  currentHDRSEIs.st2094_40.enabled = false;
}

DecModule::~DecModule() = default;

static string ToStringDecodeError(AL_ERR error)
{
  string str_error = "";
  try
  {
    str_error = string {
      AL_Codec_ErrorToString(error)
    };
  }
  catch(out_of_range& e)
  {
    str_error = "unknown error";
  }
  return string {
           str_error + string {
             " ("
           } +to_string(error) + string {
             ")"
           }
  };
}

static ModuleInterface::ErrorType ToModuleError(int errorCode)
{
  switch(errorCode)
  {
  case AL_SUCCESS: return ModuleInterface::SUCCESS;
  case AL_ERR_CHAN_CREATION_NO_CHANNEL_AVAILABLE: return ModuleInterface::CHANNEL_CREATION_NO_CHANNEL_AVAILABLE;
  case AL_ERR_CHAN_CREATION_RESOURCE_UNAVAILABLE: return ModuleInterface::CHANNEL_CREATION_RESOURCE_UNAVAILABLE;
  case AL_ERR_CHAN_CREATION_LOAD_DISTRIBUTION: return ModuleInterface::CHANNEL_CREATION_LOAD_DISTRIBUTION;
  case AL_ERR_CHAN_CREATION_HW_CAPACITY_EXCEEDED: return ModuleInterface::CHANNEL_CREATION_HARDWARE_CAPACITY_EXCEEDED;
  case AL_ERR_REQUEST_MALFORMED: // fallthrough
  case AL_ERR_CMD_NOT_ALLOWED: // fallthrough
  case AL_ERR_INVALID_CMD_VALUE: return ModuleInterface::BAD_PARAMETER;
  case AL_ERR_NO_MEMORY: return ModuleInterface::NO_MEMORY;
  default: return ModuleInterface::UNDEFINED;
  }

  return ModuleInterface::UNDEFINED;
}

void DecModule::EndParsing(AL_TBuffer* parsedFrame, int parsingID)
{
  assert(parsedFrame);
  auto handlesMeta = (AL_THandleMetaData*)AL_Buffer_GetMetaData(parsedFrame, AL_META_TYPE_HANDLE);

  if(!handlesMeta)
    return;

  int numHandles = AL_HandleMetaData_GetNumHandles(handlesMeta);
  assert(parsingID < numHandles);

  AL_TDecMetaHandle* pDecMetaHandle = (AL_TDecMetaHandle*)AL_HandleMetaData_GetHandle(handlesMeta, parsingID);

  bool isEarlyCallbackEnabled;
  media->Get(SETTINGS_INDEX_LLP2_EARLY_CB, &isEarlyCallbackEnabled);

  bool const frameStillExists = handles.Exist(parsedFrame);

  if(isEarlyCallbackEnabled && (!frameStillExists))
  {
    // On LLP2, we display the frame on the first slice due to bEarlyCallback
    // We do not attach other metadata than the first slice one
    return;
  }

  if(pDecMetaHandle->eState == AL_DEC_HANDLE_STATE_PROCESSED)
  {
    AL_TBuffer* stream = pDecMetaHandle->pHandle;
    assert(stream);
    AL_Buffer_Ref(stream);
    auto seiMeta = (AL_TSeiMetaData*)AL_Buffer_GetMetaData(stream, AL_META_TYPE_SEI);

    if(seiMeta != nullptr)
    {
      AL_Buffer_RemoveMetaData(stream, (AL_TMetaData*)seiMeta);
      auto seis = displaySeis.Pop(parsedFrame);
      seis.push_back(seiMeta);
      displaySeis.Add(parsedFrame, seis);
    }

    auto handleIn = handles.Get(stream);
    auto handleOut = handles.Get(parsedFrame);
    assert(handleOut);
    callbacks.associate(handleIn, handleOut);
    AL_Buffer_Unref(stream);
    return;
  }

  assert(0);
}

void DecModule::EndDecoding(AL_TBuffer* decodedFrame)
{
  if(!decodedFrame)
  {
    Error(AL_Decoder_GetLastError(decoder));
    return;
  }

  bool isInputParsed;
  media->Get(SETTINGS_INDEX_INPUT_PARSED, &isInputParsed);

  if(!isInputParsed)
  {
    auto handleOut = handles.Get(decodedFrame);
    assert(handleOut);
    callbacks.associate(nullptr, handleOut);
  }
}

void DecModule::ReleaseBufs(AL_TBuffer* frame)
{
  auto handleOut = handles.Pop(frame);
  dpb.Remove(handleOut->data);
  callbacks.release(false, handleOut);
  AL_Buffer_Unref(frame);
  return;
}

void DecModule::CopyIfRequired(AL_TBuffer* frameToDisplay, int size)
{
  if(!shouldBeCopied.Exist(frameToDisplay))
    return;
  auto buffer = (unsigned char*)(shouldBeCopied.Get(frameToDisplay));
  copy(AL_Buffer_GetData(frameToDisplay), AL_Buffer_GetData(frameToDisplay) + size, buffer);
}

void DecModule::Display(AL_TBuffer* frameToDisplay, AL_TInfoDecode* info)
{
  auto isRelease = (frameToDisplay && info == nullptr);

  if(isRelease)
    return ReleaseBufs(frameToDisplay);

  auto isEOS = (frameToDisplay == nullptr && info == nullptr);

  if(isEOS)
  {
    callbacks.filled(nullptr);
    return;
  }

  int frameWidth = info->tDim.iWidth - (info->tCrop.uCropOffsetLeft + info->tCrop.uCropOffsetRight);
  int frameHeight = info->tDim.iHeight - (info->tCrop.uCropOffsetTop + info->tCrop.uCropOffsetBottom);

  Resolution resolution;
  media->Get(SETTINGS_INDEX_RESOLUTION, &resolution);

  if((resolution.dimension.horizontal != frameWidth) || (resolution.dimension.vertical != frameHeight))
  {
    resolution.dimension.horizontal = frameWidth;
    resolution.dimension.vertical = frameHeight;

    media->Set(SETTINGS_INDEX_RESOLUTION, &resolution);

    callbacks.event(Callbacks::Event::RESOLUTION_CHANGED, &resolution.dimension);
  }

  auto seis = displaySeis.Pop(frameToDisplay);

  if(!seis.empty())
  {
    for(auto const& sei: seis)
    {
      auto payload = sei->payload;

      for(int i = 0; i < sei->numPayload; ++i, ++payload)
      {
        if(payload->bPrefix)
          ParsedPrefixSei(payload->type, payload->pData, payload->size);
      }
    }
  }

  ResetHDR();
  auto pHDR = (AL_THDRMetaData*)AL_Buffer_GetMetaData(frameToDisplay, AL_META_TYPE_HDR);

  if(pHDR)
  {
    currentTransferCharacteristics = ConvertSoftToModuleTransferCharacteristics(pHDR->eTransferCharacteristics);
    currentColourMatrix = ConvertSoftToModuleColourMatrix(pHDR->eColourMatrixCoeffs);
    currentColorPrimaries = ConvertSoftToModuleColorPrimaries(pHDR->eColourDescription);
    currentHDRSEIs = ConvertSoftToModuleHDRSEIs(pHDR->tHDRSEIs);
  }

  BufferSizes bufferSizes {};
  media->Get(SETTINGS_INDEX_BUFFER_SIZES, &bufferSizes);
  auto size = bufferSizes.output;
  CopyIfRequired(frameToDisplay, size);
  currentDisplayPictureInfo.type = info->ePicStruct;
  auto const frame_error = AL_Decoder_GetFrameError(decoder, frameToDisplay);
  currentDisplayPictureInfo.concealed = (frame_error == AL_WARN_CONCEAL_DETECT || frame_error == AL_WARN_HW_CONCEAL_DETECT || frame_error == AL_WARN_INVALID_ACCESS_UNIT_STRUCTURE);
  auto handleOut = handles.Pop(frameToDisplay);
  handleOut->offset = 0;
  handleOut->payload = size;
  callbacks.filled(handleOut);

  if(!seis.empty())
  {
    for(auto const& sei: seis)
    {
      auto payload = sei->payload;

      for(int i = 0; i < sei->numPayload; ++i, ++payload)
      {
        if(!payload->bPrefix)
          ParsedSuffixSei(payload->type, payload->pData, payload->size);
      }

      AL_MetaData_Destroy((AL_TMetaData*)sei);
    }
  }

  currentDisplayPictureInfo.type = -1;
  currentDisplayPictureInfo.concealed = false;
}

void DecModule::ResolutionFound(int bufferNumber, AL_TStreamSettings const& settings, AL_TCropInfo const& crop)
{
  (void)bufferNumber, (void)crop;

  if(resolutionFoundHasBeenCalled)
    return;

  resolutionFoundHasBeenCalled = true;
  initialDimension.horizontal = settings.tDim.iWidth;
  initialDimension.vertical = settings.tDim.iHeight;
  media->settings.tStream = settings;

  StrideAlignments strideAlignments;
  media->Get(SETTINGS_INDEX_STRIDE_ALIGNMENTS, &strideAlignments);

  AL_TPicFormat const tPicFormat = AL_GetDecPicFormat(settings.eChroma, settings.iBitDepth, media->settings.eFBStorageMode, false, AL_PLANE_MODE_MAX_ENUM);

  media->stride.horizontal = RoundUp(static_cast<int>(AL_Decoder_GetMinPitch(settings.tDim.iWidth, &tPicFormat)), strideAlignments.horizontal);
  media->stride.vertical = RoundUp(static_cast<int>(AL_Decoder_GetMinStrideHeight(settings.tDim.iHeight, &tPicFormat)), strideAlignments.vertical);

  callbacks.event(Callbacks::Event::RESOLUTION_DETECTED, nullptr);
}

void DecModule::ParsedPrefixSei(int type, uint8_t* payload, int size)
{
  Sei sei {
    type, payload, size
  };
  callbacks.event(Callbacks::Event::SEI_PREFIX_PARSED, &sei);
}

void DecModule::ParsedSuffixSei(int type, uint8_t* payload, int size)
{
  Sei sei {
    type, payload, size
  };
  callbacks.event(Callbacks::Event::SEI_SUFFIX_PARSED, &sei);
}

void DecModule::Error(AL_ERR error)
{
  if(AL_IS_WARNING_CODE(error))
    LOG_WARNING(ToStringDecodeError(error));

  if(AL_IS_ERROR_CODE(error))
  {
    LOG_ERROR(ToStringDecodeError(error));
    callbacks.event(Callbacks::Event::ERROR, (void*)ToModuleError(error));
  }
}

ModuleInterface::ErrorType DecModule::CreateDecoder(bool shouldPrealloc)
{
  if(decoder)
  {
    LOG_ERROR("Decoder is ALREADY created");
    return UNDEFINED;
  }

  AL_TDecCallBacks decCallbacks {};
  decCallbacks.endParsingCB = { RedirectionEndParsing, this };
  decCallbacks.endDecodingCB = { RedirectionEndDecoding, this };
  decCallbacks.displayCB = { RedirectionDisplay, this };
  decCallbacks.resolutionFoundCB = { RedirectionResolutionFound, this };
  decCallbacks.parsedSeiCB = { RedirectionParsedSei, this };
  decCallbacks.errorCB = { RedirectionError, this };

  bool inputParsed;
  media->Get(SETTINGS_INDEX_INPUT_PARSED, &inputParsed);

  if(inputParsed)
    decCallbacks.parsedSeiCB = { nullptr, nullptr };

  if(shouldPrealloc)
  {
    media->settings.tStream.tDim.iHeight = RoundUp(media->settings.tStream.tDim.iHeight, 16);
    media->settings.tStream.tDim.iWidth = RoundUp(media->settings.tStream.tDim.iWidth, 16);
  }

  AL_ERR errorCode;

  auto scheduler = device->Init();

  if(device->GetDeviceContext())
    errorCode = AL_Decoder_CreateWithCtx(&decoder, device->GetDeviceContext(), allocator.get(), &media->settings, &decCallbacks);
  else
  errorCode = AL_Decoder_Create(&decoder, scheduler, allocator.get(), &media->settings, &decCallbacks);

  if(AL_IS_ERROR_CODE(errorCode))
  {
    LOG_ERROR(string { "Failed to create Decoder: " } +ToStringDecodeError(errorCode));
    return ToModuleError(errorCode);
  }

  if(shouldPrealloc)
  {
    if(!AL_Decoder_PreallocateBuffers(decoder))
    {
      auto errorCode = AL_Decoder_GetLastError(decoder);
      DestroyDecoder();
      return ToModuleError(errorCode);
    }
  }

  if((media->initialDisplayResolution.vertical != -1) && (media->initialDisplayResolution.horizontal != -1))
  {
    // After the decoder has been created, configure initial resolution expected by GST again.
    media->settings.tStream.tDim.iHeight = media->initialDisplayResolution.vertical;
    media->settings.tStream.tDim.iWidth = media->initialDisplayResolution.horizontal;
  }
  return SUCCESS;
}

bool DecModule::DestroyDecoder()
{
  if(!decoder)
  {
    LOG_ERROR("Decoder isn't created");
    return false;
  }

  AL_Decoder_Destroy(decoder);
  device->Deinit();
  decoder = nullptr;
  resolutionFoundHasBeenCalled = false;
  initialDimension = { -1, -1 };

  return true;
}

ModuleInterface::ErrorType DecModule::Restart()
{
  if(!decoder)
  {
    LOG_ERROR("Decoder isn't created");
    return UNDEFINED;
  }

  AL_Decoder_Destroy(decoder);
  device->Deinit();
  decoder = nullptr;

  return CreateDecoder(true);
}

void DecModule::Free(void* buffer)
{
  if(!buffer)
    return;

  if(dpb.Exist((char*)buffer))
  {
    auto handle = dpb.Pop((char*)buffer);
    assert(!handles.Exist(handle));
    AL_Buffer_Unref(handle);
  }

  if(allocated.Exist(buffer))
  {
    auto handle = allocated.Pop(buffer);
    AL_Allocator_Free(allocator.get(), handle);
  }
}

void DecModule::FreeDMA(int fd)
{
  if(fd < 0)
    return;

  auto buffer = (char*)((intptr_t)fd);

  if(dpb.Exist(buffer))
  {
    auto handle = dpb.Pop(buffer);
    assert(!handles.Exist(handle));
    AL_Buffer_Unref(handle);
  }

  if(allocatedDMA.Exist(fd))
  {
    auto handle = allocatedDMA.Pop(fd);
    AL_Allocator_Free(allocator.get(), handle);
  }
}

void* DecModule::Allocate(size_t size)
{
  auto handle = AL_Allocator_Alloc(allocator.get(), size);

  if(!handle)
  {
    LOG_ERROR("No more memory");
    return nullptr;
  }

  auto addr = AL_Allocator_GetVirtualAddr(allocator.get(), handle);
  assert(addr);
  allocated.Add(addr, handle);
  return addr;
}

int DecModule::AllocateDMA(int size)
{
  auto handle = AL_Allocator_Alloc(allocator.get(), size);

  if(!handle)
  {
    LOG_ERROR("No more memory");
    return -1;
  }

  auto fd = AL_LinuxDmaAllocator_GetFd((AL_TLinuxDmaAllocator*)allocator.get(), handle);
  allocatedDMA.Add(fd, handle);
  return fd;
}

static void StubCallbackEvent(Callbacks::Event, void*)
{
}

bool DecModule::SetCallbacks(Callbacks callbacks)
{
  if(!callbacks.emptied || !callbacks.associate || !callbacks.filled || !callbacks.release)
    return false;

  if(!callbacks.event)
    callbacks.event = &StubCallbackEvent;

  this->callbacks = callbacks;

  return true;
}

void DecModule::InputBufferDestroy(AL_TBuffer* input)
{
  auto handleIn = handles.Pop(input);

  AL_Buffer_Destroy(input);

  handleIn->offset = 0;
  handleIn->payload = 0;
  callbacks.emptied(handleIn);
}

void DecModule::InputBufferFreeWithoutDestroyingMemory(AL_TBuffer* input)
{
  auto handleIn = handles.Pop(input);

  input->iChunkCnt = 0;
  AL_Buffer_Destroy(input);

  handleIn->offset = 0;
  handleIn->payload = 0;
  callbacks.emptied(handleIn);
}

static bool isFd(BufferHandleType type)
{
  return type == BufferHandleType::BUFFER_HANDLE_FD;
}

static bool isCharPtr(BufferHandleType type)
{
  return type == BufferHandleType::BUFFER_HANDLE_CHAR_PTR;
}

AL_TBuffer* DecModule::CreateInputBuffer(char* buffer, int size)
{
  AL_TBuffer* input {};
  BufferHandles bufferHandles {};
  media->Get(SETTINGS_INDEX_BUFFER_HANDLES, &bufferHandles);

  if(isFd(bufferHandles.input))
  {
    auto fd = static_cast<int>((intptr_t)buffer);

    if(fd < 0)
      throw invalid_argument("fd");

    auto dmaHandle = AL_LinuxDmaAllocator_ImportFromFd((AL_TLinuxDmaAllocator*)allocator.get(), fd);

    if(!dmaHandle)
    {
      LOG_ERROR(string { "Failed to import fd: " } +to_string(fd));
      return nullptr;
    }

    input = AL_Buffer_Create(allocator.get(), dmaHandle, size, RedirectionInputBufferDestroy);
  }

  if(isCharPtr(bufferHandles.input))
  {
    if(allocated.Exist(buffer))
      input = AL_Buffer_Create(allocator.get(), allocated.Get(buffer), size, RedirectionInputBufferFreeWithoutDestroyingMemory);
    else
    {
      bool isInputParsed;
      media->Get(SETTINGS_INDEX_INPUT_PARSED, &isInputParsed);

      if(isInputParsed || device->GetDeviceContext())
      {
        input = AL_Buffer_Create_And_Allocate(allocator.get(), size, RedirectionInputBufferDestroy);
        copy(buffer, buffer + size, AL_Buffer_GetData(input));
      }
      else
        input = AL_Buffer_WrapData((uint8_t*)buffer, size, RedirectionInputBufferDestroy);
    }
  }

  if(input == nullptr)
    return nullptr;

  AL_Buffer_SetUserData(input, this);
  AL_Buffer_Ref(input);

  return input;
}

bool DecModule::CreateAndAttachStreamMeta(AL_TBuffer& buf)
{
  auto meta = (AL_TMetaData*)(AL_StreamMetaData_Create(1));

  if(!meta)
    return false;

  if(!AL_Buffer_AddMetaData(&buf, meta))
  {
    AL_MetaData_Destroy(meta);
    return false;
  }
  return true;
}

AL_EStreamBufFlags ConvertModuleToSoftStreamBufFlag(Flags flags)
{
  AL_EStreamBufFlags streamBufFlag = AL_STREAM_BUF_FLAG_UNKNOWN;

  if(flags.isEndOfSlice)
    streamBufFlag = static_cast<AL_EStreamBufFlags>(streamBufFlag | AL_STREAM_BUF_FLAG_ENDOFSLICE);

  if(flags.isEndOfFrame)
    streamBufFlag = static_cast<AL_EStreamBufFlags>(streamBufFlag | AL_STREAM_BUF_FLAG_ENDOFFRAME);

  return streamBufFlag;
}

AL_ESectionFlags ConvertModuleToSoftSectionFlags(Flags flags)
{
  AL_ESectionFlags softFlags = AL_SECTION_NO_FLAG;

  if(flags.isSync)
    softFlags = static_cast<AL_ESectionFlags>(softFlags | AL_SECTION_SYNC_FLAG);

  if(flags.isEndOfFrame)
    softFlags = static_cast<AL_ESectionFlags>(softFlags | AL_SECTION_END_FRAME_FLAG);

  return softFlags;
}

bool DecModule::Empty(BufferHandleInterface* handle)
{
  if(!decoder)
    return false;

  auto eos = (!handle || handle->payload == 0);

  if(eos)
  {
    AL_Decoder_Flush(decoder);
    return true;
  }

  auto buffer = handle->data;
  AL_TBuffer* input = CreateInputBuffer(buffer, handle->payload);

  if(!input)
    return false;
  bool inputParsed = false;
  media->Get(SETTINGS_INDEX_INPUT_PARSED, &inputParsed);

  if(inputParsed)
  {
    if(!AL_Buffer_GetMetaData(input, AL_META_TYPE_STREAM))
    {
      if(!CreateAndAttachStreamMeta(*input))
      {
        AL_Buffer_Unref(input);
        return false;
      }
    }
    auto streamMeta = (AL_TStreamMetaData*)(AL_Buffer_GetMetaData(input, AL_META_TYPE_STREAM));
    AL_StreamMetaData_ClearAllSections(streamMeta);
    AL_StreamMetaData_AddSection(streamMeta, 0, handle->payload, ConvertModuleToSoftSectionFlags(currentFlags));

    if(!AL_Buffer_GetMetaData(input, AL_META_TYPE_SEI))
    {
      int maxSei = 32;
      int maxSeiBuf = 10 * 1024;
      auto pSeiMeta = AL_SeiMetaData_Create(maxSei, maxSeiBuf);

      if(!pSeiMeta)
      {
        AL_Buffer_Unref(input);
        return false;
      }
      AL_Buffer_AddMetaData(input, (AL_TMetaData*)pSeiMeta);
    }
  }

  handles.Add(input, handle);

  auto pushed = AL_Decoder_PushStreamBuffer(decoder, input, handle->payload, ConvertModuleToSoftStreamBufFlag(currentFlags));

  AL_Buffer_Unref(input);

  return pushed;
}

static AL_TMetaData* CreatePixMapMeta(AL_TStreamSettings const& streamSettings, AL_EFbStorageMode storage, Resolution resolution)
{
  auto picFormat = AL_GetDecPicFormat(streamSettings.eChroma, static_cast<uint8_t>(streamSettings.iBitDepth), storage, false, AL_PLANE_MODE_MAX_ENUM);
  auto fourCC = AL_GetDecFourCC(picFormat);
  auto stride = resolution.stride.horizontal;
  auto sliceHeight = resolution.stride.vertical;
  auto meta = AL_PixMapMetaData_CreateEmpty(fourCC);
  meta->tDim = { resolution.dimension.horizontal, resolution.dimension.vertical };
  AL_TPlane planeY = { 0, 0, stride };
  auto success = AL_PixMapMetaData_AddPlane(meta, planeY, AL_PLANE_Y);
  assert(success);

  if(AL_IsMonochrome(fourCC))
    return (AL_TMetaData*)meta;
  assert((AL_IsSemiPlanar(fourCC) || (AL_GetChromaMode(fourCC) == AL_CHROMA_4_4_4 && AL_GetPlaneMode(fourCC) == AL_PLANE_MODE_PLANAR))
         && "Unsupported chroma format");

  if(AL_IsSemiPlanar(fourCC))
  {
    AL_TPlane planeUV = { 0, stride * sliceHeight, stride };
    success = AL_PixMapMetaData_AddPlane(meta, planeUV, AL_PLANE_UV);
    assert(success);
  }
  else
  {
    // Only 4_4_4 with U_V order as asserted before
    AL_TPlane planeU = { 0, stride * sliceHeight, stride };
    success = AL_PixMapMetaData_AddPlane(meta, planeU, AL_PLANE_U);
    assert(success);
    AL_TPlane planeV = { 0, stride * sliceHeight * 2, stride };
    success = AL_PixMapMetaData_AddPlane(meta, planeV, AL_PLANE_V);
    assert(success);
  }
  return (AL_TMetaData*)meta;
}

static AL_TMetaData* CreateHDRMeta()
{
  return (AL_TMetaData*)AL_HDRMetaData_Create();
}

void DecModule::OutputBufferDestroy(AL_TBuffer* output)
{
  output->iChunkCnt = 0;
  AL_Buffer_Destroy(output);
}

void DecModule::OutputDmaBufferDestroy(AL_TBuffer* output)
{
  AL_Buffer_Destroy(output);
}

void DecModule::OutputBufferDestroyAndFree(AL_TBuffer* output)
{
  shouldBeCopied.Pop(output);
  AL_Buffer_Destroy(output);
}

AL_TBuffer* DecModule::CreateOutputBuffer(char* buffer, int size)
{
  Resolution resolution {};
  media->Get(SETTINGS_INDEX_RESOLUTION, &resolution);

  std::vector<AL_TMetaData*> pendingMetas;
  auto scopeFunc = scopeExit([&pendingMetas]() {
    for(const auto& meta : pendingMetas)
      if(meta)
        AL_MetaData_Destroy(meta);
  });

  pendingMetas.push_back(CreatePixMapMeta(media->settings.tStream, media->settings.eFBStorageMode, resolution));
  pendingMetas.push_back(CreateHDRMeta());

  if(!pendingMetas[0] || !pendingMetas[1])
    return nullptr;

  AL_TBuffer* output {};
  BufferHandles bufferHandles {};
  media->Get(SETTINGS_INDEX_BUFFER_HANDLES, &bufferHandles);

  if(isFd(bufferHandles.output))
  {
    auto fd = static_cast<int>((intptr_t)buffer);

    if(fd < 0)
      throw invalid_argument("fd");

    auto dmaHandle = AL_LinuxDmaAllocator_ImportFromFd((AL_TLinuxDmaAllocator*)allocator.get(), fd);

    if(!dmaHandle)
    {
      LOG_ERROR(string { "Failed to import fd: " } +to_string(fd));
      return nullptr;
    }

    output = AL_Buffer_Create(allocator.get(), dmaHandle, size, RedirectionOutputDmaBufferDestroy);
  }

  if(isCharPtr(bufferHandles.output))
  {
    if(allocated.Exist(buffer))
      output = AL_Buffer_Create(allocator.get(), allocated.Get(buffer), size, RedirectionOutputBufferDestroy);
    else
    {
      output = AL_Buffer_Create_And_Allocate(allocator.get(), size, RedirectionOutputBufferDestroyAndFree);

      if(output)
        shouldBeCopied.Add(output, buffer);
    }
  }

  if(output == nullptr)
    return nullptr;

  for(const auto& meta : pendingMetas)
  {
    auto attachedPixMapMeta = AL_Buffer_AddMetaData(output, meta);
    assert(attachedPixMapMeta);
  }

  pendingMetas.clear();

  AL_Buffer_SetUserData(output, this);
  dpb.Add(buffer, output);
  AL_Buffer_Ref(output);

  return output;
}

bool DecModule::Fill(BufferHandleInterface* handle)
{
  if(!decoder || !handle)
    return false;

  auto buffer = handle->data;

  auto output = dpb.Exist(buffer) ? dpb.Get(buffer) : CreateOutputBuffer(buffer, handle->size);

  if(!output)
    return false;

  handles.Add(output, handle);

  AL_Decoder_PutDisplayPicture(decoder, output);
  return true;
}

ModuleInterface::ErrorType DecModule::Start(bool shouldPrealloc)
{
  if(decoder)
  {
    LOG_ERROR("You can't call Start twice");
    return UNDEFINED;
  }

  return CreateDecoder(shouldPrealloc);
}

bool DecModule::Stop()
{
  if(!decoder)
    return false;

  DestroyDecoder();
  return true;
}

ModuleInterface::ErrorType DecModule::SetDynamic(std::string index, void const* param)
{
  if(index == "DYNAMIC_INDEX_STREAM_FLAGS")
  {
    currentFlags = *static_cast<Flags const*>(param);
    return SUCCESS;
  }

  return BAD_INDEX;
}

ModuleInterface::ErrorType DecModule::GetDynamic(std::string index, void* param)
{
  if(index == "DYNAMIC_INDEX_CURRENT_DISPLAY_PICTURE_INFO")
  {
    auto displayPictureInfo = static_cast<DisplayPictureInfo*>(param);
    *displayPictureInfo = currentDisplayPictureInfo;
    return SUCCESS;
  }

  if(index == "DYNAMIC_INDEX_TRANSFER_CHARACTERISTICS")
  {
    auto tc = static_cast<TransferCharacteristicsType*>(param);
    *tc = currentTransferCharacteristics;
    return SUCCESS;
  }

  if(index == "DYNAMIC_INDEX_COLOUR_MATRIX")
  {
    auto cm = static_cast<ColourMatrixType*>(param);
    *cm = currentColourMatrix;
    return SUCCESS;
  }

  if(index == "DYNAMIC_INDEX_COLOR_PRIMARIES")
  {
    auto cp = static_cast<ColorPrimariesType*>(param);
    *cp = currentColorPrimaries;
    return SUCCESS;
  }

  if(index == "DYNAMIC_INDEX_HIGH_DYNAMIC_RANGE_SEIS")
  {
    auto hdrSEIs = static_cast<HighDynamicRangeSeis*>(param);
    *hdrSEIs = currentHDRSEIs;
    return SUCCESS;
  }

  if(index == "DYNAMIC_INDEX_MAX_RESOLUTION_CHANGE_SUPPORTED")
  {
    auto dimension = static_cast<Dimension<int>*>(param);
    dimension->horizontal = initialDimension.horizontal;
    dimension->vertical = initialDimension.vertical;
    return SUCCESS;
  }

  return BAD_INDEX;
}

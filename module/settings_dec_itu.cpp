// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include <cassert>
#include <utility/round.h>
#include "module/module_enums.h"
#include "settings_dec_itu.h"
#include "settings_checks.h"
#include "convert_module_soft.h"
#include "convert_module_soft_dec.h"

extern "C"
{
#include <lib_common/StreamBuffer.h>
}

using namespace std;

Clock CreateClock(AL_TDecSettings const& settings)
{
  Clock clock;

  clock.framerate = settings.uFrameRate / 1000;
  clock.clockratio = settings.uClkRatio;

  return clock;
}

bool UpdateClock(AL_TDecSettings& settings, Clock clock)
{
  if(!CheckClock(clock))
    return false;

  settings.uFrameRate = clock.framerate * 1000;
  settings.uClkRatio = clock.clockratio;
  settings.bForceFrameRate = settings.uFrameRate && settings.uClkRatio;

  return true;
}

int CreateInternalEntropyBuffer(AL_TDecSettings const& settings)
{
  return settings.iStackSize;
}

bool UpdateInternalEntropyBuffer(AL_TDecSettings& settings, int internalEntropyBuffer)
{
  if(!CheckInternalEntropyBuffer(internalEntropyBuffer))
    return false;

  settings.iStackSize = internalEntropyBuffer;

  return true;
}

SequencePictureModeType CreateSequenceMode(AL_TDecSettings const& settings)
{
  auto stream = settings.tStream;
  return ConvertSoftToModuleSequenceMode(stream.eSequenceMode);
}

bool UpdateSequenceMode(AL_TDecSettings& settings, SequencePictureModeType sequenceMode, vector<SequencePictureModeType> sequenceModes)
{
  if(!CheckSequenceMode(sequenceMode, sequenceModes))
    return false;

  auto& stream = settings.tStream;
  stream.eSequenceMode = ConvertModuleToSoftSequenceMode(sequenceMode);
  return true;
}

Format CreateFormat(AL_TDecSettings const& settings)
{
  Format format;
  auto stream = settings.tStream;

  format.color = ConvertSoftToModuleColor(stream.eChroma);
  format.bitdepth = stream.iBitDepth;
  format.storage = ConvertSoftToModuleStorage(settings.eFBStorageMode);

  return format;
}

bool UpdateFormat(AL_TDecSettings& settings, Format format, vector<ColorType> colors, vector<int> bitdepths, vector<StorageType> storages, Stride& stride, StrideAlignments strideAlignments)
{
  if(!CheckFormat(format, colors, bitdepths, storages))
    return false;

  auto& stream = settings.tStream;
  stream.eChroma = ConvertModuleToSoftChroma(format.color);
  stream.iBitDepth = format.bitdepth;
  settings.eFBStorageMode = ConvertModuleToSoftStorage(format.storage);

  AL_TPicFormat const tPicFormat = AL_GetDecPicFormat(stream.eChroma, stream.iBitDepth, settings.eFBStorageMode, false, AL_PLANE_MODE_MAX_ENUM);

  int minHorizontalStride = RoundUp(static_cast<int>(AL_Decoder_GetMinPitch(stream.tDim.iWidth, &tPicFormat)), strideAlignments.horizontal);
  stride.horizontal = max(minHorizontalStride, stride.horizontal);

  return true;
}

Resolution CreateResolution(AL_TDecSettings const& settings, Stride stride)
{
  auto streamSettings = settings.tStream;
  Resolution resolution;
  resolution.dimension.horizontal = streamSettings.tDim.iWidth;
  resolution.dimension.vertical = streamSettings.tDim.iHeight;
  resolution.stride.horizontal = stride.horizontal;
  resolution.stride.vertical = stride.vertical;

  return resolution;
}

static int RawAllocationSize(Stride stride, AL_EChromaMode eChromaMode)
{
  int constexpr IP_WIDTH_ALIGNMENT = 64;
  int constexpr IP_HEIGHT_ALIGNMENT = 64;
  assert(stride.horizontal % IP_WIDTH_ALIGNMENT == 0); // IP requirements
  assert(stride.vertical % IP_HEIGHT_ALIGNMENT == 0); // IP requirements
  auto size = stride.horizontal * stride.vertical;
  switch(eChromaMode)
  {
  case AL_CHROMA_MONO: return size;
  case AL_CHROMA_4_2_0: return (3 * size) / 2;
  case AL_CHROMA_4_2_2: return 2 * size;
  case AL_CHROMA_4_4_4: return 3 * size;
  default: return -1;
  }
}

BufferSizes CreateBufferSizes(AL_TDecSettings const& settings, Stride stride)
{
  BufferSizes bufferSizes {};
  auto streamSettings = settings.tStream;
  bufferSizes.input = AL_GetMaxNalSize(streamSettings.tDim, streamSettings.eChroma, streamSettings.iBitDepth, streamSettings.eProfile, streamSettings.iLevel);
  bufferSizes.output = RawAllocationSize(stride, streamSettings.eChroma);
  return bufferSizes;
}

DecodedPictureBufferType CreateDecodedPictureBuffer(AL_TDecSettings const& settings)
{
  return ConvertSoftToModuleDecodedPictureBuffer(settings.eDpbMode);
}

bool UpdateIsEnabledSubframe(AL_TDecSettings& settings, bool isSubframeEnabled)
{
  settings.bLowLat = isSubframeEnabled;
  settings.eDecUnit = isSubframeEnabled ? ConvertModuleToSoftDecodeUnit(DecodeUnitType::DECODE_UNIT_SLICE) : ConvertModuleToSoftDecodeUnit(DecodeUnitType::DECODE_UNIT_FRAME);
  return true;
}

bool UpdateDecodedPictureBuffer(AL_TDecSettings& settings, DecodedPictureBufferType decodedPictureBuffer)
{
  if(decodedPictureBuffer == DecodedPictureBufferType::DECODED_PICTURE_BUFFER_MAX_ENUM)
    return false;

  settings.eDpbMode = ConvertModuleToSoftDecodedPictureBuffer(decodedPictureBuffer);
  return true;
}

bool UpdateResolution(AL_TDecSettings& settings, Stride& stride, StrideAlignments strideAlignments, Resolution resolution)
{
  auto& streamSettings = settings.tStream;
  streamSettings.tDim = { resolution.dimension.horizontal, resolution.dimension.vertical };

  AL_TPicFormat const tPicFormat = AL_GetDecPicFormat(streamSettings.eChroma, streamSettings.iBitDepth, settings.eFBStorageMode, false, AL_PLANE_MODE_MAX_ENUM);

  int minHorizontalStride = RoundUp(static_cast<int>(AL_Decoder_GetMinPitch(streamSettings.tDim.iWidth, &tPicFormat)), strideAlignments.horizontal);
  stride.horizontal = max(minHorizontalStride, RoundUp(resolution.stride.horizontal, strideAlignments.horizontal));

  int minVerticalStride = RoundUp(static_cast<int>(AL_Decoder_GetMinStrideHeight(streamSettings.tDim.iHeight, &tPicFormat)), strideAlignments.vertical);
  stride.vertical = max(minVerticalStride, RoundUp(resolution.stride.vertical, strideAlignments.vertical));

  return true;
}

bool CreateRealtime(AL_TDecSettings const& settings)
{
  return settings.bNonRealtime;
}

bool UpdateRealtime(AL_TDecSettings& settings, bool isSubframeDisabled)
{
  settings.bNonRealtime = isSubframeDisabled;
  return true;
}

Point<int> CreateOutputPosition(AL_TDecSettings const& settings)
{
  Point<int> position {};
  position.x = settings.tOutputPosition.iX;
  position.y = settings.tOutputPosition.iY;
  return position;
}

bool UpdateOutputPosition(AL_TDecSettings& settings, Point<int> position)
{
  settings.tOutputPosition.iX = position.x;
  settings.tOutputPosition.iY = position.y;
  return true;
}


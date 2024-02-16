// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include <cstring> // memset
#include <cmath>
#include <cassert>
#include <utility/round.h>
#include "settings_dec_mjpeg.h"
#include "settings_dec_itu.h"
#include "settings_codec_itu.h"

using namespace std;

DecSettingsJPEG::DecSettingsJPEG(BufferContiguities bufferContiguities, BufferBytesAlignments bufferBytesAlignments, StrideAlignments strideAlignments)
{
  this->bufferContiguities.input = bufferContiguities.input;
  this->bufferContiguities.output = bufferContiguities.output;
  this->bufferBytesAlignments.input = bufferBytesAlignments.input;
  this->bufferBytesAlignments.output = bufferBytesAlignments.output;
  this->strideAlignments.horizontal = strideAlignments.horizontal;
  this->strideAlignments.vertical = strideAlignments.vertical;
  Reset();
}

DecSettingsJPEG::~DecSettingsJPEG() = default;

void DecSettingsJPEG::Reset()
{
  bufferHandles.input = BufferHandleType::BUFFER_HANDLE_CHAR_PTR;
  bufferHandles.output = BufferHandleType::BUFFER_HANDLE_CHAR_PTR;

  ::memset(&settings, 0, sizeof(settings));
  AL_DecSettings_SetDefaults(&settings);

  settings.iStackSize = 1;
  settings.uFrameRate = 60000;
  settings.uClkRatio = 1000;
  settings.uDDRWidth = 64;
  settings.eDecUnit = AL_AU_UNIT;
  settings.eDpbMode = AL_DPB_NORMAL;
  settings.bLowLat = false;
  settings.eFBStorageMode = AL_FB_RASTER;
  settings.eCodec = AL_CODEC_JPEG;
  settings.bUseIFramesAsSyncPoint = true;
  settings.eInputMode = AL_DEC_UNSPLIT_INPUT;

  auto& stream = settings.tStream;
  stream.tDim = { 176, 144 };
  stream.eChroma = AL_CHROMA_4_2_0;
  stream.iBitDepth = 8;
  stream.iLevel = 1;
  stream.eProfile = AL_PROFILE_JPEG;
  stream.eSequenceMode = AL_SM_PROGRESSIVE;

  AL_TPicFormat const tPicFormat = AL_GetDecPicFormat(stream.eChroma, stream.iBitDepth, settings.eFBStorageMode, false, AL_PLANE_MODE_MAX_ENUM);

  stride.horizontal = RoundUp(static_cast<int>(AL_Decoder_GetMinPitch(stream.tDim.iWidth, &tPicFormat)), strideAlignments.horizontal);
  stride.vertical = RoundUp(static_cast<int>(AL_Decoder_GetMinStrideHeight(stream.tDim.iHeight)), strideAlignments.vertical);
}

static Mimes CreateMimes()
{
  Mimes mimes;
  auto& input = mimes.input;

  input.mime = "video/x-jpeg";
  input.compression = CompressionType::COMPRESSION_MJPEG;

  auto& output = mimes.output;

  output.mime = "video/x-raw";
  output.compression = CompressionType::COMPRESSION_UNUSED;

  return mimes;
}

static int CreateLatency(AL_TDecSettings settings)
{
  double buffers = settings.iStackSize;

  double realFramerate = (static_cast<double>(settings.uFrameRate) / static_cast<double>(settings.uClkRatio));
  double timeInMilliseconds = (static_cast<double>(buffers * 1000.0) / realFramerate);

  return ceil(timeInMilliseconds);
}

static BufferCounts CreateBufferCounts(AL_TDecSettings settings)
{
  BufferCounts bufferCounts;
  bufferCounts.input = 2;
  bufferCounts.output = settings.iStackSize;
  return bufferCounts;
}

SettingsInterface::ErrorType DecSettingsJPEG::Get(std::string index, void* settings) const
{
  if(!settings)
    return BAD_PARAMETER;

  if(index == "SETTINGS_INDEX_MIMES")
  {
    *(static_cast<Mimes*>(settings)) = CreateMimes();
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_CLOCK")
  {
    *(static_cast<Clock*>(settings)) = CreateClock(this->settings);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_STRIDE_ALIGNMENTS")
  {
    *(static_cast<StrideAlignments*>(settings)) = this->strideAlignments;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_INTERNAL_ENTROPY_BUFFER")
  {
    *(static_cast<int*>(settings)) = CreateInternalEntropyBuffer(this->settings);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_LATENCY")
  {
    *(static_cast<int*>(settings)) = CreateLatency(this->settings);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_SEQUENCE_PICTURE_MODE")
  {
    *(static_cast<SequencePictureModeType*>(settings)) = CreateSequenceMode(this->settings);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_SEQUENCE_PICTURE_MODES_SUPPORTED")
  {
    *(static_cast<vector<SequencePictureModeType>*>(settings)) = this->sequenceModes;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_BUFFER_HANDLES")
  {
    *(static_cast<BufferHandles*>(settings)) = this->bufferHandles;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_BUFFER_COUNTS")
  {
    *(static_cast<BufferCounts*>(settings)) = CreateBufferCounts(this->settings);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_BUFFER_SIZES")
  {
    *(static_cast<BufferSizes*>(settings)) = CreateBufferSizes(this->settings, this->stride);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_BUFFER_CONTIGUITIES")
  {
    *(static_cast<BufferContiguities*>(settings)) = this->bufferContiguities;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_BUFFER_BYTES_ALIGNMENTS")
  {
    *(static_cast<BufferBytesAlignments*>(settings)) = this->bufferBytesAlignments;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_FORMAT")
  {
    *(static_cast<Format*>(settings)) = CreateFormat(this->settings);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_FORMATS_SUPPORTED")
  {
    SupportedFormats supported {};
    supported.input = CreateFormatsSupported(this->colors, this->bitdepths, this->storages);
    supported.output = vector<Format>({ CreateFormat(this->settings) });
    *(static_cast<SupportedFormats*>(settings)) = supported;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_SUBFRAME")
  {
    *(static_cast<bool*>(settings)) = (this->settings.eDecUnit == AL_VCL_NAL_UNIT);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_RESOLUTION")
  {
    *(static_cast<Resolution*>(settings)) = CreateResolution(this->settings, this->stride);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_DECODED_PICTURE_BUFFER")
  {
    *(static_cast<DecodedPictureBufferType*>(settings)) = CreateDecodedPictureBuffer(this->settings);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_LLP2_EARLY_CB")
  {
    *(static_cast<bool*>(settings)) = this->settings.bUseEarlyCallback;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_INPUT_PARSED")
  {
    *(static_cast<bool*>(settings)) = (this->settings.eInputMode == AL_DEC_SPLIT_INPUT);
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_OUTPUT_POSITION")
  {
    *(static_cast<Point<int>*>(settings)) = CreateOutputPosition(this->settings);
    return SUCCESS;
  }

#if AL_ENABLE_MULTI_INSTANCE

  if(index == "SETTINGS_INDEX_INSTANCE_ID")
  {
    *(static_cast<int*>(settings)) = CreateInstanceId(this->settings);
    return SUCCESS;
  }
#endif

  return BAD_INDEX;
}

SettingsInterface::ErrorType DecSettingsJPEG::Set(std::string index, void const* settings)
{
  if(!settings)
    return BAD_PARAMETER;

  if(index == "SETTINGS_INDEX_CLOCK")
  {
    auto clock = *(static_cast<Clock const*>(settings));

    if(!UpdateClock(this->settings, clock))
      return BAD_PARAMETER;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_INTERNAL_ENTROPY_BUFFER")
  {
    auto internalEntropyBuffer = *(static_cast<int const*>(settings));

    if(!UpdateInternalEntropyBuffer(this->settings, internalEntropyBuffer))
      return BAD_PARAMETER;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_SEQUENCE_PICTURE_MODE")
  {
    auto sequenceMode = *(static_cast<SequencePictureModeType const*>(settings));

    if(!UpdateSequenceMode(this->settings, sequenceMode, sequenceModes))
      return BAD_PARAMETER;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_FORMAT")
  {
    auto format = *(static_cast<Format const*>(settings));

    if(!UpdateFormat(this->settings, format, this->colors, this->bitdepths, this->storages, this->stride, this->strideAlignments))
      return BAD_PARAMETER;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_BUFFER_HANDLES")
  {
    auto bufferHandles = *(static_cast<BufferHandles const*>(settings));

    if(!UpdateBufferHandles(this->bufferHandles, bufferHandles))
      return BAD_PARAMETER;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_SUBFRAME")
  {
    auto isSubframeEnabled = *(static_cast<bool const*>(settings));

    if(!UpdateIsEnabledSubframe(this->settings, isSubframeEnabled))
      return BAD_PARAMETER;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_RESOLUTION")
  {
    auto resolution = *(static_cast<Resolution const*>(settings));

    if(!UpdateResolution(this->settings, this->stride, this->strideAlignments, resolution))
      return BAD_PARAMETER;

    this->initialDisplayResolution.horizontal = resolution.dimension.horizontal;
    this->initialDisplayResolution.vertical = resolution.dimension.vertical;

    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_DECODED_PICTURE_BUFFER")
  {
    auto decodedPictureBuffer = *(static_cast<DecodedPictureBufferType const*>(settings));

    if(!UpdateDecodedPictureBuffer(this->settings, decodedPictureBuffer))
      return BAD_PARAMETER;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_LLP2_EARLY_CB")
  {
    this->settings.bUseEarlyCallback = *(static_cast<bool const*>(settings));
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_INPUT_PARSED")
  {
    this->settings.eInputMode = *(static_cast<bool const*>(settings)) ? AL_DEC_SPLIT_INPUT : AL_DEC_UNSPLIT_INPUT;
    return SUCCESS;
  }

  if(index == "SETTINGS_INDEX_OUTPUT_POSITION")
  {
    auto position = *(static_cast<Point<int> const*>(settings));

    if(!UpdateOutputPosition(this->settings, position))
      return BAD_PARAMETER;
    return SUCCESS;
  }

#if AL_ENABLE_MULTI_INSTANCE

  if(index == "SETTINGS_INDEX_INSTANCE_ID")
  {
    auto instance = *(static_cast<int const*>(settings));

    if(!UpdateInstanceId(this->settings, instance))
      return BAD_PARAMETER;
    return SUCCESS;
  }

#endif
  return BAD_INDEX;
}

bool DecSettingsJPEG::Check()
{
  // Fix: remove this line and below block when a better fix is found
  // This is a Gstreamer issue (not OMX) for allocation!
  int tmp_height = settings.tStream.tDim.iHeight;
  settings.tStream.tDim.iHeight = RoundUp(settings.tStream.tDim.iHeight, 16);

  if(AL_DecSettings_CheckValidity(&settings, stderr) != 0)
    return false;

  AL_DecSettings_CheckCoherency(&settings, stdout);

  // Fix: remove this line and below block when a better fix is found
  // This is a Gstreamer issue (not OMX) for allocation!
  settings.tStream.tDim.iHeight = tmp_height;

  return true;
}

// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "module/module_enums.h"
#include "module_structs.h"
#include <vector>

extern "C"
{
#include <lib_common_enc/Settings.h>
#include <lib_common/Allocator.h>
}

Clock CreateClock(AL_TEncSettings settings);
bool UpdateClock(AL_TEncSettings& settings, Clock clock);

Gop CreateGroupOfPictures(AL_TEncSettings settings);
bool UpdateGroupOfPictures(AL_TEncSettings& settings, Gop gop);

bool CreateConstrainedIntraPrediction(AL_TEncSettings settings);
bool UpdateConstrainedIntraPrediction(AL_TEncSettings& settings, bool isConstrainedIntraPredictionEnabled);

VideoModeType CreateVideoMode(AL_TEncSettings settings);
bool UpdateVideoMode(AL_TEncSettings& settings, VideoModeType videoMode);

Bitrate CreateBitrate(AL_TEncSettings settings);
bool UpdateBitrate(AL_TEncSettings& settings, Bitrate bitrate);

BufferSizes CreateBufferSizes(AL_TEncSettings settings, Stride stride);

bool CreateFillerData(AL_TEncSettings settings);
bool UpdateFillerData(AL_TEncSettings& settings, bool isFillerDataEnabled);

AspectRatioType CreateAspectRatio(AL_TEncSettings settings);
bool UpdateAspectRatio(AL_TEncSettings& settings, AspectRatioType aspectRatio);

ScalingListType CreateScalingList(AL_TEncSettings settings);
bool UpdateScalingList(AL_TEncSettings& settings, ScalingListType scalingList);

QPs CreateQuantizationParameter(AL_TEncSettings settings);
bool UpdateQuantizationParameter(AL_TEncSettings& settings, QPs qps);

Slices CreateSlicesParameter(AL_TEncSettings settings);
bool UpdateSlicesParameter(AL_TEncSettings& settings, Slices slices);

Format CreateFormat(AL_TEncSettings settings);
bool UpdateFormat(AL_TEncSettings& settings, Format format, std::vector<ColorType> colors, std::vector<int> bitdepths, std::vector<StorageType> storages, Stride& stride, StrideAlignments strideAlignments);

bool UpdateIsEnabledSubframe(AL_TEncSettings& settings, bool isSubframeEnabled);

Resolution CreateResolution(AL_TEncSettings settings, Stride stride);
bool UpdateResolution(AL_TEncSettings& settings, Stride& stride, StrideAlignments strideAlignments, Resolution resolution);

ColorPrimariesType CreateColorPrimaries(AL_TEncSettings settings);
bool UpdateColorPrimaries(AL_TEncSettings& settings, ColorPrimariesType colorPrimaries);

LookAhead CreateLookAhead(AL_TEncSettings settings);
bool UpdateLookAhead(AL_TEncSettings& settings, LookAhead la);

TwoPass CreateTwoPass(AL_TEncSettings settings, std::string sTwoPassLogFile);
bool UpdateTwoPass(AL_TEncSettings& settings, std::string& sTwoPassLogFile, TwoPass tp);

ColorPrimariesType CreateColorPrimaries(AL_TEncSettings settings);
bool UpdateColorPrimaries(AL_TEncSettings& settings, ColorPrimariesType colorPrimaries);

TransferCharacteristicsType CreateTransferCharacteristics(AL_TEncSettings settings);
bool UpdateTransferCharacteristics(AL_TEncSettings& settings, TransferCharacteristicsType transferCharacteristics);

ColourMatrixType CreateColourMatrix(AL_TEncSettings settings);
bool UpdateColourMatrix(AL_TEncSettings& settings, ColourMatrixType colourMatrix);

MaxPicturesSizes CreateMaxPictureSizes(AL_TEncSettings settings);
bool UpdateMaxPictureSizes(AL_TEncSettings& settings, MaxPicturesSizes sizes);

int CreateLoopFilterBeta(AL_TEncSettings settings);
bool UpdateLoopFilterBeta(AL_TEncSettings& settings, int beta);

int CreateLoopFilterTc(AL_TEncSettings settings);
bool UpdateLoopFilterTc(AL_TEncSettings& settings, int tc);

bool CreateAccessUnitDelimiter(AL_TEncSettings settings);
bool UpdateAccessUnitDelimiter(AL_TEncSettings& settings, bool isAUDEnabled);

bool CreateBufferingPeriodSEI(AL_TEncSettings settings);
bool UpdateBufferingPeriodSEI(AL_TEncSettings& settings, bool isBPEnabled);

bool CreatePictureTimingSEI(AL_TEncSettings settings);
bool UpdatePictureTimingSEI(AL_TEncSettings& settings, bool isPTEnabled);

bool CreateRecoveryPointSEI(AL_TEncSettings settings);
bool UpdateRecoveryPointSEI(AL_TEncSettings& settings, bool isRPEnabled);

bool CreateMasteringDisplayColourVolumeSEI(AL_TEncSettings settings);
bool UpdateMasteringDisplayColourVolumeSEI(AL_TEncSettings& settings, bool isMDCVEnabled);

bool CreateContentLightLevelSEI(AL_TEncSettings settings);
bool UpdateContentLightLevelSEI(AL_TEncSettings& settings, bool isCLLEnabled);

bool CreateAlternativeTransferCharacteristicsSEI(AL_TEncSettings settings);
bool UpdateAlternativeTransferCharacteristicsSEI(AL_TEncSettings& settings, bool isATCEnabled);

bool CreateST209410SEI(AL_TEncSettings settings);
bool UpdateST209410SEI(AL_TEncSettings& settings, bool isST209410Enabled);

bool CreateST209440SEI(AL_TEncSettings settings);
bool UpdateST209440SEI(AL_TEncSettings& settings, bool isST209440Enabled);

bool CreateVideoFullRange(AL_TEncSettings settings);
bool UpdateVideoFullRange(AL_TEncSettings& settings, bool isVideoFullRangeEnabled);

bool CreateRealtime(AL_TEncSettings settings);
bool UpdateRealtime(AL_TEncSettings& settings, bool isRealtimeDisabled);

RateControlPlugin CreateRateControlPlugin(AL_TAllocator* allocator, AL_TEncSettings const& settings);
bool SetRcPluginContext(AL_TAllocator* allocator, AL_TEncSettings* settings, RateControlPlugin const& rcp);
void ResetRcPluginContext(AL_TAllocator* allocator, AL_TEncSettings* settings);

Region CreateOutputCrop(AL_TEncSettings settings);
bool UpdateOutputCrop(AL_TEncSettings& settings, Region region);

Region CreateInputCrop(AL_TEncSettings settings);
bool UpdateInputCrop(AL_TEncSettings& settings, Region region);

MaxPicturesSizes CreateMaxPictureSizesInBits(AL_TEncSettings settings);
bool UpdateMaxPictureSizesInBits(AL_TEncSettings& settings, MaxPicturesSizes sizes);

bool CreateUniformSliceType(AL_TEncSettings settings);
bool UpdateUniformeSliceType(AL_TEncSettings& settings, bool isUniformSliceTypeEnable);

MinMax<int> CreateLog2CodingUnit(AL_TEncSettings settings);
bool UpdateLog2CodingUnit(AL_TEncSettings& settings, MinMax<int> log2CodingUnit);

StartCodeBytesAlignmentType CreateStartCodeBytesAlignment(AL_TEncSettings settings);
bool UpdateStartCodeBytesAlignment(AL_TEncSettings& settings, StartCodeBytesAlignmentType startCodeBytesAlignment);
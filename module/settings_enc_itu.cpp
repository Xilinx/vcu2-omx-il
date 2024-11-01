// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include "settings_enc_itu.h"
#include "module/module_enums.h"
#include "settings_checks.h"
#include "convert_module_soft.h"
#include "convert_module_soft_enc.h"
#include <utility/round.h>
#include <cassert>

extern "C"
{
#include <lib_common_enc/EncBuffers.h>
#include <lib_common/PicFormat.h>
#include <lib_common/StreamBuffer.h>
#include <lib_common/SEI.h>
#include <lib_common_enc/EncChanParam.h>
#include <lib_fpga/DmaAllocLinux.h>
#include <lib_common_enc/IpEncFourCC.h>
}

using namespace std;

Clock CreateClock(AL_TEncSettings const& settings)
{
  Clock clock;
  auto rateControl = settings.tChParam[0].tRCParam;
  clock.framerate = rateControl.uFrameRate;
  clock.clockratio = rateControl.uClkRatio;
  return clock;
}

bool UpdateClock(AL_TEncSettings& settings, Clock clock)
{
  if(!CheckClock(clock))
    return false;

  if(clock.framerate == 0)
    return false;

  auto& rateControl = settings.tChParam[0].tRCParam;

  rateControl.uFrameRate = clock.framerate;
  rateControl.uClkRatio = clock.clockratio;

  return true;
}

Gop CreateGroupOfPictures(AL_TEncSettings const& settings)
{
  Gop gop;
  auto gopParam = settings.tChParam[0].tGopParam;

  gop.b = gopParam.uNumB;
  gop.length = gopParam.uGopLength;
  gop.idrFrequency = gopParam.uFreqIDR;
  gop.rpFrequency = gopParam.uFreqRP;
  gop.mode = ConvertSoftToModuleGopControl(gopParam.eMode);
  gop.gdr = ConvertSoftToModuleGdr(gopParam.eGdrMode);
  gop.isLongTermEnabled = gopParam.bEnableLT;
  gop.ltFrequency = gopParam.uFreqLT;

  return gop;
}

static bool isGDREnabled(Gop gop)
{
  return gop.gdr == GdrType::GDR_VERTICAL || gop.gdr == GdrType::GDR_HORTIZONTAL;
}

bool UpdateGroupOfPictures(AL_TEncSettings& settings, Gop gop)
{
  if(!CheckGroupOfPictures(gop))
    return false;

  auto& gopParam = settings.tChParam[0].tGopParam;
  gopParam.uNumB = gop.b;
  gopParam.uGopLength = gop.length;
  gopParam.uFreqIDR = gop.idrFrequency;
  gopParam.uFreqRP = gop.rpFrequency;
  gopParam.eMode = ConvertModuleToSoftGopControl(gop.mode);
  gopParam.eGdrMode = ConvertModuleToSoftGdr(gop.gdr);

  if(isGDREnabled(gop))
    settings.uEnableSEI |= AL_SEI_RP;
  else
    settings.uEnableSEI &= ~AL_SEI_RP;

  gopParam.bEnableLT = gop.isLongTermEnabled;
  gopParam.uFreqLT = gop.ltFrequency;

  return true;
}

bool CreateConstrainedIntraPrediction(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  return channel.eEncTools & AL_OPT_CONST_INTRA_PRED;
}

bool UpdateConstrainedIntraPrediction(AL_TEncSettings& settings, bool isConstrainedIntraPredictionEnabled)
{
  if(!isConstrainedIntraPredictionEnabled)
    return true;

  auto& opt = settings.tChParam[0].eEncTools;
  opt = static_cast<AL_EChEncTool>(opt | AL_OPT_CONST_INTRA_PRED);

  return true;
}

VideoModeType CreateVideoMode(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  return ConvertSoftToModuleVideoMode(channel.eVideoMode);
}

bool UpdateVideoMode(AL_TEncSettings& settings, VideoModeType videoMode)
{
  if(!CheckVideoMode(videoMode))
    return false;

  auto& channel = settings.tChParam[0];
  channel.eVideoMode = ConvertModuleToSoftVideoMode(videoMode);
  return true;
}

Bitrate CreateBitrate(AL_TEncSettings const& settings)
{
  Bitrate bitrate {};
  auto rateControl = settings.tChParam[0].tRCParam;

  bitrate.target = rateControl.uTargetBitRate / 1000;
  bitrate.max = rateControl.uMaxBitRate / 1000;
  bitrate.cpb = rateControl.uCPBSize / 90;
  bitrate.ird = rateControl.uInitialRemDelay / 90;
  bitrate.quality = (rateControl.uMaxPSNR / 100) - 28;
  bitrate.maxConsecutiveSkipFrame = rateControl.uMaxConsecSkip;
  bitrate.rateControl.mode = ConvertSoftToModuleRateControl(rateControl.eRCMode);
  bitrate.rateControl.options = ConvertSoftToModuleRateControlOption(rateControl.eOptions);
  return bitrate;
}

bool UpdateBitrate(AL_TEncSettings& settings, Bitrate bitrate)
{
  auto clock = CreateClock(settings);

  if(!CheckBitrate(bitrate, clock))
    return false;

  auto& rateControl = settings.tChParam[0].tRCParam;

  rateControl.uTargetBitRate = bitrate.target * 1000;
  rateControl.uMaxBitRate = bitrate.max * 1000;
  rateControl.uCPBSize = bitrate.cpb * 90;
  rateControl.uInitialRemDelay = bitrate.ird * 90;
  rateControl.uMaxPSNR = (bitrate.quality + 28) * 100;
  rateControl.uMaxConsecSkip = bitrate.maxConsecutiveSkipFrame;
  rateControl.eRCMode = ConvertModuleToSoftRateControl(bitrate.rateControl.mode);
  rateControl.eOptions = ConvertModuleToSoftRateControlOption(bitrate.rateControl.options);
  return true;
}

static int RawAllocationSize(Stride stride, AL_EChromaMode eChromaMode, AL_ESrcMode eSrcMode)
{
  auto IP_WIDTH_ALIGNMENT = 32;
  auto IP_HEIGHT_ALIGNMENT = 8;
  assert(stride.horizontal % IP_WIDTH_ALIGNMENT == 0); // IP requirements
  assert(stride.vertical % IP_HEIGHT_ALIGNMENT == 0); // IP requirements

  AL_TPicFormat tPicFormat = GetDefaultPicFormat();
  tPicFormat.eChromaMode = eChromaMode;
  tPicFormat.eStorageMode = ConvertSoftSrcToSoftStorage(eSrcMode);
  tPicFormat.ePlaneMode = GetInternalBufPlaneMode(eChromaMode);

  auto const lumaSize = AL_GetAllocSizeSrc_PixPlane(&tPicFormat, stride.horizontal, stride.vertical, AL_PLANE_Y);

  if(eChromaMode == AL_CHROMA_MONO)
    return lumaSize;

  auto const chromaSize = eChromaMode == AL_CHROMA_4_4_4 ?
                          AL_GetAllocSizeSrc_PixPlane(&tPicFormat, stride.horizontal, stride.vertical, AL_PLANE_U) +
                          AL_GetAllocSizeSrc_PixPlane(&tPicFormat, stride.horizontal, stride.vertical, AL_PLANE_V) :
                          AL_GetAllocSizeSrc_PixPlane(&tPicFormat, stride.horizontal, stride.vertical, AL_PLANE_UV);

  auto const size = lumaSize + chromaSize;
  return size;
}

BufferSizes CreateBufferSizes(AL_TEncSettings const& settings, Stride stride)
{
  BufferSizes bufferSizes {};
  auto channel = settings.tChParam[0];
  bufferSizes.input = RawAllocationSize(stride, AL_GET_CHROMA_MODE(channel.ePicFormat), channel.eSrcMode);
  bufferSizes.output = AL_GetMitigatedMaxNalSize({ channel.uEncWidth, channel.uEncHeight }, AL_GET_CHROMA_MODE(channel.ePicFormat), AL_GET_BITDEPTH(channel.ePicFormat));

  bool bIsXAVCIntraCBG = AL_IS_XAVC_CBG(channel.eProfile) && AL_IS_INTRA_PROFILE(channel.eProfile);

  if(bIsXAVCIntraCBG)
    bufferSizes.output = AL_GetMaxNalSize({ channel.uEncWidth, channel.uEncHeight }, AL_GET_CHROMA_MODE(channel.ePicFormat), AL_GET_BITDEPTH(channel.ePicFormat), channel.eProfile, channel.uLevel);

  if(channel.bSubframeLatency)
  {
    /* Due to rounding, the slices don't have all the same height. Compute size of the biggest slice */
    size_t lcuSize = 1 << channel.uLog2MaxCuSize;
    size_t rndHeight = RoundUp(static_cast<size_t>(channel.uEncHeight), lcuSize);
    size_t outputSize = bufferSizes.output * lcuSize * (1 + rndHeight / (channel.uNumSlices * lcuSize)) / rndHeight;
    /* we need space for the headers on each slice */
    outputSize += (AL_ENC_MAX_HEADER_SIZE * channel.uNumSlices);
    /* stream size is required to be 32 bits aligned */
    size_t IP_WIDTH_ALIGNMENT = 32;
    outputSize = RoundUp(outputSize, IP_WIDTH_ALIGNMENT);

    assert(outputSize <= static_cast<size_t>((1 << ((8 * sizeof(bufferSizes.output)) - 1))));
    bufferSizes.output = outputSize;
  }

  assert(bufferSizes.output >= 0);
  return bufferSizes;
}

bool CreateFillerData(AL_TEncSettings const& settings)
{
  return settings.eEnableFillerData != AL_FILLER_DISABLE;
}

bool UpdateFillerData(AL_TEncSettings& settings, bool isFillerDataEnabled)
{
  settings.eEnableFillerData = isFillerDataEnabled ? AL_FILLER_APP : AL_FILLER_DISABLE;
  return true;
}

AspectRatioType CreateAspectRatio(AL_TEncSettings const& settings)
{
  return ConvertSoftToModuleAspectRatio(settings.eAspectRatio);
}

bool UpdateAspectRatio(AL_TEncSettings& settings, AspectRatioType aspectRatio)
{
  if(!CheckAspectRatio(aspectRatio))
    return false;

  settings.eAspectRatio = ConvertModuleToSoftAspectRatio(aspectRatio);
  return true;
}

ScalingListType CreateScalingList(AL_TEncSettings const& settings)
{
  return ConvertSoftToModuleScalingList(settings.eScalingList);
}

bool UpdateScalingList(AL_TEncSettings& settings, ScalingListType scalingList)
{
  if(!CheckScalingList(scalingList))
    return false;

  settings.eScalingList = ConvertModuleToSoftScalingList(scalingList);
  return true;
}

QPs CreateQuantizationParameter(AL_TEncSettings const& settings)
{
  QPs qps;
  qps.mode.ctrl = ConvertSoftToModuleQPControl(settings.eQpCtrlMode);
  qps.mode.table = ConvertSoftToModuleQPTable(settings.eQpTableMode);
  auto rateControl = settings.tChParam[0].tRCParam;
  qps.initial = rateControl.iInitialQP;
  qps.deltaIP = rateControl.uIPDelta;
  qps.deltaPB = rateControl.uPBDelta;

  for(int frame_type = 0; frame_type < QPs::MAX_FRAME_TYPE; frame_type++)
  {
    assert(QPs::MAX_FRAME_TYPE <= AL_MAX_FRAME_TYPE);
    qps.range[frame_type].min = rateControl.iMinQP[frame_type];
    qps.range[frame_type].max = rateControl.iMaxQP[frame_type];
  }

  return qps;
}

bool UpdateQuantizationParameter(AL_TEncSettings& settings, QPs qps)
{
  if(qps.deltaIP < 0)
    qps.deltaIP = -1;

  if(qps.deltaPB < 0)
    qps.deltaPB = -1;

  if(!CheckQuantizationParameter(qps))
    return false;

  settings.eQpCtrlMode = ConvertModuleToSoftQPControl(qps.mode.ctrl);
  settings.eQpTableMode = ConvertModuleToSoftQPTable(qps.mode.table);
  auto& rateControl = settings.tChParam[0].tRCParam;
  rateControl.iInitialQP = qps.initial;
  rateControl.uIPDelta = qps.deltaIP;
  rateControl.uPBDelta = qps.deltaPB;

  for(int frame_type = 0; frame_type < AL_MAX_FRAME_TYPE; frame_type++)
  {
    assert(AL_MAX_FRAME_TYPE <= QPs::MAX_FRAME_TYPE);
    rateControl.iMinQP[frame_type] = qps.range[frame_type].min;
    rateControl.iMaxQP[frame_type] = qps.range[frame_type].max;
  }

  return true;
}

Slices CreateSlicesParameter(AL_TEncSettings const& settings)
{
  Slices slices;
  slices.dependent = settings.bDependentSlice;
  auto channel = settings.tChParam[0];
  slices.num = channel.uNumSlices;
  slices.size = channel.uSliceSize;
  return slices;
}

bool UpdateSlicesParameter(AL_TEncSettings& settings, Slices slices)
{
  if(!CheckSlicesParameter(slices))
    return false;

  settings.bDependentSlice = slices.dependent;
  auto& channel = settings.tChParam[0];
  channel.uNumSlices = slices.num;
  channel.uSliceSize = slices.size;

  return true;
}

Format CreateFormat(AL_TEncSettings const& settings)
{
  Format format;
  auto channel = settings.tChParam[0];
  AL_EChromaMode eChromaMode = AL_GET_CHROMA_MODE(channel.ePicFormat);
  format.color = ConvertSoftToModuleColor(eChromaMode);
  format.bitdepth = AL_GET_BITDEPTH(channel.ePicFormat);
  format.storage = ConvertSoftToModuleSrcStorage(channel.eSrcMode);
  return format;
}

bool UpdateFormat(AL_TEncSettings& settings, Format format, vector<ColorType> colors, vector<int> bitdepths, vector<StorageType> storages, Stride& stride, StrideAlignments strideAlignments)
{
  if(!CheckFormat(format, colors, bitdepths, storages))
    return false;

  auto& channel = settings.tChParam[0];
  AL_SET_CHROMA_MODE(&channel.ePicFormat, ConvertModuleToSoftChroma(format.color));
  AL_SET_BITDEPTH(&channel.ePicFormat, format.bitdepth);
  channel.uSrcBitDepth = AL_GET_BITDEPTH(channel.ePicFormat);
  channel.eSrcMode = ConvertModuleToSoftSrcStorage(format.storage);

  AL_TPicFormat const tPicFormat = AL_EncGetSrcPicFormat(AL_GET_CHROMA_MODE(channel.ePicFormat), AL_GET_BITDEPTH(channel.ePicFormat), channel.eSrcMode);
  int minStride = static_cast<int>(RoundUp(AL_EncGetMinPitch(channel.uEncWidth, &tPicFormat), strideAlignments.horizontal));
  stride.horizontal = max(minStride, stride.horizontal);

  return true;
}

Resolution CreateResolution(AL_TEncSettings const& settings, Stride stride)
{
  auto channel = settings.tChParam[0];
  AL_TPicFormat const tPicFormat = AL_EncGetSrcPicFormat(AL_GET_CHROMA_MODE(channel.ePicFormat), AL_GET_BITDEPTH(channel.ePicFormat), channel.eSrcMode);

  Resolution resolution;
  resolution.dimension.horizontal = channel.uEncWidth;
  resolution.dimension.vertical = channel.uEncHeight;
  resolution.stride.horizontal = stride.horizontal;
  resolution.stride.vertical = stride.vertical;

  if(tPicFormat.eStorageMode == AL_FB_TILE_32x4 || tPicFormat.eStorageMode == AL_FB_TILE_64x4)
    resolution.stride.vertical /= 4;

  return resolution;
}

bool UpdateIsEnabledSubframe(AL_TEncSettings& settings, bool isSubframeEnabled)
{
  settings.tChParam[0].bSubframeLatency = isSubframeEnabled;
  return true;
}

bool UpdateResolution(AL_TEncSettings& settings, Stride& stride, StrideAlignments strideAlignments, Resolution resolution)
{
  auto& channel = settings.tChParam[0];
  channel.uEncWidth = resolution.dimension.horizontal;
  channel.uEncHeight = resolution.dimension.vertical;
  channel.uSrcWidth = channel.uEncWidth;
  channel.uSrcHeight = channel.uEncHeight;

  AL_TPicFormat const tPicFormat = AL_EncGetSrcPicFormat(AL_GET_CHROMA_MODE(channel.ePicFormat), AL_GET_BITDEPTH(channel.ePicFormat), channel.eSrcMode);

  int minStride = RoundUp(AL_EncGetMinPitch(channel.uEncWidth, &tPicFormat), strideAlignments.horizontal);
  stride.horizontal = max(minStride, static_cast<int>(RoundUp(resolution.stride.horizontal, strideAlignments.horizontal)));

  int minSliceHeight = RoundUp(static_cast<int>(channel.uEncHeight), strideAlignments.vertical);
  stride.vertical = max(minSliceHeight, static_cast<int>(RoundUp(resolution.stride.vertical, strideAlignments.vertical)));

  return true;
}

ColorPrimariesType CreateColorPrimaries(AL_TEncSettings const& settings)
{
  return ConvertSoftToModuleColorPrimaries(settings.tColorConfig.eColourDescription);
}

bool UpdateColorPrimaries(AL_TEncSettings& settings, ColorPrimariesType colorPrimaries)
{
  if(!CheckColorPrimaries(colorPrimaries))
    return false;

  settings.tColorConfig.eColourDescription = ConvertModuleToSoftColorPrimaries(colorPrimaries);
  return true;
}

TransferCharacteristicsType CreateTransferCharacteristics(AL_TEncSettings const& settings)
{
  return ConvertSoftToModuleTransferCharacteristics(settings.tColorConfig.eTransferCharacteristics);
}

bool UpdateTransferCharacteristics(AL_TEncSettings& settings, TransferCharacteristicsType transferCharacteristics)
{
  if(!CheckTransferCharacteristics(transferCharacteristics))
    return false;

  settings.tColorConfig.eTransferCharacteristics = ConvertModuleToSoftTransferCharacteristics(transferCharacteristics);
  return true;
}

ColourMatrixType CreateColourMatrix(AL_TEncSettings const& settings)
{
  return ConvertSoftToModuleColourMatrix(settings.tColorConfig.eColourMatrixCoeffs);
}

bool UpdateColourMatrix(AL_TEncSettings& settings, ColourMatrixType colourMatrix)
{
  if(!CheckColourMatrix(colourMatrix))
    return false;

  settings.tColorConfig.eColourMatrixCoeffs = ConvertModuleToSoftColourMatrix(colourMatrix);
  return true;
}

LookAhead CreateLookAhead(AL_TEncSettings const& settings)
{
  LookAhead la {};
  la.lookAhead = settings.LookAhead;
  la.isFirstPassSceneChangeDetectionEnabled = settings.bEnableFirstPassSceneChangeDetection;
  return la;
}

bool UpdateLookAhead(AL_TEncSettings& settings, LookAhead la)
{
  if(!CheckLookAhead(la))
    return false;

  settings.LookAhead = la.lookAhead;
  settings.bEnableFirstPassSceneChangeDetection = la.isFirstPassSceneChangeDetectionEnabled;

  return true;
}

TwoPass CreateTwoPass(AL_TEncSettings const& settings, string sTwoPassLogFile)
{
  TwoPass tp;
  tp.nPass = settings.TwoPass;
  tp.sLogFile = sTwoPassLogFile;
  return tp;
}

bool UpdateTwoPass(AL_TEncSettings& settings, string& sTwoPassLogFile, TwoPass tp)
{
  if(!CheckTwoPass(tp))
    return false;

  settings.TwoPass = tp.nPass;
  sTwoPassLogFile = tp.sLogFile;

  return true;
}

MaxPicturesSizes CreateMaxPictureSizes(AL_TEncSettings const& settings)
{
  auto rateControl = settings.tChParam[0].tRCParam;
  MaxPicturesSizes sizes;
  sizes.i = static_cast<int>(rateControl.pMaxPictureSize[AL_SLICE_I] / 1000);
  sizes.p = static_cast<int>(rateControl.pMaxPictureSize[AL_SLICE_P] / 1000);
  sizes.b = static_cast<int>(rateControl.pMaxPictureSize[AL_SLICE_B] / 1000);
  return sizes;
}

bool UpdateMaxPictureSizes(AL_TEncSettings& settings, MaxPicturesSizes sizes)
{
  if(!CheckMaxPictureSizes(sizes))
    return false;

  auto& rateControl = settings.tChParam[0].tRCParam;
  rateControl.pMaxPictureSize[AL_SLICE_I] = sizes.i * 1000;
  rateControl.pMaxPictureSize[AL_SLICE_P] = sizes.p * 1000;
  rateControl.pMaxPictureSize[AL_SLICE_B] = sizes.b * 1000;

  return true;
}

MaxPicturesSizes CreateMaxPictureSizesInBits(AL_TEncSettings const& settings)
{
  auto rateControl = settings.tChParam[0].tRCParam;
  MaxPicturesSizes sizes;
  sizes.i = static_cast<int>(rateControl.pMaxPictureSize[AL_SLICE_I]);
  sizes.p = static_cast<int>(rateControl.pMaxPictureSize[AL_SLICE_P]);
  sizes.b = static_cast<int>(rateControl.pMaxPictureSize[AL_SLICE_B]);
  return sizes;
}

bool UpdateMaxPictureSizesInBits(AL_TEncSettings& settings, MaxPicturesSizes sizes)
{
  if(!CheckMaxPictureSizes(sizes))
    return false;

  auto& rateControl = settings.tChParam[0].tRCParam;
  rateControl.pMaxPictureSize[AL_SLICE_I] = sizes.i;
  rateControl.pMaxPictureSize[AL_SLICE_P] = sizes.p;
  rateControl.pMaxPictureSize[AL_SLICE_B] = sizes.b;

  return true;
}

int CreateLoopFilterBeta(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  return channel.iBetaOffset;
}

bool UpdateLoopFilterBeta(AL_TEncSettings& settings, int beta)
{
  if(!CheckLoopFilterBeta(beta))
    return false;

  auto& channel = settings.tChParam[0];
  channel.iBetaOffset = beta;

  return true;
}

int CreateLoopFilterTc(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  return channel.iTcOffset;
}

bool UpdateLoopFilterTc(AL_TEncSettings& settings, int tc)
{
  if(!CheckLoopFilterTc(tc))
    return false;

  auto& channel = settings.tChParam[0];
  channel.iTcOffset = tc;

  return true;
}

bool CreateAccessUnitDelimiter(AL_TEncSettings const& settings)
{
  return settings.bEnableAUD;
}

bool UpdateAccessUnitDelimiter(AL_TEncSettings& settings, bool isAUDEnabled)
{
  settings.bEnableAUD = isAUDEnabled;
  return true;
}

bool CreateInputSynchronization(AL_TEncSettings const& settings)
{
  auto& channel = settings.tChParam[0];
  return channel.bSrcSync;
}

bool UpdateInputSynchronization(AL_TEncSettings& settings, bool isSrcSyncEnabled)
{
  auto& channel = settings.tChParam[0];
  channel.bSrcSync = isSrcSyncEnabled;
  return true;
}

bool CreateBufferingPeriodSEI(AL_TEncSettings const& settings)
{
  return (settings.uEnableSEI & AL_SEI_BP) != 0;
}

bool UpdateBufferingPeriodSEI(AL_TEncSettings& settings, bool isBPEnabled)
{
  isBPEnabled ? settings.uEnableSEI |= AL_SEI_BP : settings.uEnableSEI &= ~AL_SEI_BP;
  return true;
}

bool CreatePictureTimingSEI(AL_TEncSettings const& settings)
{
  return (settings.uEnableSEI & AL_SEI_PT) != 0;
}

bool UpdatePictureTimingSEI(AL_TEncSettings& settings, bool isPTEnabled)
{
  isPTEnabled ? settings.uEnableSEI |= AL_SEI_PT : settings.uEnableSEI &= ~AL_SEI_PT;
  return true;
}

bool CreateRecoveryPointSEI(AL_TEncSettings const& settings)
{
  return (settings.uEnableSEI & AL_SEI_RP) != 0;
}

bool UpdateRecoveryPointSEI(AL_TEncSettings& settings, bool isRPEnabled)
{
  isRPEnabled ? settings.uEnableSEI |= AL_SEI_RP : settings.uEnableSEI &= ~AL_SEI_RP;
  return true;
}

bool CreateMasteringDisplayColourVolumeSEI(AL_TEncSettings const& settings)
{
  return (settings.uEnableSEI & AL_SEI_MDCV) != 0;
}

bool UpdateMasteringDisplayColourVolumeSEI(AL_TEncSettings& settings, bool isMDCVEnabled)
{
  isMDCVEnabled ? settings.uEnableSEI |= AL_SEI_MDCV : settings.uEnableSEI &= ~AL_SEI_MDCV;
  return true;
}

bool CreateContentLightLevelSEI(AL_TEncSettings const& settings)
{
  return (settings.uEnableSEI & AL_SEI_CLL) != 0;
}

bool UpdateContentLightLevelSEI(AL_TEncSettings& settings, bool isCLLEnabled)
{
  isCLLEnabled ? settings.uEnableSEI |= AL_SEI_CLL : settings.uEnableSEI &= ~AL_SEI_CLL;
  return true;
}

bool CreateAlternativeTransferCharacteristicsSEI(AL_TEncSettings const& settings)
{
  return (settings.uEnableSEI & AL_SEI_ATC) != 0;
}

bool UpdateAlternativeTransferCharacteristicsSEI(AL_TEncSettings& settings, bool isATCEnabled)
{
  isATCEnabled ? settings.uEnableSEI |= AL_SEI_ATC : settings.uEnableSEI &= ~AL_SEI_ATC;
  return true;
}

bool CreateST209410SEI(AL_TEncSettings const& settings)
{
  return (settings.uEnableSEI & AL_SEI_ST2094_10) != 0;
}

bool UpdateST209410SEI(AL_TEncSettings& settings, bool isST209410Enabled)
{
  isST209410Enabled ? settings.uEnableSEI |= AL_SEI_ST2094_10 : settings.uEnableSEI &= ~AL_SEI_ST2094_10;
  return true;
}

bool CreateST209440SEI(AL_TEncSettings const& settings)
{
  return (settings.uEnableSEI & AL_SEI_ST2094_40) != 0;
}

bool UpdateST209440SEI(AL_TEncSettings& settings, bool isST209440Enabled)
{
  isST209440Enabled ? settings.uEnableSEI |= AL_SEI_ST2094_40 : settings.uEnableSEI &= ~AL_SEI_ST2094_40;
  return true;
}

bool CreateVideoFullRange(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  return channel.bVideoFullRange;
}

bool UpdateVideoFullRange(AL_TEncSettings& settings, bool isVideoFullRangeEnabled)
{
  auto& channel = settings.tChParam[0];
  channel.bVideoFullRange = isVideoFullRangeEnabled;

  return true;
}

RateControlPlugin CreateRateControlPlugin(AL_TAllocator* allocator, AL_TEncSettings const& settings)
{
  RateControlPlugin rcp;
  AL_TLinuxDmaAllocator* linuxAllocator = (AL_TLinuxDmaAllocator*)allocator;
  rcp.dmaBuf = AL_LinuxDmaAllocator_GetFd(linuxAllocator, settings.hRcPluginDmaContext);
  rcp.dmaSize = settings.tChParam[0].zRcPluginDmaSize;
  return rcp;
}

bool SetRcPluginContext(AL_TAllocator* allocator, AL_TEncSettings* settings, RateControlPlugin const& rcp)
{
  AL_TLinuxDmaAllocator* linuxAllocator = (AL_TLinuxDmaAllocator*)allocator;
  AL_HANDLE hRcPluginDmaContext = NULL;

  if(rcp.dmaBuf != -1)
  {
    hRcPluginDmaContext = AL_LinuxDmaAllocator_ImportFromFd(linuxAllocator, rcp.dmaBuf);

    if(hRcPluginDmaContext == NULL)
      return false;
  }

  if(settings->hRcPluginDmaContext)
    AL_Allocator_Free(allocator, settings->hRcPluginDmaContext);

  settings->hRcPluginDmaContext = hRcPluginDmaContext;
  settings->tChParam[0].pRcPluginDmaContext = 0;
  settings->tChParam[0].zRcPluginDmaSize = rcp.dmaSize;

  return true;
}

void ResetRcPluginContext(AL_TAllocator* allocator, AL_TEncSettings* settings)
{
  RateControlPlugin rcp;
  rcp.dmaBuf = -1;
  rcp.dmaSize = 0;
  SetRcPluginContext(allocator, settings, rcp);
}

Region CreateOutputCrop(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  Region region;
  region.point.x = channel.uOutputCropPosX;
  region.point.y = channel.uOutputCropPosY;
  region.dimension.horizontal = channel.uOutputCropWidth;
  region.dimension.vertical = channel.uOutputCropHeight;
  return region;
}

bool UpdateOutputCrop(AL_TEncSettings& settings, Region region)
{
  if(!CheckCrop(region))
    return false;

  auto& channel = settings.tChParam[0];
  channel.uOutputCropPosX = region.point.x;
  channel.uOutputCropPosY = region.point.y;
  channel.uOutputCropWidth = region.dimension.horizontal;
  channel.uOutputCropHeight = region.dimension.vertical;

  return true;
}

Region CreateInputCrop(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  Region region;
  region.point.x = channel.uSrcCropPosX;
  region.point.y = channel.uSrcCropPosY;
  region.dimension.horizontal = channel.uSrcCropWidth;
  region.dimension.vertical = channel.uSrcCropHeight;
  return region;
}

bool UpdateInputCrop(AL_TEncSettings& settings, Region region)
{
  if(!CheckCrop(region))
    return false;

  auto& channel = settings.tChParam[0];
  channel.uSrcCropPosX = region.point.x;
  channel.uSrcCropPosY = region.point.y;
  channel.uSrcCropWidth = region.dimension.horizontal;
  channel.uSrcCropHeight = region.dimension.vertical;
  channel.bEnableSrcCrop = (region.dimension.horizontal > 0) || (region.dimension.vertical > 0);

  return true;
}

bool CreateUniformSliceType(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  return channel.bUseUniformSliceType;
}

bool UpdateUniformeSliceType(AL_TEncSettings& settings, bool isUniformSliceTypeEnable)
{
  auto& channel = settings.tChParam[0];
  channel.bUseUniformSliceType = isUniformSliceTypeEnable;
  return true;
}

MinMax<int> CreateLog2CodingUnit(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  MinMax<int> log2CodingUnit;
  log2CodingUnit.min = channel.uLog2MinCuSize;
  log2CodingUnit.max = channel.uLog2MaxCuSize;
  return log2CodingUnit;
}

bool UpdateLog2CodingUnit(AL_TEncSettings& settings, MinMax<int> log2CodingUnit)
{
  if(!CheckLog2CodingUnit(log2CodingUnit))
    return false;

  auto& channel = settings.tChParam[0];
  channel.uLog2MinCuSize = log2CodingUnit.min;
  channel.uLog2MaxCuSize = log2CodingUnit.max;
  return true;
}

StartCodeBytesAlignmentType CreateStartCodeBytesAlignment(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  return ConvertSoftToModuleStartCodeBytesAlignment(channel.eStartCodeBytesAligned);
}

bool UpdateStartCodeBytesAlignment(AL_TEncSettings& settings, StartCodeBytesAlignmentType startCodeBytesAlignment)
{
  if(!CheckStartCodeBytesAlignment(startCodeBytesAlignment))
    return false;

  auto& channel = settings.tChParam[0];
  channel.eStartCodeBytesAligned = ConvertModuleToSoftStartCodeBytesAlignment(startCodeBytesAlignment);
  return true;
}

bool CreateRealtime(AL_TEncSettings const& settings)
{
  auto channel = settings.tChParam[0];
  return channel.bNonRealtime;
}

bool UpdateRealtime(AL_TEncSettings& settings, bool isRealtimeDisabled)
{
  auto& channel = settings.tChParam[0];
  channel.bNonRealtime = isRealtimeDisabled;
  return true;
}

// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include "CommandsSender.h"

#include <cassert>
#include <cmath>
#include "../common/helpers.h"

extern "C"
{
#include <OMX_Core.h>
#include <OMX_Video.h>
#include <OMX_VideoAlg.h>
#include <OMX_Component.h>
#include <OMX_Index.h>
#include <OMX_IndexAlg.h>
}

CommandsSender::CommandsSender(OMX_HANDLETYPE hEnc) :
  hEnc(hEnc)
{
}

CommandsSender::~CommandsSender() = default;
void CommandsSender::notifySceneChange(int lookAhead)
{
  OMX_ALG_VIDEO_CONFIG_NOTIFY_SCENE_CHANGE notifySceneChange;
  InitHeader(notifySceneChange);
  notifySceneChange.nPortIndex = 1;
  notifySceneChange.nLookAhead = lookAhead;
  auto error = OMX_SetConfig(hEnc, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexConfigVideoNotifySceneChange), &notifySceneChange);
  assert(error == OMX_ErrorNone);
}

void CommandsSender::notifyIsLongTerm()
{
  OMX_ALG_VIDEO_CONFIG_INSERT config;
  InitHeader(config);
  config.nPortIndex = 1;
  auto error = OMX_SetConfig(hEnc, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexConfigVideoInsertLongTerm), &config);
  assert(error == OMX_ErrorNone);
}

void CommandsSender::notifyUseLongTerm()
{
  OMX_ALG_VIDEO_CONFIG_INSERT config;
  InitHeader(config);
  config.nPortIndex = 1;
  auto error = OMX_SetConfig(hEnc, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexConfigVideoUseLongTerm), &config);
  assert(error == OMX_ErrorNone);
}

void CommandsSender::notifyIsSkip()
{
  assert(0 && "notifyIsSkip not implemented");
}

void CommandsSender::setSAO(bool bSAOEnabled)
{
  (void)bSAOEnabled;
  assert(0 && "setSAO is not supported");
}

void CommandsSender::restartGop()
{
  OMX_ALG_VIDEO_CONFIG_INSERT config;
  InitHeader(config);
  config.nPortIndex = 1;
  auto error = OMX_SetConfig(hEnc, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexConfigVideoInsertInstantaneousDecodingRefresh), &config);
  assert(error == OMX_ErrorNone);
}

void CommandsSender::restartGopRecoveryPoint()
{
  assert(0 && "restartGopRecoveryPoint is not supported");
}

void CommandsSender::setGopLength(int gopLength)
{
  OMX_ALG_VIDEO_CONFIG_GROUP_OF_PICTURES gop;
  InitHeader(gop);
  gop.nPortIndex = 1;
  OMX_GetConfig(hEnc, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexConfigVideoGroupOfPictures), &gop);
  int numB = gop.nBFrames / (gop.nPFrames + 1);

  if(gopLength <= numB)
    assert(0);

  gop.nBFrames = (numB * gopLength) / (1 + numB);
  gop.nPFrames = (numB - gopLength + 1) / (-numB - 1);

  auto const error = OMX_SetConfig(hEnc, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexConfigVideoGroupOfPictures), &gop);
  assert(error == OMX_ErrorNone);
}

void CommandsSender::setNumB(int numB)
{
  OMX_ALG_VIDEO_CONFIG_GROUP_OF_PICTURES gop;
  InitHeader(gop);
  gop.nPortIndex = 1;
  OMX_GetConfig(hEnc, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexConfigVideoGroupOfPictures), &gop);
  int omxGopLength = gop.nPFrames + gop.nBFrames + 1;
  gop.nBFrames = (numB * omxGopLength) / (1 + numB);
  gop.nPFrames = (numB - omxGopLength + 1) / (-numB - 1);
  auto const error = OMX_SetConfig(hEnc, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexConfigVideoGroupOfPictures), &gop);
  assert(error == OMX_ErrorNone);
}

void CommandsSender::setFreqIDR(int freqIDR)
{
  (void)freqIDR;
  assert(0 && "setFreqIDR is not supported");
}

void CommandsSender::setFrameRate(int frameRate, int clockRatio)
{
  OMX_CONFIG_FRAMERATETYPE xFramerate;
  InitHeader(xFramerate);
  xFramerate.nPortIndex = 1;
  OMX_GetConfig(hEnc, OMX_IndexConfigVideoFramerate, &xFramerate);
  auto const framerateInQ16 = ((frameRate * 1000.0) / clockRatio) * 65536.0;
  xFramerate.xEncodeFramerate = std::ceil(framerateInQ16);
  auto const error = OMX_SetConfig(hEnc, OMX_IndexConfigVideoFramerate, &xFramerate);
  assert(error == OMX_ErrorNone);
}

void CommandsSender::setBitRate(int bitRate)
{
  OMX_VIDEO_CONFIG_BITRATETYPE bitrate;
  InitHeader(bitrate);
  bitrate.nPortIndex = 1;
  OMX_GetConfig(hEnc, OMX_IndexConfigVideoBitrate, &bitrate);
  bitrate.nEncodeBitrate = bitRate / 1000;
  auto const error = OMX_SetConfig(hEnc, OMX_IndexConfigVideoBitrate, &bitrate);
  assert(error == OMX_ErrorNone);
}

void CommandsSender::setMaxBitRate(int iTargetBitRate, int iMaxBitRate)
{
  (void)iTargetBitRate;
  (void)iMaxBitRate;
  assert(0 && "setMaxBitRate is not supported");
}

void CommandsSender::setQP(int qp)
{
  (void)qp;
  assert(0 && "setQP is not supported");
}

void CommandsSender::setQPOffset(int iQpOffset)
{
  (void)iQpOffset;
  assert(0 && "setQPOffset is not supported");
}

void CommandsSender::setQPBounds(int iMinQP, int iMaxQP)
{
  (void)iMinQP;
  (void)iMaxQP;
  assert(0 && "setQPBounds is not supported");
}

void CommandsSender::setQPBounds_I(int iMinQP_I, int iMaxQP_I)
{
  (void)iMinQP_I;
  (void)iMaxQP_I;
  assert(0 && "setQPBounds_I is not supported");
}

void CommandsSender::setQPBounds_P(int iMinQP_P, int iMaxQP_P)
{
  (void)iMinQP_P;
  (void)iMaxQP_P;
  assert(0 && "setQPBounds_P is not supported");
}

void CommandsSender::setQPBounds_B(int iMinQP_B, int iMaxQP_B)
{
  (void)iMinQP_B;
  (void)iMaxQP_B;
  assert(0 && "setQPBounds_B is not supported");
}

void CommandsSender::setQPIPDelta(int iQPDelta)
{
  (void)iQPDelta;
  assert(0 && "setQPIPDelta is not supported");
}

void CommandsSender::setQPPBDelta(int iQPDelta)
{
  (void)iQPDelta;
  assert(0 && "setQPPBDelta is not supported");
}

void CommandsSender::setDynamicInput(int iInputIdx)
{
  (void)iInputIdx;
  assert(0 && "setDynamicInput is not supported");
}

void CommandsSender::setLFBetaOffset(int iBetaOffset)
{
  (void)iBetaOffset;
  assert(0 && "setLFBetaOffset is not supported");
}

void CommandsSender::setLFTcOffset(int iTcOffset)
{
  (void)iTcOffset;
  assert(0 && "setLFTcOffset is not supported");
}

void CommandsSender::setCostMode(bool bCostMode)
{
  (void)bCostMode;
  assert(0 && "setCostMode is not supported");
}

void CommandsSender::setMaxPictureSize(int iMaxPictureSize)
{
  (void)iMaxPictureSize;
  assert(0 && "setMaxPictureSize is not supported");
}

void CommandsSender::setMaxPictureSize_I(int iMaxPictureSize_I)
{
  (void)iMaxPictureSize_I;
  assert(0 && "setMaxPictureSize_I is not supported");
}

void CommandsSender::setMaxPictureSize_P(int iMaxPictureSize_P)
{
  (void)iMaxPictureSize_P;
  assert(0 && "setMaxPictureSize_P is not supported");
}

void CommandsSender::setMaxPictureSize_B(int iMaxPictureSize_B)
{
  (void)iMaxPictureSize_B;
  assert(0 && "setMaxPictureSize_B is not supported");
}

void CommandsSender::setQPChromaOffsets(int iQp1Offset, int iQp2Offset)
{
  (void)iQp1Offset;
  (void)iQp2Offset;
  assert(0 && "setQPChromaOffsets is not supported");
}

void CommandsSender::setAutoQP(bool bUseAutoQP)
{
  (void)bUseAutoQP;
  assert(0 && "setAutoQP is not supported");
}

void CommandsSender::setAutoQPThresholdQPAndDeltaQP(bool bEnableUserAutoQPValues, std::vector<int> thresholdQP, std::vector<int> deltaQP)
{
  (void)bEnableUserAutoQPValues;
  (void)thresholdQP;
  (void)deltaQP;
  assert(0 && "setAutoQPThresholdQPAndDeltaQP is not supported");
}

void CommandsSender::setHDRIndex(int iHDRIdx)
{
  (void)iHDRIdx;
  assert(0 && "setHDRIndex is not supported");
}


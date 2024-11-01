// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "ICommandsSender.h"
#include "lib_encode/lib_encoder.h"

extern "C"
{
#include <OMX_Types.h>
}

struct CommandsSender final : public ICommandsSender
{
  CommandsSender(OMX_HANDLETYPE hEnc);
  ~CommandsSender() override;
  void notifySceneChange(int lookAhead) override;
  void notifyIsLongTerm() override;
  void notifyUseLongTerm() override;
  void notifyIsSkip() override;
  virtual void setSAO(bool bSAOEnabled) override;
  void restartGop() override;
  void restartGopRecoveryPoint() override;
  void setGopLength(int gopLength) override;
  void setNumB(int numB) override;
  void setFreqIDR(int freqIDR) override;
  void setFrameRate(int frameRate, int clockRatio) override;
  void setBitRate(int bitRate) override;
  void setMaxBitRate(int iTargetBitRate, int iMaxBitRate) override;
  void setQP(int qp) override;
  void setQPOffset(int iQpOffset) override;
  void setQPBounds(int iMinQP, int iMaxQP) override;
  void setQPBounds_I(int iMinQP_I, int iMaxQP_I) override;
  void setQPBounds_P(int iMinQP_P, int iMaxQP_P) override;
  void setQPBounds_B(int iMinQP_B, int iMaxQP_B) override;
  void setQPIPDelta(int iQPDelta) override;
  void setQPPBDelta(int iQPDelta) override;
  void setDynamicInput(int iInputIdx) override;
  void setLFBetaOffset(int iBetaOffset) override;
  void setLFTcOffset(int iTcOffset) override;
  void setCostMode(bool bCostMode) override;
  void setMaxPictureSize(int iMaxPictureSize) override;
  void setMaxPictureSize_I(int iMaxPictureSize_I) override;
  void setMaxPictureSize_P(int iMaxPictureSize_P) override;
  void setMaxPictureSize_B(int iMaxPictureSize_B) override;
  void setQPChromaOffsets(int iQp1Offset, int iQp2Offset) override;
  void setAutoQP(bool bUseAutoQP) override;
  void setAutoQPThresholdQPAndDeltaQP(bool bEnableUserAutoQPValues, std::vector<int> thresholdQP, std::vector<int> deltaQP) override;
  void setHDRIndex(int iHDRIdx) override;

private:
  OMX_HANDLETYPE hEnc;
};

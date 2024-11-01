// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "module_enums.h"
#include <string>
#include <vector>
#include <cstdint>

template<typename T>
struct InputOutput
{
  T input;
  T output;
};

typedef InputOutput<BufferHandleType> BufferHandles;
typedef InputOutput<int> BufferCounts;
typedef InputOutput<int> BufferSizes;
typedef InputOutput<int> BufferBytesAlignments;
typedef InputOutput<bool> BufferContiguities;

struct Mime
{
  std::string mime;
  CompressionType compression;
};

typedef InputOutput<Mime> Mimes;

struct Format
{
  ColorType color;
  int bitdepth;
  StorageType storage;
};

typedef InputOutput<std::vector<Format>> SupportedFormats;

template<typename T>
struct Dimension
{
  T horizontal;
  T vertical;
};

typedef Dimension<int> Stride;
typedef Dimension<int> StrideAlignments;
typedef Dimension<int> InitialDisplayRes;

struct Resolution
{
  Dimension<int> dimension;
  Stride stride;
};

template<typename T>
struct MinMax
{
  T min;
  T max;
};

struct Clock
{
  int framerate;
  int clockratio;

  bool operator != (Clock const& clock) const
  {
    if(clock.framerate != framerate)
      return true;

    if(clock.clockratio != clockratio)
      return true;
    return false;
  }

  bool operator == (Clock const& clock) const
  {
    if(clock.framerate != framerate)
      return false;

    if(clock.clockratio != clockratio)
      return false;
    return true;
  }
};

struct ProfileLevel
{
  ProfileType profile;
  int level;
};

struct Gop
{
  int b;
  int length;
  long long int idrFrequency;
  long long int rpFrequency;
  long long int ltFrequency;
  bool isLongTermEnabled;
  GopControlType mode;
  GdrType gdr;
};

struct QPMode
{
  QPControlType ctrl;
  QPTableType table;
};

struct QPs
{
  static int constexpr MAX_FRAME_TYPE {
    3
  };
  int initial;
  int deltaIP;
  int deltaPB;
  MinMax<int> range[MAX_FRAME_TYPE];
  QPMode mode;
};

struct RateControlOptions
{
  bool isSceneChangeResilienceEnabled;
  bool isDelayEnabled;
  bool isStaticSceneEnabled;
  bool isSkipEnabled;
  bool isSceneChangePrevention;
};

struct MaxPicturesSizes
{
  int i;
  int p;
  int b;
};

struct RateControl
{
  RateControlType mode;
  RateControlOptions options;
  MaxPicturesSizes sizes;
};

struct Bitrate
{
  int target; // In kbits
  int max; // In kbits
  int cpb; // CPB in milliseconds
  int ird; // InitialRemovalDelay in milliseconds
  int quality;
  uint32_t maxConsecutiveSkipFrame;
  RateControl rateControl;
};

struct Slices
{
  int num;
  int size;
  bool dependent;
};

template<typename T>
struct Point
{
  T x;
  T y;
};

struct Region
{
  Point<int> point;
  Dimension<int> dimension;
};

struct RegionQuality
{
  Region region;
  union
  {
    QualityType byPreset;
    int byValue;
  } quality;
};

struct LookAhead
{
  int lookAhead;
  bool isFirstPassSceneChangeDetectionEnabled;
};

struct TwoPass
{
  int nPass;
  std::string sLogFile;
};

struct Sei
{
  int type;
  uint8_t* data;
  int payload;
};

struct DisplayPictureInfo
{
  int type;
  bool concealed;
};

struct Flags
{
  bool isConfig = false;
  bool isSync = false;
  bool isEndOfSlice = false;
  bool isEndOfFrame = false;
  bool isCorrupt = false;
};

typedef Point<uint16_t> ChromaCoord;

struct MasteringDisplayColourVolume
{
  ChromaCoord displayPrimaries[3];
  ChromaCoord whitePoint;
  uint32_t maxDisplayMasteringLuminance;
  uint32_t minDisplayMasteringLuminance;
};

struct ContentLightLevel
{
  uint16_t maxContentLightLevel;
  uint16_t maxPicAverageLightLevel;
};

struct AlternativeTransferCharacteristics
{
  TransferCharacteristicsType preferredTransferCharacteristics;
};

/*  --------- ST2094_10 --------- */
#define MAX_MANUAL_ADJUSTMENT_ST2094_10 16

struct ProcessingWindow_ST2094_10
{
  uint16_t activeAreaLeftOffset;
  uint16_t activeAreaRightOffset;
  uint16_t activeAreaTopOffset;
  uint16_t activeAreaBottomOffset;
};

struct ImageCharacteristics_ST2094_10
{
  uint16_t minPQ;
  uint16_t maxPQ;
  uint16_t avgPQ;
};

struct ManualAdjustment_ST2094_10
{
  uint16_t targetMaxPQ;
  uint16_t trimSlope;
  uint16_t trimOffset;
  uint16_t trimPower;
  uint16_t trimChromaWeight;
  uint16_t trimSaturationGain;
  int16_t msWeight;
};

struct DynamicMeta_ST2094_10
{
  uint8_t applicationVersion;
  bool processingWindowFlag;
  ProcessingWindow_ST2094_10 processingWindow;
  ImageCharacteristics_ST2094_10 imageCharacteristics;
  uint8_t numManualAdjustments;
  ManualAdjustment_ST2094_10 manualAdjustments[MAX_MANUAL_ADJUSTMENT_ST2094_10];
};

/*  --------- ST2094_40 --------- */
static int constexpr MIN_WINDOW_ST2094_40(1);
static int constexpr MAX_WINDOW_ST2094_40(3);
static int constexpr MAX_MAXRGB_PERCENTILES_ST2094_40(15);
static int constexpr MAX_BEZIER_CURVE_ANCHORS_ST2094_40(15);
static int constexpr MAX_ROW_ACTUAL_PEAK_LUMINANCE_ST2094_40(25);
static int constexpr MAX_COL_ACTUAL_PEAK_LUMINANCE_ST2094_40(25);

struct ProcessingWindow_ST2094_1
{
  uint16_t upperLeftCornerX;
  uint16_t upperLeftCornerY;
  uint16_t lowerRightCornerX;
  uint16_t lowerRightCornerY;
};

struct ProcessingWindow_ST2094_40
{
  ProcessingWindow_ST2094_1 baseProcessingWindow;
  uint16_t centerOfEllipseX;
  uint16_t centerOfEllipseY;
  uint8_t rotationAngle;
  uint16_t semimajorAxisInternalEllipse;
  uint16_t semimajorAxisExternalEllipse;
  uint16_t semiminorAxisExternalEllipse;
  uint8_t overlapProcessOption;
};

struct DisplayPeakLuminance_ST2094_40
{
  bool actualPeakLuminanceFlag;
  uint8_t numRowsActualPeakLuminance;
  uint8_t numColsActualPeakLuminance;
  uint8_t actualPeakLuminance[MAX_ROW_ACTUAL_PEAK_LUMINANCE_ST2094_40][MAX_COL_ACTUAL_PEAK_LUMINANCE_ST2094_40];
};

struct TargetedSystemDisplay_ST2094_40
{
  uint32_t maximumLuminance;
  DisplayPeakLuminance_ST2094_40 peakLuminance;
};

struct ToneMapping_ST2094_40
{
  bool toneMappingFlag;
  uint16_t kneePointX;
  uint16_t kneePointY;
  uint8_t numBezierCurveAnchors;
  uint16_t bezierCurveAnchors[MAX_BEZIER_CURVE_ANCHORS_ST2094_40];
};

struct ProcessingWindowTransform_ST2094_40
{
  uint32_t maxscl[3];
  uint32_t averageMaxrgb;
  uint8_t numDistributionMaxrgbPercentiles;
  uint8_t distributionMaxrgbPercentages[MAX_MAXRGB_PERCENTILES_ST2094_40];
  uint32_t distributionMaxrgbPercentiles[MAX_MAXRGB_PERCENTILES_ST2094_40];
  uint8_t fractionBrightPixels;
  ToneMapping_ST2094_40 toneMapping;
  bool colorSaturationMappingFlag;
  uint8_t colorSaturationWeight;
};

struct DynamicMeta_ST2094_40
{
  uint8_t applicationVersion;
  uint8_t numWindows;
  ProcessingWindow_ST2094_40 processingWindows[MAX_WINDOW_ST2094_40 - 1];
  TargetedSystemDisplay_ST2094_40 targetedSystemDisplay;
  DisplayPeakLuminance_ST2094_40 masteringDisplayPeakLuminance;
  ProcessingWindowTransform_ST2094_40 processingWindowTransforms[MAX_WINDOW_ST2094_40];
};

template<class F>
struct Feature
{
  bool enabled;
  F feature;
};

struct HighDynamicRangeSeis
{
  Feature<MasteringDisplayColourVolume> mdcv;
  Feature<ContentLightLevel> cll;
  Feature<AlternativeTransferCharacteristics> atc;
  Feature<DynamicMeta_ST2094_10> st2094_10;
  Feature<DynamicMeta_ST2094_40> st2094_40;
};

struct RateControlPlugin
{
  int dmaBuf;
  uint32_t dmaSize;
};

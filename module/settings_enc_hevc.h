// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "module/module_enums.h"
#include "settings_enc_interface.h"

extern "C"
{
#include <lib_common/Allocator.h>
}

#include <vector>
#include <memory>

struct EncSettingsHEVC final : EncSettingsInterface
{
  EncSettingsHEVC(BufferContiguities bufferContiguities, BufferBytesAlignments bufferBytesAlignments, StrideAlignments strideAlignments, bool isSeparateConfigurationFromDataEnabled, std::shared_ptr<AL_TAllocator> const& allocator);
  ~EncSettingsHEVC() override;

  ErrorType Get(std::string index, void* settings) const override;
  ErrorType Set(std::string index, void const* settings) override;
  void Reset() override;

  bool Check() override;

private:
  BufferContiguities bufferContiguities;
  BufferBytesAlignments bufferBytesAlignments;
  StrideAlignments strideAlignments;
  bool isSeparateConfigurationFromDataEnabled;
  BufferHandles bufferHandles;
  std::string sTwoPassLogFile;
  std::shared_ptr<AL_TAllocator> allocator;

  std::vector<HEVCProfileType> const profiles
  {
    HEVCProfileType::HEVC_PROFILE_MAIN,
    HEVCProfileType::HEVC_PROFILE_MAIN_10,
    HEVCProfileType::HEVC_PROFILE_MAIN_STILL,
    HEVCProfileType::HEVC_PROFILE_MONOCHROME,
    HEVCProfileType::HEVC_PROFILE_MONOCHROME_10,
    HEVCProfileType::HEVC_PROFILE_MAIN_422,
    HEVCProfileType::HEVC_PROFILE_MAIN_422_10,
    HEVCProfileType::HEVC_PROFILE_MAIN_INTRA,
    HEVCProfileType::HEVC_PROFILE_MAIN_10_INTRA,
    HEVCProfileType::HEVC_PROFILE_MAIN_422_INTRA,
    HEVCProfileType::HEVC_PROFILE_MAIN_422_10_INTRA,
    HEVCProfileType::HEVC_PROFILE_MAIN_HIGH_TIER,
    HEVCProfileType::HEVC_PROFILE_MAIN_10_HIGH_TIER,
    HEVCProfileType::HEVC_PROFILE_MAIN_STILL_HIGH_TIER,
    HEVCProfileType::HEVC_PROFILE_MONOCHROME_HIGH_TIER,
    HEVCProfileType::HEVC_PROFILE_MONOCHROME_10_HIGH_TIER,
    HEVCProfileType::HEVC_PROFILE_MAIN_422_HIGH_TIER,
    HEVCProfileType::HEVC_PROFILE_MAIN_422_10_HIGH_TIER,
    HEVCProfileType::HEVC_PROFILE_MAIN_INTRA_HIGH_TIER,
    HEVCProfileType::HEVC_PROFILE_MAIN_10_INTRA_HIGH_TIER,
    HEVCProfileType::HEVC_PROFILE_MAIN_422_INTRA_HIGH_TIER,
    HEVCProfileType::HEVC_PROFILE_MAIN_422_10_INTRA_HIGH_TIER,
  };

  std::vector<int> const levels
  {
    10,
    20,
    21,
    30,
    31,
    40,
    41,
    50,
    51,
    52,
    60,
    61,
    62,
  };

  std::vector<ColorType> const colors
  {
    ColorType::COLOR_400,
    ColorType::COLOR_420,
    ColorType::COLOR_422,
    ColorType::COLOR_444,
  };

  std::vector<int> const bitdepths
  {
    8,
    10,
    12,
  };

  std::vector<StorageType> const storages
  {
    StorageType::STORAGE_RASTER,
    StorageType::STORAGE_TILE_32x4,
    StorageType::STORAGE_TILE_64x4,
  };

  std::vector<VideoModeType> const videoModes
  {
    VideoModeType::VIDEO_MODE_PROGRESSIVE,
    VideoModeType::VIDEO_MODE_ALTERNATE_TOP_BOTTOM_FIELD,
    VideoModeType::VIDEO_MODE_ALTERNATE_BOTTOM_TOP_FIELD,
  };
};

// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#pragma once

#include "module/module_enums.h"
#include "settings_dec_interface.h"

#include <vector>

struct DecSettingsJPEG final : DecSettingsInterface
{
  DecSettingsJPEG(BufferContiguities bufferContiguities, BufferBytesAlignments bufferBytesAlignments, StrideAlignments strideAlignments);
  ~DecSettingsJPEG() override;

  ErrorType Get(std::string index, void* settings) const override;
  ErrorType Set(std::string index, void const* settings) override;
  void Reset() override;

  bool Check() override;

private:
  BufferContiguities bufferContiguities;
  BufferBytesAlignments bufferBytesAlignments;
  StrideAlignments strideAlignments;
  BufferHandles bufferHandles;

  std::vector<JPEGProfileType> const profiles
  {
    JPEGProfileType::JPEG_PROFILE,
    JPEGProfileType::JPEG_PROFILE_EXT_HUFF,
    JPEGProfileType::JPEG_PROFILE_LOSSLESS
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

  std::vector<SequencePictureModeType> const sequenceModes
  {
    SequencePictureModeType::SEQUENCE_PICTURE_MODE_UNKNOWN,
    SequencePictureModeType::SEQUENCE_PICTURE_MODE_FRAME,
  };
};

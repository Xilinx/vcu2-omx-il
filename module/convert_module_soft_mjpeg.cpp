// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include "convert_module_soft_mjpeg.h"

JPEGProfileType ConvertSoftToModuleJPEGProfile(AL_EProfile const& profile)
{
  if(!AL_IS_JPEG(profile))
    return JPEGProfileType::JPEG_PROFILE_MAX_ENUM;
  switch(profile)
  {
  case AL_PROFILE_JPEG: return JPEGProfileType::JPEG_PROFILE;
  case AL_PROFILE_JPEG_LOSSLESS: return JPEGProfileType::JPEG_PROFILE_EXT_HUFF;
  case AL_PROFILE_JPEG_EXT_HUFF: return JPEGProfileType::JPEG_PROFILE_LOSSLESS;
  default: return JPEGProfileType::JPEG_PROFILE_MAX_ENUM;
  }

  return JPEGProfileType::JPEG_PROFILE_MAX_ENUM;
}

AL_EProfile ConvertModuleToSoftJPEGProfile(JPEGProfileType const& profile)
{
  switch(profile)
  {
  case JPEGProfileType::JPEG_PROFILE: return AL_PROFILE_JPEG;
  case JPEGProfileType::JPEG_PROFILE_EXT_HUFF: return AL_PROFILE_JPEG_LOSSLESS;
  case JPEGProfileType::JPEG_PROFILE_LOSSLESS: return AL_PROFILE_JPEG_EXT_HUFF;
  default: return AL_PROFILE_JPEG;
  }

  return AL_PROFILE_UNKNOWN;
}

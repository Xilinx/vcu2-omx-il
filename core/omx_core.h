// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <OMX_Core.h>

typedef struct
{
  char const name[OMX_MAX_STRINGNAME_SIZE];
  void* pLibHandle;
  char const* pSoLibName;
  char const role[OMX_MAX_STRINGNAME_SIZE];
}omx_comp_type;

static omx_comp_type AL_COMP_LIST[] =
{

  {
    "OMX.allegro.h265.encoder",
    nullptr,
    "libOMX.allegro.video_encoder.so",
    "video_encoder.hevc",
  },
  {
    "OMX.allegro.h265.riscv.encoder",
    nullptr,
    "libOMX.allegro.video_encoder.so",
    "video_encoder.hevc",
  },

  {
    "OMX.allegro.h264.encoder",
    nullptr,
    "libOMX.allegro.video_encoder.so",
    "video_encoder.avc",
  },
  {
    "OMX.allegro.h264.riscv.encoder",
    nullptr,
    "libOMX.allegro.video_encoder.so",
    "video_encoder.avc",
  },

  {
    "OMX.allegro.h265.decoder",
    nullptr,
    "libOMX.allegro.video_decoder.so",
    "video_decoder.hevc",
  },
  {
    "OMX.allegro.h265.riscv.decoder",
    nullptr,
    "libOMX.allegro.video_decoder.so",
    "video_decoder.hevc",
  },

  {
    "OMX.allegro.h264.decoder",
    nullptr,
    "libOMX.allegro.video_decoder.so",
    "video_decoder.avc",
  },
  {
    "OMX.allegro.h264.riscv.decoder",
    nullptr,
    "libOMX.allegro.video_decoder.so",
    "video_decoder.avc",
  },

  {
    "OMX.allegro.mjpeg.decoder",
    nullptr,
    "libOMX.allegro.video_decoder.so",
    "video_decoder.mjpeg",
  },
  {
    "OMX.allegro.mjpeg.riscv.decoder",
    nullptr,
    "libOMX.allegro.video_decoder.so",
    "video_decoder.mjpeg",
  },

};

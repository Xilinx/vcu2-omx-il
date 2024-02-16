// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#pragma once

enum class Codec
{
  HEVC,
#if AL_ENABLE_RISCV
  HEVC_RISCV,
#endif

  AVC,
#if AL_ENABLE_RISCV
  AVC_RISCV,
#endif

  MJPEG,
#if AL_ENABLE_RISCV
  MJPEG_RISCV,
#endif
};

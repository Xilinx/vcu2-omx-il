// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include <sys/mman.h>
#include <unistd.h>
#include <iostream>

#include "helpers.h"

struct fourcc_map
{
  const char* fourcc;
  enum OMX_ALG_COLOR_FORMATTYPE format;
};

static struct fourcc_map supported_fourcc[]
{
// VCU1
  {
    "Y800", OMX_ALG_COLOR_FormatL8
  },
  {
    "Y010", OMX_ALG_COLOR_FormatL10bit
  },
  {
    "Y012", OMX_ALG_COLOR_FormatL12bit
  },

  {
    "NV12", OMX_ALG_COLOR_FormatYUV420SemiPlanar
  },
  {
    "P010", OMX_ALG_COLOR_FormatYUV420SemiPlanar10bit
  },
  {
    "P012", OMX_ALG_COLOR_FormatYUV420SemiPlanar12bit
  },

  {
    "NV16", OMX_ALG_COLOR_FormatYUV422SemiPlanar
  },
  {
    "P210", OMX_ALG_COLOR_FormatYUV422SemiPlanar10bit
  },
  {
    "P212", OMX_ALG_COLOR_FormatYUV422SemiPlanar12bit
  },

  {
    "I444", OMX_ALG_COLOR_FormatYUV444Planar8bit
  },
  {
    "I4AL", OMX_ALG_COLOR_FormatYUV444Planar10bit
  },
  {
    "I4CL", OMX_ALG_COLOR_FormatYUV444Planar12bit
  },

  // 32x4
  {
    "T5M8", OMX_ALG_COLOR_FormatL8bitTiled32x4
  },
  {
    "T5MA", OMX_ALG_COLOR_FormatL10bitTiled32x4
  },
  {
    "T5MC", OMX_ALG_COLOR_FormatL12bitTiled32x4
  },
  {
    "T508", OMX_ALG_COLOR_FormatYUV420SemiPlanar8bitTiled32x4
  },
  {
    "T50A", OMX_ALG_COLOR_FormatYUV420SemiPlanar10bitTiled32x4
  },
  {
    "T50C", OMX_ALG_COLOR_FormatYUV420SemiPlanar12bitTiled32x4
  },
  {
    "T528", OMX_ALG_COLOR_FormatYUV422SemiPlanar8bitTiled32x4
  },
  {
    "T52A", OMX_ALG_COLOR_FormatYUV422SemiPlanar10bitTiled32x4
  },
  {
    "T52C", OMX_ALG_COLOR_FormatYUV422SemiPlanar12bitTiled32x4
  },
  {
    "T548", OMX_ALG_COLOR_FormatYUV444Planar8bitTiled32x4
  },
  {
    "T54A", OMX_ALG_COLOR_FormatYUV444Planar10bitTiled32x4
  },
  {
    "T54C", OMX_ALG_COLOR_FormatYUV444Planar12bitTiled32x4
  },

  // 64x4
  {
    "T6M8", OMX_ALG_COLOR_FormatL8bitTiled64x4
  },
  {
    "T6MA", OMX_ALG_COLOR_FormatL10bitTiled64x4
  },
  {
    "T6MC", OMX_ALG_COLOR_FormatL12bitTiled64x4
  },
  {
    "T608", OMX_ALG_COLOR_FormatYUV420SemiPlanar8bitTiled64x4
  },
  {
    "T60A", OMX_ALG_COLOR_FormatYUV420SemiPlanar10bitTiled64x4
  },
  {
    "T60C", OMX_ALG_COLOR_FormatYUV420SemiPlanar12bitTiled64x4
  },
  {
    "T628", OMX_ALG_COLOR_FormatYUV422SemiPlanar8bitTiled64x4
  },
  {
    "T62A", OMX_ALG_COLOR_FormatYUV422SemiPlanar10bitTiled64x4
  },
  {
    "T52C", OMX_ALG_COLOR_FormatYUV422SemiPlanar12bitTiled32x4
  },
  {
    "T62C", OMX_ALG_COLOR_FormatYUV422SemiPlanar12bitTiled64x4
  },
  {
    "T648", OMX_ALG_COLOR_FormatYUV444Planar8bitTiled64x4
  },
  {
    "T64A", OMX_ALG_COLOR_FormatYUV444Planar10bitTiled64x4
  },
  {
    "T64C", OMX_ALG_COLOR_FormatYUV444Planar12bitTiled64x4
  },
  {
    NULL, OMX_ALG_COLOR_FormatUnused
  }
};

static inline size_t AlignToPageSize(size_t size)
{
  unsigned long pagesize = sysconf(_SC_PAGESIZE);

  if((size % pagesize) == 0)
    return size;
  return size + pagesize - (size % pagesize);
}

void Buffer_FreeData(char* data, bool use_dmabuf)
{
  if(use_dmabuf)
    close((int)(uintptr_t)data);
  else
    free(data);
}

char* Buffer_MapData(char* data, size_t offset, size_t size, bool use_dmabuf)
{
  if(!use_dmabuf)
    return data + offset;

  int fd = (int)(intptr_t)data;
  auto mapSize = AlignToPageSize(size);

  data = (char*)mmap(0, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if(data == MAP_FAILED)
  {
    std::cerr << "MAP_FAILED!" << std::endl;
    return nullptr;
  }
  return data + offset;
}

void Buffer_UnmapData(char* data, size_t zSize, bool use_dmabuf)
{
  if(!use_dmabuf)
    return;
  munmap(data, zSize);
}

bool setChroma(std::string user_chroma, OMX_COLOR_FORMATTYPE* chroma)
{
  int i;
  char fcc[4];
  struct fourcc_map* entry;

  if(user_chroma.size() != 4)
    return false;

  for(i = 0; i < 4; ++i)
    fcc[i] = toupper(user_chroma.c_str()[i]);

  i = 0;

  for(;;)
  {
    entry = &supported_fourcc[i];

    if(entry->fourcc == NULL)
      break;

    if(strncmp(entry->fourcc, fcc, 4) == 0)
    {
      *chroma = (OMX_COLOR_FORMATTYPE)entry->format;
      return true;
    }
    ++i;
  }

  return false;
}

extern "C" bool setChromaWrapper(char* user_chroma, OMX_COLOR_FORMATTYPE* chroma)
{
  std::string user_chroma_string {
    user_chroma
  };
  return setChroma(user_chroma_string, chroma);
}

bool isFormatSupported(OMX_COLOR_FORMATTYPE format)
{
  struct fourcc_map* m;
  int i;

  i = 0;

  for(;;)
  {
    m = &supported_fourcc[i];

    if(m->fourcc == NULL)
      break;

    if(m->format == (OMX_ALG_COLOR_FORMATTYPE)format)
      return true;
    ++i;
  }

  return false;
}

void appendSupportedFourccString(std::string& str)
{
  struct fourcc_map* m;
  int i;

  i = 0;

  for(;;)
  {
    m = &supported_fourcc[i];

    if(m->fourcc == NULL)
      break;

    str += m->fourcc;
    str += " | ";

    ++i;
  }

  if(i)
    str.resize(str.length() - 3);
}

bool is8bits(OMX_COLOR_FORMATTYPE format)
{
  switch((OMX_U32)format)
  {
  case OMX_COLOR_FormatL8:
  case OMX_ALG_COLOR_FormatL8bitTiled32x4:
  case OMX_ALG_COLOR_FormatL8bitTiled64x4:

  case OMX_COLOR_FormatYUV420SemiPlanar:
  case OMX_ALG_COLOR_FormatYUV420SemiPlanar8bitTiled32x4:
  case OMX_ALG_COLOR_FormatYUV420SemiPlanar8bitTiled64x4:

  case OMX_COLOR_FormatYUV422SemiPlanar:
  case OMX_ALG_COLOR_FormatYUV422SemiPlanar8bitTiled32x4:
  case OMX_ALG_COLOR_FormatYUV422SemiPlanar8bitTiled64x4:

  case OMX_ALG_COLOR_FormatYUV444Planar8bit:
  case OMX_ALG_COLOR_FormatYUV444Planar8bitTiled32x4:
  case OMX_ALG_COLOR_FormatYUV444Planar8bitTiled64x4:
    return true;

  default:
    return false;
  }
}

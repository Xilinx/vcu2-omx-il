// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <atomic>
#include <unistd.h>

#if defined(ANDROID) || defined(__ANDROID_API__)
#define LOG_NDEBUG 0
#define LOG_TAG "AL_OMX_MAIN_EXE"
#include <android/log.h>
#include "HardwareAPI.h"
#endif /* ANDROID */

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Types.h>
#include <OMX_Video.h>
#include <OMX_VideoAlg.h>
#include <OMX_ComponentExt.h>
#include <OMX_IndexAlg.h>
#include <OMX_IVCommonAlg.h>

#include "CommandsSender.h"
#include "EncCmdMngr.h"

#include <utility/logger.h>
#include <utility/locked_queue.h>
#include <utility/semaphore.h>
#include <utility/round.h>
#include <utility/omx_translate.h>

#include "../common/helpers.h"
#include "../common/getters.h"
#include "../common/CommandLineParser.h"
#include "../common/codec.h"
#include "../common/YuvReadWrite.h"

extern "C"
{
#include <lib_fpga/DmaAlloc.h>
#include <lib_fpga/DmaAllocLinux.h>
#include <lib_common/Context.h>
#include "lib_encode/LibEncoderRiscv.h"
}

#include "RCPlugin.h"

#define DEFAULT_MAX_FRAMES 0

using namespace std;

struct Ports
{
  int index;
  bool isDMA;
  atomic<bool> isEOS;
  atomic<bool> isFlushing;
  vector<OMX_BUFFERHEADERTYPE*> buffers;
};

static inline OMX_ALG_BUFFER_MODE GetBufferMode(bool isDMA)
{
  return isDMA ? OMX_ALG_BUF_DMA : OMX_ALG_BUF_NORMAL;
}

struct Settings
{
  int width;
  int height;
  int framerate;
  Codec codec;
  OMX_COLOR_FORMATTYPE format;
  int lookahead;
  int pass;
  string twoPassLogFile;
  bool isDummySeiEnabled;
  string deviceName = string("/dev/allegroIP");

  OMX_VIDEO_CONTROLRATETYPE eControlRate;
  int targetBitrate;
  bool isVideoFullRangeEnabled;
  int maxFrames = DEFAULT_MAX_FRAMES;
};

struct Application
{
  semaphore encoderEventSem;
  semaphore eof;
  semaphore encoderEventState;

  OMX_HANDLETYPE hEncoder;

  Settings settings;
  std::mutex mutex;
  condition_variable cv;
  bool read;
  AL_TAllocator* pAllocator;

  AL_RiscV_Ctx pRiscvContext;

  Ports input;
  Ports output;

  CEncCmdMngr* encCmd;
  CommandsSender* cmdSender;
};

static inline void SetDefaultSettings(Settings& settings)
{
  settings.width = 176;
  settings.height = 144;
  settings.framerate = 1;
  settings.codec = Codec::HEVC;
  settings.format = OMX_COLOR_FormatYUV420SemiPlanar;
  settings.lookahead = 0;
  settings.pass = 0;
  settings.twoPassLogFile = "";
  settings.isDummySeiEnabled = false;
  settings.targetBitrate = 64000;
  settings.eControlRate = OMX_Video_ControlRateConstant;
  settings.isVideoFullRangeEnabled = false;
}

static inline void SetDefaultApplication(Application& app)
{
  SetDefaultSettings(app.settings);

  app.input.index = 0;
  app.input.isDMA = false;
  app.input.isFlushing = false;
  app.input.isEOS = false;
  app.output.index = 1;
  app.output.isDMA = false;
  app.output.isFlushing = false;
  app.output.isEOS = false;
  app.read = false;
}

static string input_file;
static string output_file;
static string cmd_file;
static ifstream infile;
static ofstream outfile;
static int user_slice = 0;
static OMX_BOOL isSrcSyncEnabled = OMX_FALSE;

static OMX_PARAM_PORTDEFINITIONTYPE paramPort;

static OMX_ERRORTYPE setEnableLongTerm(Application& app)
{
  OMX_ALG_VIDEO_PARAM_LONG_TERM lt;
  InitHeader(lt);
  lt.nPortIndex = 0;
  OMX_CALL(OMX_GetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoLongTerm), &lt));
  lt.bEnableLongTerm = OMX_TRUE;
  OMX_CALL(OMX_SetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoLongTerm), &lt));
  return OMX_ErrorNone;
}

static int AllocDmabufFd(AL_TAllocator* pAllocator, size_t size)
{
  AL_HANDLE hBuf = AL_Allocator_Alloc(pAllocator, size);

  if(!hBuf)
  {
    LOG_ERROR(string { "Failed to allocate Buffer for dma" });
    assert(0);
  }
  auto fd = AL_LinuxDmaAllocator_GetFd((AL_TLinuxDmaAllocator*)(pAllocator), hBuf);
  fd = dup(fd);

  if(fd == -1)
  {
    LOG_ERROR(string { "Failed to ExportToFd: " } +ToStringAddr(hBuf));
    assert(0);
  }

  AL_Allocator_Free(pAllocator, hBuf);

  return fd;
}

static OMX_ERRORTYPE setPortParameters(Application& app)
{
  OMX_VIDEO_PARAM_PORTFORMATTYPE inParamFormat;
  InitHeader(inParamFormat);
  inParamFormat.nPortIndex = 0;
  OMX_CALL(OMX_GetParameter(app.hEncoder, OMX_IndexParamVideoPortFormat, &inParamFormat));

  inParamFormat.eColorFormat = app.settings.format;
  inParamFormat.xFramerate = app.settings.framerate << 16;

  OMX_CALL(OMX_SetParameter(app.hEncoder, OMX_IndexParamVideoPortFormat, &inParamFormat));

  InitHeader(paramPort);
  paramPort.nPortIndex = 0;
  OMX_CALL(OMX_GetParameter(app.hEncoder, OMX_IndexParamPortDefinition, &paramPort));
  paramPort.format.video.nFrameWidth = app.settings.width;
  paramPort.format.video.nFrameHeight = app.settings.height;
  paramPort.format.video.nStride = is8bits(app.settings.format) ? app.settings.width :
                                   (app.settings.width * 2);

  paramPort.format.video.nSliceHeight = RoundUp(app.settings.height, 8);

  OMX_CALL(OMX_SetParameter(app.hEncoder, OMX_IndexParamPortDefinition, &paramPort));
  OMX_CALL(OMX_GetParameter(app.hEncoder, OMX_IndexParamPortDefinition, &paramPort));

  auto updateBufferMode = [&](OMX_ALG_PORT_PARAM_BUFFER_MODE& mode)
                          {
                            mode.eMode = (static_cast<int>(mode.nPortIndex) == app.input.index) ? GetBufferMode(app.input.isDMA) : GetBufferMode(app.output.isDMA);
                          };
  OMX_CALL(PortSetup<OMX_ALG_PORT_PARAM_BUFFER_MODE>(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexPortParamBufferMode), updateBufferMode, 0));
  OMX_CALL(PortSetup<OMX_ALG_PORT_PARAM_BUFFER_MODE>(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexPortParamBufferMode), updateBufferMode, 1));

  OMX_ALG_PORT_PARAM_SYNCHRONIZATION srcSync;
  InitHeader(srcSync);
  srcSync.nPortIndex = 0;
  srcSync.bEnableSrcSynchronization = isSrcSyncEnabled;
  OMX_SetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexPortParamSynchronization), &srcSync);

  if(user_slice)
  {
    OMX_ALG_VIDEO_PARAM_SLICES slices;
    InitHeader(slices);
    slices.nPortIndex = 1;
    slices.nNumSlices = user_slice;
    OMX_SetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoSlices), &slices);

    OMX_ALG_VIDEO_PARAM_SUBFRAME sub;
    InitHeader(sub);
    sub.nPortIndex = 1;
    sub.bEnableSubframe = OMX_TRUE;
    OMX_SetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoSubframe), &sub);
  }

  OMX_ALG_VIDEO_PARAM_SKIP_FRAME skip;
  InitHeader(skip);
  OMX_GetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoSkipFrame), &skip);
  assert(skip.nMaxConsecutiveSkipFrame == UINT32_MAX);
  skip.bEnableSkipFrame = OMX_FALSE;
  skip.nMaxConsecutiveSkipFrame = 1;
  OMX_SetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoSkipFrame), &skip);

  OMX_ALG_VIDEO_PARAM_VIDEO_FULL_RANGE yuv_range;
  InitHeader(yuv_range);
  OMX_GetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoFullRange), &yuv_range);
  yuv_range.bVideoFullRangeEnabled = static_cast<OMX_BOOL>(app.settings.isVideoFullRangeEnabled);
  OMX_SetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoFullRange), &yuv_range);

  OMX_VIDEO_PARAM_BITRATETYPE bitrate;
  InitHeader(bitrate);
  bitrate.eControlRate = app.settings.eControlRate;
  bitrate.nTargetBitrate = app.settings.targetBitrate;
  OMX_SetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_IndexParamVideoBitrate), &bitrate);

  if(bitrate.eControlRate == (OMX_VIDEO_CONTROLRATETYPE)OMX_ALG_Video_ControlRatePlugin)
  {
    if(!app.input.isDMA)
      throw std::runtime_error("RC Plugin isn't supported in non-dmabuf mode.");

    OMX_ALG_VIDEO_PARAM_RATE_CONTROL_PLUGIN rcPlugin;
    InitHeader(rcPlugin);
    rcPlugin.nDmaSize = sizeof(RCPlugin);

    AL_HANDLE hBuf = AL_Allocator_Alloc(app.pAllocator, rcPlugin.nDmaSize);

    if(!hBuf)
    {
      LOG_ERROR(string { "Failed to allocate Buffer for dma" });
      assert(0);
    }
    auto fd = AL_LinuxDmaAllocator_GetFd((AL_TLinuxDmaAllocator*)(app.pAllocator), hBuf);
    fd = dup(fd);

    if(fd == -1)
    {
      LOG_ERROR(string { "Failed to ExportToFd: " } +ToStringAddr(hBuf));
      assert(0);
    }

    rcPlugin.nDmabuf = fd;

    RCPlugin* rc = (RCPlugin*)AL_Allocator_GetVirtualAddr(app.pAllocator, hBuf);
    RCPlugin_Init(rc);
    rc->capacity = 1;
    RCPlugin_SetNextFrameQP(rc);
    // Using the example RC Plugin, the first word is the qp for the sequence
    OMX_SetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoRateControlPlugin), &rcPlugin);

    AL_Allocator_Free(app.pAllocator, hBuf);
  }

  if(app.settings.lookahead)
  {
    OMX_ALG_VIDEO_PARAM_LOOKAHEAD la;
    InitHeader(la);
    la.nPortIndex = 1;
    la.nLookAhead = app.settings.lookahead;
    la.bEnableFirstPassSceneChangeDetection = OMX_FALSE;
    OMX_SetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoLookAhead), &la);
  }

  if(app.settings.pass)
  {
    OMX_ALG_VIDEO_PARAM_TWOPASS tp;
    InitHeader(tp);
    tp.nPortIndex = 1;
    tp.nPass = app.settings.pass;
    strncpy((char*)tp.cLogFile, app.settings.twoPassLogFile.c_str(), OMX_MAX_STRINGNAME_SIZE - 1);
    OMX_SetParameter(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexParamVideoTwoPass), &tp);
  }

  setEnableLongTerm(app);

  OMX_PARAM_PORTDEFINITIONTYPE paramPortForActual;
  InitHeader(paramPortForActual);
  paramPortForActual.nPortIndex = 0;
  OMX_CALL(OMX_GetParameter(app.hEncoder, OMX_IndexParamPortDefinition, &paramPortForActual));
  paramPortForActual.nBufferCountActual = paramPortForActual.nBufferCountMin + 4; // alloc max for b frames
  OMX_CALL(OMX_SetParameter(app.hEncoder, OMX_IndexParamPortDefinition, &paramPortForActual));
  paramPortForActual.nPortIndex = 1;
  OMX_CALL(OMX_GetParameter(app.hEncoder, OMX_IndexParamPortDefinition, &paramPortForActual));
  paramPortForActual.nBufferCountActual = paramPortForActual.nBufferCountMin + 4; // alloc max for b frames
  OMX_CALL(OMX_SetParameter(app.hEncoder, OMX_IndexParamPortDefinition, &paramPortForActual));
  OMX_CALL(OMX_GetParameter(app.hEncoder, OMX_IndexParamPortDefinition, &paramPort));

  LOG_VERBOSE(string { "Input picture: " } +to_string(app.settings.width) + string { "x" } +to_string(app.settings.height));
  return OMX_ErrorNone;
}

static void Usage(CommandLineParser& opt, char* ExeName)
{
  cerr << "Usage: " << ExeName << " <InputFile> [options]" << endl;
  cerr << "Options:" << endl;

  for(auto& command: opt.displayOrder)
    cerr << "  " << opt.descs[command] << endl;
}

static OMX_VIDEO_CONTROLRATETYPE parseControlRate(std::string const& cr)
{
  if(cr == "CBR")
    return OMX_Video_ControlRateConstant;
  else if(cr == "VBR")
    return OMX_Video_ControlRateVariable;
  else if(cr == "CONST_QP")
    return OMX_Video_ControlRateDisable;
  else if(cr == "PLUGIN")
    return (OMX_VIDEO_CONTROLRATETYPE)OMX_ALG_Video_ControlRatePlugin;

  throw std::runtime_error("Unknown rate control mode: " + cr);
}

static void parseCommandLine(int argc, char** argv, Application& app)
{
  Settings& settings = app.settings;
  bool help = false;
  string fourcc = "nv12";
  string controlRate = "";

  string supported_fourccc;

  CommandLineParser opt {};
  opt.addString("input_file", &input_file, "Input file");
  opt.addFlag("--help", &help, "Show this help");
  opt.addInt("--width", &settings.width, "Input width ('176')");
  opt.addInt("--height", &settings.height, "Input height ('144')");
  opt.addInt("--framerate", &settings.framerate, "Input fps ('1')");
  opt.addString("--device", &settings.deviceName, "Device's name");
  opt.addString("--out", &output_file, "Output compressed file name");

  std::string str;
  str = "Input file format";
  appendSupportedFourccString(str);
  str.append(" ('NV12') ");
  opt.addString("--fourcc", &fourcc, str);
  str.clear();

  opt.addFlag("--hevc", &settings.codec, "Use the default hevc encoder", Codec::HEVC);
  opt.addFlag("--avc", &settings.codec, "Use the default avc encoder", Codec::AVC);

  opt.addFlag("--hevc-riscv", &settings.codec, "Use riscv hevc encoder", Codec::HEVC_RISCV);
  opt.addFlag("--avc-riscv", &settings.codec, "Use riscv avc encoder", Codec::AVC_RISCV);

  opt.addFlag("--dma-in", &app.input.isDMA, "Use dmabufs on input port");
  opt.addFlag("--dma-out", &app.output.isDMA, "Use dmabufs on output port");
  opt.addFlag("--input-src-sync", &isSrcSyncEnabled, "Enable Input Src Sync");
  opt.addInt("--subframe", &user_slice, "<4 || 8 || 16>: activate subframe latency '(0)'");
  opt.addString("--cmd-file", &cmd_file, "File to precise for dynamic cmd");
  opt.addInt("--lookahead", &settings.lookahead, "<0 || above 2>: activate lookahead mode '(0)'");
  opt.addInt("--pass", &settings.pass, "<0 || 1 || 2>: specify which pass we encode'(0)'");
  opt.addString("--pass-logfile", &settings.twoPassLogFile, "LogFile to transmit dualpass statistics");
  opt.addFlag("--dummy-sei", &settings.isDummySeiEnabled, "Enable dummy seis on firsts frames");
  opt.addString("--rate-control-type", &controlRate, "Available rate control mode: CONST_QP, CBR, VBR and PLUGIN");
  opt.addInt("--target-bitrate", &settings.targetBitrate, "Targeted bitrate (Not applicable in CONST_QP)");
  opt.addFlag("--video-full-range", &settings.isVideoFullRangeEnabled, "Enable Video Full Range");
  opt.addUint("--max-frames", &settings.maxFrames, "Specify number or frames to encode (default: 0 -> continue until EOF)");

  opt.parse(argc, argv);

  if(help)
  {
    Usage(opt, argv[0]);
    exit(0);
  }

  if(!controlRate.empty())
    settings.eControlRate = parseControlRate(controlRate);

  if(!setChroma(fourcc, &settings.format) || !isFormatSupported(settings.format))
  {
    cerr << "[Error] format not supported" << endl;
    Usage(opt, argv[0]);
    exit(1);
  }

  if(!(user_slice == 0 || user_slice == 4 || user_slice == 8 || user_slice == 16))
  {
    Usage(opt, argv[0]);
    cerr << "[Error] subframe parameter was incorrectly set" << endl;
    exit(1);
  }

  if(input_file == "")
  {
    Usage(opt, argv[0]);
    cerr << "[Error] No input file found" << endl;
    exit(1);
  }

  if(output_file == "")
  {
    switch(settings.codec)
    {
    case Codec::AVC:
      output_file = "output.hardware.h264";
      break;
    case Codec::AVC_RISCV:
      output_file = "output.riscv.h264";
      break;

    case Codec::HEVC:
      output_file = "output.hardware.h265";
      break;

    case Codec::HEVC_RISCV:
      output_file = "output.riscv.h265";
      break;

    default:
      assert(0);
      break;
    }
  }
}

// Callbacks implementation of the video encoder component
static OMX_ERRORTYPE onComponentEvent(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_EVENTTYPE eEvent, OMX_U32 Data1, OMX_U32 /*Data2*/, OMX_PTR /*pEventData*/)
{
  auto app = static_cast<Application*>(pAppData);
  LOG_IMPORTANT(string { "Event from encoder: " } +ToStringOMXEvent(eEvent));
  switch(eEvent)
  {
  case OMX_EventCmdComplete:
  {
    auto cmd = static_cast<OMX_COMMANDTYPE>(Data1);
    LOG_IMPORTANT(string { "Command: " } +ToStringOMXCommand((OMX_COMMANDTYPE)cmd));
    switch(cmd)
    {
    case OMX_CommandStateSet:
    {
      app->encoderEventState.notify();
      break;
    }
    case OMX_CommandPortEnable:
    case OMX_CommandPortDisable:
    {
      app->encoderEventSem.notify();
      break;
    }
    case OMX_CommandMarkBuffer:
    {
      app->encoderEventSem.notify();
      break;
    }
    case OMX_CommandFlush:
    {
      app->encoderEventSem.notify();
      break;
    }
    default:
      break;
    }

    break;
  }

  case OMX_EventError:
  {
    auto cmd = static_cast<OMX_ERRORTYPE>(Data1);
    LOG_ERROR(string { "Component (" } +ToStringAddr(hComponent) + string { "): " } +ToStringOMXEvent(eEvent) + string { "(" } +ToStringOMXError(cmd) + string { ")" });
    exit(1);
  }
  /* this event will be fired by the component but we have nothing special to do with them */
  case OMX_EventBufferFlag: // fallthrough
  case OMX_EventPortSettingsChanged:
  {
    LOG_IMPORTANT(string { "Component " } +ToStringAddr(hComponent) + string { ": got " } +ToStringOMXEvent(eEvent));
    break;
  }
  default:
  {
    LOG_IMPORTANT(string { "Component " } +ToStringAddr(hComponent) + string { ": unsupported " } +ToStringOMXEvent(eEvent));
    return OMX_ErrorNotImplemented;
  }
  }

  return OMX_ErrorNone;
}

static void Read(OMX_BUFFERHEADERTYPE* pBuffer, Application& app)
{
  static int frameCount = 0;

  pBuffer->nFlags = 0; // clear flags;

  int width = paramPort.format.video.nFrameWidth;
  int height = paramPort.format.video.nFrameHeight;
  OMX_S32 bufferPlaneStride = paramPort.format.video.nStride;
  OMX_U32 bufferPlaneStrideHeight = paramPort.format.video.nSliceHeight;
  auto color = app.settings.format;

  LOG_VERBOSE(string { string { "Reading input frame: " } +to_string(frameCount) });
  LOG_VERBOSE(string { std::to_string(width) + string { "x" } +to_string(height) + string { "( " } +to_string(bufferPlaneStride) + string { "x" } +to_string(bufferPlaneStrideHeight) + string { ")" }
              });

  char* dst = Buffer_MapData((char*)(pBuffer->pBuffer), pBuffer->nOffset, pBuffer->nAllocLen, app.input.isDMA);
  int read_count = readOneYuvFrame(infile, color, width, height, dst, bufferPlaneStride, bufferPlaneStrideHeight);
  Buffer_UnmapData((char*)pBuffer->pBuffer, pBuffer->nAllocLen, app.input.isDMA);

  if(read_count)
  {
// pBuffer->nFilledLen = read_count;
// assert(pBuffer->nFilledLen <= pBuffer->nAllocLen);
    pBuffer->nFilledLen = pBuffer->nAllocLen;
    pBuffer->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
    app.encCmd->Process(app.cmdSender, frameCount);
    ++frameCount;
  }

  if((read_count == 0) || (frameCount == app.settings.maxFrames))
  {
    frameCount = 0;

    pBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
    app.input.isEOS = true;
    LOG_IMPORTANT("Waiting for EOS...");
  }
}

static OMX_ERRORTYPE onInputBufferAvailable(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer)
{
  auto app = static_cast<Application*>(pAppData);
  assert(pBuffer->nFilledLen == 0);
  assert(hComponent == app->hEncoder);

  if(app->input.isEOS)
    return OMX_ErrorNone;

  if(app->input.isFlushing)
    return OMX_ErrorNone;

  unique_lock<mutex> lock(app->mutex);
  app->cv.wait(lock, [&] { return app->read = true;
               });

  Read(pBuffer, *app);
  OMX_EmptyThisBuffer(hComponent, pBuffer);

  return OMX_ErrorNone;
}

static OMX_ERRORTYPE onOutputBufferAvailable(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBufferHdr)
{
  auto app = static_cast<Application*>(pAppData);
  assert(hComponent == app->hEncoder);

  if(app->output.isEOS)
    return OMX_ErrorNone;

  if(app->output.isFlushing)
    return OMX_ErrorNone;

  if(!pBufferHdr)
    assert(0);

  auto zMapSize = pBufferHdr->nAllocLen;

  if(zMapSize)
  {
    auto data = Buffer_MapData((char*)(pBufferHdr->pBuffer), pBufferHdr->nOffset, zMapSize, app->output.isDMA);

    if(data)
    {
      outfile.write((char*)data, pBufferHdr->nFilledLen);
      outfile.flush();
    }

    Buffer_UnmapData(data, zMapSize, app->output.isDMA);
  }

  if(pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
  {
    app->eof.notify();
    app->output.isEOS = true;
  }
  pBufferHdr->nFilledLen = 0;
  pBufferHdr->nFlags = 0;
  OMX_CALL(OMX_FillThisBuffer(hComponent, pBufferHdr));

  return OMX_ErrorNone;
}

static void useBuffers(OMX_U32 nPortIndex, bool use_dmabuf, Application& app)
{
  Getters get(&app.hEncoder);
  auto size = get.GetBuffersSize(nPortIndex);
  auto minBuf = get.GetBuffersCount(nPortIndex);
  auto isInput = ((int)nPortIndex == app.input.index);

  for(auto nbBuf = 0; nbBuf < minBuf; nbBuf++)
  {
    OMX_U8* pBufData {
      nullptr
    };

    if(use_dmabuf)
      pBufData = (OMX_U8*)(uintptr_t)AllocDmabufFd(app.pAllocator, size);
    else
      pBufData = (OMX_U8*)calloc(size, sizeof(OMX_U8));

    OMX_BUFFERHEADERTYPE* pBufHeader;
    OMX_UseBuffer(app.hEncoder, &pBufHeader, nPortIndex, &app, size, pBufData);
    isInput ? app.input.buffers.push_back(pBufHeader) : app.output.buffers.push_back(pBufHeader);
  }
}

static void allocBuffers(OMX_U32 nPortIndex, Application& app)
{
  Getters get {
    &app.hEncoder
  };
  auto size = get.GetBuffersSize(nPortIndex);
  auto minBuf = get.GetBuffersCount(nPortIndex);
  auto isInput = ((int)nPortIndex == app.input.index);

  for(auto nbBuf = 0; nbBuf < minBuf; nbBuf++)
  {
    OMX_BUFFERHEADERTYPE* pBufHeader;
    OMX_AllocateBuffer(app.hEncoder, &pBufHeader, nPortIndex, &app, size);
    isInput ? app.input.buffers.push_back(pBufHeader) : app.output.buffers.push_back(pBufHeader);
  }
}

static void freeUseBuffers(OMX_U32 nPortIndex, Application& app)
{
  Getters get(&app.hEncoder);
  auto minBuf = get.GetBuffersCount(nPortIndex);
  auto buffers = ((int)nPortIndex == app.input.index) ? app.input.buffers : app.output.buffers;
  auto isDMA = ((int)nPortIndex == app.input.index) ? app.input.isDMA : app.output.isDMA;

  for(auto nbBuf = 0; nbBuf < minBuf; nbBuf++)
  {
    auto pBufHeader = buffers.back();
    buffers.pop_back();
    Buffer_FreeData((char*)pBufHeader->pBuffer, isDMA);
    OMX_FreeBuffer(app.hEncoder, nPortIndex, pBufHeader);
  }
}

static void freeAllocBuffers(OMX_U32 nPortIndex, Application& app)
{
  Getters get(&app.hEncoder);
  auto minBuf = get.GetBuffersCount(nPortIndex);
  auto buffers = ((int)nPortIndex == app.input.index) ? app.input.buffers : app.output.buffers;

  for(auto nbBuf = 0; nbBuf < minBuf; nbBuf++)
  {
    auto pBuf = buffers.back();
    buffers.pop_back();
    OMX_FreeBuffer(app.hEncoder, nPortIndex, pBuf);
  }
}

static string chooseComponent(Codec codec)
{
  switch(codec)
  {
  case Codec::AVC:
    return "OMX.allegro.h264.encoder";
  case Codec::AVC_RISCV:
    return "OMX.allegro.h264.riscv.encoder";

  case Codec::HEVC:
    return "OMX.allegro.h265.encoder";
  case Codec::HEVC_RISCV:
    return "OMX.allegro.h265.riscv.encoder";

  default:
    assert(0);
  }
}

static OMX_ERRORTYPE safeMain(int argc, char** argv)
{
  Application app;
  SetDefaultApplication(app);
  parseCommandLine(argc, argv, app);

  infile.open(input_file, ios::binary);

  if(!infile.is_open())
  {
    cerr << "Error in opening input file '" << input_file << "'" << endl;
    return OMX_ErrorUndefined;
  }

  auto scopeInfile = scopeExit([]() {
    infile.close();
  });

  outfile.open(output_file, ios::binary);

  if(!outfile.is_open())
  {
    cerr << "Error in opening output file '" << output_file << "'" << endl;
    return OMX_ErrorUndefined;
  }

  auto scopeOutfile = scopeExit([]() {
    outfile.close();
  });

  LOG_IMPORTANT(string { "cmd file = " } +cmd_file);
  ifstream cmdfile(cmd_file != "" ? cmd_file.c_str() : "/dev/null");
  auto scopeCmdfile = scopeExit([&]() {
    cmdfile.close();
  });
  auto encCmd = CEncCmdMngr(cmdfile, 3, -1);
  app.encCmd = &encCmd;

  OMX_CALL(OMX_Init());
  auto scopeOMX = scopeExit([]() {
    OMX_Deinit();
  });
  auto component = chooseComponent(app.settings.codec);

  OMX_CALLBACKTYPE videoEncoderCallbacks;
  videoEncoderCallbacks.EventHandler = onComponentEvent;
  videoEncoderCallbacks.EmptyBufferDone = onInputBufferAvailable;
  videoEncoderCallbacks.FillBufferDone = onOutputBufferAvailable;

  OMX_ALG_COREINDEXTYPE coreType = OMX_ALG_CoreIndexUnused;
  OMX_PTR coreSettings;

  OMX_CALL(OMX_ALG_GetHandle(&app.hEncoder, (OMX_STRING)component.c_str(), &app, const_cast<OMX_CALLBACKTYPE*>(&videoEncoderCallbacks), coreType, coreSettings));
  auto scopeHandle = scopeExit([&]() {
    OMX_FreeHandle(app.hEncoder);
  });

  OMX_CALL(showComponentVersion(&app.hEncoder));

  app.pAllocator = nullptr;
  app.pRiscvContext = nullptr;

  if(app.input.isDMA || app.output.isDMA)
  {

    if(app.settings.codec == Codec::AVC_RISCV || app.settings.codec == Codec::HEVC_RISCV || app.settings.codec == Codec::MJPEG_RISCV)
    {
      app.pRiscvContext = AL_Riscv_Encode_CreateCtx(app.settings.deviceName.c_str());
      app.pAllocator = AL_Riscv_Encode_DmaAlloc_Create(app.pRiscvContext);
    }
    else
    app.pAllocator = AL_DmaAlloc_Create(app.settings.deviceName.c_str());

    if(!app.pAllocator)
      throw runtime_error(string("Couldn't create dma allocator (using ") + app.settings.deviceName + string(")"));
  }

  auto scopeAlloc = scopeExit([&]() {
    if(app.pAllocator)
      AL_Allocator_Destroy(app.pAllocator);

    if(app.pRiscvContext)
      AL_Riscv_Encode_DestroyCtx(app.pRiscvContext);
  });

  auto ret = setPortParameters(app);

  if(ret != OMX_ErrorNone)
    return ret;

  Getters get(&app.hEncoder);

  if(!isFormatSupported(app.settings.format))
  {
    LOG_ERROR(string { "Unsupported color format: " } +to_string(app.settings.format));
    return OMX_ErrorUnsupportedSetting;
  }

  /** sending command to video encoder component to go to idle state */
  OMX_SendCommand(app.hEncoder, OMX_CommandStateSet, OMX_StateIdle, nullptr);

  get.IsComponentSupplier(app.input.index) ?
  allocBuffers(app.input.index, app) : useBuffers(app.input.index, app.input.isDMA, app);

  get.IsComponentSupplier(app.output.index) ?
  allocBuffers(app.output.index, app) : useBuffers(app.output.index, app.output.isDMA, app);

  app.encoderEventState.wait();

  /** sending command to video encoder component to go to executing state */
  OMX_CALL(OMX_SendCommand(app.hEncoder, OMX_CommandStateSet, OMX_StateExecuting, nullptr));
  app.encoderEventState.wait();

  for(auto i = 0; i < get.GetBuffersCount(app.output.index); ++i)
    OMX_CALL(OMX_FillThisBuffer(app.hEncoder, app.output.buffers.at(i)));

  unique_lock<mutex> lock(app.mutex);
  app.read = false;
  lock.unlock();

  auto cmdSender = CommandsSender(app.hEncoder);
  app.cmdSender = &cmdSender;

  OMX_ALG_VIDEO_CONFIG_SEI seiPrefix;
  OMX_ALG_VIDEO_CONFIG_SEI seiSuffix;
  InitHeader(seiPrefix);
  InitHeader(seiSuffix);
  seiPrefix.nType = 15;
  seiPrefix.pBuffer = new OMX_U8[128];
  auto scopeSeiPrefix = scopeExit([&]() { delete[] seiPrefix.pBuffer; });
  seiPrefix.nOffset = 0;
  seiPrefix.nFilledLen = 128;
  seiPrefix.nAllocLen = 128;
  seiSuffix.nType = 18;
  seiSuffix.pBuffer = new OMX_U8[128];
  auto scopeSeiSuffix = scopeExit([&]() { delete[] seiSuffix.pBuffer; });
  seiSuffix.nOffset = 0;
  seiSuffix.nFilledLen = 128;
  seiSuffix.nAllocLen = 128;

  for(int i = 0; i < static_cast<int>(seiPrefix.nFilledLen); i++)
  {
    seiPrefix.pBuffer[i] = i;
    seiSuffix.pBuffer[i] = seiSuffix.nFilledLen - 1 - i;
  }

  for(auto i = 0; i < 1; ++i)
  {
    auto buf = app.input.buffers.at(i);
    Read(buf, app);

    if(app.settings.isDummySeiEnabled)
    {
      OMX_SetConfig(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexConfigVideoInsertPrefixSEI), &seiPrefix);
      OMX_SetConfig(app.hEncoder, static_cast<OMX_INDEXTYPE>(OMX_ALG_IndexConfigVideoInsertSuffixSEI), &seiSuffix);
    }
    OMX_EmptyThisBuffer(app.hEncoder, buf);

    if(app.input.isEOS)
      break;
  }

  lock.lock();
  app.read = true;
  app.cv.notify_one();
  lock.unlock();

  app.eof.wait();
  LOG_VERBOSE("EOS received\n");

  /** send flush in input port */
  app.input.isFlushing = true;
  OMX_CALL(OMX_SendCommand(app.hEncoder, OMX_CommandFlush, app.input.index, nullptr));
  app.encoderEventSem.wait();
  app.input.isFlushing = false;

  /** send flush on output port */
  app.output.isFlushing = true;
  OMX_CALL(OMX_SendCommand(app.hEncoder, OMX_CommandFlush, app.output.index, nullptr));
  app.encoderEventSem.wait();
  app.output.isFlushing = false;

  /** state change of all components from executing to idle */
  OMX_CALL(OMX_SendCommand(app.hEncoder, OMX_CommandStateSet, OMX_StateIdle, nullptr));
  app.encoderEventState.wait();

  /** sending command to all components to go to loaded state */
  OMX_CALL(OMX_SendCommand(app.hEncoder, OMX_CommandStateSet, OMX_StateLoaded, nullptr));

  /** free buffers */
  get.IsComponentSupplier(app.input.index) ? freeAllocBuffers(app.input.index, app) : freeUseBuffers(app.input.index, app);
  get.IsComponentSupplier(app.output.index) ? freeAllocBuffers(app.input.index, app) : freeUseBuffers(app.output.index, app);

  app.encoderEventState.wait();

  return OMX_ErrorNone;
}

int main(int argc, char** argv)
{
  try
  {
    if(safeMain(argc, argv) != OMX_ErrorNone)
    {
      cerr << "Fatal error" << endl;
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  catch(runtime_error const& error)
  {
    cerr << endl << "Exception caught: " << error.what() << endl;
    return EXIT_FAILURE;
  }
}

// SPDX-FileCopyrightText: © 2024 Allegro DVT <github-ip@allegrodvt.com>
// SPDX-License-Identifier: MIT

#include "omx_wrapper_codec_entry_point.h"
#include <string>
#include <utility/logger.h>
#include <utility/omx_translate.h>

using namespace std;

OMXComponentInterface* GetThis(OMX_IN OMX_HANDLETYPE hComponent)
{
  if(!hComponent)
    return nullptr;
  return static_cast<OMXComponentInterface*>((((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate));
}

OMX_ERRORTYPE SendCommand(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_COMMANDTYPE Cmd, OMX_IN OMX_U32 nParam1, OMX_IN OMX_PTR pCmdData)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", Cmd: " } +ToStringOMXCommand(Cmd) + string { ", nParam1: " } +to_string(nParam1) + string { ", pCmdData: " } +ToStringAddr(pCmdData));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->SendCommand(Cmd, nParam1, pCmdData);
}

OMX_ERRORTYPE GetState(OMX_IN OMX_HANDLETYPE hComponent, OMX_OUT OMX_STATETYPE* pState)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", pState: " } +ToStringAddr(pState));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->GetState(pState);
}

OMX_ERRORTYPE SetCallbacks(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_CALLBACKTYPE* pCallbacks, OMX_IN OMX_PTR pAppData)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", pCallbacks: " } +ToStringAddr(pCallbacks) + string { ", pAppData: " } +ToStringAddr(pAppData));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->SetCallbacks(pCallbacks, pAppData);
}

OMX_ERRORTYPE GetParameter(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pParam)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", nParamIndex: " } +ToStringOMXIndex(nParamIndex) + string { ", pParam: " } +ToStringAddr(pParam));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->GetParameter(nParamIndex, pParam);
}

OMX_ERRORTYPE SetParameter(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_IN OMX_PTR pParam)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", nParamIndex: " } +ToStringOMXIndex(nParamIndex) + string { ", pParam: " } +ToStringAddr(pParam));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->SetParameter(nParamIndex, pParam);
}

OMX_ERRORTYPE UseBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_OUT OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_IN OMX_U32 nPortIndex, OMX_IN OMX_PTR pAppPrivate, OMX_IN OMX_U32 nSizeBytes, OMX_IN OMX_U8* pBuffer)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", ppBufferHdr: " } +ToStringAddr(ppBufferHdr) + string { ", nPortIndex: " } +to_string(nPortIndex) + string { ", pAppPrivate: " } +ToStringAddr(pAppPrivate) + string { ", nSizeBytes: " } +to_string(nSizeBytes) + string { ", pBuffer: " } +ToStringAddr(pBuffer));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->UseBuffer(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
}

OMX_ERRORTYPE AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_IN OMX_U32 nPortIndex, OMX_IN OMX_PTR pAppPrivate, OMX_IN OMX_U32 nSizeBytes)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", ppBufferHdr: " } +ToStringAddr(ppBufferHdr) + string { ", nPortIndex: " } +to_string(nPortIndex) + string { ", pAppPrivate: " } +ToStringAddr(pAppPrivate) + string { ", nSizeBytes: " } +to_string(nSizeBytes));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->AllocateBuffer(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes);
}

OMX_ERRORTYPE FreeBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_U32 nPortIndex, OMX_IN OMX_BUFFERHEADERTYPE* pBufferHdr)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", nPortIndex: " } +to_string(nPortIndex) + string { ", pBufferHdr: " } +ToStringAddr(pBufferHdr));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->FreeBuffer(nPortIndex, pBufferHdr);
}

OMX_ERRORTYPE EmptyThisBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_BUFFERHEADERTYPE* pBufferHdr)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", pBufferHdr: " } +ToStringAddr(pBufferHdr));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->EmptyThisBuffer(pBufferHdr);
}

OMX_ERRORTYPE FillThisBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_BUFFERHEADERTYPE* pBufferHdr)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", pBufferHdr: " } +ToStringAddr(pBufferHdr));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->FillThisBuffer(pBufferHdr);
}

OMX_ERRORTYPE GetComponentVersion(OMX_IN OMX_HANDLETYPE hComponent, OMX_OUT OMX_STRING pComponentName, OMX_OUT OMX_VERSIONTYPE* pComponentVersion, OMX_OUT OMX_VERSIONTYPE* pSpecVersion, OMX_OUT OMX_UUIDTYPE pComponentUUID[128])
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", pComponentName: " } +ToStringAddr(pComponentName) + string { ", pComponentVersion: " } +ToStringAddr(pComponentVersion) + string { ", pSpecVersion: " } +ToStringAddr(pSpecVersion) + string { ", pComponentUUID: " } +ToStringAddr(pComponentUUID));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->GetComponentVersion(pComponentName, pComponentVersion, pSpecVersion);
}

OMX_ERRORTYPE GetConfig(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nConfigIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", nConfigIndex: " } +ToStringOMXIndex(nConfigIndex) + string { ", pComponentConfigStructure: " } +ToStringAddr(pComponentConfigStructure));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->GetConfig(nConfigIndex, pComponentConfigStructure);
}

OMX_ERRORTYPE SetConfig(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nConfigIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", nConfigIndex: " } +ToStringOMXIndex(nConfigIndex) + string { ", pComponentConfigStructure: " } +ToStringAddr(pComponentConfigStructure));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->SetConfig(nConfigIndex, pComponentConfigStructure);
}

OMX_ERRORTYPE GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE* pIndexType)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", cParameterName: " } +cParameterName + string { ", pIndexType: " } +ToStringAddr(pIndexType));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->GetExtensionIndex(cParameterName, pIndexType);
}

OMX_ERRORTYPE ComponentTunnelRequest(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_U32 nPort, OMX_IN OMX_HANDLETYPE hTunneledComp, OMX_IN OMX_U32 nTunneledPort, OMX_INOUT OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", nPort: " } +to_string(nPort) + string { ", hTunneledComp: " } +ToStringAddr(hTunneledComp) + string { ", nTunneledPort: " } +to_string(nTunneledPort) + string { ", pTunnelSetup: " } +ToStringAddr(pTunnelSetup));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->ComponentTunnelRequest(nPort, hTunneledComp, nTunneledPort, pTunnelSetup);
}

OMX_ERRORTYPE UseEGLImage(OMX_IN OMX_HANDLETYPE hComponent, OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_IN OMX_U32 nPortIndex, OMX_IN OMX_PTR pAppPrivate, OMX_IN void* eglImage)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", ppBufferHdr: " } +ToStringAddr(ppBufferHdr) + string { ", nPortIndex: " } +to_string(nPortIndex) + string { ", pAppPrivate: " } +ToStringAddr(pAppPrivate) + string { ", eglImage: " } +ToStringAddr(eglImage));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->UseEGLImage(ppBufferHdr, nPortIndex, pAppPrivate, eglImage);
}

OMX_ERRORTYPE ComponentRoleEnum(OMX_IN OMX_HANDLETYPE hComponent, OMX_OUT OMX_U8* cRole, OMX_IN OMX_U32 nIndex)
{
  LOG_IMPORTANT(string { "hComponent: " } +ToStringAddr(hComponent) + string { ", cRole: " } +ToStringAddr(cRole) + string { ", nIndex: " } +to_string(nIndex));
  auto pThis = GetThis(hComponent);

  if(!pThis)
    return OMX_ErrorBadParameter;
  return pThis->ComponentRoleEnum(cRole, nIndex);
}

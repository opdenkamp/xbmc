/*
 *      Copyright (C) 2012-2013 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "Application.h"
#include "Addon.h"
#include "AddonCallbacksCodec.h"
#include "cores/dvdplayer/DVDCodecs/DVDCodecs.h"

namespace ADDON
{

CAddonCallbacksCodec::CAddonCallbacksCodec(CAddon* addon)
{
  m_addon     = addon;
  m_callbacks = new CB_CODECLib;

  /* write XBMC addon-on specific add-on function addresses to the callback table */
  m_callbacks->GetCodecId         = GetCodecId;
  m_callbacks->CodecIsVideo       = CodecIsVideo;
  m_callbacks->CodecIsAudio       = CodecIsAudio;
  m_callbacks->CodecIsData        = CodecIsData;
  m_callbacks->CodecIsSubtitle    = CodecIsSubtitle;
  m_callbacks->CodecIsAttachment  = CodecIsAttachment;
  m_callbacks->CodecIsUnknown     = CodecIsUnknown;
}

CAddonCallbacksCodec::~CAddonCallbacksCodec()
{
  /* delete the callback table */
  delete m_callbacks;
}

xbmc_codec_t CAddonCallbacksCodec::GetCodecId(const void* addonData, const char* strCodecName)
{
  (void)addonData;
  xbmc_codec_t retVal = XBMC_INVALID_CODEC;

  static const char* strCodecNameAC3        = "AC3";
  static const char* strCodecNameEAC3       = "EAC3";
  static const char* strCodecNameMPEG2AUDIO = "MPEG2AUDIO";
  static const char* strCodecNameAAC        = "AAC";
  static const char* strCodecNameAACLATM    = "AACLATM";
  static const char* strCodecNameVORBIS     = "VORBIS";
  static const char* strCodecNameDTS        = "DTS";

  static const char* strCodecNameMPEG2VIDEO = "MPEG2VIDEO";
  static const char* strCodecNameH264       = "H264";
  static const char* strCodecNameVP8        = "VP8";
  static const char* strCodecNameMPEG4VIDEO = "MPEG4VIDEO";

  static const char* strCodecNameDVDSUB     = "DVBSUB";
  static const char* strCodecNameTEXTSUB    = "TEXTSUB";
  static const char* strCodecNameTELETEXT   = "TELETEXT";

  // audio types
  if(!strcmp(strCodecName, strCodecNameAC3))
  {
    retVal.codec_type = AVMEDIA_TYPE_AUDIO;
    retVal.codec_id   = CODEC_ID_AC3;
    retVal.name       = strCodecNameAC3;
  }
  else if(!strcmp(strCodecName, strCodecNameEAC3))
  {
    retVal.codec_type = AVMEDIA_TYPE_AUDIO;
    retVal.codec_id   = CODEC_ID_EAC3;
    retVal.name       = strCodecNameEAC3;
  }
  else if(!strcmp(strCodecName, strCodecNameMPEG2AUDIO))
  {
    retVal.codec_type = AVMEDIA_TYPE_AUDIO;
    retVal.codec_id   = CODEC_ID_MP2;
    retVal.name       = strCodecNameMPEG2AUDIO;
  }
  else if(!strcmp(strCodecName, strCodecNameAAC))
  {
    retVal.codec_type = AVMEDIA_TYPE_AUDIO;
    retVal.codec_id   = CODEC_ID_AAC;
    retVal.name       = strCodecNameAAC;
  }
  else if(!strcmp(strCodecName, strCodecNameAACLATM))
  {
    retVal.codec_type = AVMEDIA_TYPE_AUDIO;
    retVal.codec_id   = CODEC_ID_AAC_LATM;
    retVal.name       = strCodecNameAACLATM;
  }
  else if(!strcmp(strCodecName, strCodecNameVORBIS))
  {
    retVal.codec_type = AVMEDIA_TYPE_AUDIO;
    retVal.codec_id   = CODEC_ID_VORBIS;
    retVal.name       = strCodecNameVORBIS;
  }
  else if(!strcmp(strCodecName, strCodecNameDTS))
  {
    retVal.codec_type = AVMEDIA_TYPE_AUDIO;
    retVal.codec_id   = CODEC_ID_DTS;
    retVal.name       = strCodecNameDTS;
  }

  // video types
  else if(!strcmp(strCodecName, strCodecNameMPEG2VIDEO))
  {
    retVal.codec_type = AVMEDIA_TYPE_VIDEO;
    retVal.codec_id   = CODEC_ID_MPEG2VIDEO;
    retVal.name       = strCodecNameMPEG2VIDEO;
  }
  else if(!strcmp(strCodecName, strCodecNameH264))
  {
    retVal.codec_type = AVMEDIA_TYPE_VIDEO;
    retVal.codec_id   = CODEC_ID_H264;
    retVal.name       = strCodecNameH264;
  }
  else if(!strcmp(strCodecName, strCodecNameVP8))
  {
    retVal.codec_type = AVMEDIA_TYPE_VIDEO;
    retVal.codec_id   = CODEC_ID_VP8;
    retVal.name       = strCodecNameVP8;
  }
  else if(!strcmp(strCodecName, strCodecNameMPEG4VIDEO))
  {
    retVal.codec_type = AVMEDIA_TYPE_VIDEO;
    retVal.codec_id   = CODEC_ID_MPEG4;
    retVal.name       = strCodecNameMPEG4VIDEO;
  }

  // subtitle types
  else if(!strcmp(strCodecName, strCodecNameDVDSUB))
  {
    retVal.codec_type = AVMEDIA_TYPE_SUBTITLE;
    retVal.codec_id   = CODEC_ID_DVB_SUBTITLE;
    retVal.name       = strCodecNameDVDSUB;
  }
  else if(!strcmp(strCodecName, strCodecNameTEXTSUB))
  {
    retVal.codec_type = AVMEDIA_TYPE_SUBTITLE;
    retVal.codec_id   = CODEC_ID_TEXT;
    retVal.name       = strCodecNameTEXTSUB;
  }
  else if(!strcmp(strCodecName, strCodecNameTELETEXT))
  {
    retVal.codec_type = AVMEDIA_TYPE_SUBTITLE;
    retVal.codec_id   = CODEC_ID_DVB_TELETEXT;
    retVal.name       = strCodecNameTELETEXT;
  }

  return retVal;
}

bool CAddonCallbacksCodec::CodecIsVideo(const void* addonData, xbmc_codec_type_t type)
{
  (void)addonData;
  return type == AVMEDIA_TYPE_VIDEO;
}

bool CAddonCallbacksCodec::CodecIsAudio(const void* addonData, xbmc_codec_type_t type)
{
  (void)addonData;
  return type == AVMEDIA_TYPE_AUDIO;
}

bool CAddonCallbacksCodec::CodecIsData(const void* addonData, xbmc_codec_type_t type)
{
  (void)addonData;
  return type == AVMEDIA_TYPE_DATA;
}

bool CAddonCallbacksCodec::CodecIsSubtitle(const void* addonData, xbmc_codec_type_t type)
{
  (void)addonData;
  return type == AVMEDIA_TYPE_SUBTITLE;
}

bool CAddonCallbacksCodec::CodecIsAttachment(const void* addonData, xbmc_codec_type_t type)
{
  (void)addonData;
  return type == AVMEDIA_TYPE_ATTACHMENT;
}

bool CAddonCallbacksCodec::CodecIsUnknown(const void* addonData, xbmc_codec_type_t type)
{
  (void)addonData;
  return type == AVMEDIA_TYPE_UNKNOWN;
}

}; /* namespace ADDON */


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
#include "DllAvCodec.h"
#include "DllAvFormat.h"

namespace ADDON
{
class CCodecIds
{
public:
  virtual ~CCodecIds(void) {}

  static CCodecIds& Get(void)
  {
    static CCodecIds _instance;
    return _instance;
  }

  xbmc_codec_t GetCodecByName(const char* strCodecName)
  {
    xbmc_codec_t retVal = XBMC_INVALID_CODEC;
    if (strlen(strCodecName) == 0)
      return retVal;

    std::map<std::string, xbmc_codec_t>::const_iterator it = m_lookup.find(CStdString(strCodecName).ToUpper());
    if (it != m_lookup.end())
      retVal = it->second;

    return retVal;
  }

private:
  CCodecIds(void)
  {
    // load ffmpeg and register formats
    if (!m_dllAvCodec.Load() || !m_dllAvFormat.Load())
    {
      CLog::Log(LOGWARNING, "failed to load ffmpeg");
      return;
    }
    m_dllAvFormat.av_register_all();

    // get ids and names
    AVCodec* codec = NULL;
    xbmc_codec_t tmp;
    while ((codec = m_dllAvCodec.av_codec_next(codec)))
    {
      if (m_dllAvCodec.av_codec_is_decoder(codec))
      {
        tmp.codec_type = (xbmc_codec_type_t)codec->type;
        tmp.codec_id   = codec->id;
        m_lookup.insert(std::make_pair(CStdString(codec->name).ToUpper(), tmp));
      }
    }

    // teletext is not returned by av_codec_next. we got our own decoder
    tmp.codec_type = XBMC_CODEC_TYPE_DATA;
    tmp.codec_id   = AV_CODEC_ID_DVB_TELETEXT;
    m_lookup.insert(std::make_pair("TELETEXT", tmp));
  }

  DllAvCodec                          m_dllAvCodec;
  DllAvFormat                         m_dllAvFormat;
  std::map<std::string, xbmc_codec_t> m_lookup;
};

CAddonCallbacksCodec::CAddonCallbacksCodec(CAddon* addon)
{
  m_addon     = addon;
  m_callbacks = new CB_CODECLib;

  /* write XBMC addon-on specific add-on function addresses to the callback table */
  m_callbacks->GetCodecByName   = GetCodecByName;
}

CAddonCallbacksCodec::~CAddonCallbacksCodec()
{
  /* delete the callback table */
  delete m_callbacks;
}

xbmc_codec_t CAddonCallbacksCodec::GetCodecByName(const void* addonData, const char* strCodecName)
{
  (void)addonData;
  return CCodecIds::Get().GetCodecByName(strCodecName);
}

}; /* namespace ADDON */


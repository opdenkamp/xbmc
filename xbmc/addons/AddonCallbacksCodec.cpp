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

  xbmc_codec_t GetCodecId(const char* strCodecName)
  {
    xbmc_codec_t retVal = XBMC_INVALID_CODEC;

    std::map<std::string, xbmc_codec_t>::const_iterator it = m_lookup.find(std::string(strCodecName));
    if (it != m_lookup.end())
    {
      retVal = it->second;
    }
    else
    {
      AVCodec* codec = NULL;
      while ((codec = m_dllAvCodec.av_codec_next(codec)))
      {
        if (!strcmp(codec->name, strCodecName))
        {
          retVal.codec_type = (xbmc_codec_type_t)codec->type;
          retVal.codec_id   = codec->id;
          m_lookup.insert(std::make_pair(std::string(strCodecName), retVal));
        }
      }
    }

    return retVal;
  }

private:
  CCodecIds(void)
  {
    m_dllAvCodec.Load();
  }

  DllAvCodec                          m_dllAvCodec;
  std::map<std::string, xbmc_codec_t> m_lookup;
};

CAddonCallbacksCodec::CAddonCallbacksCodec(CAddon* addon)
{
  m_addon     = addon;
  m_callbacks = new CB_CODECLib;

  /* write XBMC addon-on specific add-on function addresses to the callback table */
  m_callbacks->GetCodecId         = GetCodecId;
}

CAddonCallbacksCodec::~CAddonCallbacksCodec()
{
  /* delete the callback table */
  delete m_callbacks;
}

xbmc_codec_t CAddonCallbacksCodec::GetCodecId(const void* addonData, const char* strCodecName)
{
  (void)addonData;
  return CCodecIds::Get().GetCodecId(strCodecName);
}

}; /* namespace ADDON */


#pragma once
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

#include "AddonCallbacks.h"

namespace ADDON
{

class CAddonCallbacksCodec
{
public:
  CAddonCallbacksCodec(CAddon* addon);
  ~CAddonCallbacksCodec();

  /*!
   * @return The callback table.
   */
  CB_CODECLib *GetCallbacks() { return m_callbacks; }

  static xbmc_codec_t GetCodecId(const void* addonData, const char* strCodecName);
  static bool         CodecIsVideo(const void* addonData, xbmc_codec_type_t type);
  static bool         CodecIsAudio(const void* addonData, xbmc_codec_type_t type);
  static bool         CodecIsData(const void* addonData, xbmc_codec_type_t type);
  static bool         CodecIsSubtitle(const void* addonData, xbmc_codec_type_t type);
  static bool         CodecIsAttachment(const void* addonData, xbmc_codec_type_t type);
  static bool         CodecIsUnknown(const void* addonData, xbmc_codec_type_t type);

private:
  CB_CODECLib*                           m_callbacks; /*!< callback addresses */
  CAddon*                                m_addon;     /*!< the add-on */
};

}; /* namespace ADDON */


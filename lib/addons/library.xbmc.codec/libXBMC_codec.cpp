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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include "../../../addons/library.xbmc.codec/libXBMC_codec.h"
#include "addons/AddonCallbacks.h"

#ifdef _WIN32
#include <windows.h>
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

using namespace std;

extern "C"
{

DLLEXPORT void* CODEC_register_me(void *hdl)
{
  CB_CODECLib *cb = NULL;
  if (!hdl)
    fprintf(stderr, "libXBMC_codec-ERROR: %s is called with NULL handle\n", __FUNCTION__);
  else
  {
    cb = ((AddonCB*)hdl)->CODECLib_RegisterMe(((AddonCB*)hdl)->addonData);
    if (!cb)
      fprintf(stderr, "libXBMC_codec-ERROR: %s can't get callback table from XBMC\n", __FUNCTION__);
  }
  return cb;
}

DLLEXPORT void CODEC_unregister_me(void *hdl, void* cb)
{
  if (hdl && cb)
    ((AddonCB*)hdl)->CODECLib_UnRegisterMe(((AddonCB*)hdl)->addonData, (CB_CODECLib*)cb);
}

DLLEXPORT xbmc_codec_t CODEC_get_codec_id(void *hdl, void* cb, const char* strCodecName)
{
  xbmc_codec_t retVal;
  retVal.codec_id   = XBMC_INVALID_CODEC_ID;
  retVal.codec_type = XBMC_INVALID_CODEC_TYPE;

  if (cb != NULL)
    retVal = ((CB_CODECLib*)cb)->GetCodecId(((AddonCB*)hdl)->addonData, strCodecName);

  return retVal;
}

DLLEXPORT bool CODEC_codec_is_video(void *hdl, void* cb, xbmc_codec_type_t type)
{
  if (cb != NULL)
    return ((CB_CODECLib*)cb)->CodecIsVideo(((AddonCB*)hdl)->addonData, type);
  return false;
}

DLLEXPORT bool CODEC_codec_is_audio(void *hdl, void* cb, xbmc_codec_type_t type)
{
  if (cb != NULL)
    return ((CB_CODECLib*)cb)->CodecIsAudio(((AddonCB*)hdl)->addonData, type);
  return false;
}

DLLEXPORT bool CODEC_codec_is_data(void *hdl, void* cb, xbmc_codec_type_t type)
{
  if (cb != NULL)
    return ((CB_CODECLib*)cb)->CodecIsData(((AddonCB*)hdl)->addonData, type);
  return false;
}

DLLEXPORT bool CODEC_codec_is_subtitle(void *hdl, void* cb, xbmc_codec_type_t type)
{
  if (cb != NULL)
    return ((CB_CODECLib*)cb)->CodecIsSubtitle(((AddonCB*)hdl)->addonData, type);
  return false;
}

DLLEXPORT bool CODEC_codec_is_attachment(void *hdl, void* cb, xbmc_codec_type_t type)
{
  if (cb != NULL)
    return ((CB_CODECLib*)cb)->CodecIsAttachment(((AddonCB*)hdl)->addonData, type);
  return false;
}

DLLEXPORT bool CODEC_codec_is_unknown(void *hdl, void* cb, xbmc_codec_type_t type)
{
  if (cb != NULL)
    return ((CB_CODECLib*)cb)->CodecIsUnknown(((AddonCB*)hdl)->addonData, type);
  return false;
}

};

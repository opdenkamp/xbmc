#pragma once
/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#include <string>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "xbmc_codec_types.h"
#include "../library.xbmc.addon/libXBMC_addon.h"

#ifdef _WIN32
#define CODEC_HELPER_DLL "\\library.xbmc.codec\\libXBMC_codec" ADDON_HELPER_EXT
#else
#define CODEC_HELPER_DLL_NAME "libXBMC_codec-" ADDON_HELPER_ARCH ADDON_HELPER_EXT
#define CODEC_HELPER_DLL "/library.xbmc.codec/" CODEC_HELPER_DLL_NAME
#endif

class CHelper_libXBMC_codec
{
public:
  CHelper_libXBMC_codec(void)
  {
    m_libXBMC_codec = NULL;
    m_Handle        = NULL;
  }

  ~CHelper_libXBMC_codec(void)
  {
    if (m_libXBMC_codec)
    {
      CODEC_unregister_me(m_Handle, m_Callbacks);
      dlclose(m_libXBMC_codec);
    }
  }

  /*!
   * @brief Resolve all callback methods
   * @param handle Pointer to the add-on
   * @return True when all methods were resolved, false otherwise.
   */
  bool RegisterMe(void* handle)
  {
    m_Handle = handle;

    std::string libBasePath;
    libBasePath  = ((cb_array*)m_Handle)->libPath;
    libBasePath += CODEC_HELPER_DLL;

#if defined(ANDROID)
      struct stat st;
      if(stat(libBasePath.c_str(),&st) != 0)
      {
        std::string tempbin = getenv("XBMC_ANDROID_LIBS");
        libBasePath = tempbin + "/" + CODEC_HELPER_DLL_NAME;
      }
#endif

    m_libXBMC_codec = dlopen(libBasePath.c_str(), RTLD_LAZY);
    if (m_libXBMC_codec == NULL)
    {
      fprintf(stderr, "Unable to load %s\n", dlerror());
      return false;
    }

    CODEC_register_me = (void* (*)(void *HANDLE))
      dlsym(m_libXBMC_codec, "CODEC_register_me");
    if (CODEC_register_me == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    CODEC_unregister_me = (void (*)(void* HANDLE, void* CB))
      dlsym(m_libXBMC_codec, "CODEC_unregister_me");
    if (CODEC_unregister_me == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    CODEC_get_codec_id = (xbmc_codec_t (*)(void* HANDLE, void* CB, const char* strCodecName))
        dlsym(m_libXBMC_codec, "CODEC_get_codec_id");
    if (CODEC_get_codec_id == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    CODEC_codec_is_video = (bool (*)(void *HANDLE, void* CB, xbmc_codec_type_t type))
        dlsym(m_libXBMC_codec, "CODEC_codec_is_video");
    if (CODEC_codec_is_video == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    CODEC_codec_is_audio = (bool (*)(void *HANDLE, void* CB, xbmc_codec_type_t type))
        dlsym(m_libXBMC_codec, "CODEC_codec_is_audio");
    if (CODEC_codec_is_audio == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    CODEC_codec_is_data = (bool (*)(void *HANDLE, void* CB, xbmc_codec_type_t type))
        dlsym(m_libXBMC_codec, "CODEC_codec_is_data");
    if (CODEC_codec_is_data == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    CODEC_codec_is_subtitle = (bool (*)(void *HANDLE, void* CB, xbmc_codec_type_t type))
        dlsym(m_libXBMC_codec, "CODEC_codec_is_subtitle");
    if (CODEC_codec_is_subtitle == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    CODEC_codec_is_attachment = (bool (*)(void *HANDLE, void* CB, xbmc_codec_type_t type))
        dlsym(m_libXBMC_codec, "CODEC_codec_is_attachment");
    if (CODEC_codec_is_attachment == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    CODEC_codec_is_unknown = (bool (*)(void *HANDLE, void* CB, xbmc_codec_type_t type))
        dlsym(m_libXBMC_codec, "CODEC_codec_is_unknown");
    if (CODEC_codec_is_unknown == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    m_Callbacks = CODEC_register_me(m_Handle);
    return m_Callbacks != NULL;
  }

  /*!
   * @brief Get the codec id used by XBMC
   * @param strCodecName The name of the codec, in uppercase
   * @return The codec_id, or a codec_id with 0 values when not supported
   */
  xbmc_codec_t GetCodecId(const char* strCodecName)
  {
    return CODEC_get_codec_id(m_Handle, m_Callbacks, strCodecName);
  }

  bool CodecIsVideo(xbmc_codec_type_t type)
  {
    return CODEC_codec_is_video(m_Handle, m_Callbacks, type);
  }

  bool CodecIsAudio(xbmc_codec_type_t type)
  {
    return CODEC_codec_is_audio(m_Handle, m_Callbacks, type);
  }

  bool CodecIsData(xbmc_codec_type_t type)
  {
    return CODEC_codec_is_data(m_Handle, m_Callbacks, type);
  }

  bool CodecIsSubtitle(xbmc_codec_type_t type)
  {
    return CODEC_codec_is_subtitle(m_Handle, m_Callbacks, type);
  }

  bool CodecIsAttachment(xbmc_codec_type_t type)
  {
    return CODEC_codec_is_attachment(m_Handle, m_Callbacks, type);
  }

  bool CodecIsUnknown(xbmc_codec_type_t type)
  {
    return CODEC_codec_is_unknown(m_Handle, m_Callbacks, type);
  }

protected:
  void* (*CODEC_register_me)(void*);
  void (*CODEC_unregister_me)(void*, void*);
  xbmc_codec_t (*CODEC_get_codec_id)(void *HANDLE, void* CB, const char* strCodecName);
  bool (*CODEC_codec_is_video)(void *HANDLE, void* CB, xbmc_codec_type_t type);
  bool (*CODEC_codec_is_audio)(void *HANDLE, void* CB, xbmc_codec_type_t type);
  bool (*CODEC_codec_is_data)(void *HANDLE, void* CB, xbmc_codec_type_t type);
  bool (*CODEC_codec_is_subtitle)(void *HANDLE, void* CB, xbmc_codec_type_t type);
  bool (*CODEC_codec_is_attachment)(void *HANDLE, void* CB, xbmc_codec_type_t type);
  bool (*CODEC_codec_is_unknown)(void *HANDLE, void* CB, xbmc_codec_type_t type);

private:
  void* m_libXBMC_codec;
  void* m_Handle;
  void* m_Callbacks;
  struct cb_array
  {
    const char* libPath;
  };
};


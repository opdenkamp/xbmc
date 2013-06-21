#ifndef __XBMC_CODEC_TYPES_H__
#define __XBMC_CODEC_TYPES_H__

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

#ifdef __cplusplus
extern "C" {
#endif

typedef int xbmc_codec_type_t;
typedef int xbmc_codec_id_t;

typedef struct
{
  xbmc_codec_type_t codec_type;
  xbmc_codec_id_t   codec_id;
  const char*       name;
} xbmc_codec_t;

#define XBMC_INVALID_CODEC      { -1, 0, NULL }
#define XBMC_INVALID_CODEC_ID   0
#define XBMC_INVALID_CODEC_TYPE (-1)

#ifdef __cplusplus
};
#endif

#endif


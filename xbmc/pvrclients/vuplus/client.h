#pragma once
/*
 *      Copyright (C) 2005-2011 Team XBMC
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

#include "../../../addons/library.xbmc.addon/libXBMC_addon.h"
#include "../../../addons/library.xbmc.pvr/libXBMC_pvr.h"

#define DEFAULT_HOST             "127.0.0.1"
#define DEFAULT_CONNECT_TIMEOUT  30
#define DEFAULT_RESPONSE_TIMEOUT 3
#define DEFAULT_STREAM_PORT      8001 
#define DEFAULT_UPDATE_INTERVAL  2

extern bool                      m_bCreated;
extern std::string               g_strHostname;
extern int                       g_iPortStream;
extern std::string               g_strUsername;
extern std::string               g_strPassword;
extern std::string               g_strIconPath;
extern int                       g_iConnectTimeout;
extern int                       g_iResponseTimeout;
extern int 			 g_iUpdateInterval;
extern int                       g_iClientId;
extern unsigned int              g_iPacketSequence;
extern bool                      g_bShowTimerNotifications;
extern bool 	                 g_bShowTimersCompleted;
extern std::string               g_szUserPath;
extern std::string               g_szClientPath;
extern ADDON::CHelper_libXBMC_addon *   XBMC;
extern CHelper_libXBMC_pvr *     PVR;

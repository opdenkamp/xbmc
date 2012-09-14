#pragma once
/*
 *      Copyright (C) 2005-2012 Team XBMC
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

class CVariant;
namespace ANNOUNCEMENT
{
  enum AnnouncementFlag
  {
    Player        = 0x0001,
    GUI           = 0x0002,
    System        = 0x0004,
    VideoLibrary  = 0x0008,
    AudioLibrary  = 0x0010,
    Application   = 0x0020,
    Input         = 0x0040,
    PVR           = 0x0080,
    Other         = 0x0100
  };

  #define ANNOUNCE_ALL (Player | GUI | System | VideoLibrary | AudioLibrary | Application | Input | ANNOUNCEMENT::PVR | Other)

  /*!
    \brief Returns a string representation for the 
    given AnnouncementFlag
    \param notification Specific AnnouncementFlag
    \return String representation of the given AnnouncementFlag
    */
  inline const char *AnnouncementFlagToString(const AnnouncementFlag &notification)
  {
    switch (notification)
    {
    case Player:
      return "Player";
    case GUI:
      return "GUI";
    case System:
      return "System";
    case VideoLibrary:
      return "VideoLibrary";
    case AudioLibrary:
      return "AudioLibrary";
    case Application:
      return "Application";
    case Input:
      return "Input";
    case ANNOUNCEMENT::PVR:
      return "PVR";
    case Other:
      return "Other";
    default:
      return "Unknown";
    }
  }

  class IAnnouncer
  {
  public:
    IAnnouncer() { };
    virtual ~IAnnouncer() { };
    virtual void Announce(AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data) = 0;
  };
}

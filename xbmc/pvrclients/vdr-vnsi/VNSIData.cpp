/*
 *      Copyright (C) 2010 Alwin Esch (Team XBMC)
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

#include <unistd.h>
#include "VNSIData.h"
#include "responsepacket.h"
#include "requestpacket.h"
#include "vdrcommand.h"

#ifdef __WINDOWS__
#include <winsock2.h>
#undef SendMessage
#else
#include <arpa/inet.h>
#endif

#define CMD_LOCK cMutexLock CmdLock((cMutex*)&m_Mutex)

cVNSIData::cVNSIData() : m_connectionLost(false)
{
}

cVNSIData::~cVNSIData()
{
  Close();
}

bool cVNSIData::Open(const std::string& hostname, int port)
{
  if(!m_session.Open(hostname, port))
    return false;

  // store connection data for TryReconnect()
  m_hostname = hostname;
  m_port = port;

  SetDescription("VNSI Data Listener");
  Start();
  return true;
}

void cVNSIData::Close()
{
  Cancel(1);
  m_session.Abort();
  m_session.Close();
}

bool cVNSIData::TryReconnect() {
  m_session.Abort();
  m_session.Close();

  if(!Open(m_hostname, m_port)) {
    return false;
  }

  XBMC->Log(LOG_DEBUG, "%s - reconnected", __FUNCTION__);

  PVR->TriggerTimerUpdate();
  PVR->TriggerRecordingUpdate();
  PVR->TriggerChannelUpdate();

  return true;
}

cResponsePacket* cVNSIData::ReadResult(cRequestPacket* vrp)
{
  m_Mutex.Lock();

  SMessage &message(m_queue[vrp->getSerial()]);
  message.event = new cCondWait();
  message.pkt   = NULL;

  m_Mutex.Unlock();

  if(!m_session.SendMessage(vrp))
  {
    m_queue.erase(vrp->getSerial());
    return NULL;
  }

  message.event->Wait(g_iConnectTimeout * 1000);

  m_Mutex.Lock();

  cResponsePacket* vresp = message.pkt;
  delete message.event;

  m_queue.erase(vrp->getSerial());

  m_Mutex.Unlock();

  return vresp;
}

bool cVNSIData::GetTime(time_t *localTime, int *gmtOffset)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_GETTIME))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return false;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t vdrTime       = vresp->extract_U32();
  int32_t  vdrTimeOffset = vresp->extract_S32();

  *localTime = vdrTime;
  *gmtOffset = vdrTimeOffset;

  delete vresp;
  return true;
}

bool cVNSIData::GetDriveSpace(long long *total, long long *used)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_RECORDINGS_DISKSIZE))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return false;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t totalspace    = vresp->extract_U32();
  uint32_t freespace     = vresp->extract_U32();
  /* vresp->extract_U32(); percent not used */

  *total = totalspace;
  *used  = (totalspace - freespace);

  /* Convert from kBytes to Bytes */
  *total *= 1024;
  *used  *= 1024;

  delete vresp;
  return true;
}

bool cVNSIData::SupportChannelScan()
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_SCAN_SUPPORTED))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return false;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t ret = vresp->extract_U32();
  delete vresp;
  return ret == VDR_RET_OK ? true : false;
}

bool cVNSIData::EnableStatusInterface(bool onOff)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_ENABLESTATUSINTERFACE)) return false;
  if (!vrp.add_U8(onOff)) return false;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t ret = vresp->extract_U32();
  delete vresp;
  return ret == VDR_RET_OK ? true : false;
}

bool cVNSIData::EnableOSDInterface(bool onOff)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_ENABLEOSDINTERFACE)) return false;
  if (!vrp.add_U8(onOff)) return false;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t ret = vresp->extract_U32();
  delete vresp;
  return ret == VDR_RET_OK ? true : false;
}

int cVNSIData::GetChannelsCount()
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_CHANNELS_GETCOUNT))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return -1;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return -1;
  }

  uint32_t count = vresp->extract_U32();

  delete vresp;
  return count;
}

bool cVNSIData::GetChannelsList(PVR_HANDLE handle, bool radio)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_CHANNELS_GETCHANNELS))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return false;
  }
  if (!vrp.add_U32(radio))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't add parameter to cRequestPacket", __FUNCTION__);
    return false;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  while (!vresp->end())
  {
    PVR_CHANNEL tag;
    memset(&tag, 0 , sizeof(tag));

    tag.iChannelNumber    = vresp->extract_U32();
    tag.strChannelName    = vresp->extract_String();
    tag.iUniqueId         = vresp->extract_U32();
                            vresp->extract_U32(); // bouquet currently unused
    tag.iEncryptionSystem = vresp->extract_U32();
                            vresp->extract_U32(); // uint32_t vtype - currently unused
    tag.bIsRadio          = radio;
    tag.strInputFormat    = "";
    tag.strStreamURL      = "";
    tag.strIconPath       = "";
    tag.bIsHidden         = false;
    tag.bIsRecording      = false;

    PVR->TransferChannelEntry(handle, &tag);
    delete[] tag.strChannelName;
  }

  delete vresp;
  return true;
}

bool cVNSIData::GetEPGForChannel(PVR_HANDLE handle, const PVR_CHANNEL &channel, time_t start, time_t end)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_EPG_GETFORCHANNEL))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return false;
  }
  if (!vrp.add_U32(channel.iUniqueId) || !vrp.add_U32(start) || !vrp.add_U32(end - start))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't add parameter to cRequestPacket", __FUNCTION__);
    return false;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  while (!vresp->end())
  {
    EPG_TAG tag;
    memset(&tag, 0 , sizeof(tag));

    tag.iChannelNumber     = channel.iChannelNumber;
    tag.iUniqueBroadcastId = vresp->extract_U32();
    tag.startTime          = vresp->extract_U32();
    tag.endTime            = tag.startTime + vresp->extract_U32();
    uint32_t content       = vresp->extract_U32();
    tag.iGenreType         = content & 0xF0;
    tag.iGenreSubType      = content & 0x0F;
    tag.iParentalRating    = vresp->extract_U32();
    tag.strTitle           = vresp->extract_String();
    tag.strPlotOutline     = vresp->extract_String();
    tag.strPlot            = vresp->extract_String();

    PVR->TransferEpgEntry(handle, &tag);
    delete[] tag.strTitle;
    delete[] tag.strPlotOutline;
    delete[] tag.strPlot;
  }

  delete vresp;
  return true;
}


/** OPCODE's 60 - 69: VNSI network functions for timer access */

int cVNSIData::GetTimersCount()
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_TIMER_GETCOUNT))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return -1;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return -1;
  }

  uint32_t count = vresp->extract_U32();

  delete vresp;
  return count;
}

PVR_ERROR cVNSIData::GetTimerInfo(unsigned int timernumber, PVR_TIMER &tag)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_TIMER_GET))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return PVR_ERROR_UNKOWN;
  }

  if (!vrp.add_U32(timernumber))
    return PVR_ERROR_UNKOWN;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    delete vresp;
    return PVR_ERROR_UNKOWN;
  }

  uint32_t returnCode = vresp->extract_U32();
  if (returnCode != VDR_RET_OK)
  {
    delete vresp;
    if (returnCode == VDR_RET_DATAUNKNOWN)
      return PVR_ERROR_NOT_POSSIBLE;
    else if (returnCode == VDR_RET_ERROR)
      return PVR_ERROR_SERVER_ERROR;
  }

  tag.iClientIndex      = vresp->extract_U32();
  tag.bIsActive         = vresp->extract_U32();
                          vresp->extract_U32(); // uint32_t recording - currently unused
                          vresp->extract_U32(); // uint32_t pending - currently unused
  tag.iPriority         = vresp->extract_U32();
  tag.iLifetime         = vresp->extract_U32();
                          vresp->extract_U32(); // channel number - unused
  tag.iClientChannelUid = vresp->extract_U32();
  tag.startTime         = vresp->extract_U32();
  tag.endTime           = vresp->extract_U32();
  tag.firstDay          = vresp->extract_U32();
  tag.iWeekdays         = vresp->extract_U32();
  tag.bIsRepeating      = tag.iWeekdays == 0 ? false : true;
  tag.strTitle          = vresp->extract_String();
  tag.strDirectory            = "";

  delete[] tag.strTitle;
  delete vresp;
  return PVR_ERROR_NO_ERROR;
}

bool cVNSIData::GetTimersList(PVR_HANDLE handle)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_TIMER_GETLIST))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return false;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    delete vresp;
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return false;
  }

  uint32_t numTimers = vresp->extract_U32();
  if (numTimers > 0)
  {
    while (!vresp->end())
    {
      PVR_TIMER tag;
      tag.iClientIndex      = vresp->extract_U32();
      tag.bIsActive         = vresp->extract_U32();
                              vresp->extract_U32(); // uint32_t recording - currently unused
                              vresp->extract_U32(); // uint32_t pending - currently unused
      tag.iPriority         = vresp->extract_U32();
      tag.iLifetime         = vresp->extract_U32();
                              vresp->extract_U32(); // channel number - unused
      tag.iClientChannelUid = vresp->extract_U32();
      tag.startTime         = vresp->extract_U32();
      tag.endTime           = vresp->extract_U32();
      tag.firstDay          = vresp->extract_U32();
      tag.iWeekdays         = vresp->extract_U32();
      tag.bIsRepeating      = tag.iWeekdays == 0 ? false : true;
      tag.strTitle          = vresp->extract_String();
      tag.strDirectory      = "";
      tag.iMarginStart      = 0;
      tag.iMarginEnd        = 0;

      PVR->TransferTimerEntry(handle, &tag);
      delete[] tag.strTitle;
    }
  }
  delete vresp;
  return true;
}

PVR_ERROR cVNSIData::AddTimer(const PVR_TIMER &timerinfo)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_TIMER_ADD))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return PVR_ERROR_UNKOWN;
  }

  // add directory in front of the title
  std::string path;
  if(timerinfo.strDirectory != NULL && strlen(timerinfo.strDirectory) > 0) {
    path += timerinfo.strDirectory;
    if(path == "/") {
      path.clear();
    }
    else if(path.size() > 1) {
      if(path[0] == '/') {
        path = path.substr(1);
      }
    }

    if(path.size() > 0 && path[path.size()-1] != '/') {
      path += "/";
    }
  }

  if(timerinfo.strTitle != NULL) {
    path += timerinfo.strTitle;
  }

  // replace directory separators
  for(std::size_t i=0; i<path.size(); i++) {
    if(path[i] == '/' || path[i] == '\\') {
      path[i] = '~';
    }
  }

  if(path.empty()) {
    XBMC->Log(LOG_ERROR, "%s - Empty filename !", __FUNCTION__);
    return PVR_ERROR_UNKOWN;
  }

  if (!vrp.add_U32(timerinfo.bIsActive))     return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.iPriority))   return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.iLifetime))   return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.iClientChannelUid)) return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.startTime))  return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.endTime))    return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.bIsRepeating ? timerinfo.firstDay : 0))   return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.iWeekdays))return PVR_ERROR_UNKOWN;
  if (!vrp.add_String(path.c_str()))      return PVR_ERROR_UNKOWN;
  if (!vrp.add_String(""))                return PVR_ERROR_UNKOWN;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    delete vresp;
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return PVR_ERROR_UNKOWN;
  }
  uint32_t returnCode = vresp->extract_U32();
  delete vresp;
  if (returnCode == VDR_RET_DATALOCKED)
    return PVR_ERROR_ALREADY_PRESENT;
  else if (returnCode == VDR_RET_DATAINVALID)
    return PVR_ERROR_NOT_SAVED;
  else if (returnCode == VDR_RET_ERROR)
    return PVR_ERROR_SERVER_ERROR;

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::DeleteTimer(const PVR_TIMER &timerinfo, bool force)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_TIMER_DELETE))
    return PVR_ERROR_UNKOWN;

  if (!vrp.add_U32(timerinfo.iClientIndex))
    return PVR_ERROR_UNKOWN;

  if (!vrp.add_U32(force))
    return PVR_ERROR_UNKOWN;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    delete vresp;
    return PVR_ERROR_UNKOWN;
  }

  uint32_t returnCode = vresp->extract_U32();
  delete vresp;

  if (returnCode == VDR_RET_DATALOCKED)
    return PVR_ERROR_NOT_DELETED;
  if (returnCode == VDR_RET_RECRUNNING)
    return PVR_ERROR_RECORDING_RUNNING;
  else if (returnCode == VDR_RET_DATAINVALID)
    return PVR_ERROR_NOT_POSSIBLE;
  else if (returnCode == VDR_RET_ERROR)
    return PVR_ERROR_SERVER_ERROR;

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::RenameTimer(const PVR_TIMER &timerinfo, const char *newname)
{
  PVR_TIMER timerinfo1;
  PVR_ERROR ret = GetTimerInfo(timerinfo.iClientIndex, timerinfo1);
  if (ret != PVR_ERROR_NO_ERROR)
    return ret;

  timerinfo1.strTitle = newname;
  return UpdateTimer(timerinfo1);
}

PVR_ERROR cVNSIData::UpdateTimer(const PVR_TIMER &timerinfo)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_TIMER_UPDATE))        return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.iClientIndex))      return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.bIsActive))     return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.iPriority))   return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.iLifetime))   return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.iClientChannelUid)) return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.startTime))  return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.endTime))    return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.bIsRepeating ? timerinfo.firstDay : 0))   return PVR_ERROR_UNKOWN;
  if (!vrp.add_U32(timerinfo.iWeekdays))return PVR_ERROR_UNKOWN;
  if (!vrp.add_String(timerinfo.strTitle))   return PVR_ERROR_UNKOWN;
  if (!vrp.add_String(""))                return PVR_ERROR_UNKOWN;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    delete vresp;
    return PVR_ERROR_UNKOWN;
  }
  uint32_t returnCode = vresp->extract_U32();
  delete vresp;
  if (returnCode == VDR_RET_DATAUNKNOWN)
    return PVR_ERROR_NOT_POSSIBLE;
  else if (returnCode == VDR_RET_DATAINVALID)
    return PVR_ERROR_NOT_SAVED;
  else if (returnCode == VDR_RET_ERROR)
    return PVR_ERROR_SERVER_ERROR;

  return PVR_ERROR_NO_ERROR;
}

int cVNSIData::GetRecordingsCount()
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_RECORDINGS_GETCOUNT))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return -1;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return -1;
  }

  uint32_t count = vresp->extract_U32();

  delete vresp;
  return count;
}

PVR_ERROR cVNSIData::GetRecordingsList(PVR_HANDLE handle)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_RECORDINGS_GETLIST))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return PVR_ERROR_UNKOWN;
  }

  cResponsePacket* vresp = ReadResult(&vrp);
  if (!vresp)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't get response packed", __FUNCTION__);
    return PVR_ERROR_UNKOWN;
  }

  while (!vresp->end())
  {
    PVR_RECORDING tag;
    tag.recordingTime   = vresp->extract_U32();
    tag.iDuration       = vresp->extract_U32();
    tag.iPriority       = vresp->extract_U32();
    tag.iLifetime       = vresp->extract_U32();
    tag.strChannelName  = vresp->extract_String();
    tag.strTitle        = vresp->extract_String();
    tag.strPlotOutline  = vresp->extract_String();
    tag.strPlot         = vresp->extract_String();
    tag.strDirectory    = vresp->extract_String();
    tag.iClientIndex    = vresp->extract_U32();
    tag.strStreamURL    = "";

    PVR->TransferRecordingEntry(handle, &tag);

    delete[] tag.strChannelName;
    delete[] tag.strTitle;
    delete[] tag.strPlotOutline;
    delete[] tag.strPlot;
    delete[] tag.strDirectory;
  }

  delete vresp;

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::RenameRecording(const PVR_RECORDING& recinfo, const char* newname)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_RECORDINGS_RENAME))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return PVR_ERROR_UNKOWN;
  }

  // add uid
  XBMC->Log(LOG_DEBUG, "%s - uid: %u", __FUNCTION__, recinfo.iClientIndex);
  if (!vrp.add_U32(recinfo.iClientIndex))
    return PVR_ERROR_UNKOWN;

  // add new title
  if (!vrp.add_String(newname))
    return PVR_ERROR_UNKOWN;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    delete vresp;
    return PVR_ERROR_SERVER_ERROR;
  }

  uint32_t returnCode = vresp->extract_U32();
  delete vresp;

  if(returnCode != 0)
   return PVR_ERROR_NOT_POSSIBLE;

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR cVNSIData::DeleteRecording(const PVR_RECORDING& recinfo)
{
  cRequestPacket vrp;
  if (!vrp.init(VDR_RECORDINGS_DELETE))
  {
    XBMC->Log(LOG_ERROR, "%s - Can't init cRequestPacket", __FUNCTION__);
    return PVR_ERROR_UNKOWN;
  }

  if (!vrp.add_U32(recinfo.iClientIndex))
    return PVR_ERROR_UNKOWN;

  cResponsePacket* vresp = ReadResult(&vrp);
  if (vresp == NULL || vresp->noResponse())
  {
    delete vresp;
    return PVR_ERROR_UNKOWN;
  }

  uint32_t returnCode = vresp->extract_U32();
  delete vresp;

  switch(returnCode)
  {
    case VDR_RET_DATALOCKED:
      return PVR_ERROR_NOT_DELETED;

    case VDR_RET_RECRUNNING:
      return PVR_ERROR_RECORDING_RUNNING;

    case VDR_RET_DATAINVALID:
      return PVR_ERROR_NOT_POSSIBLE;

    case VDR_RET_ERROR:
      return PVR_ERROR_SERVER_ERROR;
  }

  return PVR_ERROR_NO_ERROR;
}

void cVNSIData::Action()
{
  uint32_t channelID;
  uint32_t requestID;
  uint32_t userDataLength;
  uint8_t* userData;

  bool readSuccess;

  cResponsePacket* vresp;

  while (Running())
  {
    // read channelID
    readSuccess = readData((uint8_t*)&channelID, sizeof(uint32_t));

    // just wait if we're currently not connected
    if (m_connectionLost)
    {
      usleep(1000 * 1000); // 1000 ms to relax
      continue;
    }

    if (!readSuccess) continue; // no data was read but the connection is ok.

    // Data was read
    channelID = ntohl(channelID);

    // read requestID
    if (!readData((uint8_t*)&requestID, sizeof(uint32_t)))
    {
      m_connectionLost = true;
      continue;
    }
    requestID = ntohl(requestID);

    // read userDataLength
    if (!readData((uint8_t*)&userDataLength, sizeof(uint32_t)))
    {
      m_connectionLost = true;
      continue;
    }
    userDataLength = ntohl(userDataLength);
    if (userDataLength > 5000000) {
      m_connectionLost = true;
      continue; // how big can these packets get?
    }

    // read userData
    userData = NULL;
    if (userDataLength > 0)
    {
      userData = (uint8_t*)malloc(userDataLength);
      if (!userData) continue;
      if (!userData || !readData(userData, userDataLength))
      {
        free(userData);
        m_connectionLost = true;
        continue;
      }
    }

    // assemble response packet
    vresp = new cResponsePacket();
    vresp->setResponse(requestID, userData, userDataLength);

    // CHANNEL_REQUEST_RESPONSE

    if (channelID == CHANNEL_REQUEST_RESPONSE)
    {

      CMD_LOCK;
      SMessages::iterator it = m_queue.find(requestID);
      if (it != m_queue.end())
      {
        it->second.pkt = vresp;
        it->second.event->Signal();
      }
      else
      {
        delete vresp;
      }
    }

    // CHANNEL_STATUS

    else if (channelID == CHANNEL_STATUS)
    {
      if (requestID == VDR_STATUS_MESSAGE)
      {
        uint32_t type = vresp->extract_U32();
        char* msgstr  = vresp->extract_String();
        std::string text = msgstr;

        if (g_bCharsetConv)
          XBMC->UnknownToUTF8(text);

        if (type == 2)
          XBMC->QueueNotification(QUEUE_ERROR, text.c_str());
        if (type == 1)
          XBMC->QueueNotification(QUEUE_WARNING, text.c_str());
        else
          XBMC->QueueNotification(QUEUE_INFO, text.c_str());

        delete[] msgstr;
      }
      else if (requestID == VDR_STATUS_RECORDING)
      {
                          vresp->extract_U32(); // device currently unused
        uint32_t on     = vresp->extract_U32();
        char* str1      = vresp->extract_String();
        char* str2      = vresp->extract_String();

        PVR->Recording(str1, str2, on);
        PVR->TriggerTimerUpdate();

        delete[] str1;
        delete[] str2;
      }
      else if (requestID == VDR_STATUS_TIMERCHANGE)
      {
        PVR->TriggerTimerUpdate();
      }

      delete vresp;
    }

    // UNKOWN CHANNELID

    else
    {
      XBMC->Log(LOG_ERROR, "%s - Rxd a response packet on channel %lu !!", __FUNCTION__, channelID);
    }
  }
}

bool cVNSIData::readData(uint8_t* buffer, int totalBytes)
{
  if(m_connectionLost) {
    if(TryReconnect()) {
      m_connectionLost = false;
    }
    else {
      return false;
    }
  }

  int ret = m_session.readData(buffer, totalBytes);
  if (ret == 1)
    return true;
  else if (ret == 0)
    return false;

  XBMC->Log(LOG_ERROR, "%s - connection lost !!!", __FUNCTION__);
  m_connectionLost = true;
  return false;
}

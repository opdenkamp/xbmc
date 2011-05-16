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

#include "VNSISession.h"
#include "client.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "responsepacket.h"
#include "requestpacket.h"
#include "vnsicommand.h"
#include "tools.h"

/* Needed on Mac OS/X */
 
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

cVNSISession::cVNSISession()
  : m_connectionLost(false)
  , m_fd(INVALID_SOCKET)
  , m_protocol(0)
{
}

cVNSISession::~cVNSISession()
{
  Close();
}

void cVNSISession::Abort()
{
  tcp_shutdown(m_fd);
}

void cVNSISession::Close()
{
  if(m_fd != INVALID_SOCKET)
  {
    tcp_close(m_fd);
    m_fd = INVALID_SOCKET;
  }
}

bool cVNSISession::Open(const std::string& hostname, int port, const char *name)
{
  Close();

  char errbuf[1024];
  int  errlen = sizeof(errbuf);
  if (port == 0)
    port = 34890;

  m_fd = tcp_connect(hostname.c_str(), port, errbuf, errlen, 3000);

  if (m_fd == INVALID_SOCKET)
  {
    XBMC->Log(LOG_ERROR, "%s - Can't connect to VSNI Server: %s", __FUNCTION__, errbuf);
    return false;
  }

  // store connection data
  m_hostname = hostname;
  m_port = port;

  if(name != NULL)
    m_name = name;

  return true;
}

bool cVNSISession::Login()
{
  try
  {
    cRequestPacket vrp;
    if (!vrp.init(VNSI_LOGIN))                  throw "Can't init cRequestPacket";
    if (!vrp.add_U32(VNSIPROTOCOLVERSION))      throw "Can't add protocol version to RequestPacket";
    if (!vrp.add_U8(false))                     throw "Can't add netlog flag";
    if (!m_name.empty())
    {
      if (!vrp.add_String(m_name.c_str()))      throw "Can't add client name to RequestPacket";
    }
    else
    {
      if (!vrp.add_String("XBMC Media Center")) throw "Can't add client name to RequestPacket";
    }

    // read welcome
    cResponsePacket* vresp = ReadResult(&vrp);
    if (!vresp)
      throw "failed to read greeting from server";

    uint32_t    protocol      = vresp->extract_U32();
    uint32_t    vdrTime       = vresp->extract_U32();
    int32_t     vdrTimeOffset = vresp->extract_S32();
    const char *ServerName    = vresp->extract_String();
    const char *ServerVersion = vresp->extract_String();

    m_server    = ServerName;
    m_version   = ServerVersion;
    m_protocol  = protocol;

    if (!m_name.empty())
      XBMC->Log(LOG_NOTICE, "Logged in at '%lu+%i' to '%s' Version: '%s' with protocol version '%lu'",
        vdrTime, vdrTimeOffset, ServerName, ServerVersion, protocol);

    delete[] ServerName;
    delete[] ServerVersion;

    delete vresp;
  }
  catch (const char * str)
  {
    XBMC->Log(LOG_ERROR, "%s - %s", __FUNCTION__,str);
    tcp_close(m_fd);
    m_fd = INVALID_SOCKET;
    return false;
  }

  return true;
}

cResponsePacket* cVNSISession::ReadMessage()
{
  uint32_t channelID = 0;
  uint32_t requestID;
  uint32_t userDataLength = 0;
  uint8_t* userData = NULL;
  uint32_t streamID;
  uint32_t duration;
  uint32_t opCodeID;
  int64_t  dts = 0;
  int64_t  pts = 0;

  cResponsePacket* vresp = NULL;

  if(!readData((uint8_t*)&channelID, sizeof(uint32_t)))
    return NULL;

  // Data was read

  channelID = ntohl(channelID);
  if (channelID == VNSI_CHANNEL_STREAM)
  {
    if (!readData((uint8_t*)&m_streamPacketHeader, sizeof(m_streamPacketHeader))) return NULL;

    opCodeID = ntohl(m_streamPacketHeader.opCodeID);
    streamID = ntohl(m_streamPacketHeader.streamID);
    duration = ntohl(m_streamPacketHeader.duration);
    pts = ntohll(*(int64_t*)m_streamPacketHeader.pts);
    dts = ntohll(*(int64_t*)m_streamPacketHeader.dts);
    userDataLength = ntohl(m_streamPacketHeader.userDataLength);

    if(opCodeID == VNSI_STREAM_MUXPKT) {
      DemuxPacket* p = PVR->AllocateDemuxPacket(userDataLength);
      userData = (uint8_t*)p;
      if (userDataLength > 0)
      {
        if (!userData) return NULL;
        if (!readData(p->pData, userDataLength))
        {
          PVR->FreeDemuxPacket(p);
          return NULL;
        }
      }
    }
    else if (userDataLength > 0) {
      userData = (uint8_t*)malloc(userDataLength);
      if (!userData) return NULL;
      if (!readData(userData, userDataLength))
      {
        free(userData);
        return NULL;
      }
    }

    vresp = new cResponsePacket();
    vresp->setStream(opCodeID, streamID, duration, dts, pts, userData, userDataLength);
  }
  else
  {
    if (!readData((uint8_t*)&m_responsePacketHeader, sizeof(m_responsePacketHeader))) return NULL;

    requestID = ntohl(m_responsePacketHeader.requestID);
    userDataLength = ntohl(m_responsePacketHeader.userDataLength);

    if (userDataLength > 5000000) return NULL; // how big can these packets get?
    userData = NULL;
    if (userDataLength > 0)
    {
      userData = (uint8_t*)malloc(userDataLength);
      if (!userData) return NULL;
      if (!readData(userData, userDataLength)) {
        free(userData);
        return NULL;
      }
    }

    vresp = new cResponsePacket();
    vresp->setResponse(requestID, userData, userDataLength);
  }

  return vresp;
}

bool cVNSISession::SendMessage(cRequestPacket* vrp)
{
  if (sendData(vrp->getPtr(), vrp->getLen()) != (int)vrp->getLen())
    return false;

  return true;
}

cResponsePacket* cVNSISession::ReadResult(cRequestPacket* vrp)
{
  if(!SendMessage(vrp))
  {
    SignalConnectionLost();
    return NULL;
  }

  cResponsePacket *pkt = NULL;

  while((pkt = ReadMessage()))
  {
    /* Discard everything other as response packets until it is received */
    if (pkt->getChannelID() == VNSI_CHANNEL_REQUEST_RESPONSE && pkt->getRequestID() == vrp->getSerial())
    {
      return pkt;
    }
    else
      delete pkt;
  }

  SignalConnectionLost();
  return NULL;
}

bool cVNSISession::ReadSuccess(cRequestPacket* vrp)
{
  cResponsePacket *pkt = NULL;
  if((pkt = ReadResult(vrp)) == NULL)
  {
    return false;
  }
  uint32_t retCode = pkt->extract_U32();
  delete pkt;

  if(retCode != VNSI_RET_OK)
  {
    XBMC->Log(LOG_ERROR, "%s - failed with error code '%i'", __FUNCTION__, retCode);
    return false;
  }
  return true;
}

int cVNSISession::sendData(void* bufR, size_t count)
{
  size_t bytes_sent = 0;
  int this_write;

  unsigned char* buf = (unsigned char*)bufR;

  while (bytes_sent < count)
  {
#ifdef __WINDOWS__
    do
    {
      this_write = send(m_fd,(char*) buf, count- bytes_sent,0);
    } while ( (this_write == SOCKET_ERROR) && (WSAGetLastError() == WSAEINTR) );
#else
    {
      this_write = write(m_fd, buf, count - bytes_sent);
    } while ( (this_write < 0) && (errno == EINTR) );
#endif
    if (this_write <= 0)
    {
      XBMC->Log(LOG_ERROR, "%s - this_write <= 0'", __FUNCTION__);
      break;
    }
    bytes_sent += this_write;
    buf += this_write;
  }

  if (bytes_sent < count)
  {
    return -1;
  }

  return bytes_sent;
}

void cVNSISession::OnReconnect() {
}

void cVNSISession::OnDisconnect() {
}

bool cVNSISession::TryReconnect() {
  if(!Open(m_hostname, m_port))
    return false;

  if(!Login())
    return false;

  XBMC->Log(LOG_DEBUG, "%s - reconnected", __FUNCTION__);
  m_connectionLost = false;

  OnReconnect();

  return true;
}

void cVNSISession::SignalConnectionLost()
{
  if(m_connectionLost)
    return;

  XBMC->Log(LOG_ERROR, "%s - connection lost !!!", __FUNCTION__);

  m_connectionLost = true;
  Abort();
  Close();

  OnDisconnect();
}

bool cVNSISession::readData(uint8_t* buffer, int totalBytes)
{
  int bytesRead = 0;
  int thisRead;
  int success;
  fd_set readSet;
  struct timeval timeout;

  while(bytesRead < totalBytes)
  {
    FD_ZERO(&readSet);
    FD_SET(m_fd, &readSet);
    timeout.tv_sec = g_iConnectTimeout;
    timeout.tv_usec = 0;

    success = select(m_fd + 1, &readSet, NULL, NULL, &timeout);
    if (success == -1)
    {
      return false;
    }
    else if (success < 1)
      return false;  // error, or timeout

#ifdef __WINDOWS__
    thisRead = recv(m_fd, (char*)&buffer[bytesRead], totalBytes - bytesRead, 0);
#else
    thisRead = read(m_fd, &buffer[bytesRead], totalBytes - bytesRead);
#endif

    // if read returns 0 then connection is closed
    if(thisRead == 0)
    {
      return false;
    }

    // in non-blocking mode if read is called with no data available, it returns -1
    // and sets errno to EAGAIN. but we use select so it wouldn't do that anyway.
    if(thisRead == -1)
      continue;

    bytesRead += thisRead;
  }
  return true;
}

void cVNSISession::SleepMs(int ms)
{
#ifdef __WINDOWS__
  Sleep(ms);
#else
  usleep(ms * 1000);
#endif
}

/*
 *
 */

#include "PVROperations.h"
#include "Application.h"
#include "utils/log.h"

#include "pvr/PVRManager.h"
#include "pvr/channels/PVRChannelGroupsContainer.h"
#include "pvr/channels/PVRChannel.h"
#include "pvr/timers/PVRTimers.h"
#include "pvr/timers/PVRTimerInfoTag.h"
#include "epg/EpgInfoTag.h"
#include "epg/EpgContainer.h"

using namespace JSONRPC;
using namespace PVR;
using namespace EPG;

JSON_STATUS CPVROperations::ChannelSwitch(const CStdString &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  int iChannelId = (int) parameterObject["channelid"].asInteger();

  CLog::Log(LOGINFO, "JSON PVR:Switch channel: %d", iChannelId);

  if ( g_PVRManager.IsStarted() )
  {
    const CPVRChannel *channel = g_PVRChannelGroups->GetByChannelIDFromAll(iChannelId);
    if ( g_PVRManager.StartPlayback(channel, false) )
      return OK;
    else
      return InternalError;
  }
  else
  {
    CLog::Log(LOGINFO, "JSON PVR failed to Switch channels. PVR not started");
    return FailedToExecute;
  }

  return OK;
}

JSON_STATUS CPVROperations::ChannelUp(const CStdString &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  CLog::Log(LOGINFO, "PVR: channel up");

  if ( g_PVRManager.IsStarted() && g_PVRManager.IsPlaying() && g_application.m_pPlayer )
  {
    unsigned int iNewChannelNumber(0);
    g_PVRManager.ChannelUp( &iNewChannelNumber );

    CLog::Log(LOGINFO, "PVR: new channel %d", iNewChannelNumber);
  }
  return OK;
}

JSON_STATUS CPVROperations::ChannelDown(const CStdString &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  CLog::Log(LOGINFO, "PVR: channel down");

  if ( g_PVRManager.IsStarted() && g_PVRManager.IsPlaying() && g_application.m_pPlayer )
  {
    unsigned int iNewChannelNumber(0);
    g_PVRManager.ChannelDown( &iNewChannelNumber );

    CLog::Log(LOGINFO, "PVR: new channel %d", iNewChannelNumber);
  }

  return OK;
}

JSON_STATUS CPVROperations::ChannelRecording(const CStdString &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  bool bOnOff = (bool) parameterObject["on"].asBoolean();

  CLog::Log(LOGINFO, "PVR: channel recording on/off %d", bOnOff);

  if ( g_PVRManager.IsStarted() && g_PVRManager.IsPlaying() && g_application.m_pPlayer )
  {
    g_PVRManager.StartRecordingOnPlayingChannel(bOnOff);
  }
  return OK;
}

JSON_STATUS CPVROperations::ScheduleRecording(const CStdString &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result){

  if ( g_PVRManager.IsStarted() ) {

    int iEpgId = (int) parameterObject["idepg"].asInteger();
    int iUniqueId = (int) parameterObject["uniqueid"].asInteger();
    int iStartTime = (int) parameterObject["starttime"].asInteger();

    CDateTime *startTime = new CDateTime( iStartTime );
    CEpgInfoTag *tag = g_EpgContainer.GetById(iEpgId)->GetTag(iUniqueId, *startTime);
    delete startTime;

    if ( tag ){
      CPVRTimerInfoTag *newTimer = CPVRTimerInfoTag::CreateFromEpg(*tag);
      bool bReturn = CPVRTimers::AddTimer(*newTimer);

      CLog::Log(LOGINFO, "PVR: record result %d", bReturn);

      delete newTimer;
    }
  }

  return OK;
}

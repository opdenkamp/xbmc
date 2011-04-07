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

#include "Application.h"
#include "settings/GUISettings.h"
#include "Util.h"
#include "guilib/GUIWindowManager.h"
#include "windows/GUIWindowPVR.h"
#include "GUIInfoManager.h"
#ifdef HAS_VIDEO_PLAYBACK
#include "cores/VideoRenderers/RenderManager.h"
#endif
#include "utils/log.h"
#include "threads/SingleLock.h"
#include "guilib/LocalizeStrings.h"
#include "filesystem/File.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtils.h"
#include "music/tags/MusicInfoTag.h"
#include "settings/Settings.h"
#include "filesystem/StackDirectory.h"

/* GUI Messages includes */
#include "dialogs/GUIDialogOK.h"
#include "dialogs/GUIDialogProgress.h"
#include "dialogs/GUIDialogSelect.h"

#include "PVRManager.h"
#include "addons/PVRClients.h"
#include "channels/PVRChannelGroupsContainer.h"
#include "epg/PVREpgContainer.h"
#include "recordings/PVRRecordings.h"
#include "timers/PVRTimers.h"
#include "PVRGUIInfo.h"

using namespace std;
using namespace XFILE;
using namespace MUSIC_INFO;
using namespace ADDON;

CPVRManager *CPVRManager::m_instance = NULL;

CPVRManager::CPVRManager(void) :
    Observer()
{
  m_bFirstStart   = true;
  m_bLoaded       = false;

  m_addons        = new CPVRClients;
  m_channelGroups = new CPVRChannelGroupsContainer;
  m_epg           = new CPVREpgContainer;
  m_recordings    = new CPVRRecordings;
  m_timers        = new CPVRTimers;
  m_guiInfo       = new CPVRGUIInfo;

  ResetProperties();
}

CPVRManager::~CPVRManager(void)
{
  Stop();

  delete m_epg;
  delete m_recordings;
  delete m_timers;
  delete m_channelGroups;
  delete m_addons;
  delete m_guiInfo;

  CLog::Log(LOGDEBUG,"PVRManager - destroyed");
}

void CPVRManager::Notify(const Observable &obs, const CStdString& msg)
{
}

CPVRManager *CPVRManager::Get(void)
{
  if (!m_instance)
    m_instance = new CPVRManager;

  return m_instance;
}

void CPVRManager::Destroy(void)
{
  if (m_instance)
  {
    delete m_instance;
    m_instance = NULL;
  }
}

void CPVRManager::Unload(void)
{
  CLog::Log(LOGNOTICE, "PVRManager - stopping");

  /* stop playback if a pvr file is playing */
  if (IsPlaying())
  {
    CLog::Log(LOGNOTICE,"PVRManager - %s - stopping PVR playback", __FUNCTION__);
    g_application.StopPlaying();
  }

  /* stop all update threads */
  StopUpdateThreads();

  m_epg->RemoveObserver(this);
  m_epg->Unload();

  m_recordings->Unload();
  m_timers->Unload();
  m_channelGroups->Unload();
  m_addons->Unload();

  m_bLoaded = false;
}

void CPVRManager::Start(void)
{
  /* first stop and remove any clients */
  Stop();

  /* don't start if Settings->Video->TV->Enable isn't checked */
  if (!g_guiSettings.GetBool("pvrmanager.enabled"))
    return;

  ResetProperties();

  /* create the supervisor thread to do all background activities */
  StartUpdateThreads();
}

void CPVRManager::Stop(void)
{
  if (!m_bLoaded)
    return;

  CLog::Log(LOGNOTICE, "PVRManager - stopping");

  if (IsPlaying())
  {
    CLog::Log(LOGNOTICE,"PVRManager - %s - stopping PVR playback", __FUNCTION__);
    g_application.StopPlaying();
  }

  StopUpdateThreads();
  Unload();
}

bool CPVRManager::StartUpdateThreads(void)
{
  CLog::Log(LOGNOTICE, "PVRManager - starting up");

  if (!Load())
    return false;

  m_addons->Start();
  m_guiInfo->Start();
  m_epg->AddObserver(this);
  m_epg->Start();

  Create();
  SetName("XBMC PVRManager");
  SetPriority(-1);

  return true;
}

void CPVRManager::StopUpdateThreads(void)
{
  StopThread();
  m_epg->RemoveObserver(this);
  m_epg->StopThread();
  m_guiInfo->Stop();
  m_addons->Stop();
}

bool CPVRManager::Load(void)
{
  if (m_bLoaded)
    return true;

  while (!m_addons->HasActiveClients())
  {
    m_addons->TryLoadClients(1);

    if (m_addons->HasActiveClients())
    {
      CLog::Log(LOGDEBUG, "PVRManager - %s - active clients found. continue to start", __FUNCTION__);

      /* load all channels and groups */
      m_channelGroups->Load();
      m_bTriggerChannelsUpdate = false;
      m_bTriggerChannelGroupsUpdate = false;

      /* get timers from the backends */
      m_timers->Load();
      m_bTriggerTimersUpdate = false;

      /* get recordings from the backend */
      m_recordings->Load();
      m_bTriggerRecordingsUpdate = false;
    }

    /* check if there are (still) any enabled addons */
    if (DisableIfNoClients())
    {
      CLog::Log(LOGDEBUG, "PVRManager - %s - no clients could be found. aborting startup", __FUNCTION__);
      return false;
    }
  }

  m_bLoaded = true;

  return true;
}

void CPVRManager::Process(void)
{
  /* continue last watched channel after first startup */
  if (!m_bStop && m_bFirstStart && g_guiSettings.GetInt("pvrplayback.startlast") != START_LAST_CHANNEL_OFF)
    ContinueLastChannel();

  CLog::Log(LOGDEBUG, "PVRManager - %s - entering main loop", __FUNCTION__);

  /* main loop */
  while (!m_bStop)
  {
    /* keep trying to load remaining clients */
    if (!m_addons->AllClientsLoaded())
      m_addons->TryLoadClients(1);

    /* check if the (still) are any enabled addons */
    if (DisableIfNoClients())
    {
      CLog::Log(LOGNOTICE, "PVRManager - %s - no add-ons enabled. disabling PVR functionality", __FUNCTION__);
      Stop();
      return;
    }

    UpdateChannels();
    UpdateRecordings();
    UpdateTimers();

    Sleep(1000);
  }

}

bool CPVRManager::ChannelSwitch(unsigned int iChannel)
{
  const CPVRChannel *channel = NULL;

  if (m_addons->IsPlayingRadio())
  {
    if (m_currentRadioGroup != NULL)
      channel = m_currentRadioGroup->GetByChannelNumber(iChannel);

    if (channel == NULL)
      channel = GetChannelGroups()->GetGroupAllRadio()->GetByChannelNumber(iChannel);
  }
  else
  {
    /* always use the TV group if we're not playing a radio channel */
    if (m_currentTVGroup != NULL)
      channel = m_currentTVGroup->GetByChannelNumber(iChannel);

    if (channel == NULL)
      channel = GetChannelGroups()->GetGroupAllTV()->GetByChannelNumber(iChannel);
  }

  if (channel == NULL)
  {
    CLog::Log(LOGERROR, "PVRManager - %s - cannot find channel %d", __FUNCTION__, iChannel);
    return false;
  }

  return PerformChannelSwitch(*channel, false);
}

bool CPVRManager::ChannelUpDown(unsigned int *iNewChannelNumber, bool bPreview, bool bUp)
{
  bool bReturn = false;

  CPVRChannel currentChannel;
  if (m_addons->GetPlayingChannel(&currentChannel))
  {
    const CPVRChannelGroup *group = GetPlayingGroup(currentChannel.IsRadio());
    if (group)
    {
      const CPVRChannel *newChannel = bUp ? group->GetByChannelUp(currentChannel) : group->GetByChannelDown(currentChannel);
      if (PerformChannelSwitch(*newChannel, bPreview))
      {
        *iNewChannelNumber = newChannel->ChannelNumber();
        bReturn = true;
      }
    }
  }

  return bReturn;
}

bool CPVRManager::ContinueLastChannel(void)
{
  bool bReturn = false;
  m_bFirstStart = false;

  const CPVRChannel *channel = GetChannelGroups()->GetGroupAllTV()->GetByIndex(0);
  for (int i = 0; i < GetChannelGroups()->GetGroupAllTV()->GetNumChannels(); i++)
  {
    const CPVRChannel *nextChannel = GetChannelGroups()->GetGroupAllTV()->GetByIndex(i);
    if (nextChannel->ClientID() < 0 || !m_addons->IsValidClient(nextChannel->ClientID()))
      continue;
    channel = channel->LastWatched() > nextChannel->LastWatched() ? channel : nextChannel;
  }
  for (int i = 0; i < GetChannelGroups()->GetGroupAllRadio()->GetNumChannels(); i++)
  {
    const CPVRChannel *nextChannel = GetChannelGroups()->GetGroupAllRadio()->GetByIndex(i);
    if (nextChannel->ClientID() < 0 || !m_addons->IsValidClient(nextChannel->ClientID()))
      continue;
    channel = channel->LastWatched() > nextChannel->LastWatched() ? channel : nextChannel;
  }

  if (channel)
  {
    CLog::Log(LOGNOTICE, "PVRManager - %s - continue playback on channel '%s'",
        __FUNCTION__, channel->ChannelName().c_str());
    bReturn = StartPlayback(channel, (g_guiSettings.GetInt("pvrplayback.startlast") == START_LAST_CHANNEL_MIN));
  }

  return bReturn;
}

void CPVRManager::UpdateWindow(PVRWindow window)
{
  CGUIWindowPVR *pWindow = (CGUIWindowPVR *) g_windowManager.GetWindow(WINDOW_PVR);
  if (pWindow)
    pWindow->UpdateWindow(window);
}

bool CPVRManager::IsRunningChannelScan(void)
{
  return m_addons->IsRunningChannelScan();
}

void CPVRManager::ResetProperties(void)
{
  m_bTriggerChannelsUpdate      = false;
  m_bTriggerRecordingsUpdate    = false;
  m_bTriggerTimersUpdate        = false;
  m_bTriggerChannelGroupsUpdate = false;
  m_currentFile                 = NULL;

  m_currentRadioGroup           = NULL;
  m_currentTVGroup              = NULL;
  m_PreviousChannel[0]          = -1;
  m_PreviousChannel[1]          = -1;
  m_PreviousChannelIndex        = 0;
  m_LastChannel                 = 0;
}

void CPVRManager::UpdateTimers(void)
{
  CSingleLock lock(m_critSectionTriggers);
  if (!m_bTriggerTimersUpdate)
    return;
  lock.Leave();

  CLog::Log(LOGDEBUG, "PVRManager - %s - updating timers", __FUNCTION__);

  m_timers->Update();
  UpdateWindow(PVR_WINDOW_TIMERS);

  lock.Enter();
  m_bTriggerTimersUpdate = false;
  lock.Leave();
}

void CPVRManager::UpdateRecordings(void)
{
  CSingleLock lock(m_critSectionTriggers);
  if (!m_bTriggerRecordingsUpdate)
    return;
  lock.Leave();

  CLog::Log(LOGDEBUG, "PVRManager - %s - updating recordings list", __FUNCTION__);

  m_recordings->Update();
  UpdateWindow(PVR_WINDOW_RECORDINGS);

  lock.Enter();
  m_bTriggerRecordingsUpdate = false;
  lock.Leave();
}

void CPVRManager::UpdateChannels(void)
{
  bool bUpdateGroups(false);

  CSingleLock lock(m_critSectionTriggers);
  if (!m_bTriggerChannelsUpdate && !m_bTriggerChannelGroupsUpdate)
    return;

  bUpdateGroups = m_bTriggerChannelGroupsUpdate;
  lock.Leave();

  CLog::Log(LOGDEBUG, "PVRManager - %s - updating %s list",
      __FUNCTION__, bUpdateGroups ? "channel groups" : "channels");

  m_channelGroups->Update(!bUpdateGroups);
  UpdateWindow(PVR_WINDOW_CHANNELS_TV);
  UpdateWindow(PVR_WINDOW_CHANNELS_RADIO);

  lock.Enter();
  m_bTriggerChannelsUpdate      = false;
  m_bTriggerChannelGroupsUpdate = false;
  lock.Leave();
}

bool CPVRManager::DisableIfNoClients(void)
{
  bool bReturn = false;

  if (!m_addons->HasClients())
  {
    g_guiSettings.SetBool("pvrmanager.enabled", false);
    CLog::Log(LOGNOTICE,"PVRManager - no clients enabled. pvrmanager disabled.");
    bReturn = true;
  }

  return bReturn;
}

void CPVRManager::ResetDatabase(bool bShowProgress /* = true */)
{
  CLog::Log(LOGNOTICE,"PVRManager - %s - clearing the PVR database", __FUNCTION__);

  CGUIDialogProgress* pDlgProgress = NULL;

  if (bShowProgress)
  {
    pDlgProgress = (CGUIDialogProgress*)g_windowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
    pDlgProgress->SetLine(0, "");
    pDlgProgress->SetLine(1, g_localizeStrings.Get(19186));
    pDlgProgress->SetLine(2, "");
    pDlgProgress->StartModal();
    pDlgProgress->Progress();
  }

  if (m_addons->IsPlaying())
  {
    CLog::Log(LOGNOTICE,"PVRManager - %s - stopping playback", __FUNCTION__);
    g_application.StopPlaying();
  }

  if (bShowProgress)
    pDlgProgress->SetPercentage(10);

  /* stop the thread */
  Stop();
  if (bShowProgress)
    pDlgProgress->SetPercentage(20);

  if (m_database.Open())
  {
    /* clean the EPG database */
    m_epg->Clear(true);
    if (bShowProgress)
      pDlgProgress->SetPercentage(30);

    /* delete all TV channel groups */
    m_database.DeleteChannelGroups(false);
    if (bShowProgress)
      pDlgProgress->SetPercentage(50);

    /* delete all radio channel groups */
    m_database.DeleteChannelGroups(true);
    if (bShowProgress)
      pDlgProgress->SetPercentage(60);

    /* delete all channels */
    m_database.DeleteChannels();
    if (bShowProgress)
      pDlgProgress->SetPercentage(70);

    /* delete all channel settings */
    m_database.DeleteChannelSettings();
    if (bShowProgress)
      pDlgProgress->SetPercentage(80);

    /* delete all client information */
    m_database.DeleteClients();
    if (bShowProgress)
      pDlgProgress->SetPercentage(90);

    m_database.Close();
  }

  CLog::Log(LOGNOTICE,"PVRManager - %s - PVR database cleared. restarting the PVRManager", __FUNCTION__);

  Start();

  if (bShowProgress)
  {
    pDlgProgress->SetPercentage(100);
    pDlgProgress->Close();
  }
}

void CPVRManager::ResetEPG(void)
{
  StopUpdateThreads();
  m_epg->Reset();
  StartUpdateThreads();
}

bool CPVRManager::IsPlaying(void)
{
  return m_bLoaded && m_addons->IsPlaying();
}

bool CPVRManager::GetCurrentChannel(CPVRChannel *channel)
{
  return m_addons->GetPlayingChannel(channel);
}

int CPVRManager::GetCurrentEpg(CFileItemList *results)
{
  int iReturn = -1;

  CPVRChannel channel;
  if (m_addons->GetPlayingChannel(&channel))
    iReturn = channel.GetEPG(results);
  else
    CLog::Log(LOGDEBUG,"PVRManager - %s - no current channel set", __FUNCTION__);

  return iReturn;
}

int CPVRManager::GetPreviousChannel(void)
{
  //XXX this must be the craziest way to store the last channel
  int iReturn = -1;
  CPVRChannel channel;
  if (m_addons->GetPlayingChannel(&channel))
  {
    int iLastChannel = channel.ChannelNumber();

    if ((m_PreviousChannel[m_PreviousChannelIndex ^ 1] == iLastChannel || iLastChannel != m_PreviousChannel[0]) &&
        iLastChannel != m_PreviousChannel[1])
      m_PreviousChannelIndex ^= 1;

    iReturn = m_PreviousChannel[m_PreviousChannelIndex ^= 1];
  }

  return iReturn;
}

bool CPVRManager::StartRecordingOnPlayingChannel(bool bOnOff)
{
  bool bReturn = false;

  CPVRChannel channel;
  if (!m_addons->GetPlayingChannel(&channel))
    return bReturn;

  if (m_addons->HasTimerSupport(channel.ClientID()))
  {
    /* timers are supported on this channel */
    if (bOnOff && !channel.IsRecording())
    {
      CPVRTimerInfoTag *newTimer = m_timers->InstantTimer(&channel);
      if (!newTimer)
        CGUIDialogOK::ShowAndGetInput(19033,0,19164,0);
      else
        bReturn = true;
    }
    else if (!bOnOff && channel.IsRecording())
    {
      /* delete active timers */
      bReturn = m_timers->DeleteTimersOnChannel(channel, false, true);
    }
  }

  return bReturn;
}

void CPVRManager::SaveCurrentChannelSettings(void)
{
  CPVRChannel channel;
  if (!m_addons->GetPlayingChannel(&channel))
    return;

  if (!m_database.Open())
  {
    CLog::Log(LOGERROR, "PVR - %s - could not open the database", __FUNCTION__);
    return;
  }

  if (g_settings.m_currentVideoSettings != g_settings.m_defaultVideoSettings)
  {
    CLog::Log(LOGDEBUG, "PVR - %s - persisting custom channel settings for channel '%s'",
        __FUNCTION__, channel.ChannelName().c_str());
    m_database.PersistChannelSettings(channel, g_settings.m_currentVideoSettings);
  }
  else
  {
    CLog::Log(LOGDEBUG, "PVR - %s - no custom channel settings for channel '%s'",
        __FUNCTION__, channel.ChannelName().c_str());
    m_database.DeleteChannelSettings(channel);
  }

  m_database.Close();
}

void CPVRManager::LoadCurrentChannelSettings()
{
  CPVRChannel channel;
  if (!m_addons->GetPlayingChannel(&channel))
    return;

  if (!m_database.Open())
  {
    CLog::Log(LOGERROR, "PVR - %s - could not open the database", __FUNCTION__);
    return;
  }

  if (g_application.m_pPlayer)
  {
    /* set the default settings first */
    CVideoSettings loadedChannelSettings = g_settings.m_defaultVideoSettings;

    /* try to load the settings from the database */
    m_database.GetChannelSettings(channel, loadedChannelSettings);
    m_database.Close();

    g_settings.m_currentVideoSettings = g_settings.m_defaultVideoSettings;
    g_settings.m_currentVideoSettings.m_Brightness          = loadedChannelSettings.m_Brightness;
    g_settings.m_currentVideoSettings.m_Contrast            = loadedChannelSettings.m_Contrast;
    g_settings.m_currentVideoSettings.m_Gamma               = loadedChannelSettings.m_Gamma;
    g_settings.m_currentVideoSettings.m_Crop                = loadedChannelSettings.m_Crop;
    g_settings.m_currentVideoSettings.m_CropLeft            = loadedChannelSettings.m_CropLeft;
    g_settings.m_currentVideoSettings.m_CropRight           = loadedChannelSettings.m_CropRight;
    g_settings.m_currentVideoSettings.m_CropTop             = loadedChannelSettings.m_CropTop;
    g_settings.m_currentVideoSettings.m_CropBottom          = loadedChannelSettings.m_CropBottom;
    g_settings.m_currentVideoSettings.m_CustomPixelRatio    = loadedChannelSettings.m_CustomPixelRatio;
    g_settings.m_currentVideoSettings.m_CustomZoomAmount    = loadedChannelSettings.m_CustomZoomAmount;
    g_settings.m_currentVideoSettings.m_NoiseReduction      = loadedChannelSettings.m_NoiseReduction;
    g_settings.m_currentVideoSettings.m_Sharpness           = loadedChannelSettings.m_Sharpness;
    g_settings.m_currentVideoSettings.m_InterlaceMethod     = loadedChannelSettings.m_InterlaceMethod;
    g_settings.m_currentVideoSettings.m_OutputToAllSpeakers = loadedChannelSettings.m_OutputToAllSpeakers;
    g_settings.m_currentVideoSettings.m_AudioDelay          = loadedChannelSettings.m_AudioDelay;
    g_settings.m_currentVideoSettings.m_AudioStream         = loadedChannelSettings.m_AudioStream;
    g_settings.m_currentVideoSettings.m_SubtitleOn          = loadedChannelSettings.m_SubtitleOn;
    g_settings.m_currentVideoSettings.m_SubtitleDelay       = loadedChannelSettings.m_SubtitleDelay;

    /* only change the view mode if it's different */
    if (g_settings.m_currentVideoSettings.m_ViewMode != loadedChannelSettings.m_ViewMode)
    {
      g_settings.m_currentVideoSettings.m_ViewMode = loadedChannelSettings.m_ViewMode;

      g_renderManager.SetViewMode(g_settings.m_currentVideoSettings.m_ViewMode);
      g_settings.m_currentVideoSettings.m_CustomZoomAmount = g_settings.m_fZoomAmount;
      g_settings.m_currentVideoSettings.m_CustomPixelRatio = g_settings.m_fPixelRatio;
    }

    /* only change the subtitle stream, if it's different */
    if (g_settings.m_currentVideoSettings.m_SubtitleStream != loadedChannelSettings.m_SubtitleStream)
    {
      g_settings.m_currentVideoSettings.m_SubtitleStream = loadedChannelSettings.m_SubtitleStream;

      g_application.m_pPlayer->SetSubtitle(g_settings.m_currentVideoSettings.m_SubtitleStream);
    }

    /* only change the audio stream if it's different */
    if (g_application.m_pPlayer->GetAudioStream() != g_settings.m_currentVideoSettings.m_AudioStream)
      g_application.m_pPlayer->SetAudioStream(g_settings.m_currentVideoSettings.m_AudioStream);

    g_application.m_pPlayer->SetAVDelay(g_settings.m_currentVideoSettings.m_AudioDelay);
    g_application.m_pPlayer->SetDynamicRangeCompression((long)(g_settings.m_currentVideoSettings.m_VolumeAmplification * 100));
    g_application.m_pPlayer->SetSubtitleVisible(g_settings.m_currentVideoSettings.m_SubtitleOn);
    g_application.m_pPlayer->SetSubTitleDelay(g_settings.m_currentVideoSettings.m_SubtitleDelay);
  }
}

void CPVRManager::SetPlayingGroup(CPVRChannelGroup *group)
{
  if (group && group->IsRadio())
    m_currentRadioGroup = group;
  else if (group && !group->IsRadio())
    m_currentTVGroup = group;
}

const CPVRChannelGroup *CPVRManager::GetPlayingGroup(bool bRadio /* = false */)
{
  if (bRadio && !m_currentRadioGroup)
    m_currentRadioGroup = (CPVRChannelGroup *) GetChannelGroups()->GetGroupAllRadio();
  else if (!bRadio &&!m_currentTVGroup)
    m_currentTVGroup = (CPVRChannelGroup *) GetChannelGroups()->GetGroupAllTV();

  return bRadio ? m_currentRadioGroup : m_currentTVGroup;
}

void CPVRManager::TriggerRecordingsUpdate(void)
{
  CSingleLock lock(m_critSectionTriggers);
  if (!m_bTriggerRecordingsUpdate)
  {
    m_bTriggerRecordingsUpdate = true;
    CLog::Log(LOGDEBUG, "PVRManager - %s - recordings update scheduled", __FUNCTION__);
  }
}

void CPVRManager::TriggerTimersUpdate(void)
{
  CSingleLock lock(m_critSectionTriggers);
  if (!m_bTriggerTimersUpdate)
  {
    m_bTriggerTimersUpdate = true;
    CLog::Log(LOGDEBUG, "PVRManager - %s - timers update scheduled", __FUNCTION__);
  }
}

void CPVRManager::TriggerChannelsUpdate(void)
{
  CSingleLock lock(m_critSectionTriggers);
  if (!m_bTriggerChannelsUpdate)
  {
    m_bTriggerChannelsUpdate = true;
    CLog::Log(LOGDEBUG, "PVRManager - %s - channels update scheduled", __FUNCTION__);
  }
}

void CPVRManager::TriggerChannelGroupsUpdate(void)
{
  CSingleLock lock(m_critSectionTriggers);
  if (!m_bTriggerChannelGroupsUpdate)
  {
    m_bTriggerChannelGroupsUpdate = true;
    CLog::Log(LOGDEBUG, "PVRManager - %s - channel groups update scheduled", __FUNCTION__);
  }
}

bool CPVRManager::OpenLiveStream(const CPVRChannel &tag)
{
  bool bReturn = false;
  CSingleLock lock(m_critSectionStreams);

  CLog::Log(LOGDEBUG,"PVRManager - %s - opening live stream on channel '%s'",
      __FUNCTION__, tag.ChannelName().c_str());

  if ((bReturn = m_addons->OpenLiveStream(tag)) != false)
  {
    if(m_currentFile)
    {
      delete m_currentFile;
    }
    m_currentFile = new CFileItem(tag);

    LoadCurrentChannelSettings();
  }

  return bReturn;
}

bool CPVRManager::OpenRecordedStream(const CPVRRecording &tag)
{
  bool bReturn = false;
  CSingleLock lock(m_critSectionStreams);

  CLog::Log(LOGDEBUG,"PVRManager - %s - opening recorded stream '%s'",
      __FUNCTION__, tag.m_strFile.c_str());

  if ((bReturn = m_addons->OpenRecordedStream(tag)) != false)
  {
    delete m_currentFile;
    m_currentFile = new CFileItem(tag);
  }

  return bReturn;
}

void CPVRManager::CloseStream(void)
{
  CSingleLock lock(m_critSectionStreams);

  if (m_addons->IsReadingLiveStream())
  {
    CPVRChannel channel;
    if (m_addons->GetPlayingChannel(&channel))
    {
      /* store current time in iLastWatched */
      time_t tNow;
      CDateTime::GetCurrentDateTime().GetAsTime(tNow);
      channel.SetLastWatched(tNow, true);

      /* Store current settings inside Database */
      SaveCurrentChannelSettings();
    }
  }

  m_addons->CloseStream();
  if (m_currentFile)
  {
    delete m_currentFile;
    m_currentFile = NULL;
  }
}

void CPVRManager::UpdateCurrentFile(void)
{
  CSingleLock lock(m_critSectionStreams);
  if (m_currentFile)
    UpdateItem(*m_currentFile);
}

bool CPVRManager::UpdateItem(CFileItem& item)
{
  /* Don't update if a recording is played */
  if (item.IsPVRRecording())
    return true;

  if (!item.IsPVRChannel())
  {
    CLog::Log(LOGERROR, "CPVRManager - %s - no channel tag provided", __FUNCTION__);
    return false;
  }

  CSingleLock lock(m_critSectionStreams);
  g_application.CurrentFileItem() = *m_currentFile;
  g_infoManager.SetCurrentItem(*m_currentFile);

  CPVRChannel* channelTag = item.GetPVRChannelInfoTag();
  const CPVREpgInfoTag* epgTagNow = channelTag->GetEPGNow();

  if (channelTag->IsRadio())
  {
    CMusicInfoTag* musictag = item.GetMusicInfoTag();
    if (musictag)
    {
      musictag->SetTitle(epgTagNow ? epgTagNow->Title() : g_localizeStrings.Get(19055));
      musictag->SetGenre(epgTagNow ? epgTagNow->Genre() : "");
      musictag->SetDuration(epgTagNow ? epgTagNow->GetDuration() : 3600);
      musictag->SetURL(channelTag->Path());
      musictag->SetArtist(channelTag->ChannelName());
      musictag->SetAlbumArtist(channelTag->ChannelName());
      musictag->SetLoaded(true);
      musictag->SetComment("");
      musictag->SetLyrics("");
    }
  }
  else
  {
    CVideoInfoTag *videotag = item.GetVideoInfoTag();
    if (videotag)
    {
      videotag->m_strTitle = epgTagNow ? epgTagNow->Title() : g_localizeStrings.Get(19055);
      videotag->m_strGenre = epgTagNow ? epgTagNow->Genre() : "";
      videotag->m_strPath = channelTag->Path();
      videotag->m_strFileNameAndPath = channelTag->Path();
      videotag->m_strPlot = epgTagNow ? epgTagNow->Plot() : "";
      videotag->m_strPlotOutline = epgTagNow ? epgTagNow->PlotOutline() : "";
      videotag->m_iEpisode = epgTagNow ? epgTagNow->EpisodeNum() : 0;
    }
  }

  CPVRChannel* tagPrev = item.GetPVRChannelInfoTag();
  if (tagPrev && tagPrev->ChannelNumber() != m_LastChannel)
  {
    m_LastChannel         = tagPrev->ChannelNumber();
    m_LastChannelChanged  = CTimeUtils::GetTimeMS();
  }
  if (CTimeUtils::GetTimeMS() - m_LastChannelChanged >= (unsigned int) g_guiSettings.GetInt("pvrplayback.channelentrytimeout") && m_LastChannel != m_PreviousChannel[m_PreviousChannelIndex])
     m_PreviousChannel[m_PreviousChannelIndex ^= 1] = m_LastChannel;

  return false;
}

bool CPVRManager::StartPlayback(const CPVRChannel *channel, bool bPreview /* = false */)
{
  bool bReturn = false;
  g_settings.m_bStartVideoWindowed = bPreview;

  if (g_application.PlayFile(CFileItem(*channel)))
  {
    CLog::Log(LOGNOTICE, "PVRManager - %s - started playback on channel '%s'",
        __FUNCTION__, channel->ChannelName().c_str());
    bReturn = true;
  }
  else
  {
    CLog::Log(LOGERROR, "PVRManager - %s - failed to start playback on channel '%s'",
        __FUNCTION__, channel ? channel->ChannelName().c_str() : "NULL");
  }

  return bReturn;
}

bool CPVRManager::PerformChannelSwitch(const CPVRChannel &channel, bool bPreview)
{
  CSingleLock lock(m_critSectionStreams);

  CLog::Log(LOGDEBUG, "PVRManager - %s - switching to channel '%s'",
      __FUNCTION__, channel.ChannelName().c_str());

  SaveCurrentChannelSettings();
  if (m_currentFile)
  {
    delete m_currentFile;
    m_currentFile = NULL;
  }

  if (channel.ClientID() < 0 || !m_addons->SwitchChannel(channel))
  {
    CLog::Log(LOGERROR, "PVRManager - %s - failed to switch to channel '%s'",
        __FUNCTION__, channel.ChannelName().c_str());
    CGUIDialogOK::ShowAndGetInput(19033,0,19136,0);
    return false;
  }

  m_currentFile = new CFileItem(channel);
  LoadCurrentChannelSettings();

  CLog::Log(LOGNOTICE, "PVRManager - %s - switched to channel '%s'",
      __FUNCTION__, channel.ChannelName().c_str());

  return true;
}

int CPVRManager::GetTotalTime(void) const
{
  return m_guiInfo->GetTotalTime();
}

int CPVRManager::GetStartTime(void) const
{
  return m_guiInfo->GetStartTime();
}

const CStdString &CPVRManager::ConvertGenreIdToString(int iID, int iSubID)
{
  unsigned int iLabelId = 19499;
  switch (iID)
  {
    case EPG_EVENT_CONTENTMASK_MOVIEDRAMA:
      iLabelId = (iSubID <= 8) ? 19500 + iSubID : 19500;
      break;
    case EPG_EVENT_CONTENTMASK_NEWSCURRENTAFFAIRS:
      iLabelId = (iSubID <= 4) ? 19516 + iSubID : 19516;
      break;
    case EPG_EVENT_CONTENTMASK_SHOW:
      iLabelId = (iSubID <= 3) ? 19532 + iSubID : 19532;
      break;
    case EPG_EVENT_CONTENTMASK_SPORTS:
      iLabelId = (iSubID <= 11) ? 19548 + iSubID : 19548;
      break;
    case EPG_EVENT_CONTENTMASK_CHILDRENYOUTH:
      iLabelId = (iSubID <= 5) ? 19564 + iSubID : 19564;
      break;
    case EPG_EVENT_CONTENTMASK_MUSICBALLETDANCE:
      iLabelId = (iSubID <= 6) ? 19580 + iSubID : 19580;
      break;
    case EPG_EVENT_CONTENTMASK_ARTSCULTURE:
      iLabelId = (iSubID <= 11) ? 19596 + iSubID : 19596;
      break;
    case EPG_EVENT_CONTENTMASK_SOCIALPOLITICALECONOMICS:
      iLabelId = (iSubID <= 3) ? 19612 + iSubID : 19612;
      break;
    case EPG_EVENT_CONTENTMASK_EDUCATIONALSCIENCE:
      iLabelId = (iSubID <= 7) ? 19628 + iSubID : 19628;
      break;
    case EPG_EVENT_CONTENTMASK_LEISUREHOBBIES:
      iLabelId = (iSubID <= 7) ? 19644 + iSubID : 19644;
      break;
    case EPG_EVENT_CONTENTMASK_SPECIAL:
      iLabelId = (iSubID <= 3) ? 19660 + iSubID : 19660;
      break;
    case EPG_EVENT_CONTENTMASK_USERDEFINED:
      iLabelId = (iSubID <= 3) ? 19676 + iSubID : 19676;
      break;
    default:
      break;
  }

  return g_localizeStrings.Get(iLabelId);
}

bool CPVRManager::TranslateBoolInfo(DWORD dwInfo) const
{
  return m_guiInfo->TranslateBoolInfo(dwInfo);
}

const char* CPVRManager::TranslateCharInfo(DWORD dwInfo) const
{
  return m_guiInfo->TranslateCharInfo(dwInfo);
}

int CPVRManager::TranslateIntInfo(DWORD dwInfo) const
{
  return m_guiInfo->TranslateIntInfo(dwInfo);
}

bool CPVRManager::HasTimer(void) const
{
  return m_guiInfo->HasTimers();
}

bool CPVRManager::IsRecording(void) const
{
  return m_guiInfo->IsRecording();
}

const CPVREpgInfoTag *CPVRManager::GetPlayingTag(void) const
{
  return m_guiInfo->GetPlayingTag();
}

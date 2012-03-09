#include "VuData.h"

#include <curl/curl.h>
#include "client.h" 

using namespace ADDON;

#define CMD_LOCK cMutexLock CmdLock((cMutex*)&m_Mutex)


Vu::Vu() 
{
  m_bIsConnected = false;
  m_strServerName = "Vu";
  CStdString strURL = "";

  // simply add user@pass in front of the URL if username/password is set
  if ((g_strUsername.length() > 0) && (g_strPassword.length() > 0))
    strURL.Format("%s:%s@", g_strUsername.c_str(), g_strPassword.c_str());
  strURL.Format("http://%s%s:%u/", strURL.c_str(), g_strHostname.c_str(), g_iPortWeb);
  m_strURL = strURL.c_str();
  m_iNumChannels = 0;
  m_iNumTimers = 0; 
  m_iNumRecordings = 0;
  m_iNumChannelGroups = 0;
}

// Curl callback
int Vu::VuWebResponseCallback(void *contents, int iLength, int iSize, void *memPtr)
{
  int iRealSize = iSize * iLength;
  struct VuWebResponse *resp = (struct VuWebResponse*) memPtr;

  resp->response = (char*) realloc(resp->response, resp->iSize + iRealSize + 1);

  if (resp->response == NULL)
  {
    XBMC->Log(LOG_ERROR, "%s Could not allocate memeory!", __FUNCTION__);
    return 0;
  }

  memcpy(&(resp->response[resp->iSize]), contents, iRealSize);
  resp->iSize += iRealSize;
  resp->response[resp->iSize] = 0;

  return iRealSize;
}

bool Vu::Open()
{
  m_bIsConnected = false;

  // Load the TV channels - close connection if no channels are found
  if (!LoadChannelGroups())
    return false;

  // Load Channels
  for (int i = 0;i<m_iNumChannelGroups;  i++) 
  {
    VuChannelGroup &myGroup = m_groups.at(i);
    LoadChannels(myGroup.strServiceReference, myGroup.strGroupName);
  }

  // Load the radio channels - continue if no channels are found 
  CStdString strTmp;
  strTmp.Format("1:7:1:0:0:0:0:0:0:0:FROM BOUQUET \"userbouquet.favourites.radio\" ORDER BY bouquet");
  LoadChannels(strTmp, "radio");

  XBMC->Log(LOG_INFO, "%s Starting separate client update thread...", __FUNCTION__);

  SetDescription("VU+ Updater");
  Start(); 
  m_bIsConnected = true;

  return Running(); 
}

void  Vu::Action()
{
  XBMC->Log(LOG_DEBUG, "%s - starting", __FUNCTION__);

  while(Running())
  {
    CMD_LOCK;
    // Trigger Timer and Recording updates acording to the addon settings
    cCondWait::SleepMs(1000 * g_iUpdateInterval * 60);
    XBMC->Log(LOG_INFO, "%s Perform Updates!", __FUNCTION__);
    PVR->TriggerTimerUpdate();
    PVR->TriggerRecordingUpdate();
    if (g_bAutomaticTimerlistCleanup) 
    {
      CStdString strTmp;
      strTmp.Format("web/timercleanup?cleanup=true");
      CStdString strResult;
      if(!SendSimpleCommand(strTmp, strResult))
        XBMC->Log(LOG_ERROR, "%s - AutomaticTimerlistCleanup failed!", __FUNCTION__);
    }
  }
  XBMC->Log(LOG_DEBUG, "%s - exiting", __FUNCTION__);
}

void Vu::Close()
{
  m_bIsConnected = false;
}

bool Vu::LoadChannelGroups() 
{
  CStdString strTmp; 

  strTmp.Format("%sweb/getservices", m_strURL.c_str());

  CStdString strXML = GetHttpXML(strTmp);  

  XMLResults xe;
  XMLNode xMainNode = XMLNode::parseString(strXML.c_str(), NULL, &xe);  

  if(xe.error != 0)  {    
    XBMC->Log(LOG_ERROR, "%s Unable to parse XML. Error: '%s' ", __FUNCTION__, XMLNode::getError(xe.error));    
    return false;  
  }  

  m_groups.clear();
  m_iNumChannelGroups = 0;

  XMLNode xNode = xMainNode.getChildNode("e2servicelist");
  int n = xNode.nChildNode("e2service");

  XBMC->Log(LOG_INFO, "%s Number of elements: '%d'", __FUNCTION__, n);

  for (int i = 0; i<n; i++)
  {
    XMLNode xTmp = xNode.getChildNode("e2service", i);

    CStdString strTmp;
    
    if (!GetString(xTmp, "e2servicereference", strTmp))
      continue;
    
    // Check whether the current element is not just a label
    if (strTmp.compare(0,5,"1:64:") == 0)
      continue;

    VuChannelGroup newGroup;
    newGroup.strServiceReference = strTmp;

    if (!GetString(xTmp, "e2servicename", strTmp)) 
      continue;

    newGroup.strGroupName = strTmp;
 
    m_groups.push_back(newGroup);

    XBMC->Log(LOG_INFO, "%s Loaded channelgroup: %s", __FUNCTION__, newGroup.strGroupName.c_str());
    m_iNumChannelGroups++; 
  }

  XBMC->Log(LOG_INFO, "%s Loaded %d Channelsgroups", __FUNCTION__, m_iNumChannelGroups);
  return true;
}

bool Vu::LoadChannels(CStdString strServiceReference, CStdString strGroupName) 
{
  XBMC->Log(LOG_INFO, "%s loading channel group: '%s'", __FUNCTION__, strGroupName.c_str());

  CStdString strTmp;
  strTmp.Format("%sweb/getservices?sRef=%s", m_strURL.c_str(), URLEncodeInline(strServiceReference.c_str()));

  CStdString strXML = GetHttpXML(strTmp);  

  XMLResults xe;
  XMLNode xMainNode = XMLNode::parseString(strXML.c_str(), NULL, &xe);  

  if(xe.error != 0)  {    
    XBMC->Log(LOG_ERROR, "%s Unable to parse XML. Error: '%s' ", __FUNCTION__, XMLNode::getError(xe.error));    
    return false;  
  }  

  XMLNode xNode = xMainNode.getChildNode("e2servicelist");
  int n = xNode.nChildNode("e2service");

  XBMC->Log(LOG_INFO, "%s Number of elements: '%d'", __FUNCTION__, n);
  bool bRadio;

  bRadio = !strGroupName.compare("radio");

  for (int i = 0; i<n; i++)
  {
    XMLNode xTmp = xNode.getChildNode("e2service", i);
    CStdString strTmp;
    
    if (!GetString(xTmp, "e2servicereference", strTmp))
      continue;
    
    // Check whether the current element is not just a label
    if (strTmp.compare(0,5,"1:64:") == 0)
      continue;

    VuChannel newChannel;
    newChannel.bRadio = bRadio;
    newChannel.strGroupName = strGroupName;
    newChannel.iUniqueId = m_iNumChannels+1;
    newChannel.iChannelNumber = m_iNumChannels;
    newChannel.strServiceReference = strTmp;

    if (!GetString(xTmp, "e2servicename", strTmp)) 
      continue;

    newChannel.strChannelName = strTmp;
 
    CStdString strIcon;
    strIcon.Format("%s", newChannel.strServiceReference);

    std::replace(strIcon.begin(), strIcon.end(), ':','_');
    if (strIcon.size()>2)
      strIcon.erase(strIcon.size() - 1); 
    strTmp.Format("%s%s.png", g_strIconPath, strIcon.c_str());
    newChannel.strIconPath = strTmp;
    
    strTmp.Format("http://%s:%d/%s", g_strHostname, g_iPortStream, newChannel.strServiceReference);
    newChannel.strStreamURL = strTmp;

    m_channels.push_back(newChannel);
    XBMC->Log(LOG_INFO, "%s Loaded channel: %s, Icon: %s", __FUNCTION__, newChannel.strChannelName.c_str(), newChannel.strIconPath.c_str());


    m_iNumChannels++; 
  }

  XBMC->Log(LOG_INFO, "%s Loaded %d Channels", __FUNCTION__, m_iNumChannels);
  return true;
}

bool Vu::IsConnected() 
{
  return m_bIsConnected;
}

CStdString Vu::GetHttpXML(CStdString& url) 
{
  CURL* curl_handle;

  XBMC->Log(LOG_INFO, "%s Open webAPI with URL: '%s'", __FUNCTION__, url.c_str());

  struct VuWebResponse response;

  response.response = (char*) malloc(1);
  response.iSize = 0;

  // retrieve the webpage and store it in memory
  curl_global_init(CURL_GLOBAL_ALL);
  curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &VuWebResponseCallback);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&response);
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "vuplus-pvraddon-agent/1.0");
  curl_easy_perform(curl_handle);

  if (response.iSize == 0)
  {
    XBMC->Log(LOG_INFO, "%s Could not open webAPI", __FUNCTION__);
    return "";
  }

  CStdString strTmp;
  strTmp.Format("%s", response.response);

  XBMC->Log(LOG_INFO, "%s Got result. Length: %u", __FUNCTION__, strTmp.length());
  
  free(response.response);
  curl_easy_cleanup(curl_handle);

  return strTmp;
}

const char * Vu::GetServerName() 
{
  return m_strServerName.c_str();  
}

int Vu::GetChannelsAmount()
{
  return m_iNumChannels;
}

int Vu::GetTimersAmount()
{
  return m_iNumTimers;
}

unsigned int Vu::GetRecordingsAmount() {
  return m_iNumRecordings;
}

PVR_ERROR Vu::GetChannels(PVR_HANDLE handle, bool bRadio) 
{
    for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  {
    VuChannel &channel = m_channels.at(iChannelPtr);
    if (channel.bRadio == bRadio)
    {
      PVR_CHANNEL xbmcChannel;
      memset(&xbmcChannel, 0, sizeof(PVR_CHANNEL));

      xbmcChannel.iUniqueId         = channel.iUniqueId;
      xbmcChannel.bIsRadio          = channel.bRadio;
      xbmcChannel.iChannelNumber    = channel.iChannelNumber;
      xbmcChannel.strChannelName    = channel.strChannelName.c_str();
      xbmcChannel.strInputFormat    = ""; // unused
      xbmcChannel.strStreamURL      = channel.strStreamURL.c_str();
      xbmcChannel.iEncryptionSystem = 0;
      
      xbmcChannel.strIconPath = channel.strIconPath.c_str();
      xbmcChannel.bIsHidden         = false;

      PVR->TransferChannelEntry(handle, &xbmcChannel);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

Vu::~Vu() 
{
  m_channels.clear();  
  m_timers.clear();
  m_recordings.clear();
  m_groups.clear();
}

PVR_ERROR Vu::GetEPGForChannel(PVR_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  VuChannel &myChannel = m_channels.at(channel.iUniqueId-1);

  CStdString url;
  url.Format("%s%s%s",  m_strURL.c_str(), "web/epgservice?sRef=",  myChannel.strServiceReference); 
 
  CStdString strXML;
  strXML = GetHttpXML(url);

  int iNumEPG = 0;

  XMLResults xe;
  XMLNode xMainNode = XMLNode::parseString(strXML.c_str(), NULL, &xe);

  if(xe.error != 0)  {
    XBMC->Log(LOG_ERROR, "%s Unable to parse XML. Error: '%s' ", __FUNCTION__, XMLNode::getError(xe.error));
    return PVR_ERROR_SERVER_ERROR;
  }

  XMLNode xNode = xMainNode.getChildNode("e2eventlist");
  int n = xNode.nChildNode("e2event");

  XBMC->Log(LOG_INFO, "%s Number of elements: '%d'", __FUNCTION__, n);

  for (int i = 0; i<n; i++)
  {
    XMLNode xTmp = xNode.getChildNode("e2event", i);

    CStdString strTmp;
    int iTmpStart;
    int iTmp;

    // check and set event starttime and endtimes
    if (!GetInt(xTmp, "e2eventstart", iTmpStart)) 
      continue;
 
    if (!GetInt(xTmp, "e2eventduration", iTmp))
      continue;

    if ((iEnd > 1) && (iEnd < (iTmpStart + iTmp)))
       continue;
    
    VuEPGEntry entry;
    entry.startTime = iTmpStart;
    entry.endTime = iTmpStart + iTmp;

    if (!GetInt(xTmp, "e2eventid", entry.iEventId))  
      continue;

    entry.iChannelId = channel.iUniqueId;
    
    if(!GetString(xTmp, "e2eventtitle", strTmp))
      continue;

    entry.strTitle = strTmp;
    
    VuChannel &myChannel = m_channels.at(channel.iUniqueId-1);
    entry.strServiceReference = myChannel.strServiceReference;

    if (GetString(xTmp, "e2eventdescriptionextended", strTmp))
      entry.strPlot = strTmp;

    if (GetString(xTmp, "e2eventdescription", strTmp))
       entry.strPlotOutline = strTmp;

    EPG_TAG broadcast;
    memset(&broadcast, 0, sizeof(EPG_TAG));

    broadcast.iUniqueBroadcastId  = entry.iEventId;
    broadcast.strTitle            = entry.strTitle.c_str();
    broadcast.iChannelNumber      = channel.iChannelNumber;
    broadcast.startTime           = entry.startTime;
    broadcast.endTime             = entry.endTime;
    broadcast.strPlotOutline      = entry.strPlotOutline.c_str();
    broadcast.strPlot             = entry.strPlot.c_str();
    broadcast.strIconPath         = ""; // unused
    broadcast.iGenreType          = 0; // unused
    broadcast.iGenreSubType       = 0; // unused
    broadcast.strGenreDescription = "";
    broadcast.firstAired          = 0;  // unused
    broadcast.iParentalRating     = 0;  // unused
    broadcast.iStarRating         = 0;  // unused
    broadcast.bNotify             = false;
    broadcast.iSeriesNumber       = 0;  // unused
    broadcast.iEpisodeNumber      = 0;  // unused
    broadcast.iEpisodePartNumber  = 0;  // unused
    broadcast.strEpisodeName      = ""; // unused

    PVR->TransferEpgEntry(handle, &broadcast);

    iNumEPG++; 

    XBMC->Log(LOG_INFO, "%s loaded EPG entry '%d:%s' channel '%d' start '%d' end '%d'", __FUNCTION__, broadcast.iUniqueBroadcastId, broadcast.strTitle, entry.iChannelId, entry.startTime, entry.endTime);
  }

  XBMC->Log(LOG_INFO, "%s Loaded %u EPG Entries for channel '%s'", __FUNCTION__, iNumEPG, channel.strChannelName);
  return PVR_ERROR_NO_ERROR;
}

int Vu::GetChannelNumber(CStdString strServiceReference)  
{
  for (int i = 0;i<m_iNumChannels;  i++) 
  {
    VuChannel &myChannel = m_channels.at(i);
    if (!strServiceReference.compare(myChannel.strServiceReference))
      return i+1;
  }
  return -1;
}

PVR_ERROR Vu::GetTimers(PVR_HANDLE handle)
{
  CStdString url; 
  url.Format("%s%s", m_strURL.c_str(), "web/timerlist"); 

  CStdString strXML;
  strXML = GetHttpXML(url);

  XMLResults xe;
  XMLNode xMainNode = XMLNode::parseString(strXML.c_str(), NULL, &xe);

  if(xe.error != 0)  {
    XBMC->Log(LOG_ERROR, "%s Unable to parse XML. Error: '%s' ", __FUNCTION__, XMLNode::getError(xe.error));
    return PVR_ERROR_SERVER_ERROR;
  }

  XMLNode xNode = xMainNode.getChildNode("e2timerlist");
  int n = xNode.nChildNode("e2timer");

  XBMC->Log(LOG_INFO, "%s Number of elements: '%d'", __FUNCTION__, n);
  
  m_iNumTimers = 0;
  m_timers.clear();

  while(n>0)
  {
    int i = n-1;
    n--;
    XMLNode xTmp = xNode.getChildNode("e2timer", i);

    CStdString strTmp;
    int iTmp;
    bool bTmp;

    if (((!g_bShowTimersCompleted) && (GetInt(xTmp, "e2state", iTmp))) && (iTmp == 3))
      continue;

    if ((!GetInt(xTmp, "e2disabled", iTmp)) || (iTmp != 0))
      continue;

    VuTimer timer;

    if (GetString(xTmp, "e2servicereference", strTmp))
      timer.iChannelId = GetChannelNumber(strTmp.c_str());

    if (!GetInt(xTmp, "e2timebegin", iTmp)) 
      continue; 
 
    timer.startTime         = iTmp;
    
    if (!GetInt(xTmp, "e2timeend", iTmp)) 
      continue; 
 
    timer.endTime           = iTmp;
    
    if (GetString(xTmp, "e2name", strTmp)) {
      timer.strTitle          = strTmp;
    }

    if (GetString(xTmp, "e2description", strTmp))
      timer.strPlot        = strTmp.c_str();
 
    if (GetInt(xTmp, "e2repeated", iTmp))
      timer.iWeekdays         = iTmp;
    else 
      timer.iWeekdays = 0;

    if (timer.iWeekdays != 0)
      timer.bRepeating      = true; 
    else
      timer.bRepeating = false;
    
    if (GetInt(xTmp, "e2eit", iTmp))
      timer.iEpgID = iTmp;
    else 
      timer.iEpgID = 0;

    timer.state = PVR_TIMER_STATE_INVALID;

    if (!GetInt(xTmp, "e2state", iTmp))
      continue;

    XBMC->Log(LOG_INFO, "%s e2state is: %d ", __FUNCTION__, iTmp);
  
    if (iTmp == 0) {
      timer.state = PVR_TIMER_STATE_SCHEDULED;
      XBMC->Log(LOG_INFO, "%s Timer state is: SCHEDULED", __FUNCTION__);
    }
    
    if (iTmp == 2) {
      timer.state = PVR_TIMER_STATE_RECORDING;
      XBMC->Log(LOG_INFO, "%s Timer state is: RECORDING", __FUNCTION__);
    }
    
    if (iTmp == 3) {
      timer.state = PVR_TIMER_STATE_COMPLETED;
      XBMC->Log(LOG_INFO, "%s Timer state is: COMPLETED", __FUNCTION__);
    }

    if (GetBoolean(xTmp, "e2cancled", bTmp)) {
      if (bTmp)  {
        timer.state = PVR_TIMER_STATE_CANCELLED;
        XBMC->Log(LOG_INFO, "%s Timer state is: CANCELLED", __FUNCTION__);
      }
    }

    PVR_TIMER tag;
    memset(&tag, 0, sizeof(PVR_TIMER));
    tag.iClientChannelUid = timer.iChannelId;
    tag.startTime         = timer.startTime;
    tag.endTime           = timer.endTime;
    tag.strTitle          = timer.strTitle.c_str();
    tag.strDirectory      = "/";   // unused
    tag.strSummary        = timer.strPlot.c_str();
    tag.state             = timer.state;
    tag.iPriority         = 0;     // unused
    tag.iLifetime         = 0;     // unused
    tag.bIsRepeating      = timer.bRepeating;
    tag.firstDay          = 0;     // unused
    tag.iWeekdays         = timer.iWeekdays;
    tag.iEpgUid           = timer.iEpgID;
    tag.iMarginStart      = 0;     // unused
    tag.iMarginEnd        = 0;     // unused
    tag.iGenreType        = 0;     // unused
    tag.iGenreSubType     = 0;     // unused
    tag.iClientIndex = m_iNumTimers;

    PVR->TransferTimerEntry(handle, &tag);

    m_iNumTimers++; 
    m_timers.push_back(timer);

    XBMC->Log(LOG_INFO, "%s loaded Timer entry '%s'", __FUNCTION__, tag.strTitle);
  }

  XBMC->Log(LOG_INFO, "%s Loaded %u Timer Entries", __FUNCTION__, m_iNumTimers);
  return PVR_ERROR_NO_ERROR; 
}

CStdString Vu::URLEncodeInline(const CStdString& strData)
{
  CStdString buffer = strData;
  CURL* handle = curl_easy_init();
  char* encodedURL = curl_easy_escape(handle, strData.c_str(), strlen(strData.c_str()));

  buffer.Format("%s", encodedURL);
  curl_free(encodedURL);
  curl_easy_cleanup(handle);

  return buffer;
}

bool Vu::SendSimpleCommand(const CStdString& strCommandURL, CStdString& strResultText)
{
  CStdString url; 
  url.Format("%s%s", m_strURL.c_str(), strCommandURL.c_str()); 

  CStdString strXML;
  strXML = GetHttpXML(url);

  XMLResults xe;
  XMLNode xMainNode = XMLNode::parseString(strXML.c_str(), NULL, &xe);

  if(xe.error != 0)  {
    XBMC->Log(LOG_ERROR, "%s Unable to parse XML. Error: '%s' ", __FUNCTION__, XMLNode::getError(xe.error));
    return false;
  }

  XMLNode xNode = xMainNode.getChildNode("e2simplexmlresult");

  bool bTmp;

  if (!GetBoolean(xNode, "e2state", bTmp)) {
    XBMC->Log(LOG_ERROR, "%s Could not parse e2state from result!", __FUNCTION__);
    strResultText.Format("Could not parse e2state!");
    return false;
  }

  if (!GetString(xNode, "e2statetext", strResultText)) {
    XBMC->Log(LOG_ERROR, "%s Could not parse e2state from result!", __FUNCTION__);
    return false;
  }

  if (!bTmp)
    XBMC->Log(LOG_ERROR, "%s Error message from backend: '%s'", __FUNCTION__, strResultText.c_str());

  return bTmp;
}


PVR_ERROR Vu::AddTimer(const PVR_TIMER &timer)
{
  XBMC->Log(LOG_DEBUG, "%s - channelUid=%d title=%s epgid=%d", __FUNCTION__, timer.iClientChannelUid, timer.strTitle, timer.iEpgUid);

  CStdString strTmp;
  CStdString strServiceReference = m_channels.at(timer.iClientChannelUid-1).strServiceReference.c_str();

  // check if we got a event id
  //if (timer.iEpgUid > 0) 
  //  strTmp.Format("web/timeraddbyeventid?sRef=%s&eventid=%d", strServiceReference, timer.iEpgUid);
  //else
  strTmp.Format("web/timeradd?sRef=%s&repeated=%d&begin=%d&end=%d&name=%s&description=%s&eit=%d", strServiceReference, timer.iWeekdays, timer.startTime, timer.endTime, URLEncodeInline(timer.strTitle), URLEncodeInline(timer.strSummary),timer.iEpgUid);

  CStdString strResult;
  if(!SendSimpleCommand(strTmp, strResult)) 
    return PVR_ERROR_SERVER_ERROR;
  
  PVR->TriggerTimerUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR Vu::DeleteTimer(const PVR_TIMER &timer) 
{
  CStdString strTmp;
  CStdString strServiceReference = m_channels.at(timer.iClientChannelUid-1).strServiceReference.c_str();

  strTmp.Format("web/timerdelete?sRef=%s&begin=%d&end=%d", strServiceReference, timer.startTime, timer.endTime);

  CStdString strResult;
  if(!SendSimpleCommand(strTmp, strResult)) 
    return PVR_ERROR_SERVER_ERROR;

  PVR->TriggerTimerUpdate();
  if (timer.state == PVR_TIMER_STATE_RECORDING)
    PVR->TriggerRecordingUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR Vu::GetRecordings(PVR_HANDLE handle)
{
  CStdString url;
  url.Format("%s%s", m_strURL.c_str(), "web/movielist"); 
 
  CStdString strXML;
  strXML = GetHttpXML(url);

  XMLResults xe;
  XMLNode xMainNode = XMLNode::parseString(strXML.c_str(), NULL, &xe);

  if(xe.error != 0)  {
    XBMC->Log(LOG_ERROR, "%s Unable to parse XML. Error: '%s' ", __FUNCTION__, XMLNode::getError(xe.error));
    return PVR_ERROR_SERVER_ERROR;
  }

  XMLNode xNode = xMainNode.getChildNode("e2movielist");
  int n = xNode.nChildNode("e2movie");

  XBMC->Log(LOG_INFO, "%s Number of elements: '%d'", __FUNCTION__, n);
  
  m_iNumRecordings = 0;
  m_recordings.clear();

  while(n>0)
  {
    int i = n-1;
    n--;
    XMLNode xTmp = xNode.getChildNode("e2movie", i);
    CStdString strTmp;
    int iTmp;

    VuRecording recording;
    if (GetString(xTmp, "e2servicereference", strTmp))
      recording.strRecordingId = strTmp;

    if (GetString(xTmp, "e2title", strTmp))
      recording.strTitle = strTmp;
    
    if (GetString(xTmp, "e2description", strTmp))
      recording.strPlotOutline = strTmp;

    if (GetString(xTmp, "e2descriptionextended", strTmp))
      recording.strPlot = strTmp;
    
    if (GetString(xTmp, "e2servicename", strTmp))
      recording.strChannelName = strTmp;

    if (GetInt(xTmp, "e2time", iTmp)) 
      recording.startTime = iTmp;

    if (GetString(xTmp, "e2length", strTmp)) {
      iTmp = TimeStringToSeconds(strTmp.c_str());
      recording.iDuration = iTmp;
    }
    else
      recording.iDuration = 0;

    if (GetString(xTmp, "e2filename", strTmp)) {
      strTmp.Format("http://%s/file?file=%s", g_strHostname, URLEncodeInline(strTmp.c_str()));
      recording.strStreamURL = strTmp;
    }
    
    PVR_RECORDING tag;
    memset(&tag, 0, sizeof(PVR_RECORDING));
    tag.strRecordingId    = recording.strRecordingId.c_str();
    tag.strTitle          = recording.strTitle.c_str();
    tag.strStreamURL      = recording.strStreamURL.c_str();
    tag.strPlotOutline    = recording.strPlotOutline.c_str();
    tag.strPlot           = recording.strPlot.c_str();
    tag.strChannelName    = recording.strChannelName.c_str();
    tag.recordingTime     = recording.startTime;
    tag.iDuration         = recording.iDuration;
    tag.strDirectory      = "/";   // unused

    PVR->TransferRecordingEntry(handle, &tag);

    m_iNumRecordings++; 
    m_recordings.push_back(recording);

    XBMC->Log(LOG_INFO, "%s loaded Recording entry '%s', start '%d', length '%d'", __FUNCTION__, tag.strTitle, recording.startTime, recording.iDuration);
  }

  XBMC->Log(LOG_INFO, "%s Loaded %u Recording Entries", __FUNCTION__, m_iNumRecordings);

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR Vu::DeleteRecording(const PVR_RECORDING &recinfo) 
{
  CStdString strTmp;

  strTmp.Format("web/moviedelete?sRef=%s", URLEncodeInline(recinfo.strRecordingId));

  CStdString strResult;
  if(!SendSimpleCommand(strTmp, strResult)) 
    return PVR_ERROR_NOT_DELETED;

  PVR->TriggerRecordingUpdate();

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR Vu::UpdateTimer(const PVR_TIMER &timer)
{
  PVR->TriggerTimerUpdate();

  return PVR_ERROR_NOT_IMPLEMENTED;
}

bool Vu::GetInt(XMLNode xRootNode, const char* strTag, int& iIntValue)
{
  XMLNode xNode = xRootNode.getChildNode(strTag );
  if (xNode.isEmpty())
     return false;
  iIntValue = atoi(xNode.getText());
  return true;
}

bool Vu::GetBoolean(XMLNode xRootNode, const char* strTag, bool& bBoolValue)
{
  XMLNode xNode = xRootNode.getChildNode(strTag );
  if (xNode.isEmpty()) 
    return false;

  CStdString strEnabled = xNode.getText();

  strEnabled.ToLower();
  if (strEnabled == "off" || strEnabled == "no" || strEnabled == "disabled" || strEnabled == "false" || strEnabled == "0" )
    bBoolValue = false;
  else
  {
    bBoolValue = true;
    if (strEnabled != "on" && strEnabled != "yes" && strEnabled != "enabled" && strEnabled != "true")
      return false; // invalid bool switch - it's probably some other string.
  }
  return true;
}

bool Vu::GetString(XMLNode xRootNode, const char* strTag, CStdString& strStringValue)
{
  XMLNode xNode = xRootNode.getChildNode(strTag );
  if (!xNode.isEmpty())
  {
    strStringValue = xNode.getText();
    return true;
  }
  strStringValue.Empty();
  return false;
}

long Vu::TimeStringToSeconds(const CStdString &timeString)
{
  CStdStringArray secs;
  SplitString(timeString, ":", secs);
  int timeInSecs = 0;
  for (unsigned int i = 0; i < secs.size(); i++)
  {
    timeInSecs *= 60;
    timeInSecs += atoi(secs[i]);
  }
  return timeInSecs;
}

int Vu::SplitString(const CStdString& input, const CStdString& delimiter, CStdStringArray &results, unsigned int iMaxStrings)
{
  int iPos = -1;
  int newPos = -1;
  int sizeS2 = delimiter.GetLength();
  int isize = input.GetLength();

  results.clear();
  std::vector<unsigned int> positions;

  newPos = input.Find (delimiter, 0);

  if ( newPos < 0 )
  {
    results.push_back(input);
    return 1;
  }

  while ( newPos > iPos )
  {
    positions.push_back(newPos);
    iPos = newPos;
    newPos = input.Find (delimiter, iPos + sizeS2);
  }

  // numFound is the number of delimeters which is one less
  // than the number of substrings
  unsigned int numFound = positions.size();
  if (iMaxStrings > 0 && numFound >= iMaxStrings)
    numFound = iMaxStrings - 1;

  for ( unsigned int i = 0; i <= numFound; i++ )
  {
    CStdString s;
    if ( i == 0 )
    {
      if ( i == numFound )
        s = input;
      else
        s = input.Mid( i, positions[i] );
    }
    else
    {
      int offset = positions[i - 1] + sizeS2;
      if ( offset < isize )
      {
        if ( i == numFound )
          s = input.Mid(offset);
        else if ( i > 0 )
          s = input.Mid( positions[i - 1] + sizeS2,
                         positions[i] - positions[i - 1] - sizeS2 );
      }
    }
    results.push_back(s);
  }
  // return the number of substrings
  return results.size();
}

PVR_ERROR Vu::GetChannelGroups(PVR_HANDLE handle)
{
  for(unsigned int iTagPtr = 0; iTagPtr < m_groups.size(); iTagPtr++)
  {
    PVR_CHANNEL_GROUP tag;
    memset(&tag, 0 , sizeof(PVR_CHANNEL_GROUP));

    tag.bIsRadio     = false;
    tag.strGroupName = m_groups[iTagPtr].strGroupName.c_str();

    PVR->TransferChannelGroup(handle, &tag);
  }

  return PVR_ERROR_NO_ERROR;
}


unsigned int Vu::GetNumChannelGroups() {
  return m_iNumChannelGroups;
}

CStdString Vu::GetGroupServiceReference(CStdString strGroupName)  
{
  for (int i = 0;i<m_iNumChannelGroups;  i++) 
  {
    VuChannelGroup &myGroup = m_groups.at(i);
    if (!strGroupName.compare(myGroup.strGroupName))
      return myGroup.strServiceReference;
  }
  return "error";
}

PVR_ERROR Vu::GetChannelGroupMembers(PVR_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  XBMC->Log(LOG_DEBUG, "%s - group '%s'", __FUNCTION__, group.strGroupName);
  CStdString strTmp = group.strGroupName;
  for (int i = 0;i<m_iNumChannels;  i++) 
  {
    VuChannel &myChannel = m_channels.at(i);
    if (!strTmp.compare(myChannel.strGroupName)) 
    {
      PVR_CHANNEL_GROUP_MEMBER tag;
      memset(&tag,0 , sizeof(PVR_CHANNEL_GROUP_MEMBER));

      tag.strGroupName     = group.strGroupName;
      tag.iChannelUniqueId = myChannel.iUniqueId;
      tag.iChannelNumber   = myChannel.iChannelNumber;

      XBMC->Log(LOG_DEBUG, "%s - add channel %s (%d) to group '%s' channel number %d",
          __FUNCTION__, myChannel.strChannelName.c_str(), tag.iChannelUniqueId, group.strGroupName, myChannel.iChannelNumber);

      PVR->TransferChannelGroupMember(handle, &tag);
    }
  }
  return PVR_ERROR_NO_ERROR;
}

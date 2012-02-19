#pragma once 

#include "utils/StdString.h"
#include "xmlParser.h"
#include "client.h"
#include "thread.h"
    
#include "../../../addons/library.xbmc.addon/libXBMC_addon.h"

struct VuWebResponse {
  char *response;
  int iSize;
};

struct VuChannelGroup {
  std::string strServiceReference;
  std::string strGroupName;
};

struct VuEPGEntry 
{
  int iEventId;
  std::string strServiceReference;
  std::string strTitle;
  int iChannelId;
  time_t startTime;
  time_t endTime;
  std::string strPlotOutline;
  std::string strPlot;
};

struct VuTimer
{
  std::string strTitle;
  std::string strPlot;
  int iChannelId;
  time_t startTime;
  time_t endTime;
  bool bRepeating; 
  int iWeekdays;
  int iEpgID;
  PVR_TIMER_STATE state; 
};

struct VuRecording
{
  std::string strRecordingId;
  time_t startTime;
  int iDuration;
  std::string strTitle;
  std::string strStreamURL;
  std::string strPlot;
  std::string strPlotOutline;
  std::string strChannelName;
};
 
struct VuChannel
{
  bool bRadio;
  int iUniqueId;
  int iChannelNumber;
  std::string strGroupName;
  std::string strChannelName;
  std::string strServiceReference;
  std::string strStreamURL;
  std::string strIconPath;
};

class Vu  : public cThread
{
private:

  // members
  bool  m_bIsConnected;
  std::string m_strServerName;
  std::string m_strURL;
  int m_iNumChannels;
  int m_iNumTimers;
  int m_iNumRecordings;
  int m_iNumChannelGroups;
  std::vector<VuChannel> m_channels;
  std::vector<VuTimer> m_timers;
  std::vector<VuRecording> m_recordings;
  std::vector<VuChannelGroup> m_groups;
 
  cMutex m_Mutex;

  // functions

  CStdString GetHttpXML(CStdString& url);
  int GetChannelNumber(CStdString strServiceReference);
  CStdString URLEncodeInline(const CStdString& strData);
  bool SendSimpleCommand(const CStdString& strCommandURL, CStdString& strResult);
  static int VuWebResponseCallback(void *contents, int iLength, int iSize, void *memPtr); 
  CStdString GetGroupServiceReference(CStdString strGroupName);
  bool LoadChannels(CStdString strServerReference, CStdString strGroupName);
  bool LoadChannelGroups();

  // helper functions
  static bool GetInt(XMLNode xRootNode, const char* strTag, int& iIntValue);
  static bool GetBoolean(XMLNode xRootNode, const char* strTag, bool& bBoolValue);
  static bool GetString(XMLNode xRootNode, const char* strTag, CStdString& strStringValue);
  static long TimeStringToSeconds(const CStdString &timeString);
  static int SplitString(const CStdString& input, const CStdString& delimiter, CStdStringArray &results, unsigned int iMaxStrings = 0);



public:
  Vu(void);
  ~Vu();

  const char * GetServerName();
  bool IsConnected(); 
  int GetChannelsAmount(void);
  PVR_ERROR GetChannels(PVR_HANDLE handle, bool bRadio);
  PVR_ERROR GetEPGForChannel(PVR_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd);
  int GetTimersAmount(void);
  PVR_ERROR GetTimers(PVR_HANDLE handle);
  PVR_ERROR AddTimer(const PVR_TIMER &timer);
  PVR_ERROR UpdateTimer(const PVR_TIMER &timer);
  PVR_ERROR DeleteTimer(const PVR_TIMER &timer);
  unsigned int GetRecordingsAmount();
  PVR_ERROR    GetRecordings(PVR_HANDLE handle);
  PVR_ERROR    DeleteRecording(const PVR_RECORDING &recinfo);
  unsigned int GetNumChannelGroups(void);
  PVR_ERROR    GetChannelGroups(PVR_HANDLE handle);
  PVR_ERROR    GetChannelGroupMembers(PVR_HANDLE handle, const PVR_CHANNEL_GROUP &group);

  bool Open();
  void Action();
  void Close();


};


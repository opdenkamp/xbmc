/*
*      Copyright (C) 2005-2010 Team XBMC
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

#include "GUIDialogPVRTimerSerie.h"
#include "Application.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/GUIRadioButtonControl.h"
#include "guilib/GUILabelControl.h"
#include "dialogs/GUIDialogOK.h"

#include "pvr/PVRManager.h"
#include "pvr/addons/PVRClients.h"
#include "pvr/channels/PVRChannelGroupsContainer.h"
#include "epg/EpgInfoTag.h"
#include "pvr/timers/PVRTimers.h"
#include "pvr/timers/PVRTimerInfoTag.h"

using namespace std;
using namespace PVR;
using namespace EPG;

#define CONTROL_BTN_OK                 7
#define CONTROL_BTN_CANCEL             8
#define CONTROL_BTN_SKIP_REPEAT        9
#define CONTROL_BTN_THIS_CHANNEL       10
#define CONTROL_BTN_SAME_WEEKDAY       11
#define CONTROL_BTN_SAME_TIME          12
#define CONTROL_BTN_ONCE_A_WEEK        13
#define CONTROL_BTN_ONCE_A_DAY         14
#define CONTROL_LBL_WEEK_DAY           15

CGUIDialogPVRTimerSerie::CGUIDialogPVRTimerSerie(void)
  : CGUIDialog(WINDOW_DIALOG_PVR_TIMER_SERIE, "DialogPVRTimerSerie.xml")
  , m_progItem(new CFileItem)
{
  m_iResult=0;
  m_bConfirmed=false;
}

CGUIDialogPVRTimerSerie::~CGUIDialogPVRTimerSerie(void)
{
}

bool CGUIDialogPVRTimerSerie::UpdateButton(int ctrlButtonId, DWORD buttonFlag, const CPVRTimerInfoTag *timer, DWORD dwSupportedRules)
{
  CGUIRadioButtonControl *thisFlagButton=(CGUIRadioButtonControl*) GetControl(ctrlButtonId);
  if(thisFlagButton)
  {
    if(timer)
      thisFlagButton->SetSelected((timer->m_iSerieRule&buttonFlag)!=0);
    thisFlagButton->SetVisible((dwSupportedRules&buttonFlag)!=0);
  }
  return thisFlagButton!=NULL;
}

void CGUIDialogPVRTimerSerie::OnWindowLoaded()
{
  const CEpgInfoTag *tag;
  tag = m_progItem->GetEPGInfoTag();
  const CPVRTimerInfoTag *timer = g_PVRTimers->GetMatch(tag);
  const CPVRChannel *channel = tag->ChannelTag();
  DWORD dwSupportedRules=0;
  m_iResult=0;
  this->SetProperty("DayOfTheWeek", tag->StartAsLocalTime().GetDayOfWeek()!=0  ?  g_localizeStrings.Get(tag->StartAsLocalTime().GetDayOfWeek()+10) : g_localizeStrings.Get(17)); 
  if(channel)
    dwSupportedRules=g_PVRClients->GetAddonCapabilities(channel->ClientID()).dwSupportsRecordingRules;
  UpdateButton(CONTROL_BTN_THIS_CHANNEL, PVR_SERIE_SAME_CHANNEL, timer, dwSupportedRules);
  UpdateButton(CONTROL_BTN_SKIP_REPEAT, PVR_SERIE_SKIP_REPEAT, timer, dwSupportedRules);
  UpdateButton(CONTROL_BTN_SAME_WEEKDAY, PVR_SERIE_SAME_WEEKDAY, timer, dwSupportedRules);
  UpdateButton(CONTROL_BTN_SAME_TIME, PVR_SERIE_SAME_TIME, timer, dwSupportedRules);
  UpdateButton(CONTROL_BTN_ONCE_A_WEEK, PVR_SERIE_ONCE_PER_WEEK, timer, dwSupportedRules);
  UpdateButton(CONTROL_BTN_ONCE_A_DAY, PVR_SERIE_ONCE_PER_DAY, timer, dwSupportedRules);

  return CGUIDialog::OnWindowLoaded();
}

void CGUIDialogPVRTimerSerie::SetProgInfo(const CFileItem *item)
{
  *m_progItem = *item;
}

CFileItemPtr CGUIDialogPVRTimerSerie::GetCurrentListItem(int offset)
{
  return m_progItem;
}

bool CGUIDialogPVRTimerSerie::OnClickButtonOK(CGUIMessage &message)
{
  bool bReturn = false;

  if (message.GetSenderId() == CONTROL_BTN_OK)
  {
    Close();
    m_iResult|=PVR_SERIE_ON;
    m_bConfirmed=true;
    bReturn=true;
  }

  return bReturn;
}

bool CGUIDialogPVRTimerSerie::OnClickButtonCancel(CGUIMessage &message)
{
  bool bReturn = false;

  if (message.GetSenderId() == CONTROL_BTN_CANCEL)
  {
    Close();
    m_iResult=0;
    bReturn=true;
  }

  return bReturn;
}

void CGUIDialogPVRTimerSerie::UpdateButtonState()
{
  int iControlToUpdate=0;
  CGUIRadioButtonControl *onceADayBtn=(CGUIRadioButtonControl* ) GetControl(CONTROL_BTN_ONCE_A_DAY);
  CGUIRadioButtonControl *onceAWeekBtn=(CGUIRadioButtonControl*) GetControl(CONTROL_BTN_ONCE_A_WEEK);

  if(onceAWeekBtn)
    onceAWeekBtn->SetEnabled(!(m_iResult&PVR_SERIE_ONCE_PER_DAY));
  if(onceADayBtn)
    onceADayBtn->SetEnabled(!(m_iResult&PVR_SERIE_ONCE_PER_WEEK));
}

bool CGUIDialogPVRTimerSerie::OnButtonFlag(CGUIMessage &message)
{
  bool bReturn = false;
  DWORD dwFlag=0;
  switch(message.GetSenderId())
  {
  case CONTROL_BTN_SKIP_REPEAT: 
    dwFlag=PVR_SERIE_SKIP_REPEAT;
    break;
  case CONTROL_BTN_THIS_CHANNEL:
    dwFlag=PVR_SERIE_SAME_CHANNEL;
    break;
  case CONTROL_BTN_SAME_WEEKDAY:
    dwFlag=PVR_SERIE_SAME_WEEKDAY;
    break;
  case CONTROL_BTN_SAME_TIME:   
    dwFlag=PVR_SERIE_SAME_TIME;
    break;
  case CONTROL_BTN_ONCE_A_WEEK: 
    dwFlag=PVR_SERIE_ONCE_PER_WEEK;
    break;
  case CONTROL_BTN_ONCE_A_DAY:  
    dwFlag=PVR_SERIE_ONCE_PER_DAY;
    break;
  }
  if(dwFlag>0)
  {
    if(m_iResult&dwFlag)
      m_iResult&=~dwFlag;
    else m_iResult|=dwFlag;
    bReturn=true;
  }
  UpdateButtonState();

  return bReturn;
}

bool CGUIDialogPVRTimerSerie::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
  case GUI_MSG_WINDOW_INIT:
    CGUIDialog::OnMessage(message);
    break;
  case GUI_MSG_CLICKED:
    return OnClickButtonOK(message) || 
      OnClickButtonCancel(message) || 
      OnButtonFlag(message); 
  }

  return CGUIDialog::OnMessage(message);
}



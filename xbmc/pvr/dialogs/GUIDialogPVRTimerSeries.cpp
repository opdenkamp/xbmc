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

#include "GUIDialogPVRTimerSeries.h"
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

CGUIDialogPVRTimerSeries::CGUIDialogPVRTimerSeries(void)
  : CGUIDialog(WINDOW_DIALOG_PVR_TIMER_SERIES, "DialogPVRTimerSeries.xml")
  , m_progItem(new CFileItem)
{
  m_bConfirmed = false;
}

CGUIDialogPVRTimerSeries::~CGUIDialogPVRTimerSeries(void)
{
}

bool CGUIDialogPVRTimerSeries::UpdateButton(int ctrlButtonId, bool bIsSelected, bool bIsVisible)
{
  CGUIRadioButtonControl *thisFlagButton = (CGUIRadioButtonControl*) GetControl(ctrlButtonId);
  if(thisFlagButton)
  {
    thisFlagButton->SetSelected(bIsSelected);
    thisFlagButton->SetVisible(bIsVisible);
  }
  return thisFlagButton != NULL;
}

void CGUIDialogPVRTimerSeries::OnWindowLoaded()
{
  const CEpgInfoTag *tag = m_progItem->GetEPGInfoTag();
  CFileItemPtr timerTag = g_PVRTimers->GetTimerForEpgTag(m_progItem.get());
  bool bHasTimer = timerTag != NULL && timerTag->HasPVRTimerInfoTag();
  const CPVRChannelPtr channel = tag->ChannelTag();
  CTimerSeries SupportedRules;

  m_Result.Init();
  if(bHasTimer)
  {
    const CPVRTimerInfoTag *timer = timerTag->GetPVRTimerInfoTag();
    m_Result = timer->m_SeriesRule;
  }
  this->SetProperty("DayOfTheWeek", tag->StartAsLocalTime().GetDayOfWeek() != 0  ?  g_localizeStrings.Get(tag->StartAsLocalTime().GetDayOfWeek()+10) : g_localizeStrings.Get(17)); 
  if(channel)
    g_PVRClients->GetSupportedRules(channel->ClientID(), SupportedRules);
  UpdateButton(CONTROL_BTN_THIS_CHANNEL, m_Result.bOnThisChannelOnly, SupportedRules.bOnThisChannelOnly);
  UpdateButton(CONTROL_BTN_SKIP_REPEAT, m_Result.bSkipRepeat, SupportedRules.bSkipRepeat);
  UpdateButton(CONTROL_BTN_SAME_WEEKDAY, m_Result.bOnThisWeekDay, SupportedRules.bOnThisWeekDay);
  UpdateButton(CONTROL_BTN_SAME_TIME, m_Result.bAtThisTime, SupportedRules.bAtThisTime);
  UpdateButton(CONTROL_BTN_ONCE_A_WEEK, m_Result.bOncePerWeek, SupportedRules.bOncePerWeek);
  UpdateButton(CONTROL_BTN_ONCE_A_DAY, m_Result.bOncePerDay, SupportedRules.bOncePerDay);

  return CGUIDialog::OnWindowLoaded();
}

void CGUIDialogPVRTimerSeries::SetProgInfo(const CFileItem *item)
{
  *m_progItem = *item;
}

CFileItemPtr CGUIDialogPVRTimerSeries::GetCurrentListItem(int offset)
{
  return m_progItem;
}

bool CGUIDialogPVRTimerSeries::OnClickButtonOK(const CGUIMessage &message)
{
  bool bReturn = false;

  if (message.GetSenderId() == CONTROL_BTN_OK)
  {
    Close();
    m_Result.bRecord = true;
    m_bConfirmed = true;
    bReturn = true;
  }

  return bReturn;
}

bool CGUIDialogPVRTimerSeries::OnClickButtonCancel(const CGUIMessage &message)
{
  bool bReturn = false;

  if (message.GetSenderId() == CONTROL_BTN_CANCEL)
  {
    Close();
    m_Result.Init();
    bReturn = true;
  }

  return bReturn;
}

void CGUIDialogPVRTimerSeries::UpdateButtonState()
{
  int iControlToUpdate = 0;
  CGUIRadioButtonControl *onceADayBtn = (CGUIRadioButtonControl* ) GetControl(CONTROL_BTN_ONCE_A_DAY);
  CGUIRadioButtonControl *onceAWeekBtn = (CGUIRadioButtonControl*) GetControl(CONTROL_BTN_ONCE_A_WEEK);

  if(onceAWeekBtn)
    onceAWeekBtn->SetEnabled(!m_Result.bOncePerDay);
  if(onceADayBtn)
    onceADayBtn->SetEnabled(!m_Result.bOncePerWeek);
}

bool CGUIDialogPVRTimerSeries::OnButtonFlag(const CGUIMessage &message)
{
  bool bReturn = false;
  switch(message.GetSenderId())
  {
  case CONTROL_BTN_SKIP_REPEAT: 
    m_Result.bSkipRepeat = !m_Result.bSkipRepeat;
    bReturn = true;
    break;
  case CONTROL_BTN_THIS_CHANNEL:
    m_Result.bOnThisChannelOnly = !m_Result.bOnThisChannelOnly;
    bReturn = true;
    break;
  case CONTROL_BTN_SAME_WEEKDAY:
    m_Result.bOnThisWeekDay = !m_Result.bOnThisWeekDay;
    bReturn = true;
    break;
  case CONTROL_BTN_SAME_TIME:   
    m_Result.bAtThisTime = !m_Result.bAtThisTime;
    bReturn = true;
    break;
  case CONTROL_BTN_ONCE_A_WEEK: 
    m_Result.bOncePerWeek = !m_Result.bOncePerWeek;
    bReturn = true;
    break;
  case CONTROL_BTN_ONCE_A_DAY:  
    m_Result.bOncePerDay =! m_Result.bOncePerDay;
    bReturn = true;
    break;
  }
 
  UpdateButtonState();

  return bReturn;
}

bool CGUIDialogPVRTimerSeries::OnMessage(CGUIMessage& message)
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



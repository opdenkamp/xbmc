#pragma once
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

#include "guilib/GUIDialog.h"
#include "pvr/timers/PVRTimers.h"
#include "../addons/include/xbmc_pvr_types.h"

namespace EPG
{
  class CEpgInfoTag;
}

namespace PVR
{
  class CPVRTimerInfoTag;

  class CGUIDialogPVRTimerSeries : public CGUIDialog
  {
  public:
    CGUIDialogPVRTimerSeries(void);
    virtual ~CGUIDialogPVRTimerSeries(void);
    virtual bool OnMessage(CGUIMessage& message);
    virtual CFileItemPtr GetCurrentListItem(int offset = 0);
    virtual void OnWindowLoaded();
    CTimerSeries GetResult(){ return m_Result; };
    bool IsConfirmed() const { return m_bConfirmed; }
    void SetProgInfo(const CFileItem *item);
  protected:
    bool UpdateButton(int ctrlButtonId, bool bIsSelected, bool bIsVisible);
    void UpdateButtonState();
    bool OnClickButtonOK(const CGUIMessage &message);
    bool OnClickButtonCancel(const CGUIMessage &message);
    bool OnButtonFlag(const CGUIMessage &message);
    CFileItemPtr m_progItem;

  private:
      CTimerSeries m_Result;
      bool m_bConfirmed;
  };
}

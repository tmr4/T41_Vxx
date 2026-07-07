// library: https://github.com/PaulStoffregen/USBHost_t36
//

#include "SDT.h"

int mouseWheelValue = 0;
int menuBarSelected = false;

#if HOST_KEYBOARD_MOUSE_SUPPORT

#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
#include "Encoders.h"
#include "ft8.h"
#include "Menu.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Tune.h"

#include "USBManager.h"

/*  it would be nice to save this memory until a keyboard is plugged in
    but both USBHost and USBHIDParser are needed to automatically detect
    a new devise so we don't really save that much.  Doing this manually
    is a possibility if we need to save memory when not using a keyboard. */
USBHIDParser mouseParser(USBManager::getHost()); // each device needs a parser
MouseController mouseController(USBManager::getHost());


//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// *** TODO: this is display dependent, but also fundamental to much of how the DSP process works ***
#define SPECTRUM_RES          512

#define MOUSE_BUTTON_DOWN_LEFT  1
#define MOUSE_BUTTON_DOWN_RIGHT 2

int cursorL, cursorT, cursorR, cursorB;
int cursorX, cursorY, oldCursorX, oldCursorY;
extern int cursorW, cursorH;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void SetMouseArea(int left, int top, int width, int height) {
  cursorL = left;
  cursorT = top;
  cursorR = left + width;
  cursorB = top + height;

  cursorX = left;
  cursorY = top;
  oldCursorX = left;
  oldCursorY = top;
}

FLASHMEM void MouseInit() {
  SetMouseArea(0, 0, GetDisplayWidth(), GetDisplayHeight());
  cursorW = GetCursorWidth();
  cursorH = GetCursorHeight();
}

void MoveCursor(int x, int y) {
  int width = GetCharWidth(0);
  int height = GetCharHeight(0);

  cursorX += x * 4;
  cursorY += y * 2;

  if(cursorX > cursorR - width) cursorX = cursorR - width;
  if(cursorX < cursorL) cursorX = cursorL;
  if(cursorY > cursorB - height + 0) cursorY = cursorB - height + 0; // *** extra 8 adjustment allows cursor to cover last row on display ***
  if(cursorY < cursorT) cursorY = cursorT;

  //Serial.print("cursorX = "); Serial.print(cursorX); Serial.print(" cursorY = "); Serial.println(cursorY);

  DrawCursor(cursorX, cursorY, oldCursorX, oldCursorY);

  oldCursorX = cursorX;
  oldCursorY = cursorY;
}

void MouseButtonMenuArea(int button) {
  if(button == 1) {
    if(getMenuValueActive || getMenuOptionActive) {
      menuBarSelected = true;
    } else {
      MenuBarSelect();
    }
  } else if(button == 2) {
    ShowMenuBar();
  }
}

void MouseWheelMenuArea(int wheel) {
  if(getMenuValueActive || getMenuOptionActive) {
    mouseWheelValue = wheel;
  } else {
    MenuBarChange(wheel);
  }
}

void MouseButtonSpectrumWaterfall(int button) {
  if(t41.DemodMode == DEMOD_FT8_INTERNAL) {
    FT8MsgWindowClick(cursorX, cursorY, button);
    return;
  }
  switch(button) {
    case 1: // left click
      // there was a left click is in the spectrum or waterfall area, set the NCO frequency

      ReplaceCursor(oldCursorX, oldCursorY);

      t41.NCOFreq = (cursorX + cursorW / 2 - centerLine) * t41.SampleRate / (1 << t41.SpectrumZoom) / SPECTRUM_RES;

      switch(t41.DisplayState) {
        case DISPLAY_T41:
          DrawBandwidthBar();
          break;

        case DISPLAY_BEACON_MONITOR:
          break;

        case DISPLAY_FULL_MENU:
          break;

        default:
        // no screen updates at all
        break;
      }

      // background under the cursor may have changed, copy it for replacement next time
      CopyCursor(cursorX, cursorY);
      break;

    case 2: // right click
      break;

    case 4: // wheel click
      break;

    default:
      break;
  }
}

void MouseWheelSpectrumWaterfall(int wheel) {
  if(t41.DemodMode == DEMOD_FT8_INTERNAL) {
    if(wheel != 0) ScrollFt8MsgWindow(cursorX, wheel);
  } else {
    if(t41.MouseCenterTuneActive) {
      SetCenterTune((long)t41.FreqIncrement() * wheel);
    } else {
      t41.NCOFreq += t41.FtIncrement() * wheel;
    }
  }
}

void MouseLoop() {
  int x, y, button, wheel;

  if(mouseController.available()) {
    x = mouseController.getMouseX();
    y = mouseController.getMouseY();
    button = mouseController.getButtons();
    wheel = mouseController.getWheel();

    if(x || y)
      MoveCursor(x, y);

    // *** TODO: these can be refined ***
    if(button) {
      // check if the cursor is in any areas with an button action
      if(CursorInMenuArea(cursorX, cursorY)) {
        // the cursor is in the menu bar area
        MouseButtonMenuArea(button);
      } else if(CursorInFreqArea(cursorX, cursorY)) {
        // the cursor is in the frequency field
        MouseButtonFreqArea(cursorX, button);
      } else if(CursorInOpStatsArea(cursorX, cursorY)) {
        // the cursor is in the operating stats area
        MouseButtonOpStatsArea(cursorX, button);
      } else if(CursorInAudioSpectrum(cursorX, cursorY)) {
        ButtonFilter();
      } else if(CursorInSpectrumWaterfall(cursorX, cursorY)) {
        if(t41.DemodMode == DEMOD_FT8_INTERNAL) {
          FT8MsgWindowClick(cursorX, cursorY, button);
        } else {
          MouseButtonSpectrumWaterfall(button);
        }
      } else if(CursorInInfoBox(cursorX, cursorY)) {
        if(t41.DemodMode == DEMOD_FT8_INTERNAL) {
          FT8MsgWindowClick(cursorX, cursorY, button);
        } else {
          MouseButtonInfoBox(button, cursorX + cursorW / 2, cursorY + cursorH / 2);
        }
      }
    }

    if(wheel) {
      //Serial.print(",  wheel = "); Serial.println(wheel);
      if(CursorInMenuArea(cursorX, cursorY)) {
        // the cursor is in the menu bar area
        MouseWheelMenuArea(wheel);
      } else if(CursorInFreqArea(cursorX, cursorY)) {
        MouseWheelFreqArea(cursorX, wheel);
      } else if(CursorInSpectrumWaterfall(cursorX, cursorY)) {
        if(t41.DemodMode == DEMOD_FT8_INTERNAL) {
          ScrollFt8MsgWindow(cursorX, wheel);
        } else {
          MouseWheelSpectrumWaterfall(wheel);
        }
      } else if(CursorInAudioSpectrum(cursorX, cursorY)) {
        if(t41.DemodMode == DEMOD_NFM && nfmBWFilterActive) {
          // we're adjusting NFM demod bandwidth
          filter_pos_BW = last_filter_pos_BW - 5 * wheel;
        } else {
          // we're adjusting audio spectrum filter
          posFilterEncoder = lastFilterEncoder - 5 * wheel;
        }
      } else if(CursorInInfoBox(cursorX, cursorY)) {
        if(t41.LiveNoiseFloor == 2) {
          t41.NoiseFloor += wheel;
        } else {
          MouseWheelInfoBox(wheel, cursorX + cursorW / 2, cursorY + cursorH / 2);
        }
      }
    }

    mouseController.mouseDataClear();
  }
}

#endif

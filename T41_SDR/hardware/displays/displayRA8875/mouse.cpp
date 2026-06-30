
#include "..\..\SDT.h"

#include "..\..\ButtonProc.h"
#include "..\..\CWProcessing.h"
#include "Display.h"
#include "..\..\Display.h"
#include "Menu.h"
#include "..\..\MenuProc.h"
#include "..\..\Tune.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// active VFO y axis frequency position translated for center of cursor
#define FREQ_T  FREQUENCY_Y - (CURSOR_H / 2)
#define FREQ_B  FREQ_T + FREQ_H - 10

// mouse cursor is the RA8875 0x07 character which is a solid circle rendered
// in the middle of the cell.  Therefore when user selects something with the
// cursor, the reported x and y will be offset to the right and high.  These
// are used to adjust the reported position.
// *** TODO: make global set up on initialization ***
#define CURSOR_W  8
#define CURSOR_H  16

#define FREQ_W  32
#define FREQ_H  48

// active VFO x axis frequency digit left position translated for center of cursor
// 40m band and below
#define FREQ_40_6  FREQUENCY_X
#define FREQ_40_5  FREQ_40_6 + FREQ_W * 2  - (CURSOR_W / 2)
#define FREQ_40_4  FREQ_40_5 + FREQ_W
#define FREQ_40_3  FREQ_40_4 + FREQ_W
#define FREQ_40_2  FREQ_40_3 + FREQ_W * 2
#define FREQ_40_1  FREQ_40_2 + FREQ_W
#define FREQ_40_0  FREQ_40_1 + FREQ_W

// 20m and above
#define FREQ_20_7  FREQUENCY_X
#define FREQ_20_6  FREQ_20_7 + FREQ_W - (CURSOR_W / 2)
#define FREQ_20_5  FREQ_20_6 + FREQ_W * 2
#define FREQ_20_4  FREQ_20_5 + FREQ_W
#define FREQ_20_3  FREQ_20_4 + FREQ_W
#define FREQ_20_2  FREQ_20_3 + FREQ_W * 2
#define FREQ_20_1  FREQ_20_2 + FREQ_W
#define FREQ_20_0  FREQ_20_1 + FREQ_W

// *** initialized in MouseInit
int cursorW, cursorH;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM int GetCursorWidth() {
  tft.setFontScale((enum RA8875tsize)0);

  cursorW = tft.getFontWidth();

  return cursorW;
}

FLASHMEM int GetCursorHeight() {
  tft.setFontScale((enum RA8875tsize)0);

  cursorH = tft.getFontHeight();

  return cursorH;
}

void ReplaceCursor(int x, int y) {
  // replace what was previously under the cursor
  tft.BTE_move(800-8, 0, cursorW, cursorH, x, y, 2, 2);
}

void CopyCursor(int x, int y) {
  tft.BTE_move(x, y, cursorW, cursorH, 800-8, 0, 2, 2);
}

void DrawCursor(int cursorX, int cursorY, int oldCursorX, int oldCursorY) {
  // the cursor is drawn on layer 2, switch to it
  tft.writeTo(L2);

  // Other items occupy layer 2, we need to prevent the cursor from overwriting them.
  // We do this by copying what will be under the cursor for restoration later.
  // The RA8875 has limited functionality to do this.  I've hidden the data
  // on layer 2 behind the RX/TX indicator block.
  if(cursorY < SPEC_BOX_T - cursorH) { // *** entire cursor must be out of this region ***
    // there's no layer 2 items in this area, erase old cursor by simply drawing it again in black
    // this also serves to correct for when the cursor is within the data copy area
    tft.setTextColor(RA8875_BLACK);
    tft.setCursor(oldCursorX, oldCursorY);
    tft.print((char)7);
  } else {
    // replace what was previously on layer 2 under the cursor
    //BTE_move(SourceX, SourceY, Width, Height, DestX, DestY, SourceLayer, DestLayer,bool Transparent, uint8_t ROP, bool Monochrome, bool ReverseDir)
    //tft.BTE_move(0, 0, cursorW, cursorH, oldCursorX, oldCursorY, 2, 2);
    tft.BTE_move(800-8, 0, cursorW, cursorH, oldCursorX, oldCursorY, 2, 2);

    // copy the background under the cursor for replacement next time
    tft.BTE_move(cursorX, cursorY, cursorW, cursorH, 800-8, 0, 2, 2);
  }

  // draw new cursor
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(cursorX, cursorY);
  tft.print((char)7);

  tft.writeTo(L1); // switch to layer 1
}

bool CursorInMenuArea(int cursorX, int cursorY) {
  return (cursorY > MENUS_Y) && (cursorX < BOTH_MENU_WIDTHS);
}

bool CursorInFreqArea(int cursorX, int cursorY) {
  return (cursorY > FREQ_T && cursorY < FREQ_B) && (cursorX > 0 && cursorX < TIME_X - 20);
}

bool CursorInOpStatsArea(int cursorX, int cursorY) {
  return (cursorY > OPERATION_STATS_T - CURSOR_H / 2) && (cursorY < OPERATION_STATS_T - CURSOR_H / 2 + OPERATION_STATS_H) && (cursorX >= 0 && cursorX <= OPERATION_STATS_W);
}

bool CursorInAudioSpectrum(int cursorX, int cursorY) {
  return (cursorY > AUDIO_SPEC_BOX_T - CURSOR_H / 2) && (cursorY < AUDIO_SPEC_BOX_BOTTOM - CURSOR_H / 2) && cursorX > AUDIO_SPEC_BOX_L;
}

bool CursorInSpectrumWaterfall(int cursorX, int cursorY) {
  return (cursorY > SPEC_BOX_T) && (cursorX < SPEC_BOX_W);
}

bool CursorInInfoBox(int cursorX, int cursorY) {
  return (cursorY > INFO_BOX_T) && (cursorY < INFO_BOX_T + INFO_BOX_H) && (cursorX > INFO_BOX_L && cursorX < INFO_BOX_L + INFO_BOX_W);
}

void MouseButtonFreqArea(int cursorX, int button) {
  int inc = 0;
  int vfoOffset = t41.ActiveVFO == VFO_A ? 0 : VFO_B_ACTIVE_OFFSET;
  int x = cursorX - vfoOffset; // adjust cursor position for active VFO
  int TxRxFreq = t41.ActiveFreq();

  switch(button) {
    case 1:
      // we're switching to the other VFO if we click within its field
      if((t41.ActiveVFO == VFO_B && cursorX < VFO_B_ACTIVE_OFFSET - 50) || (t41.ActiveVFO == VFO_A && cursorX > VFO_B_INACTIVE_OFFSET)) {
        VFOSelect(t41.ActiveVFO == VFO_A ? VFO_B : VFO_A);
      }
      break;

    case 2:
      // we're zeroing a portion of the frequency if we're within the active VFO
      //Serial.print(cursorX); Serial.print(","); Serial.println(cursorY);

      if(TxRxFreq < 10000000) {
        if(x > FREQ_40_3 && x < FREQ_40_3 + FREQ_W) {
          inc = TxRxFreq % 10000;
        } else if(x > FREQ_40_4 && x < FREQ_40_4 + FREQ_W) {
          inc = TxRxFreq % 100000;
        } else if(x > FREQ_40_5 && x < FREQ_40_5 + FREQ_W) {
          inc = TxRxFreq % 1000000;
        } else if(x > FREQ_40_2 && x < FREQ_40_2 + FREQ_W) {
          inc = TxRxFreq % 1000;
        } else if(x > FREQ_40_1 && x < FREQ_40_1 + FREQ_W) {
          inc = TxRxFreq % 100;
        } else if(x > FREQ_40_6 && x < FREQ_40_6 + FREQ_W) {
          inc = TxRxFreq % 10000000;
        } else if(x > FREQ_40_0 && x < FREQ_40_0 + FREQ_W) {
          inc = TxRxFreq % 10;
        }
      } else {
        if(x > FREQ_20_3 && x < FREQ_20_3 + FREQ_W) {
          inc = TxRxFreq % 10000;
        } else if(x > FREQ_20_4 && x < FREQ_20_4 + FREQ_W) {
          inc = TxRxFreq % 100000;
        } else if(x > FREQ_20_5 && x < FREQ_20_5 + FREQ_W) {
          inc = TxRxFreq % 1000000;
        } else if(x > FREQ_20_2 && x < FREQ_20_2 + FREQ_W) {
          inc = TxRxFreq % 1000;
        } else if(x > FREQ_20_1 && x < FREQ_20_1 + FREQ_W) {
          inc = TxRxFreq % 100;
        } else if(x > FREQ_20_6 && x < FREQ_20_6 + FREQ_W) {
          inc = TxRxFreq % 10000000;
        } else if(x > FREQ_20_0 && x < FREQ_20_0 + FREQ_W) {
          inc = TxRxFreq % 10;
        } else if(x > FREQ_20_7 && x < FREQ_20_7 + FREQ_W) {
          inc = TxRxFreq % 100000000;
        }
      }
      if(inc < t41.SampleRate / (1 << t41.SpectrumZoom)) {
        t41.NCOFreq -= inc;
      } else {
        SetCenterTune(-inc);
      }
      break;

      default:
        break;
  }
}

void MouseWheelFreqArea(int cursorX, int wheel) {
  int inc = 0;
  int vfoOffset = t41.ActiveVFO == VFO_A ? 0 : VFO_B_ACTIVE_OFFSET;
  int x = cursorX - vfoOffset; // adjust cursor position for active VFO
  int TxRxFreq = t41.ActiveFreq();

  //Serial.println(wheel);

  if(TxRxFreq < 10000000) {
    if(x > FREQ_40_3 && x < FREQ_40_3 + FREQ_W) {
      inc = 1000;
    } else if(x > FREQ_40_4 && x < FREQ_40_4 + FREQ_W) {
      inc = 10000;
    } else if(x > FREQ_40_5 && x < FREQ_40_5 + FREQ_W) {
      inc = 100000;
    } else if(x > FREQ_40_2 && x < FREQ_40_2 + FREQ_W) {
      inc = 100;
    } else if(x > FREQ_40_1 && x < FREQ_40_1 + FREQ_W) {
      inc = 10;
    } else if(x > FREQ_40_6 && x < FREQ_40_6 + FREQ_W) {
      inc = 1000000;
    } else if(x > FREQ_40_0 && x < FREQ_40_0 + FREQ_W) {
      inc = 1;
    }
  } else {
    if(x > FREQ_20_3 && x < FREQ_20_3 + FREQ_W) {
      inc = 1000;
    } else if(x > FREQ_20_4 && x < FREQ_20_4 + FREQ_W) {
      inc = 10000;
    } else if(x > FREQ_20_5 && x < FREQ_20_5 + FREQ_W) {
      inc = 100000;
    } else if(x > FREQ_20_2 && x < FREQ_20_2 + FREQ_W) {
      inc = 100;
    } else if(x > FREQ_20_1 && x < FREQ_20_1 + FREQ_W) {
      inc = 10;
    } else if(x > FREQ_20_6 && x < FREQ_20_6 + FREQ_W) {
      inc = 1000000;
    } else if(x > FREQ_20_0 && x < FREQ_20_0 + FREQ_W) {
      inc = 1;
    } else if(x > FREQ_20_7 && x < FREQ_20_7 + FREQ_W) {
      inc = 10000000;
    }
  }
  //inc *= wheel;
  //Serial.println(inc);

  if(inc < t41.SampleRate / (1 << t41.SpectrumZoom)) {
    t41.NCOFreq += inc * wheel;
  } else {
    SetCenterTune(inc * wheel);
  }
}

void MouseButtonOpStatsArea(int cursorX, int button) {
  if(button == 1 && cursorX < OPERATION_STATS_BD - 20) {
    ResetTuning();
  } else if(cursorX > OPERATION_STATS_BD - 5 && cursorX < OPERATION_STATS_MD - 20) {
    if(button == 1) {
      ChangeBand(1);
    } else {
      ChangeBand(-1);
    }
  } else if(button == 1 && cursorX > OPERATION_STATS_MD - 5 && cursorX < OPERATION_STATS_CWF) {
    // change to the next mode: SSB -> CW -> DATA -> SSB
    ButtonMode();
  } else if(button == 1 && cursorX > OPERATION_STATS_CWF - 5 && cursorX < OPERATION_STATS_DMD - 5) {
    ToggleCWFilter();
  } else if(button == 1 && cursorX > OPERATION_STATS_DMD - 5 && cursorX < OPERATION_STATS_PWR - 5) {
    // change to the next demod mode
    ChangeDemodMode(t41.DemodMode + 1);
  } else if(cursorX > OPERATION_STATS_PWR - 5 && cursorX < OPERATION_STATS_REM - 5) {
    if(button == 1) {
      t41.TxPower += 1;
    } else if(button == 2) {
      t41.TxPower -= 1;
    }
  }
}

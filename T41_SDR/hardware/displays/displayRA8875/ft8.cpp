
#include "..\..\SDT.h"

#include "Display.h"
#include "..\..\Display.h"
#include "..\..\ft8.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern /*EXTMEM*/ RxMsg rxBuf[];
extern /*EXTMEM*/ TxMsg txBuf[];
extern /*EXTMEM*/ QsoView qsoList[];

extern int decodedMsgs, activeMsg, activeQSO, qsos;
extern bool qsoViewActive;

// selected msg detail and tx queue below msg lists
//#define FT8_WINDOW_TOP          (YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS)
//#define FT8_MSG_LIST_TOP        (YPIXELS - FT8_ROW_HEIGHT * (FT8_MSG_ROWS + 2))
//#define FT8_MSG_LIST_SUMMARY    (YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS)
//#define FT8_MSG_WINDOW_DETAIL   (YPIXELS - FT8_ROW_HEIGHT * 2)
//#define FT8_TX_QUEUE_TOP        (YPIXELS - FT8_ROW_HEIGHT)

// selected msg detail above and tx queue below msg lists
// *** TODO: incorporate "-3" adjustments in tft.print statements ***
//#define FT8_WINDOW_TOP          (YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS)
#define FT8_WINDOW_TOP          WATERFALL_T
#define FT8_MSG_LIST_TOP        (YPIXELS - FT8_ROW_HEIGHT * (FT8_MSG_ROWS + 0))
#define FT8_MSG_LIST_SUMMARY    (YPIXELS - FT8_ROW_HEIGHT * (FT8_MSG_ROWS + 1))
#define FT8_TX_QUEUE_TOP        (YPIXELS - FT8_ROW_HEIGHT)
#define FT8_MSG_WINDOW_DETAIL   FT8_WINDOW_TOP
#define FT8_QSO_VIEW_TOP        (YPIXELS - FT8_ROW_HEIGHT * 2)

// *** TODO: reconsider use of fixed row height vs display dependent in routines ***
#define FT8_MSG_ROWS    11
#define FT8_ROWS        13
#define FT8_ROW_HEIGHT  16
#define FT8_COL_WIDTH   8

//#define SPECTRUM_TESTING // plots frequency spectrum frame instead of waterfall (change frame with volume knob)
//#define TX_TESTING // generates mock RX messages for "CQ KN6ZDE CM87" TX test

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void DisplayListStats(int window);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void InitFT8Display() {
  SetWaterfallHeight(FT8_ROW_HEIGHT * FT8_ROWS);
}

#ifdef SPECTRUM_TESTING
EXTMEM uint8_t freqSpectrum[512*79];

FLASHMEM void DrawTestSpectrum(uint8_t *spec) {
  int yPlot, y1Plot = 0;

  // clear old spectrum
  //EraseSpectrumWindow();
  tft.fillRect(WATERFALL_L, FT8_WINDOW_TOP, WATERFALL_W, FT8_ROW_HEIGHT * FT8_ROWS + 3, RA8875_BLACK);

  for(int i = 0; i < 512; i++) {
    // plot within spectrum area
    //yPlot = SPECTRUM_BOTTOM - spec[i] / 2;
    //y1Plot = SPECTRUM_BOTTOM - spec[i + 1] / 2;
    //tft.drawLine(SPECTRUM_LEFT_X + i, y1Plot, SPECTRUM_LEFT_X + i, yPlot, RA8875_YELLOW);

    // plot within message area
    yPlot = YPIXELS - 10 - (int)((float)spec[i] / 1.5);
    y1Plot = YPIXELS - 10 - (int)((float)spec[i + 1] / 1.5);
    tft.drawLine(WATERFALL_L + i, y1Plot, WATERFALL_L + i, yPlot, RA8875_YELLOW);
  }
}
#endif

FLASHMEM void DisplaySelectedMessageDetail() {
  char message[100];
  RxMsg *msg = &rxBuf[activeMsg];
  tft.setFontScale((enum RA8875tsize)0);
  int rowHeight = tft.getFontHeight();

  if(decodedMsgs > 0) {
    // erase old info
    tft.fillRect(WATERFALL_L, FT8_MSG_WINDOW_DETAIL, WATERFALL_W, rowHeight, RA8875_BLACK);

    // display active message detail
        //printf("%02d%02d%02d %+05.1f %+4.2f %4.0f ~  %s\n",
        //    tm_slot_start->tm_hour, tm_slot_start->tm_min, tm_slot_start->tm_sec,
        //    snr, time_sec, freq, text);
    tft.setTextColor(YELLOW);
    tft.setCursor(WATERFALL_L, FT8_MSG_WINDOW_DETAIL - 3);
    //tft.print("Selected msg detail: ");
    if(msg->time_sec >0) {
      sprintf(message, "%02d%02d%02d %+4d +%d.%1d %4d ~  %s", msg->slot_time.tm_hour, msg->slot_time.tm_min, msg->slot_time.tm_sec,
        (int)msg->snr, (int)msg->time_sec, (int)(msg->time_sec * 10.0 - ((int)msg->time_sec) * 10.0), (int)msg->freq, msg->msg);
    } else {
      sprintf(message, "%02d%02d%02d %+4d -%d.%1d %4d ~  %s", msg->slot_time.tm_hour, msg->slot_time.tm_min, msg->slot_time.tm_sec,
        (int)msg->snr, (int)abs(msg->time_sec), (int)abs((msg->time_sec * 10.0 - ((int)msg->time_sec) * 10.0)), (int)msg->freq, msg->msg);
    }
    tft.print(message);
  }
}

FLASHMEM void DisplayStats(int window, int num, int top, int head, bool scroll) {
  int rowHeight, colWidth, columnOffset;
  int rows = qsoViewActive ? FT8_MSG_ROWS - 2 : FT8_MSG_ROWS; // two fewer message rows are available in QSO view
  bool up = num > rows ? true : false;
  bool down = up;

  tft.setFontScale((enum RA8875tsize)0);
  rowHeight = tft.getFontHeight();
  colWidth = tft.getFontWidth();
  columnOffset = colWidth * 21 * window;

  // reset message area
  tft.fillRect(columnOffset, FT8_MSG_LIST_SUMMARY, colWidth * 21, rowHeight, RA8875_BLACK);

  tft.setTextColor(YELLOW);
  tft.setCursor(WATERFALL_L + columnOffset, FT8_MSG_LIST_SUMMARY);
  switch(window) {
    case ALL_WINDOW:
      tft.print("All(");
      break;

    case CQ_WINDOW:
      tft.print("CQ(");
      break;

    case RX_WINDOW:
      tft.print("RX(");
      break;
  }

  tft.print(num);
  tft.print(")   ");
  if(scroll) {
    tft.setTextColor(RA8875_GREEN);
  } else {
    tft.setTextColor(WHITE);
  }

  if(top == 0) up = false;
  //if(top >= num-FT8_MSG_ROWS) down = false;
  if(top >= head - rows + 1) down = false;
  if(up) tft.write(30); // scroll up pointer
  if(down) tft.write(31); // scroll down pointer
}

// window: 0: all, 1: CQ, 2: RX
FLASHMEM void DisplayMessages(int window, int *list, int numMsgs, bool scroll, int &top, int head, int max) {
  char message[100];
  int rowHeight, colWidth, columnOffset;
  int count = 0; // count of rows displayed
  int i, index;
  int rows = qsoViewActive ? FT8_MSG_ROWS - 2 : FT8_MSG_ROWS; // two fewer message rows are available in QSO view

  tft.setFontScale((enum RA8875tsize)0);
  rowHeight = tft.getFontHeight();
  colWidth = tft.getFontWidth();
  columnOffset = colWidth * 21 * window;

  // reset message area
  tft.fillRect(columnOffset, FT8_MSG_LIST_TOP, colWidth * 21, rowHeight * rows, RA8875_BLACK);

  if(numMsgs > 0) {
    // set msg window top if not scrolling
    if(!scroll) {
      if(numMsgs > rows) {
        top = head - rows + 1;
        if(top < 0) {
          top += max;
        }
      } else {
        top = 0;
      }
    }

    i = top;

    // print recent messages
    while(count < numMsgs) {
      if(count >= rows) break;

      index = list[i];
      if(index == activeMsg) {
        tft.setTextColor(YELLOW);
      } else {
        tft.setTextColor(RA8875_WHITE);
      }

      sprintf(message,"%.20s", rxBuf[index].msg);
      tft.setCursor(WATERFALL_L + columnOffset, FT8_MSG_LIST_TOP + rowHeight * count - 3);
      tft.print(message);

      if(i == head) break; // window rules (1) and (2)

      ++count;
      ++i;
      if(i >= max) i = 0;
    }
  }

  DisplayListStats(window);
}

// TX msg color:
//    White:  waiting
//    Green:  next to TX
//    Red:    no response previous interval
//    Yellow: sent/acknowledged/completed
FLASHMEM int GetTxMsgColor(TxMsg *txMsg) {
  int color = WHITE; // waiting

  switch(txMsg->status) {
    case MSG_NEXT:
      if(ft8TxState) color = RA8875_GREEN;
      break;

    case MSG_SENT:
    case MSG_ACK:
      color = YELLOW;
      break;

    case MSG_TIMEOUT:
      color = RED;
      break;

    case MSG_COMPLETED:
      color = YELLOW;
      break;
  }

  return color;
}

// displays QSO requested by index into qsoList
FLASHMEM void DisplayQSO(int qso = -1) {
  int rowHeight, type, item, index;
  int rx[3], tx[3];

  if(qso >= qsos || !qsoViewActive) return;
  index = qso == -1 ? activeQSO : qso;

  type = qsoList[index].type;
  if(type == 0) {
    // CQ QSO
    rx[0] = 1;
    rx[1] = 3;
    rx[2] = 5;
    tx[0] = 0;
    tx[1] = 2;
    tx[2] = 4;
  } else {
    // CQ reply QSO
    tx[0] = 1;
    tx[1] = 3;
    tx[2] = 5;
    rx[0] = 0;
    rx[1] = 2;
    rx[2] = 4;
  }

  tft.setFontScale((enum RA8875tsize)0);
  rowHeight = tft.getFontHeight();

  // erase old info
  tft.fillRect(WATERFALL_L, FT8_QSO_VIEW_TOP, WATERFALL_W, rowHeight * 2, RA8875_BLACK);

  // print RX side of qso
  tft.setTextColor(WHITE);
  tft.setCursor(WATERFALL_L, FT8_QSO_VIEW_TOP - 3);
  if(type == 0) {
    //tft.print("RX: ");
    tft.print("    "); // indent line
  }

  for(int i = 0; i < 3; i++) {
    item = qsoList[index].msg[rx[i]];

    if(item > -1) {
      RxMsg *rxMsg = &rxBuf[item];
      //tft.setTextColor();
      tft.print(rxMsg->msg);
      tft.print("    ");
    }
  }

  // print TX side of qso
  //tft.setCursor(WATERFALL_L, FT8_TX_QUEUE_TOP - 3);
  tft.setCursor(WATERFALL_L, FT8_QSO_VIEW_TOP + FT8_ROW_HEIGHT - 3);
  if(type == 1) {
    //tft.print("TX: ");
    tft.print("    "); // indent line
  }

  for(int i = 0; i < 3; i++) {
    item = qsoList[index].msg[tx[i]];
    if(item > -1) {
      TxMsg *txMsg = &txBuf[item];
      tft.setTextColor(GetTxMsgColor(txMsg));
      tft.print(txMsg->msg);
      if(txMsg->tries > 1) {
        tft.print(" (");
        tft.print(txMsg->tries);
        tft.print(")");
      }
      tft.print("    ");
    }
  }
}

FLASHMEM int GetRow(int y) {
  // (YPIXELS - FT8_ROW_HEIGHT * FT8_ROWS) / FT8_ROW_HEIGHT = 17
  //int row = ceil((float)(y-5) / 16.0 - 17.0 + 1) - 1; // *** TODO: fine tune this ***
  //Serial.print(y); Serial.print(", "); Serial.println(row);
  //return row;
  return ceil((float)(y-5) / 16.0 - 17.0 + 1) - 1; // *** TODO: fine tune this ***
}

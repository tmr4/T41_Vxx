
#include "..\..\SDT.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

class  InfoBoxItem {
public:
  InfoBoxItem();

  const char *label;      // info box label
  const char **options;   // label options
  int *option;            // pointer to option selector or pointer to the actual value if options is NULL
  int fontSize;           // 0 - small or 1 - large font (large font takes two rows, adjust item rows and/or IB_ROW_#_Y accordingly)
  int clearWidth;         // maximum number of characters to clear when updating field
  int highlightFlag;      // 0 - highlight all options in green, 1 - don't highlight first option, 2 - first option white, second option red, other options green

  // specifying row and col index is easiest but less flexible especailly if you use both small and large fonts
  // as in the default info box
  //int col, row;           // item column and row (up to 10 rows, 2 columns)
  int col, row;           // item placement by screen pixel (up to 10 rows with small font)
  void (*followFnPtr)(int row, int col);  // function to run after info box field is updated (note that these may be hard-coded to a particular location
                          // and will need updated if the underlying item is moved)
};

class InfoBox {
public:
  InfoBox();

  void UpdateInfoBox();
  void UpdateInfoBoxItem(uint8_t item);
  void UpdateIBWPM();
  void UpdateDecodeLockIndicator();

  void DrawInfoBoxFrame();
  void ClearInfoBox();

};

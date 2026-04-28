// v12 specific hardware config file

#define VERSION "v12_dev/v0.02" // up to 14 characters can fit *** can adjust infobox ShowVersion routine to accommodate more if needed ***

#define PROFILER_ACTIVE

#define MASTER_CLK_MULT_RX 2
#define MASTER_CLK_MULT_TX 2

// pick one of the following display configurations
//#define DISPLAY_LANDSCAPE
#define DISPLAY_FLIPPED

#define READ_CENTERTUNE_ENCODER true // set to false if interrupt driven

// uncomment below for USB Host support
#define USB_HOST_SUPPORT

// uncomment below for specific USB Host device support
#define HOST_KEYBOARD_MOUSE_SUPPORT // uses about 44k of stack
#define HOST_SERIAL_SUPPORT

// Remote control
// set one line below to true to enable remote control
#define CAT_CONTROL_HOST  false  // cable from USB host on this unit to serial on remote
#define CAT_CONTROL       false  // cable from serial on this unit to USB host on remote
#define T41_USB_AUDIO     false  // *** for passing audio back and forth with WSJT-X over USB at 44.1kHz sample rate in FT8 mode ***

// Select one of the noted serial objects according to compile options for enabled services.
// For use with PC apps and connecting to other CAT controlled units over USB serial or the USB host connector.
//   Serial:        compiling with a single serial object with multiple services are enabled. Arduino IDE must be closed to connect to PC apps.
//   SerialUSB1:    compiling with Dual or Triple Serial (need to figure which COM port is associated with each)
//   SerialUSB2:    compiling with Triple Serial with two or more services enabled (need to figure which COM port is associated with each)
//   usbHostSerial: for CAT control with another radio over USB host (not for use with PC)
//
// Notes:
// Set disabled services to Serial. Any messages from these services are sent to Arduino serial monitor.
// *** note: debug messages go out over Serial and will be transmitted if controlSerial
// is set to Serial and the unit is connected to the USB host of another unit ***
#define controlSerial usbHostSerial // Serial or SerialUSB1 for USB port or usbHostSerial for USB Host port
#define beaconSerial Serial // Serial or SerialUSB2
#define wsjtSerial Serial // Serial or SerialUSB1 or SerialUSB2

#define I2C_DELAY_LONG 1000L   // How long to show I2C screen with errors
//#define I2C_DELAY_LONG 10000L   // How long to show I2C screen with errors
#define I2C_DELAY_SHORT 1000L   // How long to show I2C screen when no error

#define FRONT_PANEL_POLLING_OPS
#define V12_PANEL_MCP23017_ADDR_1 0x20
#define V12_PANEL_MCP23017_ADDR_2 0x21

// Set the I2C addresses of the LPF, BPF, and RF boards
#define V12_LPF_MCP23017_ADDR 0x25
#define BPF_MCP23017_ADDR 0x24
#define RF_MCP23017_ADDR 0x27

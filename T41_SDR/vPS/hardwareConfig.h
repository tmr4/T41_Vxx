// vPS specific hardware config file for running on Project System

#define VERSION "vPS_dev.1" // Change this for updates. If you make this longer than 9 characters, brace yourself for surprises

#define PROFILER_ACTIVE

#define MASTER_CLK_MULT 4ULL // FOURSQRP QSD frontend requires 4x clock

// pick one of the following display configurations
//#define DISPLAY_LANDSCAPE
#define DISPLAY_FLIPPED

// hardware specific pin assignmens
#define  PROJECTSYSTEM // some pin assignments change per project system design

// pick one of the following front panel configurations
//#define MCP23017_FRONTPANEL // MCP23017 driven front panel
//#define FOURSQRP_FRONTPANEL // resistive switch matrix front panel
//#define PROJECTSYSTEM_EXPANDED_IO
//#define PROJECTSYSTEM_FINETUNE_ENCODER // for testing w/ project system
//#define PROJECTSYSTEM_VOLUME_ENCODER // for testing w/ project system
#define PROJECTSYSTEM_ENCODER_1 // for testing w/ project system

// uncomment below for USB Host support
#define USB_HOST_SUPPORT

// uncomment below for specific USB Host device support
#define HOST_KEYBOARD_MOUSE_SUPPORT // uses about 44k of stack
#define HOST_SERIAL_SUPPORT
//#define HOST_CAT_CONTROL_SUPPORT // enables CAT control over USB host

//#define T41_REMOTE_DISPLAY

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
#define controlSerial Serial // Serial or SerialUSB1 for USB port or usbHostSerial for USB Host port
#define beaconSerial Serial // Serial or SerialUSB2
#define wsjtSerial Serial // Serial or SerialUSB1 or SerialUSB2

// can also use Project System I/O Expanders
#ifdef PROJECTSYSTEM_EXPANDED_IO
// the Project System only has one MCP23017
// can test other half of front panel by selecting address 0x24 (or 0x20) below
// (consider coding for the sn74cbtlv3251 if fully functional front panel on Project System is needed)
#define V12_PANEL_MCP23017_ADDR_1 0x24

// can also solder JP1 for address 0x20
//#define V12_PANEL_MCP23017_ADDR_1 0x20
#endif

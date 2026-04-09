# Other T41 Related Apps

## Ongoing Work

  * T41 desktop companion utilizing the ProtoSupplies.com AudioPlatform.  More to come.

## Standalone T41 Apps

### PC beacon monitor for T41

* PC app version of T41 beacon monitor. See [Beacon Monitor](https://github.com/tmr4/BeaconMonitor).

### PC control app

![PC Control App](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/T41_pcControlApp.png)

Adds communications with PC control app over SerialUSB1 (must select `Dual` or `Triple` USB Type when compiling).  A separate control app running on your PC is required [T41_Views](https://github.com/tmr4/T41_Views).  The control app has the following features:

* Live view of frequency and audio spectrums, S-meter, waterfall, and filter bandwidth
* Live updates can be paused or started with the button at the lower left of the waterfall
* T41 clock set to PC time upon connection
* Change frequency of active VFO by the active increment with the mouse wheel
* Change the active increment with a mouse click (center or fine tune indicated by the green highlight in the info box)
* Zero out the 1000s portion of the active VFO with a right-mouse click
* Reset tuning of the active VFO with a mouse click on the Center Frequency
* Switch to the inactive VFO with a mouse click on the inactive VFO
* Set the noise floor with a mouse click on NF Set and a mouse wheel in the frequency spectrum (this occurs live unlike the base T41 software which stops operation while the noise floor is adjusted)
* Change the following up or down with the mouse wheel (on the corresponding indicator):
  * Band
  * Operating mode
  * Demodulation mode
  * Transmit power
  * Volume
  * AGC
  * Center and fine tune increment

### PC debug windows

Windows console apps designed to facilitate communication between the T41 and multiple PC applications over a single USB serial connection.  Multiple debug windows can be open at the same time.

![T41Server](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/T41Server.png)

![T41Debug](https://github.com/tmr4/T41_Vxx/blob/dev/v0.01/images/t41Server_Debug.png)

 See [T41Server](https://github.com/tmr4/T41Server) and [T41Debug](https://github.com/tmr4/T41Debug).

### Set T41 Clock

To lazy to install a battery to maintain your T41 clock.  Try this PC app [SetT41Clock](https://github.com/tmr4/SetT41Clock).

### Interconnect T41 radios

USB host connection to another T41.  Mainly used in v12 calibration. More to come.

### Remote display

Adds code to enable connection to a remote display over Bluetooth.  See this [Reddit post](https://www.reddit.com/r/T41_EP/comments/1etxkq8/t41_wireless_remote_display/) for a demo and discussion.  The code for the remote head is in this [repository](https://github.com/tmr4/T41_Remote_Head).

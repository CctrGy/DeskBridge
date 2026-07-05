/*
  The PlatformIO TinyUSB 0.17.0 package excludes the Synopsys DWC2 source
  from its library filter. STM32F4 USB OTG FS uses that TinyUSB port.
*/
#include "portable/synopsys/dwc2/dcd_dwc2.c"

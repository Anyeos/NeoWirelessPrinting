# Neo Wireless Printing #


Print wirelessly from [Cura](https://ultimaker.com/en/products/cura-software), [PrusaControl](http://prusacontrol.org/), or [Slic3r PE](https://github.com/prusa3d/Slic3r/releases) to your 3D printer connected to an [ESP8266](https://espressif.com/en/products/hardware/esp8266ex/overview) module.

Or print directly from your web browser.


# Under development

It is still under development so some features will be missing or incomplete and expect some bugs.
Anyway I use it on my own 3D printers so actually it have enought features and is working well enough.


## Features:

* Works as a mini OctoPrint service.
* Serial over WiFi (you can connect from any slicer program as a network serial port).
* Nice and complete responsive web interface (mobile compliant).
* File management to handle gcode files from SDcard.
* Configurable (no need to flash again just to change some settings).
* Asynchronous (can connect from more than one device at time).
* Own WiFi Manager that works fast and better.
* NTP support (real date and time from Internet).
* Basic authentication protection for some actions.


## Hardware

Any ESP8266 module that have enough flash memory (at least 2MB) and enought GPIO to attach an SD card to the SPI pins and, of course let available the RX/TX pins for connect to the 3D printer.
Take care about the 3.3 V requisite of the ESP8266, some 3D printer boards use 5V signals.

For the SD card you can use the diagrams found on this site (it is not related with me, so don't ask there about this project because he may not know about it): [How to use SD card with esp8266 and Arduino](https://www.mischianti.org/2019/12/15/how-to-use-sd-card-with-esp8266-and-arduino/)

To connect to the 3D printer you must know some about electronics. Make your own solution, for example connect the 5V power lines from the 3D printer to a 3.3 V regulator connected to the ESP8266. And the TX/RX lines to the RX/TX serial port of the printer. Remember taking care about 5V signals level from the 3D printer, use the appropiate logic level converter if needed. Or be sure that the printer uses 3.3V signals level directly.

You can use a Wemos D1 that includes the 3.3 V regulator and a appropriate SD card shield. But to save some money I made an microSD card "connector" using the included SD card adapter that comes with the microSD card (the one that adapts microSD size to SD size) soldering directly to the pads of the adapter.

### Building

Use Arduino IDE 1.8.19 or Visual Code with Arduino plugin.
Install [ESP8266 core for Arduino](https://github.com/esp8266/Arduino) version 2.7.4.

The next options will work:
* Board: Generic ESP8266 Module
* CPU Frequency: 160 Mhz
* Flash Size: 2MB (FS:256KB OTA:~896KB)
* Debug port: Disabled
* Debug Level: None
* IwIP Variant: v2 Higher Bandwidth (no features)
* VTables: Flash
* Exceptions: Disabled (new can abort)
* Espressif FW: nonos-sdk 2.2.1+119 (191122)
* SSL Support: Basic SSL ciphers (lower ROM use)

You can test other options and versions but it is tested and working with the ones mentioned above.
Because the ESP8266 is some limited by the self nature of the MCU (specially by RAM), you can observe random reboots if you choose the wrong options. This project put the ESP8266 to the memory limits, so the above options are the only that I know works well.


#### Required libraries:

- [ArduinoLog](https://github.com/thijse/Arduino-Log). It is not actually used but the code is there for debug or logging purposes. In a near future it will log to a file on the SDCard.
- [ESPAsyncWebServer](https://github.com/philbowles/ESPAsyncWebServer) from [philbowles](https://github.com/philbowles).
- [ESPAsyncTCP](https://github.com/philbowles/ESPAsyncTCP) as required by above.
- [ESPDateTime](https://github.com/mcxiaoke/ESPDateTime) to have DateTime support, if not you can uncomment DISABLE_DATETIME define.


### Downloading

Go to [GitHub Releases](https://github.com/Anyeos/NeoWirelessPrinting/releases), download and unzip the firmware on some folder on your computer. Next flash it and complete the Step 1 and Step 2.

To flash the firmware you can use a command like:
```
esptool.py -cf NeoWirelessPrinting.ino.bin
```
To know more about esptool please read the documentation of that project: [esptool.py](https://github.com/espressif/esptool)


### Flashing

First you will need to flash it using the serial interface. If you already did that with another project and have WiFi access to your ESP8266 then you can flash via Arduino OTA (need ~896KB of OTA space).
Next you can update always using Arduino OTA.

**Default username: admin
**Default password: password

## Step 1 - Initial WiFi Configuration
The first time the sketch is uploaded the ESP will enter in Access Point mode, so you have to open the wifi manager of your system and connect to wifi "neowifiprinting_XXXXXX", use default password "password". Then open your browser and type http://192.168.4.1, there you will see the WiFi configuration page, wait some seconds if you want to see a list of available WiFi networks, or type the values manually. 

## Step 2 - Upload the website
This project uses a complete website hosted under the internal FS (LittleFS) of ESP8266. After configuring WiFi when you try to open the resulting IP (if you connected to an AP with DHCP you must figure out what IP get the ESP8266, I use Fing from my Android phone, or you can wait a little and look at your Arduino IDE -> Tools -> Port menu) you will be presented by a page to upload files.
Choose all files under the folder "data/w" of this github repository. All files will be gziped except someones like fonts. Be sure to upload all required files and next press the RESTART button.

Note: If you cloned this repository you will need to generate that files, use the provided script: make-website.sh

After your device restarted it will be ready to be used.


## Wireless printing with Cura

Cura 2.6 and later come with a bundled plugin which discovers OctoPrint instances using Zeroconf and enables printing directly to them. In newer versions of Cura, you need to install the [Cura OctoPrint Plugin](https://github.com/fieldOfView/Cura-OctoPrintPlugin) from the "Toolbox" menu. To use it,
- In Cura, add a Printer matching the 3D printer you have connected to WirelessPrint
- Select "Connect to OctoPrint" on the Manage Printers page
- Select your OctoPrint instance from the list
- Enter an API key (for now a random one is sufficient)
- Click "Connect", then click "Close"
From this point on, the print monitor should be functional and you should see a "Print with OctoPrint" button on the bottom of the sidebar. Use this button to print wirelessly.

## Wireless printing with Slic3r PE

Slic3r PE 1.36.0 and later discovers OctoPrint instances using Zeroconf and enables printing directly to them. No further software needs to be installed. To use it,
- In Slic3r PE, add the corresponding profile for your printer
- Select the "Printer Settings" tab
- Under "OctoPrint upload", enter the IP address of your WirelessPrinting device (in the future, it may be discoverable by Bonjour)
- Click "Test"
From this point on, you should see a "Send to printer" button on the "Plater" tab. Use this button to print wirelessly.

## Wireless printing using a browser or the command line

To print, just open http://the-ip-address/ and upload a G-Code file using the form.

You can also print from the command line using curl:

```
curl -F "file=@/path/to/some.gcode" -F "print=true" http://the-ip-address/print
```

## Take care
When you are printing try to not open the device web from more than two devices (computer, phone, etc) at the same time. This firmware puts the ESP8266 at memory limits, so it can reboots if you reach the memory limit and then ruin your printing.

First do your own checks printing something to know if it will work well with your printer and your way of working.

Anyway it will not offer any security protection / encryption. Anybody can control the printer from your network. It only includes a basic password auth for OTA and some parts. But that only force you to take care about what you are doing, it is not intended to protect aginst intruders. Be sure that your network is free of intruders before using Neo Wireless Printing (ie. use WPA2 protection on all your WiFi network).

I am not responsible for loss of data and/or printings. Use it at your own risk.


### Note

And I want to thanks to the developers of [WirelessPrinting](https://github.com/probonopd/WirelessPrinting) that made possible this project too. Because my first attemp was trying to improve that. And now I completely developed a new project based on parts of code of Wireless Printing. So it was possible thanks to the existence of Wireless Printing.



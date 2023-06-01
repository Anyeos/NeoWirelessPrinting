/* 
 * This file is part of Neo Wireless Printing (https://github.com/Anyeos/NeoWirelessPrinting).
 * Copyright (c) 2023 Andrés G. Schwartz (Anyeos).
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef ESP32
  // LittleFS has higher priority than SPIFFS
  #if ( defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 2) )
    #define USE_LITTLEFS    true
    #define USE_SPIFFS      false
  #elif defined(ARDUINO_ESP32C3_DEV)
    // For core v1.0.6-, ESP32-C3 only supporting SPIFFS and EEPROM. To use v2.0.0+ for LittleFS
    #define USE_LITTLEFS          false
    #define USE_SPIFFS            true
  #endif

  #if USE_LITTLEFS
    // Use LittleFS
    #include "FS.h"

    // Check cores/esp32/esp_arduino_version.h and cores/esp32/core_version.h
    //#if ( ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0) )  //(ESP_ARDUINO_VERSION_MAJOR >= 2)
    #if ( defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 2) )
      #if (_ESPASYNC_WIFIMGR_LOGLEVEL_ > 3)
        #warning Using ESP32 Core 1.0.6 or 2.0.0+
      #endif
      
      // The library has been merged into esp32 core from release 1.0.6
      #include <LittleFS.h>       // https://github.com/espressif/arduino-esp32/tree/master/libraries/LittleFS
      
      FS* filesystem =      &LittleFS;
      #define FileFS        LittleFS
      #define FS_Name       "LittleFS"
    #else
      #if (_ESPASYNC_WIFIMGR_LOGLEVEL_ > 3)
        #warning Using ESP32 Core 1.0.5-. You must install LITTLEFS library
      #endif
   
      // The library has been merged into esp32 core from release 1.0.6
      #include <LITTLEFS.h>       // https://github.com/lorol/LITTLEFS
      
      FS* filesystem =      &LITTLEFS;
      #define FileFS        LITTLEFS
      #define FS_Name       "LittleFS"
    #endif
    
  #elif USE_SPIFFS
    #include <SPIFFS.h>
    FS* filesystem =      &SPIFFS;
    #define FileFS        SPIFFS
    #define FS_Name       "SPIFFS"
  #else
    // Use FFat
    #include <FFat.h>
    FS* filesystem =      &FFat;
    #define FileFS        FFat
    #define FS_Name       "FFat"
  #endif
  //////

#else
  #define USE_LITTLEFS      true
  
  #if USE_LITTLEFS
    #include <LittleFS.h>
    FS* filesystem = &LittleFS;
    #define FileFS    LittleFS
    #define FS_Name   "LittleFS"
  #else
    FS* filesystem = &SPIFFS;
    #define FileFS    SPIFFS
    #define FS_Name   "SPIFFS"
  #endif
#endif

static size_t FileFSfilesize(String path)
{
  FS:File file = FileFS.open(path, "r");
  size_t file_size = 0;
  if (file) {
    file_size = file.size();
    file.close();
  }
  return file_size;
}

static String FileFShumanFileSize( size_t size )
{
    if (size > 1000000) {
      return String(size / 1000000.0)+" MB";
    }
    if (size > 1000) {
      return String(size / 1000.0)+" kB";
    }

    return String(size)+" bytes";
}


bool FileFSformat() 
{
  if (!FileFS.format()) {
    Log.error("Format filesystem failed!" CR);
    return false;
  } else {
    Log.info("Filesystem formatted" CR);
  }
  return true;
}

void FileFS_begin() 
{
  Log.info("Filesystem size %d" CR, FS_PHYS_SIZE);
  
// Initialize LittleFS/SPIFFS file-system
#if (ESP32)
  // Format SPIFFS if not yet
  if (!FileFS.begin(true))
  {
    Log.error("SPIFFS/LittleFS mount failed." CR);
#else
  if (!FileFS.begin())
  {
    FileFSformat();
#endif
    
    if (!FileFS.begin())
    {
      while (true)
      {
#if USE_LITTLEFS
        Log.error("LittleFS mount failed!" CR);
#else
        Log.error("SPIFFS mount failed!" CR);
#endif
        // Stay forever here as useless to go further
        delay(5000);
        if (FileFS.begin()) { break; }
      }
    }
  }
};

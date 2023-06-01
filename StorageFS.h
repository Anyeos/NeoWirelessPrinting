/* 
 * This file is part of Neo Wireless Printing (https://github.com/Anyeos/NeoWirelessPrinting).
 * Copyright (c) 2023 Andrés G. Schwartz (Anyeos).
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "FileWrapper.h"

#if defined(ESP8266)
  extern sdfat::SdFat SD;
#endif

class StorageFS {
  private:
    static bool hasSD;
    static unsigned int maxPathLength;

  public:
    inline static void begin(const bool fastSD) {
      #if defined(ESP8266)
        hasSD = SD.begin(SS, fastSD ? SD_SCK_MHZ(50) : SPI_HALF_SPEED); // https://github.com/esp8266/Arduino/issues/1853
      #elif defined(ESP32)
        SPI.begin(14, 2, 15, 13); // TTGO-T1 V1.3 internal microSD slot
        hasSD = SD.begin(SS, SPI, fastSD ? 50000000 : 4000000);
      #endif
      if (hasSD)
        maxPathLength = 255;
      /*else {
        #if defined(ESP8266)
          hasSPIFFS = SPIFFS.begin();
          if (hasSPIFFS) {
            fs::FSInfo fs_info;
            maxPathLength = SPIFFS.info(fs_info) ? fs_info.maxPathLength - 1 : 11;
          }
        #elif defined(ESP32)
          hasSPIFFS = SPIFFS.begin(true);
          maxPathLength = 11;
        #endif
      }*/
    }

    inline static bool activeSD() {
      return hasSD;
    }

    inline static bool activeSPIFFS() {
      return false;
    }

    inline static bool isActive() {
      return activeSD() || activeSPIFFS();
    }

    inline static String getActiveFS() {
      return activeSD() ? "SD" : (activeSPIFFS() ? "SPIFFS" : "NO FS");
    }

    inline static unsigned int getMaxPathLength() {
      return maxPathLength;
    }

    inline operator bool() {
      return hasSD;
    }

    static FileWrapper open(const String path, const char *openMode = "r");
    static bool remove(const String filename);
    static bool rename(const String filename, const String newfilename);
};

extern StorageFS storageFS;

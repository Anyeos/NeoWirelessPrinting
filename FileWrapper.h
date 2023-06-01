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

//#define FS_NO_GLOBALS // allow spiffs to coexist with SD card, define BEFORE including FS.h
#include <FS.h>
#if defined(ESP8266)
  #include <SdFat.h>
#elif defined(ESP32)
  #include <SPIFFS.h>
  #define FORMAT_SPIFFS_IF_FAILED true
  #include <SD.h>
#endif

class FileWrapper : public Stream {
  friend class StorageFS;

  private:
    sdfat::File sdFile;
    String Name;
  public:
    // Print methods
    virtual size_t write(uint8_t datum);
    virtual size_t write(const uint8_t *buf, size_t size);

    // Stream methods
    virtual void flush();
    virtual int available();
    virtual bool seek(uint32_t pos);
    virtual int peek();
    virtual int read();

    inline operator bool() {
      return sdFile;
    }

    String name();
    uint32_t size();
    time_t getCreationTime();
    int read(uint8_t *buf, size_t size);
    String readStringUntil(char eol);
    String readStringUntilToBack(char eol, uint32_t pos);
    bool close();

    inline bool isDirectory() {
      return sdFile.isDir();
    }

    FileWrapper openNextFile();
    bool isGcode();
};

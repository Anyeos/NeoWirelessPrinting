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

#include "StorageFS.h"
#include "ESPDateTime.h"

sdfat::SdFat SD;
StorageFS storageFS;

bool StorageFS::hasSD;
unsigned int StorageFS::maxPathLength;

FileWrapper StorageFS::open(const String path, const char *openMode) {
  FileWrapper file;

  /*if (openMode == NULL || openMode[0] == '\0')
    return file;*/

  if (hasSD) {
    #if defined(ESP8266)
      // NOTE: Needs improvement
      file.sdFile.open(path.c_str(), openMode[0] == 'w' 
                                                   ? (sdfat::O_WRITE | sdfat::O_CREAT | sdfat::O_TRUNC) :
                                openMode[0] == 'a' ? (sdfat::O_WRITE | sdfat::O_CREAT | sdfat::O_APPEND | sdfat::O_SYNC) : 
                                                      sdfat::FILE_READ);
      file.sdFile.dateTimeCallback([](uint16_t* date, uint16_t* time) {
        uint16_t year = 2023;
        uint8_t month = 1, day = 1, hour = 0, minute = 0, second = 0;
      #if !defined(DISABLE_DATETIME)
        time_t now = DateTime.getTime();
        tm *ltime = localtime(&now);
        year = ltime->tm_year + 1900;
        month = ltime->tm_mon + 1;
        day = ltime->tm_mday;
        hour = ltime->tm_hour;
        minute = ltime->tm_min;
        second = ltime->tm_sec;
      #endif
        *date = sdfat::FAT_DATE(year, month, day);
        *time = sdfat::FAT_TIME(hour, minute, second);
      });
    
      if (file.sdFile && file.sdFile.isDir())
        file.sdFile.rewind();
    #elif defined(ESP32)
      file.sdFile = SD.open(path, openMode);
    #endif
  }
  /*else if (hasSPIFFS) {
    #if defined(ESP8266)
      if (path.endsWith("/")) {
        file.fsDir = SPIFFS.openDir(path);
        file.fsDirType = FileWrapper::DirSource;
      }
      else
        file.fsFile = SPIFFS.open(path, openMode);
    #elif defined(ESP32)
      file.fsFile = SPIFFS.open(path, openMode);
    #endif
  }*/

  return file;
}

bool StorageFS::remove(const String filename) {
  if (hasSD)
    return SD.remove(filename.c_str());
  /*else if (hasSPIFFS)
    SPIFFS.remove(filename);*/
  return false;
}


bool StorageFS::rename(const String filename, const String newfilename) {
  if (hasSD)
    return SD.rename(filename.c_str(), newfilename.c_str());
  /*else if (hasSPIFFS)
    SPIFFS.rename(filename, newfilename);*/
  return false;
}

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
#include "FileWrapper.h"

size_t FileWrapper::write(uint8_t b) {
  uint8_t buf[] = { b };
  return write(buf, 1);
}

size_t FileWrapper::write(const uint8_t *buf, size_t len) {
  if (sdFile)
    return sdFile.write(buf, len);
  return -1;
}

void FileWrapper::flush() {
  if (sdFile)
    sdFile.sync();
}

int FileWrapper::available() {
  if (sdFile) {
    return sdFile.available();
  }
  return 0;
}

bool FileWrapper::seek(uint32_t pos) {
  if (sdFile)
    return sdFile.seekSet(pos);
  return false;
}

int FileWrapper::peek() {
  if (sdFile)
    return sdFile.peek();
  return -1;
}

int FileWrapper::read() {
  if (sdFile)
    return sdFile.read();
  return -1;
}

String FileWrapper::name() {
  if (Name.length()<=0) {
    if (sdFile) {
      char str[64];
      sdFile.getName(str, 64);
      Name = String(str);
    }
  }
  return Name;
}

uint32_t FileWrapper::size() {
  if (sdFile)
    return sdFile.fileSize();
  return 0;
}

time_t FileWrapper::getCreationTime() {
  time_t creation_time;
  tm time;
  if (sdFile) {
    sdfat::dir_t dir;
    if (!sdFile.dirEntry(&dir)) {
      return 0;
    }
    time.tm_year = sdfat::FAT_YEAR(dir.creationDate) - 1900;
    time.tm_mon = sdfat::FAT_MONTH(dir.creationDate) - 1;
    time.tm_mday = sdfat::FAT_DAY(dir.creationDate);
    time.tm_hour = sdfat::FAT_HOUR(dir.creationTime);
    time.tm_min = sdfat::FAT_MINUTE(dir.creationTime);
    time.tm_sec = sdfat::FAT_SECOND(dir.creationTime);
    //strptime(output.getString().c_str(), "%Y-%m-%d %H:%M:%S", &time);
    creation_time = mktime(&time);
    return creation_time;
  }

  return 0;
}

int FileWrapper::read(uint8_t *buf, size_t size) {
  if (sdFile)
    return sdFile.read(buf, size);

  return 0;
}

String FileWrapper::readStringUntil(char eol) {
  if (sdFile) {
    String ret = "";
    int c = sdFile.read();
    while(c >= 0 && (char)c != eol) {
        ret += (char) c;
        c = sdFile.read();
    }
    return ret;
  }

  return String("");
}

String FileWrapper::readStringUntilToBack(char eol, uint32_t pos) {
  if (sdFile) {
    String ret = "";
    char c = 0;
    if (seek(pos)) {
      sdFile.read(&c, 1);
      while(c != eol) {
          if (!seek(pos-1)) {
            break;
          }
          pos--;
          sdFile.read(&c, 1);
      }
    }
    if (seek(pos+1)) {
      int b = sdFile.read();
      while(b >= 0 && (char)b != eol) {
        ret += (char) b;
        b = sdFile.read();
      }
      return ret;
    }
  }

  return String("");
}

bool FileWrapper::close() {
  bool ret = false;
  if (sdFile) {
    ret = sdFile.close();
    sdFile = sdfat::File();
  }

  return ret;
}

FileWrapper FileWrapper::openNextFile() {
  FileWrapper fw = FileWrapper();

  if (sdFile) {
    fw.sdFile.openNext(&sdFile);
  }
  return fw;
}

bool FileWrapper::isGcode() {
  return name().indexOf(".gcode") != -1;
}


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

#ifndef _HOMETHING_MANAGER_H_
#define _HOMETHING_MANAGER_H_

#ifndef DEFAULT_ADMIN_USER
  #define DEFAULT_ADMIN_USER "admin"
#endif
#ifndef DEFAULT_ADMIN_PASSWORD
  #define DEFAULT_ADMIN_PASSWORD "password"
#endif
#ifndef SETTINGS_FILENAME
  #define SETTINGS_FILENAME "/settings.dat"
#endif

#ifndef DEFAULT_DATETIME_TIMEZONE
  #define DEFAULT_DATETIME_TIMEZONE "<-03>3"
#endif
#ifndef DEFAULT_NTP_SERVERS
  #define DEFAULT_NTP_SERVERS {"0.south-america.pool.ntp.org", "0.north-america.pool.ntp.org", "0.europe.pool.ntp.org"}
#endif
#ifndef NTP_WAIT_MS
  #define NTP_WAIT_MS 15000
#endif

#if !( defined(ESP8266) || defined(ESP32) )
  #error This code is intended to run on the ESP8266 or ESP32 platform! Please check your Tools->Board setting.
#endif

#ifdef ESP32
#include <esp_wifi.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>
//#include <WiFiMulti.h>
//WiFiMulti wifiMulti;
uint32_t ESP_getChipId() {
  uint32_t chipId = 0;
  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
	}
  return chipId;
}

#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  //#include <ESPAsyncDNSServer.h>
  #include <ESP8266mDNS.h>
  #include <ESPAsyncTCP.h> // https://github.com/philbowles/ESPAsyncTCP
  #define ESP_getChipId() (ESP.getChipId())
  #define WIFI_AUTH_OPEN  ENC_TYPE_NONE
#endif

#ifndef HTTP_PORT
  #if !ASYNC_TCP_SSL_ENABLED
    #define HTTP_PORT 80
  #else
    #define HTTP_PORT 443
  #endif
#endif

#include <ArduinoLog.h>

#include "htmlstrings.h"
#include "FileSystem.hpp"

class HomeThingManager
{
  public:
    bool restart_device = false;

    typedef struct {
        uint8_t count;
        const char **names;
    } FilesList;

    HomeThingManager(const String &name, const String &hostname, FilesList files);
    void startLogging(int level = LOG_LEVEL_VERBOSE, Print *output = &Serial, bool showLevel = true);
    bool isWebSiteReady();
    bool isFileInList(const char *filename);
    void begin();
    void handle();
    bool loadsettings(const String& filename = SETTINGS_FILENAME);
    bool savesettings();
    void factory_reset();
    String getAdmin() { return admin_user; }
    void setAdmin(const String &username ) { admin_user = username; }
    String getAdminPass() { return admin_password; }
    void setAdminPass(const String &password ) { admin_password = password; }
    String getHostName() { return hostName; }
    void setHostName(const String &hostname) { 
      hostName = hostname;
      hostName.replace(" ", "");
    }
    void setHostNameUnique(const String &hostname);
  private:
    String Name;
    String hostName;

    void OTA_begin();
#ifndef DISABLE_DATETIME
    String timezone = DEFAULT_DATETIME_TIMEZONE;
    String NTPservers[3] = DEFAULT_NTP_SERVERS;
    void DateTime_begin();
    void DateTime_handle();
#endif

#ifndef DISABLE_WEBOFTHINGS
    void WoT_handle();
#endif

    FilesList _files;      
    String settings_filename = SETTINGS_FILENAME;

    String admin_user = DEFAULT_ADMIN_USER;
    String admin_password = DEFAULT_ADMIN_PASSWORD;

    typedef struct __attribute__ ((__packed__)) {
        char hostname[31];
        char ntpservers[3][31];
        char admin[31];
        char admin_pwd[63];
        uint16_t checksum; // checksum always at the end
    } _settings;

    void web_format();
    void web_restart();
    void web_step2();

    String file_extension_gz(const String &filename);
    String file_basename_gz(const String &filename);
    String file_basefile_gz(const String &filename);

    void website();
};

#ifndef DISABLE_DATETIME
  #include "DateTime.hpp"
#endif

#include "WebService.hpp"

#ifndef DISABLE_WEBOFTHINGS
  #include "WoT.hpp"
#endif

#include "ThingWiFi.hpp"
#include "OTA.hpp"
#include "HomeThingManager.hpp"

#endif
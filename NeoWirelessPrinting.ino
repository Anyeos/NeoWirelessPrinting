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

#include <Arduino.h>

//#define DISABLE_DATETIME
#define DISABLE_LOGGING
#define DISABLE_WEBOFTHINGS
//#define HTTP_PORT 80
#define DISABLE_TELNET

#define BRAND_NAME "Neo Wireless Printing"
const char *hostName = "neowifiprinting";

#define NUMBER_OF_FILES 12
const char *files[NUMBER_OF_FILES] = {
  "index.html.gz",
  "setup.html.gz",
  "wifi.html.gz",
  "sistema.html.gz",
  "grids-responsive-min.css.gz",
  "index.css.gz",
  "pure-min.css.gz",
  "styles.css.gz",
  "typicons.css.gz",
  "typicons.woff2",
  "ui.js.gz",
  "light-search.js.gz",
};

// Global vars
uint32_t ms = 0; // Milliseconds saved on this var
bool ota_uploading = false;

#include "src/HomeThingManager.h"
HomeThingManager::FilesList Files = {
  .count = NUMBER_OF_FILES,
  .names = files,
};
HomeThingManager ThingManager(BRAND_NAME, hostName, Files);

#include "Printer.hpp"

#ifndef DISABLE_DATETIME
uint16_t StringTimeToMinutes(String stime)
{
  String hours = "";
  String minutes = "";
  bool is_hours = true;
  for (uint8_t i=0; i<stime.length(); i++) {
    if (stime[i] == ':'){
      is_hours = false;
    } else {
      if (is_hours)
        hours += stime[i];
      else
        minutes += stime[i];
    }
  }
  return hours.toInt()*60+minutes.toInt();
}

String MinutesToStringTime(uint16_t minutes)
{
  float fhours = minutes / 60.0;
  uint16_t hours = (uint16_t) fhours;
  uint16_t mins = (uint16_t)((fhours - hours) * 60.0);
  char str[6];
  sprintf(str, "%02d:%02d", hours, mins);
  return String(str);
}
#endif


void setup() {
  #if defined(LED_BUILTIN)
    pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  #endif
  PrinterSetup();

  webServer.on("/deleteindex", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if(WiFiService.isConfigured() && !request->authenticate(ThingManager.getAdmin().c_str(), ThingManager.getAdminPass().c_str()))
      return request->requestAuthentication();

    if (request->hasParam("prompt")) {
      AsyncWebParameter *p = request->getParam("prompt");
      if (p->value() == "BORRAR SITIO") {
        FileFS.remove(String("/w/")+String(files[0]));
        request->send(200, "text/plain", "Index borrado. Espera y recarga para ir al Step 2.");
        ThingManager.restart_device = true;
        return;
      }
    }
    request->redirect("/");
  });

  webServer.on("/restart", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if(WiFiService.isConfigured() && !request->authenticate(ThingManager.getAdmin().c_str(), ThingManager.getAdminPass().c_str()))
      return request->requestAuthentication();
    ThingManager.restart_device = true;
    request->redirect("/");
  });

  webServer.on("/factoryreset", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if(WiFiService.isConfigured() && !request->authenticate(ThingManager.getAdmin().c_str(), ThingManager.getAdminPass().c_str()))
      return request->requestAuthentication();
    request->redirect("/");
    ThingManager.factory_reset();
    return;
  });

  webServer.on("/config", HTTP_GET | HTTP_POST, [&](AsyncWebServerRequest *request) {
    if(WiFiService.isConfigured() && !request->authenticate(ThingManager.getAdmin().c_str(), ThingManager.getAdminPass().c_str()))
      return request->requestAuthentication();

    if (request->method() == HTTP_GET) {
      AsyncResponseStream *response = request->beginResponseStream("application/json");
      DynamicJsonDocument doc(1024);
      
      doc["user"] = ThingManager.getAdmin();
      doc["hostname"] = ThingManager.getHostName();
      doc["version"] = VERSION;

      serializeJson(doc, *response);
      request->send(response);
      return;
    }

    request->redirect("sistema.html");

    const char* field_hostname = "hostname";
    const char* field_user = "user";
    const char* field_pass = "pass";

    int params = request->params();
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
        if (p->name() == (field_hostname)) {
          if (p->value().length() > 0)
            ThingManager.setHostName(p->value());
          else
            ThingManager.setHostNameUnique(hostName);
        }
        if (p->name() == (field_user)) {
          ThingManager.setAdmin(p->value());
        }
        if (p->name() == (field_pass)) {
          if (p->value().length() > 0)
            ThingManager.setAdminPass(p->value());
        }
    }
    ThingManager.savesettings();
    ThingManager.restart_device = true;
  });

  ArduinoOTA.onStart([]() {
    ota_uploading = true;
    webServer.end();
  });

  ArduinoOTA.onEnd([]() {
    ota_uploading = false;
  });

  ArduinoOTA.onError([](ota_error_t error) {
    ota_uploading = false;
  });

  ThingManager.begin();

  // NOTE: There are errors somewhere because the service just claim "race-condition"
  // OctoPrint API
  // Unfortunately, Slic3r doesn't seem to recognize it
  MDNS.addService("octoprint", "tcp", 80);
  MDNS.addServiceTxt("octoprint", "tcp", "path", "/");
  MDNS.addServiceTxt("octoprint", "tcp", "api", API_VERSION);
  MDNS.addServiceTxt("octoprint", "tcp", "version", OCTOPRINT_VERSION);

  MDNS.addServiceTxt("http", "tcp", "path", "/");
  MDNS.addServiceTxt("http", "tcp", "api", API_VERSION);
  MDNS.addServiceTxt("http", "tcp", "version", OCTOPRINT_VERSION);
};

void loop() {
  ms = millis();
  ThingManager.handle();
  //yield();
  if (!ota_uploading) 
    PrinterHandle();
};

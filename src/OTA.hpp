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

#include <ArduinoOTA.h>
//AsyncEventSource ota("/ota");

void HomeThingManager::OTA_begin() {
  //Send OTA events to the browser
  ArduinoOTA.onStart([]() { 
    //ota.send("Update Start", "ota");
    });
  ArduinoOTA.onEnd([]() { 
    //ota.send("Update End", "ota");
    });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    /*char p[32];
    sprintf(p, "Progress: %u%%\n", (progress/(total/100)));
    ota.send(p, "ota");*/
  });
  ArduinoOTA.onError([](ota_error_t error) {
    /*if(error == OTA_AUTH_ERROR) ota.send("Auth Failed", "ota");
    else if(error == OTA_BEGIN_ERROR) ota.send("Begin Failed", "ota");
    else if(error == OTA_CONNECT_ERROR) ota.send("Connect Failed", "ota");
    else if(error == OTA_RECEIVE_ERROR) ota.send("Recieve Failed", "ota");
    else if(error == OTA_END_ERROR) ota.send("End Failed", "ota");*/
  });
  ArduinoOTA.setHostname(hostName.c_str());
  ArduinoOTA.setPassword(admin_password.c_str());
  ArduinoOTA.begin();

  //MDNS.addService("http","tcp",80);

  /*ota.onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
  });*/
};

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

#include "WebThingAdapter.h" // https://github.com/WebThingsIO/webthing-arduino
#include "Thing.h"

#ifndef WEBOFTHINGS_PORT
  #define WEBOFTHINGS_PORT 8080
#endif

WebThingAdapter *adapter = nullptr;
void WoT_init();
void WoT_update();

void HomeThingManager::WoT_handle()
{
  if (adapter != nullptr) {
    WoT_update();
    adapter->update();
    return;
  }
  if (WiFi.isConnected())
  {
    adapter = new WebThingAdapter(hostName, WiFi.localIP(), WEBOFTHINGS_PORT, true);
    WoT_init();
    adapter->begin();
  } else {
    delete adapter;
    adapter = nullptr;
  }
}

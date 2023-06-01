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

#include "ESPDateTime.h" // https://github.com/mcxiaoke/ESPDateTime
#include "TZ.h"

void HomeThingManager::DateTime_begin()
{
   DateTime.setServer(NTPservers[0].c_str(), NTPservers[1].c_str(), NTPservers[2].c_str());
   DateTime.setTimeZone(timezone.c_str());
}

uint32_t last_datetime_ms = 0;
void HomeThingManager::DateTime_handle()
{
   uint32_t ms = millis();
   if (ms - last_datetime_ms < NTP_WAIT_MS)
      return;

   if (!DateTime.isTimeValid()) {
      last_datetime_ms = ms;
      DateTime.begin();
      if (!DateTime.isTimeValid()) {
         Log.error("Failed to get time from server, will retry in %d seconds" CR, NTP_WAIT_MS / 1000);
         return;
      }
      Log.info("Date is %s" CR, DateTime.toISOString().c_str());
   }
}
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

#ifndef DISABLE_TELNET
#include "CommandQueue.h"

WiFiServer telnetServer(23);
WiFiClient telnetClient;

void TelnetSetup() {
   telnetServer.begin();
   telnetServer.setNoDelay(true);
}

String telnetCommand;
void TelnetHandle() {
  if (telnetServer.hasClient() && (!telnetClient || !telnetClient.connected())) {
    if (telnetClient) {
      telnetClient.stop();
      while (telnetClient.available())
         telnetClient.read();
    }
      

    telnetClient = telnetServer.available();
    telnetClient.flush();  // clear input buffer, else you get strange characters
  }

  while (telnetClient && telnetClient.available() > 0) {  // get data from Client
    char ch = telnetClient.read();
    if (ch == '\r' || ch == '\n') {
      if (telnetCommand.length() > 0) {
        commandQueue.push(telnetCommand);
        telnetCommand = "";
      }
    }
    else
    {
      //telnetClient.println((int) ch);
      if (telnetCommand.length() <= 0 && (int) ch == 3) {
        telnetClient.stop();
        return;
      }
      telnetCommand += String(ch);
    }
  }
}
#endif


inline void telnetSend(const String line) {
#ifndef DISABLE_TELNET
  if (telnetClient && telnetClient.connected())     // send data to telnet client if connected
   telnetClient.println(line);
#endif
}

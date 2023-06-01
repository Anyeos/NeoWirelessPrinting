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

#define COMMAND_BUFFER_SIZE   10

#include <Arduino.h>

class CommandQueue {
  private:
    static int head, sendTail, tail;
    static String commandBuffer[COMMAND_BUFFER_SIZE];

    // Returns the next buffer slot (after index slot) if it's in between the size of the buffer
    static inline int nextBufferSlot(int index) {
      int next = index + 1;
  
      return next >= COMMAND_BUFFER_SIZE ? 0 : next;
    }

  public:
    // Check if buffer is empty
    static inline bool isEmpty() {
      return head == tail;
    }

    // Returns true if the command to be sent was the last sent (so there is no pending response)
    static inline bool isAckEmpty() {
      return tail == sendTail;
    }

    static int getFreeSlots();

    static inline void clear() {
      head = sendTail = tail;
    }

    static bool push(const String command);

    // If there is a command pending to be sent returns it
    inline static String peekSend() {
      return (sendTail == head) ? String() : commandBuffer[sendTail];
    }
    
    static String popSend();
    static String popAcknowledge();
};

extern CommandQueue commandQueue;

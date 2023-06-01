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

#include "CommandQueue.h"

CommandQueue commandQueue;    //FIFO Queue

int CommandQueue::head = 0;
int CommandQueue::sendTail = 0;
int CommandQueue::tail = 0;
String CommandQueue::commandBuffer[COMMAND_BUFFER_SIZE];

int CommandQueue::getFreeSlots() {
  int freeSlots = COMMAND_BUFFER_SIZE - 1;

  int next = tail;
  while (next != head) {
    --freeSlots;
    next = nextBufferSlot(next);
  }

  return freeSlots;
}

// Tries to Add a command to the queue, returns true if possible
bool CommandQueue::push(const String command) {
  int next = nextBufferSlot(head);
  if (next == tail || command == "")
    return false;

  commandBuffer[head] = command;
  head = next;

  return true;
}

// Returns the next command to be sent, and advances to the next
String CommandQueue::popSend() {
  if (sendTail == head)
    return String();

  const String command = commandBuffer[sendTail];
  sendTail = nextBufferSlot(sendTail);
  
  return command;
}

// Returns the last command sent if it was received by the printer, otherwise returns empty
String CommandQueue::popAcknowledge() {
  if (isAckEmpty())
    return String();

  const String command = commandBuffer[tail];
  tail = nextBufferSlot(tail);

  return command;
}

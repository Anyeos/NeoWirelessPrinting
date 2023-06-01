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
#include "CommandQueue.h"

// On ESP8266 use the normal Serial() for now, but name it PrinterSerial for compatibility with ESP32
// On ESP32, use Serial1 (rather than the normal Serial0 which prints stuff during boot that confuses the printer)
#ifdef ESP8266
#define PrinterSerial Serial
#include "WiFiServer.h"
#include "WiFiClient.h"
#endif
#ifdef ESP32
HardwareSerial PrinterSerial(1);
#endif

WiFiServer telnetServer(23);
WiFiClient telnetClient;

// Configurable parameters
#define OCTOPRINT_VERSION "1.0"
#define USE_FAST_SD                     // Use Default fast SD clock, comment if your SD is an old or slow one.

#define MAX_SUPPORTED_EXTRUDERS 6       // Number of supported extruder
#define REPEAT_M115_TIMES 4             // M115 retries with same baud (MAX 255)

#define PRINTER_RX_BUFFER_SIZE 0        // This is printer firmware 'RX_BUFFER_SIZE'. If such parameter is unknown please use 0
#define TEMPERATURE_REPORT_INTERVAL 2   // Ask the printer for its temperatures status every 2 seconds
#define KEEPALIVE_INTERVAL 2500         // Marlin defaults to 2 seconds, get a little of margin
const uint32_t serialBauds[] = { 115200, 57600, 250000, 500000, 921600 };
//const uint32_t serialBauds[] = { 115200 };

#define API_VERSION     "0.1"
#define VERSION         "0.7.0"

#define MAX_FILES_PER_LIST  10 // Maximum number of files sent by the file listing json

// Information from M115
String fwMachineType = "Unknown";
uint8_t fwExtruders = 1;
bool fwAutoreportTempCap = false, fwProgressCap = false, fwBuildPercentCap = false;

// Printer status
bool printerConnected = false,
     startPrint = false,
     isPrinting = false,
     printPause = false,
     restartPrint = false,
     cancelPrint = false,
     autoreportTempEnabled = false;

uint32_t printStartTime = 0;
uint32_t printTime = 0;
float printCompletion = 0.0;

// Serial communication
String lastCommandSent = "", lastReceivedResponse = "";
uint32_t lastPrintedLine = 0;

uint8_t serialBaudIndex = 0;
uint16_t printerUsedBuffer = 0;
uint32_t serialReceiveTimeoutTimer = 0;

// Uploaded file information
String uploadedFullname = "";
size_t uploadedFileSize = 0, filePos = 0;
time_t uploadedFileCreationTime = 0;

// Temperature for printer status reporting
#define TEMP_COMMAND      "M105"
#define AUTOTEMP_COMMAND  "M155 S"

struct Temperature {
  int actual, target;
  //String actual, target;
};

uint32_t temperatureTimer;

Temperature toolTemperature[MAX_SUPPORTED_EXTRUDERS];
Temperature bedTemperature;


inline void setLed(const bool status) {
  #if defined(LED_BUILTIN)
    digitalWrite(LED_BUILTIN, status ? LOW : HIGH);   // Note: LOW turn the LED on
  #endif
}

inline void telnetSend(const String line) {
  if (telnetClient && telnetClient.connected())     // send data to telnet client if connected
    telnetClient.println(line);
}

bool isFloat(const String value) {
  for (uint i = 0; i < value.length(); ++i) {
    char ch = value[i];
    if (ch != ' ' && ch != '.' && ch != '-' && !isDigit(ch))
      return false;
  }

  return true;
}

// Parse temperatures from printer responses like
// ok T:32.8 /0.0 B:31.8 /0.0 T0:32.8 /0.0 @:0 B@:0
bool parseTemp(const String response, const String whichTemp, Temperature *temperature) {
  int tpos = response.indexOf(whichTemp + ":");
  if (tpos != -1) { // This response contains a temperature
    int slashpos = response.indexOf(" /", tpos);
    int spacepos = response.indexOf(" ", slashpos + 1);
    // if match mask T:xxx.xx /xxx.xx
    if (slashpos != -1 && spacepos != -1) {
      String actual = response.substring(tpos + whichTemp.length() + 1, slashpos);
      String target = response.substring(slashpos + 2, spacepos);
      if (isFloat(actual) && isFloat(target)) {
        temperature->actual = actual.toFloat()*100;
        temperature->target = target.toFloat()*100;
        return true;
      }
    }
  }

  return false;
}

// Parse temperatures from prusa firmare (sent when heating)
// ok T:32.8 E:0 B:31.8
bool parsePrusaHeatingTemp(const String response, const String whichTemp, Temperature *temperature) {
  int tpos = response.indexOf(whichTemp + ":");
  if (tpos != -1) { // This response contains a temperature
    int spacepos = response.indexOf(" ", tpos);
    if (spacepos == -1)
      spacepos = response.length();
    String actual = response.substring(tpos + whichTemp.length() + 1, spacepos);
    if (isFloat(actual)) {
      temperature->actual = actual.toFloat()*100;

      return true;
    }
  }

  return false;
}

int8_t parsePrusaHeatingExtruder(const String response) {
  Temperature tmpTemperature;

  return parsePrusaHeatingTemp(response, "E", &tmpTemperature) ? tmpTemperature.actual/100 : -1;
}

bool parseTemperatures(const String response) {
  bool tempResponse;

  if (fwExtruders == 1)
    tempResponse = parseTemp(response, "T", &toolTemperature[0]);
  else {
    tempResponse = false;
    for (int t = 0; t < fwExtruders; t++)
      tempResponse |= parseTemp(response, "T" + String(t), &toolTemperature[t]);
  }
  tempResponse |= parseTemp(response, "B", &bedTemperature);
  if (!tempResponse) {
    // Parse Prusa heating temperatures
    int e = parsePrusaHeatingExtruder(response);
    tempResponse = e >= 0 && e < MAX_SUPPORTED_EXTRUDERS && parsePrusaHeatingTemp(response, "T", &toolTemperature[e]);
    tempResponse |= parsePrusaHeatingTemp(response, "B", &bedTemperature);
    }

  return tempResponse;
}

// Parse position responses from printer like
// X:-33.00 Y:-10.00 Z:5.00 E:37.95 Count X:-3300 Y:-1000 Z:2000
inline bool parsePosition(const String response) {
  return response.indexOf("X:") != -1 && response.indexOf("Y:") != -1 &&
         response.indexOf("Z:") != -1 && response.indexOf("E:") != -1;
}

inline void lcd(const String text) {
  commandQueue.push("M117 " + text);
}

inline void playSound() {
  commandQueue.push("M300 S500 P50");
}

inline String getUploadedFilename() {
  return uploadedFullname == "" ? "Unknown" : uploadedFullname.substring(1);
}

void handlePrint() {
  static FileWrapper gcodeFile;
  static float prevM73Completion, prevM532Completion = 0.0;

  if (isPrinting) {
    const bool abortPrint = (restartPrint || cancelPrint);
    if (abortPrint || !gcodeFile.available()) {
      //yield();
      gcodeFile.close();
      if (fwProgressCap)
        commandQueue.push("M530 S0");
      if (!abortPrint) {
        lcd("Complete");
      }
      printPause = false;
      isPrinting = false;
    }
    else if (!printPause && commandQueue.getFreeSlots() > 4) {    // Keep some space for "service" commands
      ++lastPrintedLine;
      String line = gcodeFile.readStringUntil('\n'); // The G-Code line being worked on
      filePos += line.length()+1; // readStringUntil does not include the eol char.
      if (filePos > uploadedFileSize)
        filePos = uploadedFileSize;
      int pos = line.indexOf(';');
      if (line.length() > 0 && pos != 0 && line[0] != '(' && line[0] != '\r') {
        if (pos != -1)
          line = line.substring(0, pos);
        commandQueue.push(line);
      }

      // Send to printer completion (if supported)
      printCompletion = (float)filePos / uploadedFileSize * 100.0;
      printTime = (ms - printStartTime) / 1000;
      if (fwBuildPercentCap && printCompletion - prevM73Completion >= 1) {
        commandQueue.push("M73 P" + String((int)printCompletion));
        prevM73Completion = printCompletion;
      }
      if (fwProgressCap && printCompletion - prevM532Completion >= 0.1) {
        commandQueue.push("M532 X" + String((int)(printCompletion * 10) / 10.0));
        prevM532Completion = printCompletion;
      }
    }
  }

  if (!isPrinting && (startPrint || restartPrint)) {
    startPrint = restartPrint = false;

    filePos = 0;
    lastPrintedLine = 0;
    prevM73Completion = prevM532Completion = 0.0;

    gcodeFile = storageFS.open(uploadedFullname);
    if (!gcodeFile)
      lcd("Can't open file");
    else {
      lcd("Printing...");
      playSound();
      printStartTime = ms;
      isPrinting = true;
      if (fwProgressCap) {
        commandQueue.push("M530 S1 L0");
        commandQueue.push("M531 " + getUploadedFilename());
      }
    }
  }
}

void saveUploadedFullname() {
  FileWrapper uploadedfile = storageFS.open("/uploaded.txt", "w");
  for (uint i=0; i<uploadedFullname.length(); i++)
    uploadedfile.write(uploadedFullname.c_str()[i]);
  uploadedfile.close();
}

bool loadUploadedFullname() {
  FileWrapper uploadedfile = storageFS.open("/uploaded.txt", "r");
  if (uploadedfile) {
    uploadedFullname = uploadedfile.readString();
    uploadedfile.close();
    return true;
  }

  return false;
}


uint8_t receivecount = 0;
String lastUploadedFullname = "";
String tempFilename = "";
size_t tmpFileSize = 0;
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  static FileWrapper file;

  if (!index) {
    int pos = filename.lastIndexOf("/");
    uploadedFullname = pos == -1 ? "/" + filename : filename.substring(pos);
    if (uploadedFullname.length() > storageFS.getMaxPathLength())
      uploadedFullname = "/received.gcode";   // TODO maybe a different solution

    if (lastUploadedFullname != uploadedFullname) {
      // Uncomment next code if you want to remove the last file
      /*if (lastUploadedFullname != "") {
        storageFS.remove(lastUploadedFullname);
      }*/
    }

    receivecount++;
    receivecount = receivecount % 4; // We can receive 4 temporary files
    tempFilename = String("/tmp")+String(receivecount);
    file = storageFS.open(tempFilename, "w"); // create or truncate file
    if (file) {
      lcd("Receiving: "+uploadedFullname);
      lastUploadedFullname = uploadedFullname;
    } else {
      lcd("Error receiving file");
    }
  }

  //if (receivecount > 1)
  //  return;

  if (file) {
    file.write(data, len);
    file.flush();
  }

  if (final) { // upload finished
    if (file) {
      //uploadedFileCreationTime = file.getCreationTime();
      uploadedFileCreationTime = DateTime.getTime();
      file.close();
    }
    tmpFileSize = index + len;
    // Early solution: A small size can tell us that there are not a gcode file
    // Why: Cura send us two additional files with "true" or "false" inside.
    // The word "true" and "false" have less than 5 characters so we can be sure that
    // a file bigger than that is the real one.
    if (tmpFileSize > 5) {
      storageFS.remove(uploadedFullname);
      storageFS.rename(tempFilename, uploadedFullname);
      uploadedFileSize = tmpFileSize;
      saveUploadedFullname();
    }
  }
  else
    tmpFileSize = 0;
}

int apiJobHandler(JsonObject root) {
  const char* command = root["command"];
  if (command != NULL) {
    if (strcmp(command, "cancel") == 0) {
      if (!isPrinting)
        return 409;
      cancelPrint = true;
    }
    else if (strcmp(command, "start") == 0) {
      if (isPrinting || !printerConnected || uploadedFullname == "")
        return 409;
      startPrint = true;
    }
    else if (strcmp(command, "restart") == 0) {
      if (!printPause)
        return 409;
      restartPrint = true;
    }
    else if (strcmp(command, "pause") == 0) {
      if (!isPrinting)
        return 409;
      const char* action = root["action"];
      if (action == NULL)
        printPause = !printPause;
      else {
        if (strcmp(action, "pause") == 0)
          printPause = true;
        else if (strcmp(action, "resume") == 0)
          printPause = false;
        else if (strcmp(action, "toggle") == 0)
          printPause = !printPause;
      }
    }
  }

  return 204;
}

String M115ExtractString(const String response, const String field) {
  int spos = response.indexOf(field + ":");
  if (spos != -1) {
    spos += field.length() + 1;
    int epos = response.indexOf(':', spos);
    if (epos == -1) {
      epos = response.indexOf('\n', spos);
      return response.substring(spos, epos-1);
    }
    else {
      while ((epos > spos) && (response[epos] != ' ') && (response[epos] != '\n'))
        --epos;
      return response.substring(spos, epos);
    }
  }

  return "";
}

bool M115ExtractBool(const String response, const String field, const bool onErrorValue = false) {
  String result = M115ExtractString(response, field);
  /*telnetSend("field: "+field);
  telnetSend("response: "+response);
  telnetSend("Result: "+result);*/
  return result == "" ? onErrorValue : (result == "1" ? true : false);
}

inline String getDeviceName() {
  #if defined(ESP8266)
    return fwMachineType + " (" + String(ESP.getChipId(), HEX) + ")";
  #elif defined(ESP32)
    uint64_t chipid = ESP.getEfuseMac();

    return fwMachineType + " (" + String((uint16_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX) + ")";
  #else
    #error Unimplemented chip!
  #endif
}

static bool detectPrinter() {
  static int printerDetectionState = 0;
  static byte nM115 = 0;

  //telnetSend("detectPrinter()");
  switch (printerDetectionState) {
    case 0:
      // Start printer detection
      telnetSend("Starting printer detection...");
      serialBaudIndex = 0;
      printerDetectionState = 10;
      break;

    case 10:
      // Initialize baud and send a request to printezr
      if (nM115 == 0) {
        telnetSend("Connecting at " + String(serialBauds[serialBaudIndex]));
        #ifdef ESP8266
        PrinterSerial.end();
        delay(50);
        PrinterSerial.begin(serialBauds[serialBaudIndex]); // See note above; we have actually renamed Serial to Serial1
        #endif
        #ifdef ESP32
        PrinterSerial.begin(serialBauds[serialBaudIndex], SERIAL_8N1, 32, 33); // gpio32 = rx, gpio33 = tx
        #endif
      }
      //telnetSend("Trying M115...");
      commandQueue.push("M115"); // M115 - Firmware Info
      printerDetectionState = 20;
      //delay(50);
      break;

    case 20:
      telnetSend("Check printer response...");
      // Check if there is a printer response
      if (commandQueue.isEmpty()) {
        String value = M115ExtractString(lastReceivedResponse, "MACHINE_TYPE");
        if (value == "") {
          telnetSend("no value");
          if (nM115++ >= REPEAT_M115_TIMES) {
            nM115 = 0;
            ++serialBaudIndex;
            if (serialBaudIndex < sizeof(serialBauds) / sizeof(serialBauds[0]))
              printerDetectionState = 10;
            else
              printerDetectionState = 0;   
          }
          else
            printerDetectionState = 10;
        }
        else {
          telnetSend("Connected");

          fwMachineType = value;
          value = M115ExtractString(lastReceivedResponse, "EXTRUDER_COUNT");
          fwExtruders = value == "" ? 1 : min(value.toInt(), (long)MAX_SUPPORTED_EXTRUDERS);
          fwAutoreportTempCap = M115ExtractBool(lastReceivedResponse, "AUTOREPORT_TEMP");
          fwProgressCap = M115ExtractBool(lastReceivedResponse, "Cap:PROGRESS");
          fwBuildPercentCap = M115ExtractBool(lastReceivedResponse, "Cap:BUILD_PERCENT");
          //M115ExtractBool(lastReceivedResponse, "Cap:SDCARD");
          //M115ExtractBool(lastReceivedResponse, "Cap:ARCS");

          String text = WiFiService.getCurrentIP().toString() + " " + storageFS.getActiveFS();
          lcd(text);
          playSound();

          if (fwAutoreportTempCap)
            commandQueue.push(AUTOTEMP_COMMAND + String(TEMPERATURE_REPORT_INTERVAL));   // Start auto report temperatures
          else
            temperatureTimer = ms;
          return true;
        }
      }
      //delay(50);
      break;
  }

  return false;
}

/*static String list = "";
#define ITEMS_PER_PAGE 10 // Compromiso en limites de memoria
inline String listGcodeFiles(String sel = "", uint8 page = 0, String search = "") {
  MD5Builder md5;
  String hash = "";
  list = "<ul>";
  FileWrapper file;
  FileWrapper dir = storageFS.open("/");
  uint16_t n = 0;
  uint16_t found_n = 0;
  
  String filename;
  search.toLowerCase();
  bool found;
  
  if (dir) {
    file = dir.openNextFile();
    while (file) {
      if (isGcode(file.name()) && !file.isDirectory())
      {
        n++;
        filename = String(file.name());
        filename.toLowerCase();
        found = (search.length() > 0) && (filename.indexOf(search) != -1);
        if (found) { found_n++; }
        if ((n >= page*ITEMS_PER_PAGE) || found)
        {
          md5.begin();
          md5.add(file.name());
          md5.calculate();
          hash = md5.toString();
          if (sel.length() > 0)
          {
            if (strcmp(sel.c_str(), hash.c_str()) == 0) {
              list = file.name();
              file.close();
              dir.close();
              return list;
            }
          }
          else
          if ((search.length() > 0) && !found) {
          }
          else
          if ((n < ((page+1)*ITEMS_PER_PAGE)) || (found && found_n <= ITEMS_PER_PAGE) )
          {
            list += "<li>"
                      +String(n)+" - "
                      "<button onclick=\"if (confirm('Delete &laquo;"+file.name()+"&raquo;?')) { window.location.href='/?del="+hash+"&p="+String(page)+"'; };\">DEL</button>"
                      " <a href=\"/?sel="+hash+"&p="+String(page)+"\">"+file.name()+"</a> "
                      " "+String(file.size())+" bytes."
                    "</li>";
          }
        }
      }
      file.close();
      file = dir.openNextFile();
    }
    dir.close();
  }
  if (sel.length() > 0)
  {
    list = "";
    return list;
  }

  list += "</ul>";
  uint8 pages = n / ITEMS_PER_PAGE;
  list += "<div class=\"pages\">";
  for (page = 0; page<=pages; page++) {
    list += " <a href=\"/?&p="+String(page)+"\">"+String(page)+"</a> |";
  }
  list += "</div>";
  list += "Total: "+String(n)+" files";
  if (search.length() > 0) {
    list += " - Found: "+String(found_n);
  }
  return list;
}
*/


void initUploadedFilename(String filename = "") {
  FileWrapper file;

  if (filename.length() > 0) {
      uploadedFullname = "/" + filename;
      file = storageFS.open(uploadedFullname);
  }
  else
  {
    if (loadUploadedFullname())
      file = storageFS.open(uploadedFullname);
    else
    {
      FileWrapper dir = storageFS.open("/");
      if (dir) {
        file = dir.openNextFile();
        while ( (file && !file.isGcode()) || file.isDirectory() )
        {
          file.close();
          file = dir.openNextFile();
        }
        dir.close();
      }
    }
  }

  if (file) {
    uploadedFullname = "/" + file.name();
    uploadedFileCreationTime = file.getCreationTime();
    uploadedFileSize = file.size();
    file.close();
    saveUploadedFullname();
  } 
}


inline String getState() {
  if (!printerConnected)
    return "Discovering printer";
  else if (cancelPrint)
    return "Cancelling";
  else if (printPause)
    return "Paused";
  else if (isPrinting)
    return "Printing";
  else
    return "Operational";
}

inline String stringify(bool value) {
  return value ? "true" : "false";
}

inline void filesList(DynamicJsonDocument &doc, uint8_t index, String id = "") {
    MD5Builder md5;
    FileWrapper file;
    FileWrapper dir = storageFS.open("/");
    uint16_t i = 0;
    uint8_t count = 0;
    if (dir) {
      file = dir.openNextFile();
      while (file) {
          if (file.isGcode() && !file.isDirectory())
          {
            if (i>=index)
            {
              md5.begin();
              md5.add(file.name());
              md5.calculate();
              if (id == "" || (id != "" && id == md5.toString())) {
                doc["files"][count]["name"] = file.name();
                doc["files"][count]["size"] = file.size();
                doc["files"][count]["id"] = md5.toString();
                count++;
              }
            }
            i++;
          }
          file.close();
          if (count < MAX_FILES_PER_LIST)
            file = dir.openNextFile();
      }
      dir.close();
    }

    doc["next"] = i;
}

#ifndef DISABLE_LOGGING
#define LOG_FILENAME "/log.txt"
//#define MAX_LOG_FILESIZE 16384
static FileWrapper logfile;
#endif

void PrinterSetup() {
  #ifdef USE_FAST_SD
	storageFS.begin(true);
  #else
  storageFS.begin(false);
  #endif

  #ifndef DISABLE_LOGGING
  if (storageFS) {
    logfile = storageFS.open(LOG_FILENAME, "a");
    ThingManager.startLogging(LOG_LEVEL_VERBOSE, &logfile);
  }
  #endif

  commandQueue.clear();

  for (int t = 0; t < MAX_SUPPORTED_EXTRUDERS; t++)
    toolTemperature[t] = { 0, 0 };
  bedTemperature = { 0, 0 };

  telnetServer.begin();
  telnetServer.setNoDelay(true);

  initUploadedFilename();

  // Info page
  webServer.on("/info", HTTP_GET, [](AsyncWebServerRequest * request) {
    String message = "<pre>"
                     "Free heap: " + String(ESP.getFreeHeap()) + "\n\n"
                     "File system: " + storageFS.getActiveFS() + "\n";
    if (storageFS.isActive()) {
      message += "Filename length limit: " + String(storageFS.getMaxPathLength()) + "\n";
      if (uploadedFullname != "") {
        message += "Uploaded file: " + getUploadedFilename() + "\n"
                   "Uploaded file size: " + String(uploadedFileSize) + "\n";
      }
    }
    message += "\n"
               "Last command sent: " + lastCommandSent + "\n"
               "Last received response: " + lastReceivedResponse + "\n";
    if (printerConnected) {
      message += "\n"
                 "EXTRUDER_COUNT: " + String(fwExtruders) + "\n"
                 "AUTOREPORT_TEMP: " + stringify(fwAutoreportTempCap);
      if (fwAutoreportTempCap)
        message += " Enabled: " + stringify(autoreportTempEnabled);
      message += "\n"
                 "PROGRESS: " + stringify(fwProgressCap) + "\n"
                 "BUILD_PERCENT: " + stringify(fwBuildPercentCap) + "\n";
    }
    message += "</pre>";
    request->send(200, "text/html", message);
  });

#ifndef DISABLE_LOGGING
  webServer.on("/log", HTTP_GET, [](AsyncWebServerRequest * request) {
    int max_lines = 10;
    if (request->hasParam("lines")) {
      AsyncWebParameter *p = request->getParam("lines");
      max_lines = p->value().toInt();
    }

    FileWrapper log = storageFS.open(LOG_FILENAME);
    if (log) {
      int log_pos = log.size()-1;
      int lines = 0;
      String text = "<pre>Log size: "+String(log.size())+"</pre>";
      while (lines <= max_lines && log_pos > 0) {
        String line = log.readStringUntilToBack('\n', log_pos);
        text += "<pre>"+line+"</pre>";
        lines++;
        log_pos = log_pos - line.length() - 1;
      }
      request->send(200, "text/html", text);
      log.close();
    } else {
      request->send(404, "text/html", "There are no log file!");
    }
  });
#endif

  webServer.on("/status", HTTP_GET, [&](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(1300);

    doc["name"] = getDeviceName();
    doc["state"] = getState();
    doc["printing"] = isPrinting;
    doc["lastCommand"] = lastCommandSent;
    doc["lastResponse"] = lastReceivedResponse;

    //doc["free_heap"] = ESP.getFreeHeap();
    //doc["filesystem"] = storageFS.getActiveFS();
    //doc["filename_max_length"] = storageFS.getMaxPathLength();
    doc["ip"] = WiFiService.getCurrentIP().toString();
    doc["uploaded_file"]["name"] = getUploadedFilename();
    doc["uploaded_file"]["size"] = uploadedFileSize;
    //doc["last_command_sent"] = lastCommandSent;
    //doc["last_received_response"] = lastReceivedResponse;
    //doc["EXTRUDER_COUNT"] = fwExtruders;
    //doc["AUTOREPORT_TEMP"] = fwAutoreportTempCap;
    //doc["AUTOREPORT_TEMP_ENABLED"] = autoreportTempEnabled;
    //doc["PROGRESS"] = fwProgressCap;
    //doc["BUILD_PERCENT"] = fwBuildPercentCap;
    doc["print_completion"] = String(printCompletion);
    
    doc["printing_time"]["elapsed"] = printTime;
    doc["printing_time"]["remaining"] = (printCompletion > 0) ? printTime / printCompletion * (100 - printCompletion) : 0;

    doc["bed_temperature"]["actual"] = bedTemperature.actual/100.0;
    doc["bed_temperature"]["target"] = bedTemperature.target/100.0;
    for (uint8_t t = 0; t < MAX_SUPPORTED_EXTRUDERS; t++) {
      doc["tool_temperature"][t]["actual"] = toolTemperature[t].actual/100.0;
      doc["tool_temperature"][t]["target"] = toolTemperature[t].target/100.0;
    }

    doc["ms"] = ms;
#ifndef DISABLE_DATETIME
    doc["datetime"] = DateTime.format(DateFormatter::COMPAT);
#else
    doc["datetime"] = "00000000_000000"
#endif

    serializeJson(doc, *response);
    request->send(response);
  });

  webServer.on("/files/list", HTTP_GET, [&](AsyncWebServerRequest *request) {
    uint8_t index = 0;
    if (request->hasParam("i")) {
      AsyncWebParameter *p = request->getParam("i");
      index = p->value().toInt();
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(1536);
    filesList(doc, index);
    serializeJson(doc, *response);
    request->send(response);
  });

  webServer.on("/files/delete", HTTP_GET, [&](AsyncWebServerRequest *request) {
    String id = "";
    if (request->hasParam("id")) {
      AsyncWebParameter *p = request->getParam("id");
      id = p->value();
    }
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(256);
    filesList(doc, 0, id);
    if (doc["files"][0]["id"] == id) {
      String filename = doc["files"][0]["name"];
      storageFS.remove("/"+filename);
    }
    serializeJson(doc, *response);
    request->send(response);
  });

  webServer.on("/files/choose", HTTP_GET, [&](AsyncWebServerRequest *request) {
    String id = "";
    if (request->hasParam("id")) {
      AsyncWebParameter *p = request->getParam("id");
      id = p->value();
    }
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(256);
    filesList(doc, 0, id);
    if (doc["files"][0]["id"] == id) {
      initUploadedFilename(doc["files"][0]["name"]);
    }
    serializeJson(doc, *response);
    request->send(response);
  });

  webServer.on("/move", HTTP_GET, [&](AsyncWebServerRequest *request) {    
    bool result = false;
    if (isPrinting)
    {
      request->send(200, "text/plain", String(result));
      return;
    }

    int8_t dir = -1;
    if (request->hasParam("m")) {
      AsyncWebParameter *p = request->getParam("m");
      dir = p->value().toInt();
    }
    uint8_t distance = 10;
    if (request->hasParam("d")) {
      AsyncWebParameter *p = request->getParam("d");
      distance = abs(p->value().toInt());
    }

    switch (dir)
    {
    case 5:
      result = commandQueue.push("G28 XY");
      break;

    case 8:
      if (commandQueue.push("G91"))
        result = commandQueue.push("G0 Y"+String(distance));
      break;

    case 2:
      if (commandQueue.push("G91"))
        result = commandQueue.push("G0 Y-"+String(distance));
      break;
    
    case 4:
      if (commandQueue.push("G91"))
        result = commandQueue.push("G0 X-"+String(distance));
      break;

    case 6:
      if (commandQueue.push("G91"))
        result = commandQueue.push("G0 X"+String(distance));
      break;

    // Z
    case 0:
      result = commandQueue.push("G28 Z");
      break;

    case 9:
      if (commandQueue.push("G91"))
        result = commandQueue.push("G0 Z"+String(distance));
      break;

    case 3:
      if (commandQueue.push("G91"))
        result = commandQueue.push("G0 Z-"+String(distance));
      break;

    default:
      break;
    }

    commandQueue.push("G90");
    request->send(200, "text/plain", String(result));
  });

  // Download page
  webServer.on("/download", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse("application/x-gcode", uploadedFileSize, [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      static size_t downloadBytesLeft;
      static FileWrapper downloadFile;

      if (!index) {
        downloadFile = storageFS.open(uploadedFullname);
        downloadBytesLeft = uploadedFileSize;
      }
      size_t bytes = min(downloadBytesLeft, maxLen);
      bytes = min(bytes, (size_t)1024);
      bytes = downloadFile.read(buffer, bytes);
      downloadBytesLeft -= bytes;
      if (bytes <= 0)
        downloadFile.close();

      return bytes;
    });
    response->addHeader("Content-Disposition", "attachment; filename=\"" + getUploadedFilename()+ "\"");
    request->send(response);    
  });

  webServer.on("/api/login", HTTP_POST, [](AsyncWebServerRequest * request) {
    // https://docs.octoprint.org/en/master/api/general.html#post--api-login
    // https://github.com/fieldOfView/Cura-OctoPrintPlugin/issues/155#issuecomment-596109663
    request->send(200, "application/json", "{}");  });

  webServer.on("/api/version", HTTP_GET, [](AsyncWebServerRequest * request) {
    // http://docs.octoprint.org/en/master/api/version.html
    request->send(200, "application/json", "{\r\n"
                                           "  \"api\": \"" API_VERSION "\",\r\n"
                                           "  \"server\": \"" VERSION "\"\r\n"
                                           "}");  });

  webServer.on("/api/connection", HTTP_GET, [](AsyncWebServerRequest * request) {
    // http://docs.octoprint.org/en/master/api/connection.html#get-connection-settings
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(1024);
    
    doc["current"]["state"] = getState();
    doc["current"]["port"] = "Serial";
    doc["current"]["baudrate"] = serialBauds[serialBaudIndex];
    doc["current"]["printerProfile"] = "Default";

    doc["options"]["ports"] = "Serial";
    doc["options"]["baudrate"] = serialBauds[serialBaudIndex];
    doc["options"]["printerProfiles"] = "Default";
    doc["options"]["portPreference"] = "Serial";
    doc["options"]["baudratePreference"] = serialBauds[serialBaudIndex];
    doc["options"]["printerProfilePrference"] = "Default";
    doc["options"]["autoconnect"] = true;

    serializeJson(doc, *response);
    request->send(response);

    /*request->send(200, "application/json", "{\r\n"
                                           "  \"current\": {\r\n"
                                           "    \"state\": \"" + getState() + "\",\r\n"
                                           "    \"port\": \"Serial\",\r\n"
                                           "    \"baudrate\": " + serialBauds[serialBaudIndex] + ",\r\n"
                                           "    \"printerProfile\": \"Default\"\r\n"
                                           "  },\r\n"
                                           "  \"options\": {\r\n"
                                           "    \"ports\": \"Serial\",\r\n"
                                           "    \"baudrate\": " + serialBauds[serialBaudIndex] + ",\r\n"
                                           "    \"printerProfiles\": \"Default\",\r\n"
                                           "    \"portPreference\": \"Serial\",\r\n"
                                           "    \"baudratePreference\": " + serialBauds[serialBaudIndex] + ",\r\n"
                                           "    \"printerProfilePreference\": \"Default\",\r\n"
                                           "    \"autoconnect\": true\r\n"
                                           "  }\r\n"
                                           "}");*/
  });

  // Todo: http://docs.octoprint.org/en/master/api/connection.html#post--api-connection

  // File Operations
  // For Slic3r OctoPrint compatibility
  webServer.on("/api/files/local", HTTP_POST, [](AsyncWebServerRequest * request) {
    // https://docs.octoprint.org/en/master/api/files.html?highlight=api%2Ffiles%2Flocal#upload-file-or-create-folder
    lcd("Received");
    playSound();

    // We are not using
    // if (request->hasParam("print", true))
    // due to https://github.com/fieldOfView/Cura-OctoPrintPlugin/issues/156
    
    startPrint = printerConnected && !isPrinting && uploadedFullname != "";

    // OctoPrint sends 201 here; https://github.com/fieldOfView/Cura-OctoPrintPlugin/issues/155#issuecomment-596110996
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(512);
    doc["files"]["local"]["name"] = getUploadedFilename();
    doc["files"]["local"]["time"] = uploadedFileCreationTime;
    doc["files"]["local"]["size"] = uploadedFileSize;
    doc["files"]["local"]["origin"] = "local";
    doc["done"] = true;

    serializeJson(doc, *response);
    response->setCode(201);
    request->send(response);
    /*request->send(201, "application/json", "{\r\n"
                                           "  \"files\": {\r\n"
                                           "    \"local\": {\r\n"
                                           "      \"name\": \"" + getUploadedFilename() + "\",\r\n"
                                           "      \"origin\": \"local\"\r\n"
                                           "    }\r\n"
                                           "  },\r\n"
                                           "  \"done\": true\r\n"
                                           "}");*/
  }, handleUpload);

  // Pending: http://docs.octoprint.org/en/master/api/files.html#retrieve-all-files
  webServer.on("/api/files", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "application/json", "{\r\n"
                                           "  \"files\": {\r\n"
                                           "  }\r\n"
                                           "}");
  });

  webServer.on("/api/job", HTTP_GET, [](AsyncWebServerRequest * request) {
    // http://docs.octoprint.org/en/master/api/job.html#retrieve-information-about-the-current-job
    int32_t printTimeLeft = 0;
    if (isPrinting) {
      printTimeLeft = (printCompletion > 0) ? printTime / printCompletion * (100 - printCompletion) : INT32_MAX;
    }
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(1024);

    doc["job"]["file"]["name"] = getUploadedFilename();
    doc["job"]["file"]["origin"] = "local";
    doc["job"]["file"]["size"] = uploadedFileSize;
    //tm *time = localtime(&uploadedFileCreationTime);
    //char str[32];
    //strftime(str, 32, "%Y-%m-%d %H:%M:%S", time);
    doc["job"]["file"]["date"] = uploadedFileCreationTime;

    //doc["job"]["estimatedPrintTime"] = estimatedPrintTime;
    doc["job"]["filament"] = "";

    doc["progress"]["completion"] = printCompletion;
    doc["progress"]["filepos"] = filePos;
    doc["progress"]["printTime"] = printTime;
    doc["progress"]["printTimeLeft"] = printTimeLeft;
    doc["progress"]["printTimeLeftOrigin"] = "linear";

    doc["state"] = getState();

    serializeJson(doc, *response);
    request->send(response);
    /*request->send(200, "application/json", "{\r\n"
                                           "  \"job\": {\r\n"
                                           "    \"file\": {\r\n"
                                           "      \"name\": \"" + getUploadedFilename() + "\",\r\n"
                                           "      \"origin\": \"local\",\r\n"
                                           "      \"size\": " + String(uploadedFileSize) + ",\r\n"
                                           "      \"date\": " + String(uploadedFileDate) + "\r\n"
                                           "    },\r\n"
                                           //"    \"estimatedPrintTime\": \"" + estimatedPrintTime + "\",\r\n"
                                           "    \"filament\": {\r\n"
                                           //"      \"length\": \"" + filementLength + "\",\r\n"
                                           //"      \"volume\": \"" + filementVolume + "\"\r\n"
                                           "    }\r\n"
                                           "  },\r\n"
                                           "  \"progress\": {\r\n"
                                           "    \"completion\": " + String(printCompletion) + ",\r\n"
                                           "    \"filepos\": " + String(filePos) + ",\r\n"
                                           "    \"printTime\": " + String(printTime) + ",\r\n"
                                           "    \"printTimeLeft\": " + String(printTimeLeft) + "\r\n"
                                           "  },\r\n"
                                           "  \"state\": \"" + getState() + "\"\r\n"
                                           "}");*/
  });

  webServer.on("/api/job", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Job commands http://docs.octoprint.org/en/master/api/job.html#issue-a-job-command
    request->send(200, "text/plain", "");
    },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      request->send(400, "text/plain", "file not supported");
    },
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      static String content;

      if (!index)
        content = "";
      for (uint i = 0; i < len; ++i)
        content += (char)data[i];
      if (content.length() >= total) {
        DynamicJsonDocument doc(1024);
        auto error = deserializeJson(doc, content);
        if (error)
          request->send(400, "text/plain", error.c_str());
        else {
          int responseCode = apiJobHandler(doc.as<JsonObject>());
          request->send(responseCode, "text/plain", "");
          content = "";
        }
      }
  });
  
  webServer.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest * request) {
    // https://github.com/probonopd/WirelessPrinting/issues/30
    // https://github.com/probonopd/WirelessPrinting/issues/18#issuecomment-321927016
    request->send(200, "application/json", "{}");
  });

  webServer.on("/api/printer/command", HTTP_POST, [](AsyncWebServerRequest *request) {
    // http://docs.octoprint.org/en/master/api/printer.html#send-an-arbitrary-command-to-the-printer
    request->send(200, "text/plain", "");
    },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      request->send(400, "text/plain", "file not supported");
    },
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      static String content;

      if (!index)
        content = "";
      for (size_t i = 0; i < len; ++i)
        content += (char)data[i];
      if (content.length() >= total) {
        DynamicJsonDocument doc(1024);
        auto error = deserializeJson(doc, content);
        if (error)
          request->send(400, "text/plain", error.c_str());
        else {
          JsonObject root = doc.as<JsonObject>();
          const char* command = root["command"];
          if (command != NULL)
            commandQueue.push(command);
          else {
            JsonArray commands = root["commands"].as<JsonArray>();
            for (JsonVariant command : commands)
              commandQueue.push(String(command.as<String>()));
            }
          request->send(204, "text/plain", "");
        }
        content = "";
      }
  });

  webServer.on("/api/printer", HTTP_GET, [](AsyncWebServerRequest * request) {
    // https://docs.octoprint.org/en/master/api/printer.html#retrieve-the-current-printer-state
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument doc(1024);

    for (uint8_t t = 0; t < fwExtruders; ++t) {
      String tooln = "tool"+String(t);
      doc["temperature"][tooln]["actual"] = toolTemperature[t].actual/100.0;
      doc["temperature"][tooln]["target"] = toolTemperature[t].target/100.0;
      doc["temperature"][tooln]["offset"] = 0;
    }

    doc["temperature"]["bed"]["actual"] = bedTemperature.actual/100.0;
    doc["temperature"]["bed"]["target"] = bedTemperature.target/100.0;
    doc["temperature"]["bed"]["offset"] = 0;

    doc["sd"]["ready"] = false;

    doc["state"]["text"] = getState();
    doc["state"]["flags"]["operational"] = printerConnected;
    doc["state"]["flags"]["paused"] = printPause;
    doc["state"]["flags"]["printing"] = isPrinting;
    doc["state"]["flags"]["pausing"] = false;
    doc["state"]["flags"]["cancelling"] = cancelPrint;
    doc["state"]["flags"]["sdReady"] = false;
    doc["state"]["flags"]["error"] = false;
    doc["state"]["flags"]["ready"] = printerConnected;
    doc["state"]["flags"]["closedOrError"] = !printerConnected;

    serializeJson(doc, *response);
    if (!printerConnected)
      response->setCode(409);// 409 Conflict – If the printer is not operational.
    request->send(response);

    /*String readyState = stringify(printerConnected);
    String message = "{\r\n"
                     "  \"state\": {\r\n"
                     "    \"text\": \"" + getState() + "\",\r\n"
                     "    \"flags\": {\r\n"
                     "      \"operational\": " + readyState + ",\r\n"
                     "      \"paused\": " + stringify(printPause) + ",\r\n"
                     "      \"printing\": " + stringify(isPrinting) + ",\r\n"
                     "      \"pausing\": false,\r\n"
                     "      \"cancelling\": " + stringify(cancelPrint) + ",\r\n"
                     "      \"sdReady\": false,\r\n"
                     "      \"error\": false,\r\n"
                     "      \"ready\": " + readyState + ",\r\n"
                     "      \"closedOrError\": " + stringify(!printerConnected) + "\r\n"
                     "    }\r\n"
                     "  },\r\n"
                     "  \"temperature\": {\r\n";
    for (int t = 0; t < fwExtruders; ++t) {
      message += "    \"tool" + String(t) + "\": {\r\n"
                 "      \"actual\": " + toolTemperature[t].actual + ",\r\n"
                 "      \"target\": " + toolTemperature[t].target + ",\r\n"
                 "      \"offset\": 0\r\n"
                 "    },\r\n";
    }
    message += "    \"bed\": {\r\n"
               "      \"actual\": " + bedTemperature.actual + ",\r\n"
               "      \"target\": " + bedTemperature.target + ",\r\n"
               "      \"offset\": 0\r\n"
               "    }\r\n"
               "  },\r\n"
               "  \"sd\": {\r\n"
               "    \"ready\": false\r\n"
               "  }\r\n"
               "}";
    request->send(200, "application/json", message);*/
  });

  // For legacy PrusaControlWireless - deprecated in favor of the OctoPrint API
  webServer.on("/print", HTTP_POST, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "Received");
  }, handleUpload);

  // For legacy Cura WirelessPrint - deprecated in favor of the OctoPrint API
  webServer.on("/api/print", HTTP_POST, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "Received");
  }, handleUpload);
}

inline void restartSerialTimeout() {
  serialReceiveTimeoutTimer = ms;
}

void SendCommands() {
  String command = commandQueue.peekSend();  //gets the next command to be sent
  if (command != "") {
    bool noResponsePending = commandQueue.isAckEmpty();
    if (noResponsePending || printerUsedBuffer < PRINTER_RX_BUFFER_SIZE * 3 / 4) {  // Let's use no more than 75% of printer RX buffer
      if (noResponsePending)
        restartSerialTimeout();   // Receive timeout has to be reset only when sending a command and no pending response is expected
      PrinterSerial.println(command); // Send to 3D Printer
      printerUsedBuffer += command.length();
      lastCommandSent = command;
      commandQueue.popSend();

      telnetSend(">" + command);
    }
  }
}

static String serialResponse = "";
static int lineStartPos = 0;
void ReceiveResponses() {
  while (PrinterSerial.available() > 0) {
  //while (false) {
    //yield();
    char ch = (char)PrinterSerial.read();
    if (ch == '\r') continue;
    serialResponse += String(ch);
    if (ch == '\n')
    { // new line
      bool incompleteResponse = false;
      String responseDetail = "";

      if (serialResponse.startsWith("ok") || serialResponse.endsWith("ok\n")) {
        if (lastCommandSent.startsWith(TEMP_COMMAND))
          parseTemperatures(serialResponse);
        else if (fwAutoreportTempCap && lastCommandSent.startsWith(AUTOTEMP_COMMAND))
          autoreportTempEnabled = (lastCommandSent[6] != '0');

        unsigned int cmdLen = commandQueue.popAcknowledge().length();     // Go on with next command
        printerUsedBuffer = max(printerUsedBuffer - cmdLen, 0u);
        responseDetail = "ok";
      }
      else if (printerConnected) {
        if (parseTemperatures(serialResponse))
          responseDetail = "autotemp";
        else if (parsePosition(serialResponse))
          responseDetail = "position";
        else if (serialResponse.startsWith("echo:busy"))
          responseDetail = "busy";
        else if (serialResponse.startsWith("echo: cold extrusion prevented")) {
          // To do: Pause sending gcode, or do something similar
          responseDetail = "cold extrusion";
        }
        else if (serialResponse.startsWith("Error:")) {
          cancelPrint = true;
          responseDetail = "ERROR";
        }
        else {
          incompleteResponse = true;
          responseDetail = "wait more";
        }
      } else {
          incompleteResponse = true;
          responseDetail = "discovering";
      }

      int responseLength = serialResponse.length();
#ifdef TELNET_CUSTOM_FORMAT
      telnetSend("<" + serialResponse.substring(lineStartPos, responseLength - 1) + "#" + responseDetail + "#");
#else
      telnetSend(serialResponse.substring(lineStartPos, responseLength - 1));
#endif
      if (incompleteResponse)
        lineStartPos = responseLength;
      else {
        lastReceivedResponse = serialResponse;
        lineStartPos = 0;
        serialResponse = "";
      }
      restartSerialTimeout();
    }
  }

  if (!commandQueue.isAckEmpty() && ((ms - serialReceiveTimeoutTimer) > KEEPALIVE_INTERVAL)) {  // Command has been lost by printer, buffer has been freed
    if (printerConnected)
      telnetSend("#TIMEOUT#");
    else
      commandQueue.clear();
    lineStartPos = 0;
    serialResponse = "";
    restartSerialTimeout();
  }
  /*
  // this resets all the neopixels to an off state
  strip.Begin();
  strip.Show();
  // strip.SetPixelColor(0, red);
  // strip.SetPixelColor(1, green);
  // strip.SetPixelColor(2, blue);
  // strip.SetPixelColor(3, white);
  int a;
  for(a=0; a<PixelCount; a++){
    strip.SetPixelColor(a, white);
  }
  strip.Show(); 
  */
}

static String clientCommand = "";
void PrinterHandle() {
  //********************
  //* Printer handling *
  //********************
  if (!printerConnected) {
    printerConnected = detectPrinter();
  } else {
    handlePrint();

    if (cancelPrint && !isPrinting) { // Only when cancelPrint has been processed by 'handlePrint'
      cancelPrint = false;
      commandQueue.clear();
      printerUsedBuffer = 0;
      // Apparently we need to decide how to handle this
      // For now using M112 - Emergency Stop
      // http://marlinfw.org/docs/gcode/M112.html
      telnetSend("Should cancel print! This is not working yet");
      //commandQueue.push("M112"); // Send to 3D Printer immediately w/o waiting for anything

      // My proposal (I am currently using this method on an 
      // Ender 3 Pro Marlin 2.0.7.2 with success):
      commandQueue.push("M410"); // Quickstop
      //commandQueue.push("G10"); // Retract filament
      commandQueue.push("M104 S0"); // Set hotend temperature to 0º
      commandQueue.push("M140 S0"); // Set bed temperature to 0º
      commandQueue.push("G27 P0"); // Park the nozzle
      commandQueue.push("M18"); // Disable steppers
      //playSound();
      //lcd("Print aborted");
    }

    if (!autoreportTempEnabled) {
      if ((signed)(temperatureTimer - ms) <= 0) {
        commandQueue.push(TEMP_COMMAND);
        temperatureTimer = ms + TEMPERATURE_REPORT_INTERVAL * 1000;
      }
    }
  }

  SendCommands();
  ReceiveResponses();

  //*******************
  //* Telnet handling *
  //*******************
  // look for Client connect trial
  if (telnetServer.hasClient() && (!telnetClient || !telnetClient.connected())) {
    if (telnetClient)
      telnetClient.stop();

    //telnetClient = telnetServer.accept();
    telnetClient = telnetServer.available();
    telnetClient.flush();  // clear input buffer, else you get strange characters
  }

  while (telnetClient && telnetClient.available() > 0) {  // get data from Client
    //yield();
    char ch = telnetClient.read();
    if (ch == '\r' || ch == '\n') {
      if (clientCommand.length() > 0) {
        if (!commandQueue.push(clientCommand)) {
          telnetSend("command queue full");
        }
        //telnetSend("Received: "+clientCommand);
        clientCommand = "";
      }
    }
    else
    {
      //telnetSend("receiving: "+String(ch));
      clientCommand += String(ch);
      //if (ch == '\3') // Ctrl+C
        //telnetClient.stop();
    }
  }
}


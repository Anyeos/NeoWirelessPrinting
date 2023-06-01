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

ThingWiFi WiFiService;

/*
   Initializes this Home Automation Thing with:
   name = Friendly name
   hostname = Name of the host (must complains as a hostname)
   files = List of files that must be served as website (take in account that the first will be served as default webpage)
*/
HomeThingManager::HomeThingManager(const String &name, const String &hostname, FilesList files) {
   Name = name;
   setHostNameUnique(hostname);
   _files = files;
}

void HomeThingManager::setHostNameUnique(const String &hostname) {
   hostName = hostname+"_"+ThingWiFi::getChipId();
   hostName.replace(" ", "");
}


void HomeThingManager::startLogging(int level, Print *output, bool showLevel) {
   Log.begin(level, output, showLevel);
   Log.info("*** HomeThingManager LOG STARTED ***" CR);
}

bool HomeThingManager::isWebSiteReady() {
   for (uint8_t i=0; i<_files.count; i++) {
      String docroot_file = "/w/"+String(_files.names[i]);
      if (!FileFS.exists(docroot_file))
         return false;
   }
   return true;
}

bool HomeThingManager::isFileInList(const char *filename) {
   for (uint8_t i=0; i<_files.count; i++) {
      if (strcmp(_files.names[i], filename) == 0)
         return true;
   }
   return false;
}

void HomeThingManager::begin() {
   FileFS_begin();
   loadsettings();
   //WiFiService.onConnect(&WiFiconnect);
   WiFiService.begin(hostName, false);
   if (MDNS.begin(hostName)) {
      Log.info("Started MDNS service." CR);
   } else {
      Log.error("MDNS service cannot be started." CR);
   }
   OTA_begin();

   if (!WiFiService.isConfigured() || !isWebSiteReady()) {
      web_format();
      web_restart();
   }

   if (!isWebSiteReady() && WiFiService.isConfigured()) {
      // Proceed with Step 2
      web_step2();
   }

   if (isWebSiteReady() && WiFiService.isConfigured()) {
      website();
#ifndef DISABLE_DATETIME
      DateTime_begin();
#endif
   }
   if (!WebService_begin()) {
#if ASYNC_TCP_SSL_ENABLED
      webServer80.on("/restart", HTTP_GET, [&](AsyncWebServerRequest *request) {
         if (request->hasParam("r")) {
            AsyncWebParameter *p = request->getParam("r");
            if (p->value() == String("RESTART")) {
               WiFiService.restart_device();
            }
         }
         request->redirect("/");
      });
#endif
   }
}

void HomeThingManager::handle() {
   if (restart_device) {
      WiFiService.restart_device();
      return;
   }

   WiFiService.handle();
   ArduinoOTA.handle();
   MDNS.update();
   DateTime_handle();
#ifndef DISABLE_WEBOFTHINGS
   WoT_handle();
#endif
}

bool HomeThingManager::loadsettings(const String& filename) {
   settings_filename = filename;
   _settings Settings;
   File file = FileFS.open(settings_filename, "r");
   if (file) {
      file.read((uint8_t *)&Settings, sizeof(_settings));
      file.close();
      uint16_t checksum = ThingWiFi::calculate_checksum((uint8_t *)&Settings, sizeof(_settings));
      if (Settings.checksum == checksum) {
         hostName = String(Settings.hostname);
#ifndef DISABLE_DATETIME
         NTPservers[0] = String(Settings.ntpservers[0]);
         NTPservers[1] = String(Settings.ntpservers[1]);
         NTPservers[2] = String(Settings.ntpservers[2]);
#endif
         admin_user = String(Settings.admin);
         admin_password = String(Settings.admin_pwd);
         Log.info("Settings loaded successfully." CR);
         return true;
      } else {
         Log.warning("Wrong settings file checksum (expected %d, got %d)." CR, checksum, Settings.checksum);
         return false;
      }
   }
   Log.warning("Cannot open settings file." CR);
   return false;
}

bool HomeThingManager::savesettings() {
   _settings Settings;
   memset((uint8_t *)&Settings, 0, sizeof(_settings));
   strncpy(Settings.hostname, hostName.c_str(), 31);
#ifndef DISABLE_DATETIME
   strncpy(Settings.ntpservers[0], NTPservers[0].c_str(), 31);
   strncpy(Settings.ntpservers[1], NTPservers[1].c_str(), 31);
   strncpy(Settings.ntpservers[2], NTPservers[2].c_str(), 31);
#endif
   strncpy(Settings.admin, admin_user.c_str(), 31);
   strncpy(Settings.admin_pwd, admin_password.c_str(), 63);
   Settings.checksum = ThingWiFi::calculate_checksum((uint8_t *)&Settings, sizeof(_settings));
   File file = FileFS.open(settings_filename, "w");
   if (file) {
      file.write((uint8_t*) &Settings, sizeof(_settings));
      file.close();
      Log.info("Settings saved successfully (checksum %d)." CR, Settings.checksum);
      return true;
   }
   Log.error("Settings file cannot be saved!" CR);
   return false;
}

void HomeThingManager::factory_reset() {
   FileFS.remove(settings_filename);
   WiFiService.factory_reset();
   restart_device = true;
}

void HomeThingManager::web_format() {
   webServer.on("/format", HTTP_GET,[&](AsyncWebServerRequest *request) {
      if (request->hasParam("format")) {
         AsyncWebParameter *p = request->getParam("format");
         if (p->value() == String("FORMAT")) {
            if (FileFSformat()) {
               restart_device = true;
            }
         }
      }
      request->redirect("/");
   });
}

void HomeThingManager::web_restart() {
   webServer.on("/restart", HTTP_GET, [&](AsyncWebServerRequest *request) {
      if (request->hasParam("r")) {
         AsyncWebParameter *p = request->getParam("r");
         if (p->value() == String("RESTART")) {
            restart_device = true;
         }
      }
      request->redirect("/");
   });
}

void HomeThingManager::web_step2() {
   Log.info("No website exists, going to Step 2." CR);
   webServer.on("/", HTTP_GET | HTTP_POST, [&](AsyncWebServerRequest *request) {
      const char* field_files = "files";

      String html;
      html = FPSTR(HTML_START);
      html += FPSTR(HTML_STYLE);
      html += FPSTR(HTML_SCRIPT);
      html += FPSTR(HTML_TITLE_BODY_START);
      html.replace(FPSTR(T_title), Name);

      String subhtml = FPSTR(HTML_CONTENT_START);
      subhtml.replace(FPSTR(T_title), "Step 2. Upload Website");
      html += subhtml;

      subhtml = FPSTR(HTML_FORM_MULTIPART_START);
      subhtml.replace(FPSTR(T_action), "/");
      html += subhtml;

      subhtml = FPSTR(HTML_FORM_UPLOAD);
      subhtml.replace(FPSTR(T_name), field_files);
      subhtml.replace(FPSTR(T_label), "Files:");
      subhtml.replace(FPSTR(T_value), "");
      html += subhtml;

      subhtml = FPSTR(HTML_FORM_END);
      subhtml.replace(FPSTR(T_submit), "Upload");
      html += subhtml;

      html += FPSTR(HTML_SEPARATOR);
      
      /*
      subhtml = FPSTR(HTML_SUBHEADER);
      subhtml.replace(FPSTR(T_label), "Actual content:");
      html += subhtml;
      */

      bool deleteall = false;
      if (request->hasParam("d")) {
         AsyncWebParameter *p = request->getParam("d");
         if (p->value() == String("DELETEALL")) {
            deleteall = true;
         }
      }

      html += "<ul class='block'>";
      String docroot_directory = "/w/";
      uint8_t rest = _files.count;
      uint8_t other = 0;
      for (uint8_t i=0; i<_files.count; i++) {
         if (!FileFS.exists(docroot_directory+String(_files.names[i]))) {
            subhtml = FPSTR(HTML_FILE_UPLOADED);
            subhtml.replace(FPSTR(T_name), _files.names[i]);
            subhtml.replace(FPSTR(T_field), "need to upload");
            subhtml.replace(FPSTR(T_value), "color: #aeaeae; background: black;");
            html += subhtml;
         }
      }
      if (FileFS.exists(docroot_directory)) {
         Dir dir = FileFS.openDir(docroot_directory);
         while (dir.next()) {
            if (dir.isFile())
            {
               if (deleteall) {
                  FileFS.remove(docroot_directory+dir.fileName());
                  continue;
               }
               bool inList = isFileInList(dir.fileName().c_str());
               if (inList) rest--;else other++;
               subhtml = FPSTR(HTML_FILE_UPLOADED);
               subhtml.replace(FPSTR(T_name), dir.fileName());
               subhtml.replace(FPSTR(T_field), FileFShumanFileSize(dir.fileSize()));
               subhtml.replace(FPSTR(T_value), inList?"":"color: #444");
               html += subhtml;
            }
         }
      }
      html += "</ul>";
      if (deleteall) {
         request->redirect("/");
         return;
      }
      if (rest > 0) {
         subhtml = "<p>{v} file(s) rest to be uploaded.</p>";
         subhtml.replace(FPSTR(T_value), String(rest));
         html += subhtml;
      } else {
         html += "<p>All needed files uploaded. Now you can <a onclick='restart();'>RESTART</a>.</p>";
      }
      if (other > 0) {
         subhtml = "<p>And there are {v} other file(s).</p>";
         subhtml.replace(FPSTR(T_value), String(other));
         html += subhtml;
      }

      FSInfo fsinfo;
      FileFS.info(fsinfo);

      subhtml = FPSTR(HTML_INFO_RIGHT_START);
      subhtml.replace(FPSTR(T_name), "fsinfo");
      html += subhtml;

      subhtml = FPSTR(HTML_INFO_FIELD);
      subhtml.replace(FPSTR(T_label), "free:");
      subhtml.replace(FPSTR(T_value), FileFShumanFileSize(fsinfo.totalBytes-fsinfo.usedBytes).c_str());
      html += subhtml;

      html += FPSTR(HTML_SEPARATOR);

      html += FPSTR(HTML_TOOLBAR_START);
      subhtml = FPSTR(HTML_TOOLBUTTON);
      subhtml.replace(FPSTR(T_action), "format()");
      subhtml.replace(FPSTR(T_label), "FORMAT");
      html += subhtml;

      subhtml = FPSTR(HTML_TOOLBUTTON);
      subhtml.replace(FPSTR(T_action), "restart()");
      subhtml.replace(FPSTR(T_label), "RESTART");
      html += subhtml;

      subhtml = FPSTR(HTML_TOOLBUTTON);
      subhtml.replace(FPSTR(T_action), "deleteall()");
      subhtml.replace(FPSTR(T_label), "DELETE ALL");
      html += subhtml;

      html += FPSTR(HTML_CONTENT_END);
      html += FPSTR(HTML_BODY_HTML_END);
      request->send(200, "text/html", html);
   }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      String docroot_directory = "/w/";
      if(!index) {
         Log.info("Upload started: %s" CR, String(docroot_directory+filename).c_str());
         FileFS.remove(docroot_directory+filename);
      }
      
      fs::File f = FileFS.open(docroot_directory+filename, "a");
      for (size_t i=0; i<len; i++)
      {
         f.write(data[i]);
         //if (i % 128 == 0)
            //f.flush();
      }
      f.close();

      if(final) {
         Log.info("Upload ended: %s (%u)" CR, String(docroot_directory+filename).c_str(), index+len);
      }
   });
}

String HomeThingManager::file_extension_gz(const String &filename) {
   return filename.substring(filename.length() - 2);
}
String HomeThingManager::file_basename_gz(const String &filename) {
   return filename.substring(0, filename.length() - 3);
}

String HomeThingManager::file_basefile_gz(const String &filename) {
   String ext = file_extension_gz(filename);
   ext.toLowerCase();
   if (ext == String("gz")) {
      return file_basename_gz(filename);
   }
   return filename;
}

void HomeThingManager::website() {
   Log.info("Begin main website." CR);
   webServer.serveStatic("/", FileFS, "/w")
      .setDefaultFile(file_basefile_gz(_files.names[0]).c_str());
      
   for (uint8_t i=0; i<_files.count; i++) {
      String filename = String(_files.names[i]);
      String ext = file_extension_gz(filename);
      ext.toLowerCase();
      if (ext == String("gz")) {
         String basename = file_basename_gz(filename);
         webServer.serveStatic(String(String("/")+basename).c_str(), FileFS, String(String("/w/")+filename).c_str());
      }
   }
}
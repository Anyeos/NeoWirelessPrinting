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

#define WIFI_RETRIES_DEFAULT 10
#define WIFI_WAIT_APMODE_SECONDS_DEFAULT 120
#define WIFI_WAIT_MILLIS_DEFAULT 1500

#define WIFI_SETTINGS_FILENAME "/wifisettings.dat"
#define WIFI_AP_DEFAULT_PSK DEFAULT_ADMIN_PASSWORD

#define WIFI_DEFAULT_SSID "HomeThing"
#define WIFI_DEFAULT_PSK ""
#define WIFI_DEFAULT_AP_MODE false
#define WIFI_DEFAULT_IP_AUTO true
#define WIFI_DEFAULT_IP ((uint32_t)IPADDR_ANY);
#define WIFI_DEFAULT_SUBNETMASK ((uint32_t)IPADDR_ANY);
#define WIFI_DEFAULT_GATEWAY ((uint32_t)IPADDR_ANY);


class ThingWiFi
{
  public:
    ThingWiFi() {
    };
    ~ThingWiFi() { };

    static String getChipId() {
        char str[16];
        sprintf(str, "%X", ESP_getChipId());
        return String(str);
    }

    void loadsettings(const String& filename = WIFI_SETTINGS_FILENAME) {
      _settings_file = filename;
      _settings Settings;
      File file = FileFS.open(_settings_file, "r");
      if (file) {
        file.read((uint8_t *)&Settings, sizeof(_settings));
        file.close();
        uint16_t checksum = calculate_checksum((uint8_t *)&Settings, sizeof(_settings));
        if (Settings.checksum == checksum) {
          _configured = true;
          _apmode = Settings.mode_ap;
          _ipauto = Settings.auto_ip;
          _ip = IPAddress(Settings.static_ip);
          _subnet = IPAddress(Settings.static_sn);
          _gateway = IPAddress(Settings.static_gw);
          _ssid = String(Settings.ssid);
          _psk = String(Settings.psk);
          Log.info("Wifi settings loaded successfully." CR);
        } else {
          Log.warning("Wrong wifi settings file checksum (expected %d, got %d)." CR, checksum, Settings.checksum);
        }
      } else {
        Log.warning("Cannot open wifi settings file." CR);
      }
    }

    void savesettings() {
      _settings Settings;
      memset((uint8_t *)&Settings, 0, sizeof(_settings));
      Settings.mode_ap = _apmode;
      Settings.auto_ip = _ipauto;
      Settings.static_ip = _ip;
      Settings.static_sn = _subnet;
      Settings.static_gw = _gateway;
      strncpy(Settings.ssid, _ssid.c_str(), 31);
      strncpy(Settings.psk, _psk.c_str(), 63);
      Settings.checksum = calculate_checksum((uint8_t *)&Settings, sizeof(_settings));
      File file = FileFS.open(_settings_file, "w");
      if (file) {
        file.write((uint8_t*) &Settings, sizeof(_settings));
        file.close();
        Log.info("Wifi settings saved successfully (checksum %d)." CR, Settings.checksum);
      } else {
        Log.error("Wifi settings file cannot be saved!" CR);
      }
    }

    void begin(const String& name = "HomeThing", bool withChipIDsuffix = true) {
      Log.info("Starting WiFi service." CR);
      Name = name;
      apmode_ssid = name;
      if (withChipIDsuffix)
      {
        apmode_ssid += "_"+getChipId();
        apmode_ssid.replace(" ", "");
      }
      if (!_configured)
        loadsettings(_settings_file);
      start(_ssid, _psk, _apmode, _ipauto);

      webServer.on("/wifiscan", HTTP_GET, [&](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        DynamicJsonDocument doc(2048);
        
        int networksFound = WiFi.scanComplete();
        if (networksFound > 0) {
          for (int8_t i = 0; i < networksFound; i++) {
            String ssid;
            int32_t rssi;
            uint8_t auth;
            uint8_t* bssid;
            int32_t channel;
            bool hidden = false;

            #ifdef ESP32
              WiFi.getNetworkInfo(i, ssid, auth, rssi, bssid, channel);
            #else
              WiFi.getNetworkInfo(i, ssid, auth, rssi, bssid, channel, hidden);
            #endif
            doc[i]["ssid"] = ssid;
            doc[i]["auth"] = auth;
            doc[i]["rssi"] = rssi;
            String BSSID =
              String(bssid[0], HEX)+":"+
              String(bssid[1], HEX)+":"+
              String(bssid[2], HEX)+":"+
              String(bssid[3], HEX)+":"+
              String(bssid[4], HEX)+":"+
              String(bssid[5], HEX);
            doc[i]["bssid"] = BSSID;
            doc[i]["channel"] = channel;
            //if (ssid.length()<=0) hidden = true;
            //doc[i].add(hidden);
          }
        }

        WiFi.scanNetworks(true, true);

        serializeJson(doc, *response);
        request->send(response);
      });

      webServer.on("/wifistatus", HTTP_GET, [&](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        DynamicJsonDocument doc(1024);
        doc["ssid"] = WiFi.SSID();
        doc["rssi"] = WiFi.RSSI();
        doc["ip"] = WiFi.localIP().toString();
        //doc["ip"] = current_ip.toString();
        doc["netmask"] = WiFi.subnetMask().toString();
        doc["gateway"] = WiFi.gatewayIP().toString();

        serializeJson(doc, *response);
        request->send(response);
      });

      if (!_configured) {
        Log.info("WiFi not configured, starting from Step 1..." CR);
        webServer.on("/", HTTP_GET | HTTP_POST, [&](AsyncWebServerRequest *request) {
          if (_restarting_wifi) {
            return;
          }

          const char *field_ssid = "ssid";
          const char *field_psk = "psk";

          _try_again = false; // Stop timeout because we entered to Settings

          String html;
          html = FPSTR(HTML_START);
          html += FPSTR(HTML_STYLE);
          html += FPSTR(HTML_SCRIPT);
          html += FPSTR(HTML_TITLE_BODY_START);
          html.replace(FPSTR(T_title), Name);
          
          String subhtml = FPSTR(HTML_CONTENT_START);
          subhtml.replace(FPSTR(T_title), "Step 1. Configure WiFi");
          html += subhtml;

          bool restart_wifi = false;
          if (!_configured) {
            _ssid = WiFi.SSID();
            _psk = WiFi.psk();
          }
          
          if (request->method() == HTTP_POST) {
            int params = request->params();
            //bool device_restart = false;
            _apmode = false;
            _ipauto = true;
            for(int i=0;i<params;i++){
              AsyncWebParameter* p = request->getParam(i);
              if ((strcmp(p->name().c_str(), field_ssid) == 0) && !_ssid.equals(p->value())) {
                //Log.notice("WiFi SSID changed from '%s' to '%s'" CR, _ssid.c_str(), p->value().c_str());
                _ssid = p->value();
                restart_wifi = true;
              }
              if ((strcmp(p->name().c_str(), field_psk) == 0) && !_psk.equals(p->value())) {
                //Log.notice("WiFi psk changed from '%s' to '%s'" CR, WiFi_psk, p->value().c_str());
                _psk = p->value();
                restart_wifi = true;
              }
            }
            savesettings();
            restart_wifi = true;

            /*if (device_restart) {
              restart_device();
              return;
            }*/
          }

          subhtml = FPSTR(HTML_FORM_START);
          subhtml.replace(FPSTR(T_action), "/");
          html += subhtml;

          subhtml = FPSTR(HTML_HEADER);
          subhtml.replace(FPSTR(T_label), "WiFi");
          html += subhtml;
          subhtml = FPSTR(HTML_INFO_START);
          subhtml.replace(FPSTR(T_name), "c");
          html += subhtml;
          
          subhtml = FPSTR(HTML_BAR_PERCENT);
          subhtml.replace(FPSTR(T_label), "Signal:");
          subhtml.replace(FPSTR(T_value), "0"/*String(((120+WiFi.RSSI())/120.0)*100.0)*/);
          html += subhtml;

          subhtml = FPSTR(HTML_INFO_FIELD);
          subhtml.replace(FPSTR(T_label), "SSID:");
          subhtml.replace(FPSTR(T_value), WiFi.SSID());
          html += subhtml;

          subhtml = FPSTR(HTML_INFO_FIELD2);
          subhtml.replace(FPSTR(T_label), "IP:");
          subhtml.replace(FPSTR(T_value), WiFi.localIP().toString());
          subhtml.replace(FPSTR(T_value2), WiFi.subnetMask().toString());
          html += subhtml;

          subhtml = FPSTR(HTML_INFO_FIELD);
          subhtml.replace(FPSTR(T_label), "Gateway:");
          subhtml.replace(FPSTR(T_value), WiFi.gatewayIP().toString());
          html += subhtml;

          html += FPSTR(HTML_INFO_END);

          html += FPSTR(HTML_SEPARATOR);

          subhtml = FPSTR(HTML_INFO_START);
          subhtml.replace(FPSTR(T_name), "s");
          html += subhtml;
          html += FPSTR(HTML_INFO_END);

          subhtml = FPSTR(HTML_FORM_INPUT);
          subhtml.replace(FPSTR(T_label), "SSID:");
          subhtml.replace(FPSTR(T_name), field_ssid);
          subhtml.replace(FPSTR(T_value), _ssid);
          html += subhtml;

          subhtml = FPSTR(HTML_FORM_INPUT_PASSWORD);
          subhtml.replace(FPSTR(T_label), "psk:");
          subhtml.replace(FPSTR(T_name), field_psk);
          subhtml.replace(FPSTR(T_value), _psk);
          html += subhtml;

          subhtml = FPSTR(HTML_FORM_END);
          subhtml.replace(FPSTR(T_submit), "Save");
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

          html += FPSTR(HTML_TOOLBAR_END);

          html += FPSTR(HTML_CONTENT_END);
          html += FPSTR(HTML_BODY_HTML_END);
          request->send(200, "text/html", html);

          if (restart_wifi) {
            Log.notice("Restarting WiFi with new settings..." CR);
            _restarting_wifi = true;
            start(_ssid, _psk, _apmode, _ipauto, true);
          }
        });
      }
      else
      webServer.on("/wificonfig", HTTP_POST | HTTP_GET, [&](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_GET)
        {
          AsyncResponseStream *response = request->beginResponseStream("application/json");
          DynamicJsonDocument doc(1024);
          doc["ssid"] = _ssid;
          doc["psk"] = _psk;
          doc["ap"] = _apmode;
          doc["static"] = !_ipauto;
          doc["ip"] = _ip.toString();
          doc["sn"] = _subnet.toString();
          doc["gw"] = _gateway.toString();
          serializeJson(doc, *response);
          request->send(response);
          return;
        }

        String url = "/";
        const char *redirect_param = "url";
        
        const char *field_ssid = "ssid";
        const char *field_psk = "psk";
        const char *field_apmode = "ap";
        const char *field_ip_static = "static";
        const char *field_ip = "ip";
        const char *field_subnet = "sn";
        const char *field_gateway = "gw";
        const char *field_admin_user = "admin_username";
        const char *field_admin_pwd = "admin_password";

        _try_again = false;

        bool restart_wifi = false;
        if (!_configured) {
          _ssid = WiFi.SSID();
          _psk = WiFi.psk();
        }
        
        int params = request->params();
        bool old_apmode = _apmode;
        bool old_ipauto = _ipauto;
        //bool device_restart = false;
        _apmode = false;
        _ipauto = true;
        for(int i=0;i<params;i++){
          AsyncWebParameter* p = request->getParam(i);
          if (strcmp(p->name().c_str(), redirect_param) == 0) {
            url = p->value();
          }

          if ((strcmp(p->name().c_str(), field_ssid) == 0) && !_ssid.equals(p->value())) {
            //Log.notice("WiFi SSID changed from '%s' to '%s'" CR, _ssid.c_str(), p->value().c_str());
            _ssid = p->value();
            restart_wifi = true;
          }
          if ((strcmp(p->name().c_str(), field_psk) == 0) && !_psk.equals(p->value())) {
            //Log.notice("WiFi psk changed from '%s' to '%s'" CR, WiFi_psk, p->value().c_str());
            _psk = p->value();
            restart_wifi = true;
          }

          if ((strcmp(p->name().c_str(), field_ip) == 0) && !_ip.toString().equals(p->value())) {
            //Log.notice("IP config changed from '%s' to '%s'" CR, _ip.toString(), p->value().c_str());
            _ip.fromString(p->value());
            restart_wifi = true;
          }
          if ((strcmp(p->name().c_str(), field_subnet) == 0) && !_subnet.toString().equals(p->value())) {
            //Log.notice("IP subnet config changed from '%s' to '%s'" CR, _ip.toString(), p->value().c_str());
            _subnet.fromString(p->value());
            restart_wifi = true;
          }
          if ((strcmp(p->name().c_str(), field_gateway) == 0) && !_gateway.toString().equals(p->value())) {
            //Log.notice("IP gateway config changed from '%s' to '%s'" CR, _ip.toString(), p->value().c_str());
            _gateway.fromString(p->value());
            restart_wifi = true;
          }

          if (strcmp(p->name().c_str(), field_apmode) == 0) {
            _apmode = !p->value().isEmpty();
          }

          if (strcmp(p->name().c_str(), field_ip_static) == 0) {
            _ipauto = p->value().isEmpty();
          }
        }
        //Log.notice("WiFi mode to %s" CR, _apmode?"AP":"STA");
        if (old_ipauto != _ipauto) {
          restart_wifi = true;
        }
        if (old_apmode != _apmode) {
          restart_wifi = true;
        }

        savesettings();
        /*if (device_restart) {
          delay(2000);
          ESP.restart();
          return;
        }*/

        request->redirect(url);

        if (restart_wifi) {
          Log.notice("Restarting WiFi with new settings..." CR);
          start(_ssid, _psk, _apmode, _ipauto, true);
        }
      });
    }

    void handle() {
      if (_try_again && _configured) {
        if (millis() - _last_wait > _wait) {
          _try_again = false;
          start(_ssid, _psk, _apmode, _ipauto, false);
        }
      }
    };

    bool isConfigured() { return _configured; }
    //void setConfigured ( bool yes ) { _configured = yes; }
    String SSID() { return _ssid; }
    String psk() { return _psk; }
    void restart_device() {
      Log.info("Restarting device..." CR);
      delay(2000);
      ESP.restart();
    }
    // TODO:
    // * Full factory reset, delete EEPROM WiFi settings too
    void factory_reset(bool save = true) {
      Log.info("WiFi factory RESET" CR);
      if (!save) {
        FileFS.remove(_settings_file);
        ESP.restart();
      } else {
        _ssid = WIFI_DEFAULT_SSID;
        _psk = WIFI_DEFAULT_PSK;
        _apmode = WIFI_DEFAULT_AP_MODE;
        _ipauto = WIFI_DEFAULT_IP_AUTO;
        _ip = WIFI_DEFAULT_IP;
        _subnet = WIFI_DEFAULT_SUBNETMASK;
        _gateway = WIFI_DEFAULT_GATEWAY;
        savesettings();
      }
    }

    IPAddress getCurrentIP() {
      if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP();
      }
      return WiFi.localIP();
    }

    void onConnect(std::function<void(void)> fn) {
      _connect_callback = fn;
    }

    static uint16_t calculate_checksum(uint8_t *buff, size_t len) {
      //Log.info("calculate_checksum with len: %d" CR, len);
      uint16_t checksum = 0;
      for (size_t c=0; c<(len-2); c++) {
        checksum = (checksum + buff[c]);
      }
      return checksum;
    }

  private:
    typedef struct __attribute__ ((__packed__)) {
      bool mode_ap;
      bool auto_ip;
      char ssid[31];
      char psk[63];
      uint32_t static_ip; // IPv4 address
      uint32_t static_sn; // SubNet
      uint32_t static_gw; // Gateway
      //char admin[31];
      //char admin_pwd[63];
      uint16_t checksum; // checksum always at the end
    } _settings;

    void start(String ssid = "", String psk = "", bool AP = false, bool autoIP = true, bool async = false)
    {
      if (WiFi.isConnected()) {
        WiFi.disconnect();
      }

      if (autoIP) {
        bool r;
        if (AP) {
          r = WiFi.softAPConfig(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));
        } else {
          r = WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));
        }
        if (!r) {
          Log.warning("WiFi config IP auto failed!" CR);
        } else {
          Log.info("WiFi auto IP mode (%s)." CR, WiFi.localIP().toString().c_str());
        }
      } else {
        Log.info("WiFi static IP mode (%s / %s gw %s)." CR,
            _ip.toString().c_str(), 
            _subnet.toString().c_str(), 
            _gateway.toString().c_str());
        bool r;
        if (AP) {
          r = WiFi.softAPConfig(_ip, _gateway, _subnet);
        } else {
          r = WiFi.config(_ip, _gateway, _subnet);
        }
        if (!r) {
          Log.warning("WiFi config IP static (%s / %s gw %s) failed!" CR, 
            _ip.toString().c_str(), 
            _subnet.toString().c_str(), 
            _gateway.toString().c_str());
        } else {
          Log.info("WiFi static IP mode (%s)." CR, WiFi.localIP().toString().c_str());
        }
      }

      if (ssid.isEmpty()) {
        Log.warning("WiFi ssid is empty!" CR);
        if (_apmode) {
          WiFi.mode(WIFI_AP);
        } else {
          WiFi.mode(WIFI_STA);
          WiFi.begin();
        }
      } else if (AP) {
        Log.info("WiFi AP mode with SSID: %s" CR, ssid.c_str());
        WiFi.persistent(true);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(ssid.c_str(), psk.c_str());
        Log.info("WiFi AP mode SSID:'%s', psk:'%s'" CR, ssid.c_str(), psk.c_str());
        //Log.info("Local IP: %s" CR, WiFi.softAPIP().toString().c_str());
        return;
      } else {
        Log.info("WiFi STA mode with SSID: %s" CR, ssid.c_str());
        WiFi.persistent(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), psk.c_str());
      }

      if (async) { 
        _try_again = true;
        _last_wait = 0;
        _wait = WIFI_WAIT_MILLIS_DEFAULT;
        _configured = true;
        return;
      }

      int retries = _retries;
      while ((WiFi.status() != WL_CONNECTED) && (retries > 0)) {
        delay(1000);
        Log.info("WiFi connecting to '%s' (%d)..." CR, WiFi.SSID().c_str(), retries);
        retries--;
      }

      if (WiFi.isConnected()) {
        Log.info("WiFi connected to '%s'" CR, WiFi.SSID().c_str());
        //_configured = false;
        Log.info("Local IP: %s" CR, WiFi.localIP().toString().c_str());
        if (_connect_callback) _connect_callback();
        
        // WiFi restarted successfully from configuration so we can proceed with Step 2.
        if (_restarting_wifi) {
          // FIXME: Calling it here crash the ESP but does not matter because we want to restart it anyway
          restart_device();
          return;
        }
      } else {
        Log.notice("AP mode at SSID:'%s', psk:'%s' will wait %d seconds and try again." CR, apmode_ssid.c_str(), WIFI_AP_DEFAULT_PSK, WIFI_WAIT_APMODE_SECONDS_DEFAULT);
        WiFi.softAPConfig(IPAddress(), IPAddress(), IPAddress());
        WiFi.mode(WIFI_AP);
        WiFi.softAP(apmode_ssid.c_str(), WIFI_AP_DEFAULT_PSK);
        _try_again = true;
        _last_wait = millis();
        _wait = WIFI_WAIT_APMODE_SECONDS_DEFAULT*1000;
        Log.info("Local IP: %s" CR, WiFi.softAPIP().toString().c_str());
      }
    };

    String Name;
    String _settings_file = WIFI_SETTINGS_FILENAME;
    String apmode_ssid;
    String _ssid = WIFI_DEFAULT_SSID;
    String _psk = WIFI_DEFAULT_PSK;
    bool _configured = false;
    bool _try_again = false;
    unsigned long _last_wait = 0;
    int _wait = WIFI_WAIT_MILLIS_DEFAULT;
    int _retries = WIFI_RETRIES_DEFAULT;
    bool _apmode = WIFI_DEFAULT_AP_MODE;
    bool _ipauto = WIFI_DEFAULT_IP_AUTO;
    bool _restarting_wifi = false;
    //IPAddress current_ip = IP_ADDR_ANY;
    IPAddress _ip = WIFI_DEFAULT_IP;
    IPAddress _subnet = WIFI_DEFAULT_SUBNETMASK;
    IPAddress _gateway = WIFI_DEFAULT_GATEWAY;
    std::function<void(void)> _connect_callback = nullptr;
};


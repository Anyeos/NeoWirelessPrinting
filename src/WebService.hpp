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

#include "ESPAsyncWebServer.h" // https://github.com/philbowles/ESPAsyncWebServer
#include <ArduinoLog.h>
#include "htmlstrings.h"
//#include "AsyncJson.h"
#include "ArduinoJson.h"

AsyncWebServer webServer(HTTP_PORT);
AsyncEventSource webEvents("/events");

/*
  Atención: El chip no está preparado para SSL, así que se espera ralentizamientos y fallas.
  Sólo el SDK 2.x.x soporta SSL, versiones 3.x para arriba no traen 'include/ssl.h'.
*/
#if ASYNC_TCP_SSL_ENABLED
AsyncWebServer webServer80(80);
#endif


//Mitigating: https://github.com/me-no-dev/ESPAsyncWebServer/issues/984
/*size_t onStaticDownLoad(uint8_t *buffer, size_t maxLen, size_t index){

    static  char *   filename;                 // file to be sent as chunks
    static  File     chunkfile;                // file handle of filename
    static  size_t   dnlLen     = 1028;        // limit to buffer size
            uint32_t countbytes = 0;           // bytes to be returned

    if (index == 0)  {
        if (staticDataType == CAM)  filename    = LOGCAM;
        else                        filename    = LOGCPS;
        samm->printf("onStaticDownLoad: START filename: %s index: %6i maxLen: %5i\n", filename, index, maxLen);
        chunkfile       = myFS->open(filename);
    }

    chunkfile.seek(index);
    countbytes = chunkfile.read(buffer, min(dnlLen, maxLen));

    if (countbytes == 0) {
        chunkfile.close();
    }
    samm->printf("onStaticDownLoad: f: %s index: %6u maxLen: %5u countb: %5u \n"
                    , filename
                    , index
                    , maxLen
                    , countbytes
                );

    return countbytes;
}*/


bool WebService_begin()
{
  Log.info("Starting web service." CR);

  webServer.onNotFound([](AsyncWebServerRequest *request){
    String str_method;
    if(request->method() == HTTP_GET)
      str_method = "GET";
    else if(request->method() == HTTP_POST)
      str_method = "POST";
    else if(request->method() == HTTP_DELETE)
      str_method = "DELETE";
    else if(request->method() == HTTP_PUT)
      str_method = "PUT";
    else if(request->method() == HTTP_PATCH)
      str_method = "PATCH";
    else if(request->method() == HTTP_HEAD)
      str_method = "HEAD";
    else if(request->method() == HTTP_OPTIONS)
      str_method = "OPTIONS";
    else
      str_method = "UNKNOWN";
    
    Log.info("NOT_FOUND: %s http://%s%s" CR, str_method.c_str(), request->host().c_str(), request->url().c_str());

    /*
    if(request->contentLength()){
      Log.info("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Log.info("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Log.info("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Log.info("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Log.info("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Log.info("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    */

    request->send(404);
  });

#if !ASYNC_TCP_SSL_ENABLED
  webServer.begin();
#else
  const char* server_cer = "/server.cer";
  const char* server_key = "/server.key";
  webServer.onSslFileRequest([](void * arg, const char *filename, uint8_t **buf) -> int {
    //Log.info("SSL File: %s\n", filename);
    File file = FileFS.open(filename, "r");
    if(file){
      size_t size = file.size();
      uint8_t * nbuf = (uint8_t*)malloc(size);
      if(nbuf){
        size = file.read(nbuf, size);
        file.close();
        *buf = nbuf;
        return size;
      }
      Log.error("Cannot allocate memory for server certificate: %s (%d bytes)" CR, filename, size);
      file.close();
    }
    *buf = 0;
    Log.error("Server certificate file cannot be opened: %s" CR, filename);
    return 0;
  }, NULL);

  if (FileFS.exists(server_cer) && FileFS.exists(server_key)) {
    webServer.beginSecure(server_cer, server_key, NULL);
  } else {
    webServer80.on("/", HTTP_GET,[&](AsyncWebServerRequest *request) {
      const char* field_files = "files";

      String html;
      html = FPSTR(HTML_START);
      html += FPSTR(HTML_STYLE);
      html += FPSTR(HTML_SCRIPT);
      html += FPSTR(HTML_TITLE_BODY_START);
      html.replace(FPSTR(T_title), "Web Server certificates");

      String subhtml = FPSTR(HTML_CONTENT_START);
      subhtml.replace(FPSTR(T_title), "Upload Web Server certificate files.");
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

      html += "<ul class='block'>";
      const char *files[2] = {"server.cer", "server.key"};
      String root_directory = "/";
      uint8_t rest = 2;
      for (uint8_t i=0; i<2; i++) {
          subhtml = FPSTR(HTML_FILE_UPLOADED);
          subhtml.replace(FPSTR(T_name), files[i]);
          if (!FileFS.exists(root_directory+String(files[i]))) {
            subhtml.replace(FPSTR(T_field), "need to upload");
            subhtml.replace(FPSTR(T_value), "color: #aeaeae; background: black;");
          } else {
            rest--;
            subhtml.replace(FPSTR(T_field), FileFShumanFileSize(FileFSfilesize(root_directory+String(files[i]))));
            subhtml.replace(FPSTR(T_value), "color: #444");
          }
          html += subhtml;
      }
      html += "</ul>";
      if (rest > 0) {
          subhtml = "<p>{v} file(s) need to be uploaded.</p>";
          subhtml.replace(FPSTR(T_value), String(rest));
          html += subhtml;
      } else {
          html += "<p>All certificate files uploaded. Now you can <a onclick='restart();'>RESTART</a>.</p>";
      }

      FSInfo fsinfo;
      FileFS.info(fsinfo);

      subhtml = FPSTR(HTML_INFO_RIGHT_START);
      subhtml.replace(FPSTR(T_name), "fsinfo");
      html += subhtml;

      subhtml = FPSTR(HTML_INFO_FIELD);
      subhtml.replace(FPSTR(T_label), "free space:");
      subhtml.replace(FPSTR(T_value), FileFShumanFileSize(fsinfo.totalBytes-fsinfo.usedBytes).c_str());
      html += subhtml;

      html += FPSTR(HTML_SEPARATOR);

      subhtml = FPSTR(HTML_TOOLBUTTON);
      subhtml.replace(FPSTR(T_action), "restart()");
      subhtml.replace(FPSTR(T_label), "RESTART");
      html += subhtml;

      html += FPSTR(HTML_CONTENT_END);
      html += FPSTR(HTML_BODY_HTML_END);
      request->send(200, "text/html", html);
    }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      String root_directory = "/";
      if(!index) {
        Log.info("Upload started: %s" CR, String(root_directory+filename).c_str());
        FileFS.remove(root_directory+filename);
      }
      
      fs::File f = FileFS.open(root_directory+filename, "a");
      for (size_t i=0; i<len; i++)
      {
        f.write(data[i]);
      }
      f.close();

      if(final) {
        Log.info("Upload ended: %s (%u)" CR, String(root_directory+filename).c_str(), index+len);
      }
    });

    webServer80.begin();
    MDNS.addService("http","tcp",80);
    return false;
  }
#endif
  MDNS.addService("http","tcp",HTTP_PORT);
  return true;
};

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

/* 
  Código básico para tener una mínima interfaz web para poder iniciar la instalación
*/

#ifndef _HTMLSTRINGS_H
#define _HTMLSTRINGS_H

static const char T_title[] PROGMEM = "{t}";
static const char T_action[] PROGMEM = "{a}";
static const char T_label[] PROGMEM = "{l}";
static const char T_value[] PROGMEM = "{v}";
static const char T_value2[] PROGMEM = "{V}";
static const char T_name[] PROGMEM = "{n}";
static const char T_field[] PROGMEM = "{f}";
static const char T_submit[] PROGMEM = "{s}";
static const char T_number[] PROGMEM = "{N}";

static const char HTML_START[] PROGMEM = R"=====(<!DOCTYPE html>
<html lang='en'>
<head>
<meta name = 'viewport' content = 'width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0'>
<meta name='format-detection' content='telephone=no'>
<meta charset='UTF-8'>)=====";

static const char HTML_STYLE[] PROGMEM =
"<style>" CR
// Base styling
"body {background-color: #808080; color: #fff; font-family: Arial, Helvetica, sans-serif;}" CR
"p {font-family: Arial, Helvetica, sans-serif; margin: 2px;}" CR
".content {margin: 10px; border: 1px solid #c6c6c6; text-align: center;}" CR
// Form / Information blocks
"form, .block {background: #a8a8a8; color: #000; display: inline-block; padding: 10px; box-shadow: 0px 0px 10px #555; border: 2px solid #ddd;vertical-align: top;}" CR
".block {display: block; border: none;}" CR
"form p, li, .info p, .infor p {display: flex; flex-wrap: wrap; text-align: left;}" CR
".infor p {text-align: right;}" CR
".info, .infor {display: inline-block; padding: 5px;min-width: 16em;}" CR
// Lists / lists forms
"p label, p input, p span, li span {flex: 1 0 0; max-width: 100%;}" CR
"li a, li b {flex: 0 0 41.666667%; max-width: 41.666667%;}" CR
"label, span, li a, li b {padding-right: 0.5em;}" CR
"ul {padding: 0;}" CR
// Headings
"h1, h2, h3, h4, h5 {margin: 4px;}" CR
// Percent bar
".bar-p {display: inline-block;background-color: #a0a0a0;width: 80%;}" CR
".bar-p-val {background-color: #fff;color: #000;padding: 2px; text-align: right;}" CR
"</style>" CR;

static const char HTML_SCRIPT[] PROGMEM = R"=====(
<script>
function format() {
  var str = prompt('Format and restart from Step 1? (write \"FORMAT\" to confirm)');
  window.location.href='/format?format='+str;
}

function deleteall() {
  if (confirm('Delete all files?'))
  {
     window.location.href='/?d=DELETEALL';
  }
}

function restart() {
  if (confirm('Restart device?'))
  {
     window.location.href='/restart?r=RESTART';
  }
}

function toggleInputPassword(n) {
  var x = document.getElementById(n);
  if (x.type === 'password') {
    x.type = 'text';
  } else {
    x.type = 'password';
  }
}

function chooseSSID(x, n) {
  var y = document.getElementsByName(n);
  y[0].value = x.innerHTML;
}

async function wifiupdate() {
  const fetchSt = await fetch('/wifistatus');
  const st = await fetchSt.json();

  const l = document.getElementsByClassName('bar-p-val');
  var rssi = Math.round(((90+st['rssi'])/90.0)*100)+'%';
  l[0].innerText=rssi;
  l[0].style.width=rssi;

  const fetchR = await fetch('/wifiscan');
  const doc = await fetchR.json();
  
  const s = document.getElementById('s');
  var html = '<ul>';
  doc.forEach( function(i) {
    html += '<li>';
    html += '<b><a href="#" onclick="chooseSSID(this, \'ssid\')">'+i.ssid+'</a></b>';
    html += '<span>'+Math.round(((90+i.rssi)/90.0)*100)+'%</span>';
    html += '<span>'+i.bssid+'</span>';
    var auth = '*';
    if (i.auth == 7) {
      auth = ' ';
    }
    html += '<span>'+auth+'</span>';
    html += '</li>';
  });
  html += '</ul>';
  s.innerHTML = html;
}

function autorun() {
  wifiupdate();
  setInterval(wifiupdate, 5000);
}
</script>)=====";


static const char HTML_TITLE_BODY_START[] PROGMEM = 
"<title>{t}</title>"
"</head><body onload='autorun()'>";
static const char HTML_CONTENT_START[] PROGMEM = "<div class='content'><h3>{t}</h3>";
static const char HTML_CONTENT_END[] PROGMEM = "</div>";
static const char HTML_BODY_HTML_END[] PROGMEM = "</body></html>";

//static const char HTML_NAVBAR_START[] PROGMEM = "<a href='#navbar' class='navbar-button'></a> <a id='brand' href='/'>{t}</a> <nav id='navbar' class='navbar'><ul>";
//static const char HTML_NAVBAR_ITEM[] PROGMEM = "<li><a href='{v}'>{l}</a></li>";
//static const char HTML_NAVBAR_END[] PROGMEM = "</ul></nav>";

static const char HTML_HEADER[] PROGMEM = "<h4>{l}</h4>";

static const char HTML_INFO_START[] PROGMEM = "<div id='{n}' class='info'>";
static const char HTML_INFO_RIGHT_START[] PROGMEM = "<div id='{n}' class='infor'>";
static const char HTML_INFO_FIELD[] PROGMEM = "<p><span>{l}</span><span>{v}</span></p>";
static const char HTML_INFO_FIELD2[] PROGMEM = "<p><span>{l}</span><span>{v} / {V}</span></p>";
static const char HTML_INFO_FIELD_ID[] PROGMEM = "<p><span>{l}</span><span id='{n}'>{v}</span></p>";
static const char HTML_INFO_END[] PROGMEM = "</div>";

static const char HTML_FORM_START[] PROGMEM = "<form action='{a}' method='POST'>";
static const char HTML_FORM_MULTIPART_START[] PROGMEM = "<form action='{a}' method='POST' enctype='multipart/form-data'>";
static const char HTML_FORM_UPLOAD[] PROGMEM = "<p><label for='{n}'>{l}</label><input type='file' name='{n}' value='{v}' multiple='multiple'></p>";
static const char HTML_FORM_INPUT[] PROGMEM = "<p><label for='{n}'>{l}</label><input type='text' name='{n}' value='{v}'><span></span></p>";
static const char HTML_FORM_INPUT_PASSWORD[] PROGMEM = "<p><label for='{n}'>{l}</label><input type='password' name='{n}' id='{n}' value='{v}'><input type='checkbox' onclick=\"toggleInputPassword('{n}')\"></p>";
static const char HTML_FORM_INPUT_CHECKBOX[] PROGMEM = "<p><label for='{n}'>{l}</label><input type='checkbox' name='{n}' {v}><span></span></p>";
//static const char HTML_FORM_INPUT_TOGGLE[] PROGMEM = "<p><label for='{n}'>{l}</label><input type='checkbox' onclick=\"toggleField('{n}', '{f}')\" name='{n}' {v}><span></span></p>";
static const char HTML_SEPARATOR[] PROGMEM = "<hr/>";
static const char HTML_FORM_END[] PROGMEM = "<p><input type='submit' value='{s}'></p></form>";

// Extra
static const char HTML_BAR_PERCENT[] PROGMEM = "<span>{l}</span><div class='bar-p'><div class='bar-p-val' style='width: {v}%;'>{v}%</div></div>";

static const char HTML_TOOLBAR_START[] PROGMEM = "<div>";
static const char HTML_TOOLBUTTON[] PROGMEM = "<span><button onclick=\"{a};\">{l}</button></span>";
static const char HTML_TOOLBAR_END[] PROGMEM = "</div>";

//static const char HTML_SUBHEADER[] PROGMEM = "<h5>{l}</h5>";
static const char HTML_FILE_UPLOADED[] PROGMEM = "<li style='{v}'><span>{n}</span> <span>{f}</span></li>";

#endif
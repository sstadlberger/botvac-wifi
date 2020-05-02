#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include <Hash.h>

#define SSID "XXX"
#define PASSWORT "XXX"

WiFiClient client;
int maxBuffer = 8192;
int bufferSize = 0;
uint8_t currentClient = 0;
uint8_t serialBuffer[8193];
ESP8266WebServer server (80);
WebSocketsServer webSocket = WebSocketsServer(81);

void botDissconect() {
  // always disable testmode on disconnect
  Serial.println("TestMode off");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  switch (type) {
    case WStype_DISCONNECTED:
      // always disable testmode on disconnect
      botDissconect();
      break;
    case WStype_CONNECTED:
      webSocket.sendTXT(num, "connected to Neato\x1A");
      // allow only one concurrent client connection
      currentClient = num;
      // all clients but the last connected client are disconnected
      for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (i != currentClient) {
          webSocket.disconnect(i);
        }
      }
      // reset serial buffer on new connection to prevent garbage
      serialBuffer[0] = '\0';
      bufferSize = 0;
      break;
    case WStype_TEXT:
      // send incoming data to bot
      Serial.printf("%s\n", payload);
      break;
    case WStype_BIN:
      webSocket.sendTXT(num, "binary transmission is not supported");
      break;
  }
}

void serverEvent() {
  // just a very simple websocket terminal, feel free to use a custom one
  server.send(200, "text/html", "<!DOCTYPE html><meta charset='utf-8' /><style>p{white-space:pre;word-wrap:break-word;font-family:monospace;}</style><title>Neato Console</title><script language='javascript' type='text/javascript'>var b='ws://'+location.hostname+':81/',c,d,e,x='';function g(){d=new WebSocket(b);d.onopen=function(){h('[connected]')};d.onclose=function(){h('[disconnected]')};d.onmessage=function(a){x+=a.data;if(x.slice(-1)==\"\\x1a\"){h('<span style=\"color: blue;\">[response]  '+x.replace(/(\\r\\n|\\n|\\r)/g,\"\\n            \").replace(/\\s{13}\\x1a$/,'')+'</span>');}};d.onerror=function(a){h('<span style=\"color: red;\">[error]     </span> '+a.data)}}\nfunction k(a){if(13==a.keyCode){a=e.value;if('/disconnect'==a)d.close();else if('/clear'==a)for(;c.firstChild;)c.removeChild(c.firstChild);else''!=a&&(h('[sent]      '+a),d.send(a));e.value='';e.focus()}}function h(a){x='';var f=document.createElement('p');f.innerHTML=a;c.appendChild(f);window.scrollTo(0,document.body.scrollHeight)}\nwindow.addEventListener('load',function(){c=document.getElementById('c');e=document.getElementById('i');g();document.getElementById('i').addEventListener('keyup',k,!1);e.focus()},!1);</script><h2>Neato Console</h2><div id='c'></div><input type='text' id='i' style=\"width:100%;font-family:monospace;\">\n");
}

void serialEvent() {
  while (Serial.available() > 0) {
    char in = Serial.read();
    // there is no propper utf-8 support so replace all non-ascii
    // characters (<127) with underscores; this should have no
    // impact on normal operations and is only relevant for non-english
    // plain-text error messages
    if (in > 127) {
      in = '_';
    }
    serialBuffer[bufferSize] = in;
    bufferSize++;
    // fill up the serial buffer until its max size (8192 bytes, see maxBuffer)
    // or unitl the end of file marker (ctrl-z; \x1A) is reached
    // a worst caste lidar result should be just under 8k, so that maxBuffer
    // limit should not be reached under normal conditions
    if (bufferSize > maxBuffer - 1 || in == '\x1A') {
      serialBuffer[bufferSize] = '\0';
      webSocket.sendTXT(currentClient, serialBuffer);
      serialBuffer[0] = '\0';
      bufferSize = 0;
    }
  }
}

void setup() {
  // start serial
  // botvac serial console is 115200 baud, 8 data bits, no parity, one stop bit (8N1)
  Serial.begin(115200);

  // start wifi
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORT);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  ArduinoOTA.setHostname("neato");
  ArduinoOTA.setPassword("neato");
  ArduinoOTA.begin();

  // start websocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // start webserver
  server.on("/", serverEvent);
  server.onNotFound(serverEvent);
  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  webSocket.loop();
  server.handleClient();
  serialEvent();
}

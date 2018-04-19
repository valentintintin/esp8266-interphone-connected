#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266SAM.h>
#include <AudioOutputI2SNoDAC.h>

#include "Timer.h"

#define DIRECT_CO

#ifndef DIRECT_CO
#include <WiFiManager.h>
#endif

#define TIME_ON 5000
#define RELAY 2

AudioOutputI2SNoDAC *out;
ESP8266SAM *sam = new ESP8266SAM;

const char plainText[] PROGMEM = {"text/html"};
const char pageHTML[] PROGMEM = {"<!DOCTYPE html><html><head><title>Interphone - CURIOUS</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><meta http-equiv=\"refresh\" content=\"%d;URL=/\"><style type=\"text/css\">body{text-align:center}.on{background:green!important}.off{background:red!important}.interphone{margin:auto;display:flex;align-items:center;justify-content:center;width:150px;height:150px;border-radius:35%%;color:white;font-weight:bold;font-size:20px;background:gray;}a,a:visited{font-size:20px;color:#fff;text-decoration:none}a:hover{font-size:25px;cursor:pointer}</style></head><body><h1>Interphone - CURIOUS</h1><a class=\"%s interphone\" href=\"/open\">Ouvrir</a><div style=\"margin-top:100px;\">Mémoire libre : %d Ko</div></body></html>"};
char page[2048] = "";
bool lastState = false;

AsyncWebServer server(80);
Timer timer(TIME_ON);

void setup() { 
  Serial.begin(115200);
  Serial.println(F("Start"));
  
  //out = new AudioOutputI2SNoDAC();
  //out->begin();
  
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  #ifndef DIRECT_CO
  WiFiManager wifiManager;
  if(!wifiManager.autoConnect("CURIOUS-INTERPHONE")) {
    Serial.println("Connexion impossible, reset !");
    ESP.reset();
    delay(1000);
  } 
  #else
  WiFi.mode ( WIFI_STA );
  WiFi.begin ( "Valentin", "0613979414" );
  //WiFi.begin ( "curieux_wifi", "curious7ruesaintjoseph" );
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );  }
  #endif
  Serial.println ( WiFi.localIP() );
  
  server.onNotFound([](AsyncWebServerRequest *request){ handleWebServerRequests(request); });
  server.on("/open", HTTP_ANY, [](AsyncWebServerRequest *request){ handleWebServerRequestsOpen(request); });
  server.begin();
  
  timer.setExpired();
  
  Serial.println(F("OK"));
}

void loop() {
  if (ESP.getFreeHeap() < 1000) {
    Serial.println(F("Plus de mémoire !"));
    ESP.restart();
  }
  
  if (lastState != shouldOpen()) {
    lastState = shouldOpen();
    if (lastState) {
      //sam->SetPhonetic(true);
      //ɡʁup kyʁju tʁwaksjɛm etaʒ
      //sam->Say(out, "Groupe Curious, troixième étage");
      Serial.println(F("Open"));
    }
    digitalWrite(RELAY, lastState);
  }

  delay(10);
}

void handleWebServerRequests(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    request->send(200, plainText, timer.hasExpired() ? F("0") : F("1"));
  } else {
    sprintf_P(page, pageHTML, TIME_ON / 1000, timer.hasExpired() ? "off" : "on");
    request->send(200, plainText, page);
  }
}

void handleWebServerRequestsOpen(AsyncWebServerRequest *request) {
  timer.restart();
  handleWebServerRequests(request);
}

bool shouldOpen() {
  return !timer.hasExpired();
}


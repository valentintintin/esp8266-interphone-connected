#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Timer.h"

#define DIRECT_CO
#define VOICE

#ifndef DIRECT_CO
#include <WiFiManager.h>
#endif

#ifdef VOICE
  #include <ESP8266SAM.h>
  #include <AudioOutputI2SNoDAC.h>

  AudioOutputI2SNoDAC *out;
  ESP8266SAM *sam = new ESP8266SAM;
#endif

#define TIME_ON 5000
#define RELAY 2

const char pageHTML[] PROGMEM = {"<!DOCTYPE html><html><head><title>Interphone - CURIOUS</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><meta http-equiv=\"refresh\" content=\"%d;URL=/\"><style type=\"text/css\">body{text-align:center}.on{background:green!important}.off{background:red!important}.interphone{margin:auto;display:flex;align-items:center;justify-content:center;width:150px;height:150px;border-radius:35%%;color:white;font-weight:bold;font-size:20px;background:gray;}a,a:visited{font-size:20px;color:#fff;text-decoration:none}a:hover{font-size:25px;cursor:pointer}</style></head><body><h1>Interphone - CURIOUS</h1><a class=\"%s interphone\" href=\"/open\">Ouvrir</a><div style=\"margin-top:100px;\">Memoire libre : %d Ko</div></body></html>"};
char page[2048] = "";
char mimeHtml[] = "text/html";
bool isDoorOpen = false;
bool mustOpenDoor = false;

AsyncWebServer server(80);
Timer timer(TIME_ON);

void setup() { 
  Serial.begin(115200);
  Serial.println(F("Start"));

  #ifdef VOICE
    out = new AudioOutputI2SNoDAC();
    out->begin();
  #endif
  
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  #ifndef DIRECT_CO
    WiFiManager wifiManager;
    if(!wifiManager.autoConnect("CURIOUS-INTERPHONE")) {
      Serial.println("No connexion, reset !");
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
    Serial.println(F("No more memery, reset !"));
    ESP.restart();
  }

  checkDoor();

  delay(10);
}

void handleWebServerRequests(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    request->send(200, mimeHtml, mustOpenDoor || isDoorOpen ? F("1") : F("0"));
  } else {
    sprintf_P(page, pageHTML, TIME_ON / 1000, mustOpenDoor || isDoorOpen ? "on" : "off", ESP.getFreeHeap());
    request->send(200, mimeHtml, page);
  }
}

void handleWebServerRequestsOpen(AsyncWebServerRequest *request) {
  mustOpenDoor = true;
  handleWebServerRequests(request);
}

void checkDoor() {
  if (mustOpenDoor && !isDoorOpen) {
    Serial.println(F("Open"));
    isDoorOpen = true;
    mustOpenDoor = false;
    #ifdef VOICE
      sam->SetPhonetic(false);
      //sam->Say(out, "Groupe Curious. étage trois");
      sam->SetPhonetic(true);
      //ɡʁup kyʁju tʁwaksjɛm etaʒ
      sam->Say(out, "GRUH4PEH KUXRIH3UHS");
    #endif
    timer.restart();
    digitalWrite(RELAY, HIGH);
  }
  
  if (!mustCloseDoor() && isDoorOpen) {
    Serial.println(F("Close"));
    isDoorOpen = false;
    digitalWrite(RELAY, LOW);
  }
}

bool mustCloseDoor() {
  return timer.hasExpired();
}


#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Timer.h"

#define DIRECT_CO
//#define VOICE

#ifndef DIRECT_CO
  #include <WiFiManager.h> // Conflict with WebServer
#endif

#ifdef VOICE
  #include <ESP8266SAM.h>
  #include <AudioOutputI2SNoDAC.h>

  AudioOutputI2SNoDAC *out;
  ESP8266SAM *sam = new ESP8266SAM;
#endif

#define TIME_ON 5000
#define RELAY 2

const char pageHTML[] PROGMEM = {"<!DOCTYPE html><title>Interphone - CURIOUS</title><meta content=\"width=device-width,initial-scale=1\"name=viewport><script crossorigin=anonymous integrity=\"sha256-FgpCb/KJQlLNfOu91ta32o/NMZxltwRo8QtmkMRdAu8=\"src=https://code.jquery.com/jquery-3.3.1.min.js></script><script>$(function() { const ESP = \"\"; const interphoneElement = $(\"#interphone\"); interphoneElement.click(function() { $.post(ESP + \"open\", function(state) { refreshHtml(state); }, \"json\").fail(function() { error(); }); }); function getStatus() { $.post(ESP + \"\", function(state) { refreshHtml(state); }, \"json\").fail(function() { error(); }); } function refreshHtml(state) { if (state) { interphoneElement.removeClass(\"off\"); interphoneElement.addClass(\"on\"); interphoneElement.html(\"Ouvert !\"); interphoneElement.attr(\"disabled\", true); } else { interphoneElement.removeClass(\"on\"); interphoneElement.addClass(\"off\"); interphoneElement.text(\"Ouvrir\"); interphoneElement.removeAttr(\"disabled\"); } } function error() { interphoneElement.removeClass(\"on\"); interphoneElement.removeClass(\"off\"); interphoneElement.html(\"Erreur !\"); interphoneElement.attr(\"disabled\", true); } getStatus(); setInterval(function() { getStatus() }, 1500); });</script><style>body{text-align:center}.on{background:green!important}.off{background:red!important}#interphone{margin:auto;display:flex;align-items:center;justify-content:center;width:150px;height:150px;border-radius:35%;color:#fff;font-weight:700;font-size:20px;background:gray;border:0}#interphone:hover:enabled{font-size:25px;cursor:pointer}</style><h1>Interphone - CURIOUS</h1><button autofocus id=interphone type=button></button>"};
const char trueStr[] PROGMEM = {"true"};
const char falseStr[] PROGMEM = {"false"};
char page[2048] = "";
char mimeHtml[] = "text/html";
char mimeJson[] = "application/json";
bool isDoorOpen = false;
bool mustOpenDoor = false;

AsyncWebServer server(80);
Timer timer(TIME_ON);

void setup() { 
  Serial.begin(115200);
  Serial.println(F("Start, connexion to curious"));

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
      ESP.restart();
    } 
  #else
    WiFi.mode(WIFI_STA);
    WiFi.begin("curieux_wifi", "curious7ruesaintjoseph");
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println(F("No connexion curious, try Valentin !"));
      WiFi.begin ( "Valentin", "0613979414" );
      if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println(F("No connexion, reset !"));
        ESP.restart();
      }
    }
  #endif
  Serial.println (WiFi.localIP());
  
  server.onNotFound(handleRequests);
  server.on("/open", HTTP_ANY, handleRequestsOpen);
  server.on("/infos", HTTP_GET, handleRequestsInfos);
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, mimeHtml, trueStr); ESP.restart(); });
  server.on("/update", HTTP_POST, endUploadProgram, handleRequestsUploadProgram);
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
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

void handleRequests(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    request->send_P(200, mimeJson, mustOpenDoor || isDoorOpen ? trueStr : falseStr);
  } else {
    request->send_P(200, mimeHtml, pageHTML);
  }
}

void handleRequestsOpen(AsyncWebServerRequest *request) {
  mustOpenDoor = true;
  handleRequests(request);
}

void handleRequestsInfos(AsyncWebServerRequest *request) {
  sprintf_P(page, PSTR("{\"mem\":%d,\"uptime\":%lu}"), ESP.getFreeHeap(), millis() / 1000);
  request->send(200, mimeJson, page);
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
  
  if (mustCloseDoor() && isDoorOpen) {
    Serial.println(F("Close"));
    isDoorOpen = false;
    digitalWrite(RELAY, LOW);
  }
}

bool mustCloseDoor() {
  return timer.hasExpired();
}

void handleRequestsUploadProgram(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    Serial.println(F("UploadStart"));
    Serial.setDebugOutput(true);

    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if(!Update.begin(maxSketchSpace)){ //start with max available size
      Update.printError(Serial);
    }
  }
  
  if(Update.write(data, len) != len){
    Update.printError(Serial);
  }
  
  if (final){
    Serial.println(F("UploadEnd"));
  
    if(Update.end(true)){ // true to set the size to the current progress
      Serial.println(F("Update Success"));
    } else {
      Update.printError(Serial);
    }
    
    Serial.setDebugOutput(false);
  }
}

void endUploadProgram(AsyncWebServerRequest *request) {
    Serial.println(F("Reset"));
    request->send_P(200, mimeJson, Update.hasError() ? falseStr : trueStr);
    ESP.restart();
}

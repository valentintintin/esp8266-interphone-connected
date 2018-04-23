#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Timer.h"

#define DIRECT_CO
//#define VOICE
//#define MP3

#ifndef DIRECT_CO
  #include <WiFiManager.h> // Conflict with WebServer
#endif


#if defined(VOICE) || defined(MP3)
  #include <AudioOutputI2SNoDAC.h>
  AudioOutputI2SNoDAC *out;
#endif
#ifdef VOICE
  #include <ESP8266SAM.h>
  
  ESP8266SAM *sam = new ESP8266SAM;
#endif
#ifdef MP3
  #include "message.h"
  #include <AudioFileSourcePROGMEM.h>
  #include <AudioGeneratorMP3.h>
  
  AudioGeneratorWAV *wav;
  AudioFileSourcePROGMEM *file;
#endif

#define TIME_ON 5000
#define RELAY 2

const char pageHTML[] PROGMEM = {"<!DOCTYPE html><title>Interphone - CURIOUS</title><meta content=\"width=device-width,initial-scale=1\"name=viewport><script crossorigin=anonymous integrity=\"sha256-FgpCb/KJQlLNfOu91ta32o/NMZxltwRo8QtmkMRdAu8=\"src=https://code.jquery.com/jquery-3.3.1.min.js></script><script>$(function() { const ESP = \"/\"; const interphoneElement = $(\"#interphone\"); interphoneElement.click(function() { $.post(ESP + \"open\", function(state) { refreshHtml(state); }, \"json\").fail(function() { error(); }); }); function getStatus() { $.post(ESP + \"\", function(state) { refreshHtml(state); }, \"json\").fail(function() { error(); }); } function refreshHtml(state) { if (state) { interphoneElement.removeClass(\"off\"); interphoneElement.addClass(\"on\"); interphoneElement.html(\"Ouvert !\"); interphoneElement.attr(\"disabled\", true); } else { interphoneElement.removeClass(\"on\"); interphoneElement.addClass(\"off\"); interphoneElement.text(\"Ouvrir\"); interphoneElement.removeAttr(\"disabled\"); } } function error() { interphoneElement.removeClass(\"on\"); interphoneElement.removeClass(\"off\"); interphoneElement.html(\"Erreur !\"); interphoneElement.attr(\"disabled\", true); } getStatus(); setInterval(function() { getStatus() }, 2000); });</script><style>body{text-align:center;margin:auto}.on{background:green!important}.off{background:red!important}#interphone{margin:auto;display:flex;align-items:center;justify-content:center;width:150px;height:150px;border-radius:35%;color:#fff;font-weight:700;font-size:20px;background:gray;border:0}#interphone:hover:enabled{font-size:25px;cursor:pointer}#infos{margin-top:200px}#infos a{display:block}</style><h1>Interphone - CURIOUS</h1><button autofocus id=interphone type=button></button><details id=infos><summary>Autres commandes</summary><a href=/open>Ouvrir (GET et POST)</a> <a href=/infos>Memoire + Uptime en JSON (GET)</a> <a href=/reset>Reset l'ESP (POST)</a>"};
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
  Serial.println(F("\nStart, connection to curious"));

  #if defined(VOICE) || defined(MP3)
    out = new AudioOutputI2SNoDAC();
    out->begin();
  #endif
  #ifdef MP3
    file = new AudioFileSourcePROGMEM(message, sizeof(message));
    out = new AudioOutputI2SNoDAC();
    wav = new AudioGeneratorWAV();
    wav->begin(file, out);
  #endif
  
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  #ifndef DIRECT_CO
    WiFiManager wifiManager;
    if(!wifiManager.autoConnect("CURIOUS-INTERPHONE")) {
      Serial.println("No connection, reset !");
      ESP.restart();
    } 
  #else
    WiFi.mode(WIFI_STA);
    WiFi.begin("curieux_wifi", "curious7ruesaintjoseph");
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println(F("No connection curious, try Valentin !"));
      WiFi.begin ( "Valentin", "0613979414" );
      if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println(F("No connection, reset !"));
        ESP.restart();
      }
    }
  #endif
  Serial.println (WiFi.localIP());
  
  server.onNotFound(handleRequest);
  server.on("/open", HTTP_ANY, handleRequestOpen);
  server.on("/infos", HTTP_GET, handleRequestInfos);
  server.on("/reset", HTTP_POST, handleRequestReset);
  //server.on("/update", HTTP_POST, handleRequestReset, handleRequestUploadProgram);
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
  
  timer.setExpired();
  
  Serial.println(F("OK"));
}

void loop() {
  if (ESP.getFreeHeap() < 1000) {
    Serial.println(F("No more memory, reset !"));
    ESP.restart();
  }

  checkDoor();
}

void handleRequest(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    request->send_P(200, mimeJson, mustOpenDoor || isDoorOpen ? trueStr : falseStr);
  } else {
    request->send_P(200, mimeHtml, pageHTML);
  }
}

void handleRequestOpen(AsyncWebServerRequest *request) {
  mustOpenDoor = true;
  handleRequest(request);
}

void handleRequestInfos(AsyncWebServerRequest *request) {
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
      sam->Say(out, "bonjour");
      delay(500);
      sam->Say(out, "porte de gauche");
      delay(500);
      sam->Say(out, "troixième étage");
      /*sam->SetPhonetic(true);
      //ɡʁup kyʁju tʁwaksjɛm etaʒ
      sam->Say(out, "GRUH4PEH KUXRIH3UHS");*/
    #endif
    #ifdef MP3
      if (wav->isRunning()) {
        if (!wav->loop()) wav->stop();
      }
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
/*
void handleRequestUploadProgram(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
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
*/
void handleRequestReset(AsyncWebServerRequest *request) {
    Serial.println(F("Reset"));
    request->send_P(200, mimeJson, Update.hasError() ? falseStr : trueStr);
    delay(1000);
    ESP.restart();
}

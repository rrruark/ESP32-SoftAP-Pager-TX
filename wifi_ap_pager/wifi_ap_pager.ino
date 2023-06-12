//ESP32 softAP Pager Transmitter for Heltec WiFi :Lora 32(V3)
//Robert Ruark
//2023

#include <AsyncEventSource.h>
#include <AsyncJson.h>
#include <AsyncWebSocket.h>
#include <AsyncWebSynchronization.h>
#include <ESPAsyncWebSrv.h>
#include <SPIFFSEditor.h>
#include <StringArray.h>
#include <WebAuthentication.h>
#include <WebHandlerImpl.h>
#include <WebResponseImpl.h>

#include <WiFi.h>
#include <RadioLib.h>

const char* ssid = "pager";
const char* password = "PagerPager";

const char* PARAM_INPUT_1 = "input1";
const char* PARAM_INPUT_2 = "input2";
const char* PARAM_INPUT_3 = "input3";

SX1262 radio = new Module(8, 14, 12, 13);
PagerClient pager(&radio);
void pager_tx(String message, int id, int message_type);

// HTML web page to handle 3 input fields (input1, input2, input3)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    Destination: <input type="text" name="input1" placeholder="1424200">
    <br>
    Message: <textarea rows="4" cols="30" name="input3"></textarea><br>
    <input type="submit" value="Submit">
  </form>
  <p>Maximum message length is 120 characters. Messages longer than 60 characters will be split into two messages.</p>
  <p> Leave destination as the default value of 1424200 to send to all pagers. Change it to 14242x0 to send to a particular pager serial number.
</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

AsyncWebServer server(80);

void setup() {
  Serial.begin(9600);
  pinMode(35, OUTPUT);
  Serial.print(F("[SX1262] Initializing ... "));
  
  int state = radio.beginFSK();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }
  Serial.print(F("[Pager] Initializing ... "));
  // base (center) frequency:     449.95 MHz
  // speed:                       1200 bps
  state = pager.begin(449.950, 1200);
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while(true);
  }  

  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);


  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String name;
    int ID;
    String inputMessage;
    String inputParam;
    if (request->hasParam(PARAM_INPUT_3)) {
      name = request->getParam(PARAM_INPUT_1)->value();
      Serial.println(name);
      int address = name.toInt();
      Serial.println(address);
      if(address<1000000 || address>1500000) address = 1424200;
      inputMessage = request->getParam(PARAM_INPUT_3)->value();
      inputParam = PARAM_INPUT_3;
      Serial.print("Message: ");
      int strLength = inputMessage.length();
      Serial.print("Message length = ");
      Serial.print(strLength);
      int chunkSize = 60;   
      for (int i = 0; i < 120 && i< strLength; i += chunkSize) 
      {
        String chunk = inputMessage.substring(i, min(i + chunkSize, strLength));
        Serial.print("Chunk: ");
        Serial.print(i);
        Serial.print(", message: ");
        Serial.print(chunk);
        Serial.print(" to address: ");
        Serial.println(address);
        pager_tx(chunk, address, 1);                         
        delay(500);
      } 
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    request->send(200, "text/html", "Pager message transmitted with value: " + inputMessage +
                                     "<br><a href=\"/\">Return to Home Page</a>");
  });
  server.onNotFound(notFound);
  server.begin();
}

void loop() 
{  
}

void pager_tx(String message, int id, int message_type)
{
  digitalWrite(35, HIGH);
  int state;
  if(message_type==0)
  {
    state = pager.transmit(message, id);
  }
  else if(message_type==1)
  {
    state = pager.transmit(message, id, RADIOLIB_PAGER_ASCII);
  }
  digitalWrite(35, LOW);
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }
}
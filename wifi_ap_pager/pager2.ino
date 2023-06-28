//ESP32 softAP Pager Transmitter for ESP32
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
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define SDA_0 21
#define SCL_0 22
 
#define SDA_1 4
#define SCL_1 15
 
TwoWire I2C_0 = TwoWire(0);
TwoWire I2C_1 = TwoWire(1);

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_1, -1);

const char* ssid = "pager";
const char* password = "PagerPager";

const char* PARAM_INPUT_1 = "input1";
const char* PARAM_INPUT_2 = "input2";
const char* PARAM_INPUT_3 = "input3";

SX1278 radio = new Module(5,26,14,35);
PagerClient pager(&radio);
void pager_tx(String message, int id, int message_type);
void OLED_display(String message, int id);

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
  I2C_1.begin(SDA_1, SCL_1, 100000);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  digitalWrite(12, LOW);
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
  state = pager.begin(449.960, 1200);
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while(true);
  }  
  radio.setOutputPower(15);
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
        OLED_display(chunk, address);                     
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
  server.onNotFound(notFound);digitalWrite(12, HIGH);
  server.begin();
  
  //OLED Stuff
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // Display static text
  display.println("PAGER 9000");
  display.display(); 
  delay(1000);
  OLED_display("Get beer",1424200);
}

void loop() 
{  
  digitalWrite(12, LOW);
}

void pager_tx(String message, int id, int message_type)
{
  digitalWrite(12, HIGH);
  int state;
  if(message_type==0)
  {
    state = pager.transmit(message, id);
  }
  else if(message_type==1)
  {
    state = pager.transmit(message, id, RADIOLIB_PAGER_ASCII);
  }
  digitalWrite(12, LOW);
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }
}

void OLED_display(String message, int id)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // Display static text
  display.println("PAGE SENT TO: ");
  display.println(id);
  display.setCursor(0, 16);
  display.println(message);
  display.display(); 
}
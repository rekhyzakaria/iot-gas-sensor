#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "cert.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "esp32-mqtt.h"
#include <math.h>
#include <ArduinoJson.h>
#include <AverageValue.h>;
#include <NTPClient.h>
#include <WiFi.h>
#include "time.h"
#include <millisDelay.h>
#include "ThingSpeak.h"


//OTA firmware version update//
String FirmwareVer = {
  "1.0"
};

#define URL_fw_Version "https://raw.githubusercontent.com/rekhyzakaria/iot-gas-sensor/main/esp32_OTA/bin_version.txt?token=GHSAT0AAAAAABVSAMYXNZBURQEHSFQL264AYVHECBQ"
#define URL_fw_Bin "https://raw.githubusercontent.com/rekhyzakaria/iot-gas-sensor/main/esp32_OTA/fw.bin?token=GHSAT0AAAAAABVSAMYWCG3DT45THFM64L4CYVHEC4Q"

void firmwareUpdate();
int FirmwareVersionCheck();
unsigned long previousMillis = 0;
unsigned long previousMillis_2 = 0;
const long interval = 60000;
const long mini_interval = 1000;

void repeatedCall() {
  static int num=0;
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillis) >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    if (FirmwareVersionCheck()) {
      firmwareUpdate();
    }
  }
  if ((currentMillis - previousMillis_2) >= mini_interval) {
    previousMillis_2 = currentMillis;
    Serial.print("idle loop...");
    Serial.print(num++);
    Serial.print(" Active fw version:");
    Serial.println(FirmwareVer);
   if(WiFi.status() == WL_CONNECTED) 
   {
       Serial.println("wifi connected");
   }
   else
   {

   }
  }
}

void firmwareUpdate(void) {
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  httpUpdate.setLedPin(LED_BUILTIN, LOW);
  t_httpUpdate_return ret = httpUpdate.update(client, URL_fw_Bin);

  switch (ret) {
  case HTTP_UPDATE_FAILED:
    Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("HTTP_UPDATE_OK");
    break;
  }
}
int FirmwareVersionCheck(void) {
  String payload;
  int httpCode;
  String fwurl = "";
  fwurl += URL_fw_Version;
  fwurl += "?";
  fwurl += String(rand());
  Serial.println(fwurl);
  WiFiClientSecure * client = new WiFiClientSecure;

  if (client) 
  {
    client -> setCACert(rootCACertificate);

    // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
    HTTPClient https;

    if (https.begin( * client, fwurl)) 
    { // HTTPS      
      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      delay(100);
      httpCode = https.GET();
      delay(100);
      if (httpCode == HTTP_CODE_OK) // if version received
      {
        payload = https.getString(); // save received version
      } else {
        Serial.print("error in downloading version file:");
        Serial.println(httpCode);
      }
      https.end();
    }
    delete client;
  }
      
  if (httpCode == HTTP_CODE_OK) // if version received
  {
    payload.trim();
    if (payload.equals(FirmwareVer)) {
      Serial.printf("\nDevice already on latest firmware version:%s\n", FirmwareVer);
      return 0;
    } 
    else 
    {
      Serial.println(payload);
      Serial.println("New firmware detected");
      return 1;
    }
  } 
  return 0;  
}

struct Button {
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};

Button button_boot = {
  0,
  0,
  false
};
void IRAM_ATTR isr() {
  button_boot.numberKeyPresses += 1;
  button_boot.pressed = true;
}
//akhir dari OTA firmware version update//

//define channel untuk store data di thingspeak//

unsigned long myChannelNumber = 1739358;
const char * myWriteAPIKey = "NMV8278FR9UEIP3G";

//trigger webhook to outlook to lark
const char* serverName_CO2 = "http://maker.ifttt.com/trigger/Smoke_warning/with/key/b0YdJ5ZVj_B1oU0H5vmbmrctKNuAHa6Ycx2QWgwUeG3";
const char* serverName_LPG = "http://maker.ifttt.com/trigger/LPG_warning/with/key/b0YdJ5ZVj_B1oU0H5vmbmrctKNuAHa6Ycx2QWgwUeG3";

unsigned long TIMEOUT_MS = 10000;

millisDelay closeTimeout;

unsigned long TIMEOUT_LPGWARNING = 5000;

//define setiap variabel yang digunakan
#define MQ135Pin  35
#define MQ2Pin    33
#define Buzzer    5
#define LED       4

int value1;
int value2;
byte time_day;
byte time_month;
int time_year;
int time_hour;
int time_min;
int time_sec;
String date;
String timestamp;

//menggunakan dualcore esp32 untuk multitasking dalam uploading data setiap 8 jam sekali
TaskHandle_t scheduledPublish;
unsigned long TIMEOUT_scheduledpublish = 8*60*60*1000;

//Menggunakan ntp server untuk mendapatkan timestamp
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 25200;

void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  time_hour = timeinfo.tm_hour;
  time_min = timeinfo.tm_min;
  time_sec = timeinfo.tm_sec;
  time_day = timeinfo.tm_mday;
  time_month = timeinfo.tm_mon + 1;
  time_year = timeinfo.tm_year + 1900;
  date = time_day + String("/") + time_month + String("/") + time_year;    
  timestamp = time_hour + String(":") + time_min + String(":") + time_sec;
}


//MQ135 untuk membaca CO2 atau mendeteksi asap

int RL_MQ135    = 20000;
float R0_CO2    = 84000;            //Nilai R0 (RZERO) diperoleh dari kalibrasi senson MQ135 dgn menjalankan program mq135_cari_rO.ino
double ATMOCO2  = 420.23;      //Nilai CO2 di atmosfer bumi sumber : https://www.co2.earth
float a         = 110.7432567;      //a dan b adalah Eksponensial regression untuk CO2 yang diperoleh dari datasheet 
float b1         = -2.856935538;     


//MQ2 untuk mendeteksi keboocoran gas LPG

int RL_MQ2      = 10000;
float m         = -0.44953;
float b2        = 1.23257;
float R0_LPG    = 3095;           //Nilai R0 diperoleh dari kalibrasi sensor MQ2

const long MAX_VALUES_NUM = 10;
AverageValue<long> averageValue(MAX_VALUES_NUM);
char buffer[100];

void setup() {

  Serial.println();
  pinMode (MQ135Pin,INPUT);
  pinMode (MQ2Pin,INPUT);
  pinMode (Buzzer,OUTPUT);
  pinMode (LED,OUTPUT);
  
  pinMode(button_boot.PIN, INPUT);
  digitalWrite(Buzzer,LOW);
  digitalWrite(LED,LOW);
  attachInterrupt(button_boot.PIN, isr, RISING);
  Serial.begin(9600);
  setupCloudIoT();
  Serial.println("Setup.....");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  xTaskCreatePinnedToCore(
            scheduledPublishCode,
            "scheduledPublish",
            10000,
            NULL,
            1,
            &scheduledPublish,
            0);
  delay(500);
    
}

unsigned long lastMillis = 0;


void scheduledPublishCode(void * pvParameters){
  Serial.println("Publish data to cloud every 8 hours");

  for(;;){
       //mencari ppm LPG
    double VRL = (analogRead(MQ2Pin))*(3.3/R0_LPG);
    double RS_LPG = ((3.3*RL_MQ2)/VRL)- RL_MQ2;
    float ratio = RS_LPG/R0_LPG;
    float ppm_LPG = pow(10, ((log10(ratio)-b2)/m));

  
  
    //mencari ppm CO2
    int adcRaw_CO2 = analogRead(MQ135Pin);
    double RS_CO2 = ((1023.00/adcRaw_CO2)*5 - 1)*RL_MQ135;
    float RSRO_CO2 = RS_CO2/R0_CO2;
    float ppm_CO2 = a * pow((float)RS_CO2 / (float)R0_CO2, b1);
    averageValue.push(ppm_CO2);

    value1 = averageValue.average();
    value2 = ppm_LPG;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    mqtt->loop();
    delay(100);  // <- fixes some issues with WiFi stability
    if (!mqttClient->connected()) {
      connect();
    }
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
  
    if (millis() - lastMillis > 60000) {
      Serial.println("Publishing value");
      float CO2 = round(value1);
      float LPG = round(value2);
      
      lastMillis = millis();

      StaticJsonDocument<100> doc;
      doc ["dateTime"] = timestamp;
      doc ["CO2"] = CO2;
      doc ["LPG"] = LPG;
      serializeJson(doc, buffer);
       publishTelemetry( buffer);
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    delay(8*60*60*1000);
  
   

     
    }
    
}
   
  



unsigned long lastTime = 0;
unsigned long timerDelay = 15000;


void loop() {
  
  digitalWrite(Buzzer,LOW);
  digitalWrite(LED,LOW);

  if (button_boot.pressed) { 
    Serial.println("Firmware update Starting..");
    firmwareUpdate();
    button_boot.pressed = false;
  }
  repeatedCall();
 
   //mencari ppm LPG
  double VRL = (analogRead(MQ2Pin))*(3.3/R0_LPG);
  double RS_LPG = ((3.3*RL_MQ2)/VRL)- RL_MQ2;
  float ratio = RS_LPG/R0_LPG;
  float ppm_LPG = pow(10, ((log10(ratio)-b2)/m));

  
  
  //mencari ppm CO2
  int adcRaw_CO2 = analogRead(MQ135Pin);
  double RS_CO2 = ((1023.00/adcRaw_CO2)*5 - 1)*RL_MQ135;
  float RSRO_CO2 = RS_CO2/R0_CO2;
  float ppm_CO2 = a * pow((float)RS_CO2 / (float)R0_CO2, b1);
  averageValue.push(ppm_CO2);

  value1 = averageValue.average();
  value2 = ppm_LPG;
  float CO2 = round(value1);
  float LPG = round(value2);

  //mengidentifikasi asap selama 10 detik 
  if(value1>2000){
    if(!closeTimeout.isRunning()){
      closeTimeout.start(TIMEOUT_MS);
    }
  }
  
    else{
      closeTimeout.stop();
    }
    if(closeTimeout.justFinished()){
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      WiFiClient client;
      HTTPClient http;
      //ThingSpeak.begin(client);
      digitalWrite(Buzzer,HIGH);
      digitalWrite(LED, HIGH);
      mqtt->loop();
      delay(100);  // <- fixes some issues with WiFi stability
      if (!mqttClient->connected()) {
        connect();
      }
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      printLocalTime();
      if(millis() - lastMillis>60000){
        Serial.println("Publishing value");
        float CO2 = round(value1);
        float LPG = round(value2);
    

      //ThingSpeak.setField(1, CO2);
      //ThingSpeak.setField(2, LPG);
      //int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
      //if(x==200){
       // Serial.println("Successfully update Gasses data to Thingspeak");
      //}
     
      //else{
      //  Serial.println("There is problem updating Thingspeak channel. HTTP error code " + String(x));
      //}
        
    
        http.begin(client,serverName_CO2);
        http.addHeader("Content-Type", "application/json");
        String httpRequestData = "{\"value1\":\"" + String(CO2) + "\"}";
        int httpResponseCode = http.POST(httpRequestData);
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        http.end();
        lastMillis = millis();
        delay(500);
        Serial.println("WARNING : SMOKE DETECTED");
        Serial.println(timestamp);
        Serial.print ("CO2 = ");
        Serial.print (value1);
        Serial.println(" ppm");
        Serial.print("LPG = ");
        Serial.print(value2);
        Serial.println(" ppm");
        StaticJsonDocument<100> doc;
        doc ["Date"] = date;
        doc ["Time"] = timestamp;
        doc ["CO2"] = CO2;
        doc ["LPG"] = LPG;
        serializeJson(doc, buffer);
        publishTelemetry( buffer);
       
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(10000);
  }
 


    
    

  else if(value2>1500){
    if(!closeTimeout.isRunning()){
      closeTimeout.start(TIMEOUT_LPGWARNING);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      //ThingSpeak.begin(client);
      digitalWrite(LED,HIGH);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(500);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(500);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(500);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(500);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(500);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(50);
      digitalWrite(Buzzer,HIGH);
      delay(50);
      digitalWrite(Buzzer,LOW);
      delay(500);    
      
      mqtt->loop();
      delay(1000);  // <- fixes some issues with WiFi stability
      if (!mqttClient->connected()) {
        connect();
      }
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      printLocalTime();
      WiFiClient client;
      HTTPClient http;
    
      http.begin(client,serverName_LPG);
      http.addHeader("Content-Type", "application/json");
      String httpRequestData = "{\"value2\":\"" + String(LPG) + "\"}";
      int httpResponseCode = http.POST(httpRequestData);
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      http.end();
      lastMillis = millis();
      
      if (millis() - lastMillis > 60000) {
        Serial.println("Publishing value");
        float CO2 = round(value1);
        float LPG = round(value2);
        
        
        //ThingSpeak.setField(1, CO2);
        //ThingSpeak.setField(2, LPG);
        //int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        //if(x==200){
          //Serial.println("Successfully update Gasses data to Thingspeak");
        //}
     
        //else{
         // Serial.println("There is problem updating Thingspeak channel. HTTP error code " + String(x));
        //}
        lastMillis = millis();
        Serial.println("WARNING : LPG LEAKED");
        Serial.println(timestamp);
        Serial.print ("CO2 = ");
        Serial.print (value1);
        Serial.println(" ppm");
        Serial.print("LPG = ");
        Serial.print(value2);
        Serial.println(" ppm");
        StaticJsonDocument<100> doc;
        doc ["Date"] = date;
        doc ["Time"] = timestamp;
        doc ["CO2"] = CO2;
        doc ["LPG"] = LPG;
        serializeJson(doc, buffer);
        publishTelemetry( buffer);
        }
    }
    
    else{
      closeTimeout.stop();
    }
    if(closeTimeout.justFinished()){
      digitalWrite(Buzzer,LOW);
      digitalWrite(LED,LOW);
      
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(10000);
  }

  
  else {

      Serial.println("The Value are: ");
      Serial.print ("CO2 = ");
      Serial.print (value1);
      Serial.println(" ppm");
      Serial.print("LPG = ");
      Serial.print(value2);
      Serial.println(" ppm");
  }

    delay(1000);
}

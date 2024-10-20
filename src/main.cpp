/**********************************************************************************
/*  Program name: Hönslucka
 *  This program is used to control 
 *  a geared motor to wind or rewind (close/open) a string conneted to the hatch.
 *  The manual function (open/close buttons) is overriding the Home Assistant 
 *  settings so if the Home assistant is down You could
 *  Using following hardware:
 *  Wemos D1 (ESP8266 ESP-12F)
 *  Motor driver board using LS9110-S
 *  Level shifter 3.3 - 5 V
 *  DC/DC 12V -> 5V
 *  External antenna
 * 
 *  2024-10-17 Håkan Torén
 **********************************************************************************/

//#define DEBUG   // This will  print messages

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define wifi_ssid "Ekeborg"
#define wifi_password "Mittekeborg54"
#define HOSTNAME Honslucka
#define mqtt_server "192.168.1.36"
#define mqtt_user "ha"
#define mqtt_password "ha"

#define state_topic1 "kibunzi/honslucka/state"
#define inTopic1 "kibunzi/honslucka/command"
#define state_topic2 "kibunzi/honslucka/input"

#define motorA 4
#define motorB 5
#define motorspeed 80
#define motorGND 0
/*
#define openLSW 14          //GPIO14 (D5)
#define closeLSW 12         //GPIO12 (D6)
*/
#define openLSW 12         //GPIO12 (D6)
#define closeLSW 14          //GPIO14 (D5)

#define OpenButton 0        //GPIO0  (D3)
#define CloseButton 13      //GPIO13 (D7)



bool motorToggle = true;
bool OpenHatch = false;
int OpenButtonState = HIGH;
int CloseButtonState = HIGH;
int LastOpenButtonState = HIGH;  // Pull up
int LastCloseButtonState = HIGH; // Pull up
unsigned long LastOpenButtonDebounceTime = 0;
unsigned long LastCloseButtonDebounceTime = 0;
unsigned int debounceDelay = 100;

WiFiClient espClient;
PubSubClient client(espClient);
 

void setup_wifi(){
  delay(10);
  Serial.print("\nConnecting to ");
  Serial.println(wifi_ssid);
  WiFi.hostname("HOSTNAME");
  WiFi.begin(wifi_ssid, wifi_password);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi connected \n");
  Serial.print("IP adress: ");
  Serial.println(WiFi.localIP());
}

void reconnect(){
  // Loop until we are connected
  while(!client.connected()){
    Serial.print("Atempting MQTT connection ...");
    // Create random client ID
    String clientId = "MQTT_Relay-";
    clientId += String(random(0xffff),HEX);
    // Attempt to connect
    if(client.connect(clientId.c_str(), mqtt_user, mqtt_password)){
      Serial.println("connected");
      // Once connected, publish an announcement..
      client.publish(state_topic1, "CLOSE");
      // ... and resubscribe
      client.subscribe(inTopic1);
    }else{
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
    
  }
}

void callback(char* topic1,byte* payload, unsigned int length){
#ifdef  DEBUG
  Serial.print("Message arrived on topic: ");
  Serial.print(topic1);
  Serial.print(". Message: ");
#endif
  String messageTemp;
  
  for(unsigned int i=0;i<length;i++){
#ifdef  DEBUG
    Serial.print((char)payload[i]);
#endif
    messageTemp += (char)payload[i];
  }
#ifdef  DEBUG
  Serial.println();
#endif
  if(String(topic1) == inTopic1){
    if(messageTemp == "OPEN"){
#ifdef  DEBUG
      Serial.print("Changing output to ON\n");
#endif
      client.publish(state_topic1, "OPEN");
      OpenHatch = true;
      motorToggle = true;
      analogWrite(motorA, motorGND);                  
      analogWrite(motorB, motorGND);
      delay(200);
      
    }else if(messageTemp == "CLOSE"){
#ifdef  DEBUG
      Serial.print("Changing output to OFF\n");
#endif
      client.publish(state_topic1, "CLOSE");
      OpenHatch = false;
      motorToggle = true;
      analogWrite(motorA, motorGND);    // Stop motor                
      analogWrite(motorB, motorGND);
      delay(200);
    }
    
    
  }
}

void setup() {

  Serial.begin(115200);
  pinMode(motorA,OUTPUT);
  pinMode(motorB,OUTPUT);
  pinMode(openLSW,INPUT_PULLUP);
  pinMode(closeLSW,INPUT_PULLUP);
  pinMode(OpenButton,INPUT_PULLUP);
  pinMode(CloseButton,INPUT_PULLUP);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

}

void loop(){
  if(!client.connected()){
    reconnect();
  }
  client.loop();



  if(OpenHatch){                                   // Command "ON", 
    if(!digitalRead(openLSW) && motorToggle){           
      analogWrite(motorA, motorGND);                // Wind up the string
      analogWrite(motorB, motorspeed);
      motorToggle = false; 
    }else if(digitalRead(openLSW) && !motorToggle){   // Stop winding up the string
#ifdef DEBUG
  Serial.println("Stop windung up the string");
#endif
      analogWrite(motorA, motorGND); 
      analogWrite(motorB, motorGND);
      //motorToggle = true; 
    }
  }else{     // Close hatch                                         
    if(!digitalRead(closeLSW) && motorToggle){
      analogWrite(motorA, motorspeed);                // Role out the string 
      analogWrite(motorB, motorGND);
      motorToggle = false;
    }else if(digitalRead(closeLSW)){                  // Stop role out the string
#ifdef DEBUG  
  Serial.println("Stop role out the string");
#endif
      analogWrite(motorA, motorGND);                    
      analogWrite(motorB, motorGND); 
      motorToggle = true;
    }
  }
  int reading = digitalRead(OpenButton);

  if(reading != LastOpenButtonState) {
    LastOpenButtonDebounceTime = millis();
  }

  if ((millis() - LastOpenButtonDebounceTime) > debounceDelay) {
    if (reading != OpenButtonState) {
      OpenButtonState = reading;
      if (OpenButtonState == LOW) {
        client.publish(state_topic1, "OPEN");
        OpenHatch = true;
        motorToggle = true;
        analogWrite(motorA, motorGND);    // Stop motor                
        analogWrite(motorB, motorGND);
        Serial.println("OFF");
      }
    }
  }
  LastOpenButtonState = reading;

 reading = digitalRead(CloseButton); 

 if(reading != LastCloseButtonState) {
    LastCloseButtonDebounceTime = millis();
  }

  if ((millis() - LastCloseButtonDebounceTime) > debounceDelay) {
    if (reading != CloseButtonState) {
      CloseButtonState = reading;
      if (CloseButtonState == LOW) {
        client.publish(state_topic1, "CLOSE");
        OpenHatch = false;
        motorToggle = true;
        analogWrite(motorA, motorGND);    // Stop motor                
        analogWrite(motorB, motorGND);
        Serial.println("ON");
      }
    }
  }
  LastCloseButtonState = reading;
  
}







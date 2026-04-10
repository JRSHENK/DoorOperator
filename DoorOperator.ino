/***

   Overhead Door Opener V1.0.16.4 Octorber 2025
   For ESP32
   OTA Working
   changed to non-blocking
   MQTT reporting status & position
   Added encoder position removed approach limit switches
   Desired door position sent from MQTT in percent open
   Changed IO pins to make easier PCB trace layout
   Added INA219s & stop on over current
   Changed to be compatable with Arduino ESP32 V3.x.x 

*/

#define DOOR_OPERATOR_VERSION "1.0.16.4"

// libraries
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP32Encoder.h>
#include <Adafruit_INA219.h>

//*************************  Set Debugging on or off **********************

// Comment the following line if not debuging
//#define __DEBUG__

#if defined(__DEBUG__)
#define DEBUG_begin(val) Serial.begin(val)
#define DEBUG_print(val) Serial.print(val)
#define DEBUG_println(val) Serial.println(val)
#else
#define DEBUG_begin(val)
#define DEBUG_print(val)
#define DEBUG_println(val)
#endif

#define sda 21  //Pin #33
#define scl 22  //Pin #36

// INA219 current and voltage sensors: to monitor battery panel & battery
Adafruit_INA219 ina219_door1(0x40);  // I2C address can be set to 0x40, 0x41, 0x44 or 0x45
Adafruit_INA219 ina219_door2(0x41);  // I2C address can be set to 0x40, 0x41, 0x44 or 0x45


//************************* Define Variables ******************************
ESP32Encoder encoder1;
ESP32Encoder encoder2;
char cPercentPosition1[10];
char cPercentPosition2[10];
String sCount1;
String sCount2;
int iCount1 = 0;
int iCount2 = 0;
int countPreset1 = 0;
int countPreset2 = 0;
int percentPosition1 = 0;
int percentPosition2 = 0;
int percentPreset1 = 0;
int percentPreset2 = 0;
#define fullCount 2100    // set this value to the number of counts needed for full door travel (My motor: 70 pulses/rev, 500 pulses/passpoint pulse)
#define slowDownCount 50  // set this value to how many counts before the end the door slows down at

#define direction_1 27    //Pin #11
#define openButton_1 36   //Pin #3
#define closeLimit_1 35   //Pin #6
#define closeButton_1 39  //Pin #4

//Reversed motor
//#define motorClose_1 25 //Pin #9
//#define motorOpen_1 33 //Pin #8
#define motorClose_1 33  //Pin #8
#define motorOpen_1 25   //Pin #9

#define stopButton_1 34  //Pin #5
#define openButton_2 15  //Pin #23
#define direction_2 12   //Pin #13
#define closeLimit_2 4   //Pin #26
#define closeButton_2 2  //Pin #24

//reversed motor
//int motorClose_2 5 //Pin #29
//int motorOpen_2 17 //Pin #28
#define motorClose_2 17  //Pin #28
#define motorOpen_2 5    //Pin #29

#define stopButton_2 0    //Pin #25
#define beam 13           //Pin #15
#define encoderCount1 32  //Pin #7
#define encoderDir1 26    //Pin #10
#define encoderCount2 16  //Pin #27
#define encoderDir2 14    //Pin #12

// Set these for desired door travel speeds 0-255
#define fast 255
#define slow 145

// Set these to a little longer than it takes for your door to open and close, so if something goes wrong the door operator does not get damaged (mS)
#define openTimeOut 32000
#define closeTimeOut 32000

// Set these to a little more than normal operating currents (mA)
#define openCurrentLimit 5100
#define closeCurrentLimit 5100

bool opening1 = false;
bool closing1 = false;
bool opening2 = false;
bool closing2 = false;
bool pubStat1 = false;
bool pubStat2 = false;
bool timeOut1 = false;
bool timeOut2 = false;
bool open1 = false;
bool open2 = false;
bool openSlow1 = false;
bool openSlow2 = false;
bool close1 = false;
bool close2 = false;
bool closeSlow1 = false;
bool closeSlow2 = false;
bool pubIP = true;
unsigned long runTime_1;
unsigned long runTime_2;
unsigned long startTime_1;
unsigned long startTime_2;
unsigned long endTime_1;
unsigned long endTime_2;
float busvoltage = 0;
float busvoltageNew = 0;
float door1_mA = 0;
float door2_mA = 0;

// pwm channel assinment
#define pwmOpen_1 0
#define pwmClose_1 1
#define pwmOpen_2 2
#define pwmClose_2 3

// pwm frequency assinment
#define frequency 1500
// pwm resolution assinment
#define resolution 8

//*************************  NETWORK INFO  ********************************
const char* ssid = "YourSSID";
const char* password = "YourPassword";
const char* hostName = "SouthBarnDoors";
const char* mqtt_server = "MQTT_Server_IP";
const char* mqtt_user = "MQTT_Username";
const char* mqtt_password = "MQTT_Password";
const char* mqtt_clent = "Overhead_Doors";

// Publish info about the status of door
const char* door1status = "Overhead_Doors/door1Status";
const char* door1position = "Overhead_Doors/door1Position";
const char* door2status = "Overhead_Doors/door2Status";
const char* door2position = "Overhead_Doors/door2Position";
const char* door1count = "Overhead_Doors/door1Count";
const char* door2count = "Overhead_Doors/door2Count";
const char* Vbattery = "Overhead_Doors/Vbattery";
const char* ip = "Overhead_Doors/ipAddress";

// Listen for incoming control messages
const char* doorControl = "Overhead_Doors/Control";
const char* door1SetValue = "Overhead_Doors/SetValue1";
const char* door2SetValue = "Overhead_Doors/SetValue2";
const char* HArefresh = "homeassistant/RefreshMQTT";

//*************************   MQTT String Setup ***************************

WiFiClient Overhead_Doors;
PubSubClient client(Overhead_Doors);
long lastMsg = 0;
char payload[50];
int value = 0;

//***************************  Setup WIFI  *********************************
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  DEBUG_println();
  DEBUG_print("Connecting to ");
  DEBUG_println(ssid);

  WiFi.begin(ssid, password);
  WiFi.setHostname(hostName);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);


  DEBUG_println("");
  DEBUG_println("WiFi connected");
  DEBUG_println("IP address: ");
  DEBUG_println(WiFi.localIP());
}
//*********************  END WIFI SETUP  ************************************

void stopDoor_1();
void stopDoor_2();

//***********************  Reconnect to MQTT  ********************************
void reconnect() {
  // Loop until we're reconnected                         Remove this part
  //while (!client.connected()) {                         Remove this part
  //DEBUG_print("Attempting MQTT connection...");       Remove this part
  // Attempt to connect
  if (client.connect(mqtt_clent, mqtt_user, mqtt_password)) {
    DEBUG_println("connected");
    // Once connected, publish an announcement...
    //client.publish("outTopic", "hello world");
    client.subscribe(doorControl);
    client.subscribe(door1SetValue);
    client.subscribe(door2SetValue);
    client.subscribe(HArefresh);
  } else {
    DEBUG_print("failed, rc=");
    DEBUG_print(client.state());
    DEBUG_println(" try again in 5 seconds");
    // Wait 5 seconds before retrying                   Remove this part
    //delay(5000);                                      Remove this part
    //}                                                   Remove this part
  }
}

//***********   void callback listens for incoming mqtt messages  ***********
void callback(char* topic, byte* payload, unsigned int length) {
  DEBUG_print("Message arrived [");
  DEBUG_print(topic);
  DEBUG_print("] ");
  for (int i = 0; i < length; i++) {
    DEBUG_print((char)payload[i]);
  }
  DEBUG_println(" ");

  //************************************* MQTT Control *******************************************

  if (strcmp(topic, "homeassistant/RefreshMQTT") == 0) {
    pubStat1 = true;
    pubStat2 = true;
    pubIP = true;
  }

  if (strcmp(topic, "Overhead_Doors/SetValue1") == 0) {

    payload[length] = '\0';                     // Add a NULL to the end of the char* to make it a string.
    int percentPreset1 = atoi((char*)payload);  //Convert payload to int
    countPreset1 = ((fullCount) * (percentPreset1 * .01));

    if (countPreset1 > iCount1) {
      open1 = true;
    } else if (countPreset1 < iCount1) {
      close1 = true;
    }
  }

  if (strcmp(topic, "Overhead_Doors/SetValue2") == 0) {

    payload[length] = '\0';                     // Add a NULL to the end of the char* to make it a string.
    int percentPreset2 = atoi((char*)payload);  //Convert payload to int
    countPreset2 = ((fullCount) * (percentPreset2 * .01));


    if (countPreset2 > iCount2) {
      open2 = true;
    } else if (countPreset2 < iCount2) {
      close2 = true;
    }
  }

  // Stop door 1
  if ((char)payload[0] == 's' && (char)payload[1] == 't' && (char)payload[2] == 'o' && (char)payload[3] == 'p' && (char)payload[4] == '1') {
    stopDoor_1();
  }

  // Stop door 2
  if ((char)payload[0] == 's' && (char)payload[1] == 't' && (char)payload[2] == 'o' && (char)payload[3] == 'p' && (char)payload[4] == '2') {
    stopDoor_2();
  }

  //***************************************************************************
}


//****************************  BEGIN SETUP  **********************************
void setup() {

  Serial.begin(115200);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  Serial.print("Barn Door Opener ");
  Serial.println(DOOR_OPERATOR_VERSION);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  enableOTAfromIDE();
  ArduinoOTA.begin();

  Wire.begin(sda, scl);

  uint32_t currentFrequency;
  ina219_door1.begin();
  ina219_door2.begin();

  if (!ina219_door1.begin() || !ina219_door2.begin()) {
    DEBUG_println("Failed to find INA219 chip");
    while (1) {
      delay(10);
    }
  }

  DEBUG_println("Measuring voltage and current with INA219 ...");

  //Setup position encoders
  encoder1.attachSingleEdge(encoderCount1, encoderDir1);
  encoder2.attachSingleEdge(encoderCount2, encoderDir2);
  encoder1.setCount(0);
  encoder2.setCount(0);

  pinMode(direction_1, OUTPUT);
  pinMode(closeLimit_1, INPUT_PULLUP);
  pinMode(openButton_1, INPUT_PULLUP);
  pinMode(closeButton_1, INPUT_PULLUP);
  pinMode(stopButton_1, INPUT_PULLUP);

  pinMode(direction_2, OUTPUT);
  pinMode(closeLimit_2, INPUT_PULLUP);
  pinMode(openButton_2, INPUT_PULLUP);
  pinMode(closeButton_2, INPUT_PULLUP);
  pinMode(stopButton_2, INPUT_PULLUP);
  pinMode(beam, INPUT_PULLUP);

  pinMode(encoderCount1, INPUT_PULLUP);
  pinMode(encoderDir1, INPUT_PULLUP);
  pinMode(encoderCount2, INPUT_PULLUP);
  pinMode(encoderDir2, INPUT_PULLUP);

  pinMode(motorOpen_1, OUTPUT);
  pinMode(motorClose_1, OUTPUT);
  pinMode(motorOpen_2, OUTPUT);
  pinMode(motorClose_2, OUTPUT);

  ledcAttachChannel(motorOpen_1, frequency, resolution, pwmOpen_1);
  ledcAttachChannel(motorClose_1, frequency, resolution, pwmClose_1);
  ledcAttachChannel(motorOpen_2, frequency, resolution, pwmOpen_2);
  ledcAttachChannel(motorClose_2, frequency, resolution, pwmClose_2);

  ledcWriteChannel(pwmOpen_1, 0);
  ledcWriteChannel(pwmOpen_2, 0);
  ledcWriteChannel(pwmClose_1, 0);
  ledcWriteChannel(pwmClose_2, 0);
}
//*********************  END SETUP  ************************************


//*********************  BEGIN LOOP  ***********************************
void loop() {

  /*********      Check OTA Updates       ****************/
  ArduinoOTA.handle();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  /**********************  Open Door 1  *******************************/
  if ((digitalRead(openButton_1) == LOW) && (iCount1 != fullCount)) {
    countPreset1 = fullCount;
    open1 = true;
  }

  if (iCount1 > (countPreset1 - slowDownCount) && (opening1 == true)) {
    openSlow1 = true;
  }

    //publish door position as door moves
  if ((open1 == true) || (openSlow1 == true)) {
    pubStat1 = true;
  }

  if ((closing1 == false) && (open1 == true) && (openSlow1 == false)) {
    ledcWriteChannel(pwmOpen_1, fast);
    client.publish(door1status, "Opening");
    open1 = false;
    opening1 = true;
    startTime_1 = millis();
    endTime_1 = startTime_1;
    timeOut1 == false;
  }

  if (opening1 == true) {
    endTime_1 = millis();
    sCount1 = String((int32_t)encoder1.getCount());
    iCount1 = sCount1.toInt();
    DEBUG_println(iCount1);

    door1_mA = ina219_door1.getCurrent_mA();

    if (door1_mA > openCurrentLimit) {
      stopDoor_1();
      client.publish(door1status, "trouble(I)");
      DEBUG_println("Door opening stopped over current!");
    }
  }

  if (openSlow1 == true && closing1 == false) {
    ledcWriteChannel(pwmOpen_1, slow);
    openSlow1 = false;
    client.publish(door1status, "Opening slow");
  }

  if ((digitalRead(stopButton_1) == LOW) || (digitalRead(beam) == LOW) || (countPreset1 <= iCount1) && (opening1 == true)) {
    stopDoor_1();
  }

  if (((endTime_1 - startTime_1) >= openTimeOut) && timeOut1 == false) {
    stopDoor_1();
    DEBUG_println("Door opening timed out!");
    client.publish(door1status, "trouble(T)");
    timeOut1 = true;
  }

  /**********************  Close Door 1  *******************************/
  if ((digitalRead(closeButton_1) == LOW) && (digitalRead(closeLimit_1) == HIGH)) {
    countPreset1 = 0;
    close1 = true;
  }

  if (iCount1 < (countPreset1 + slowDownCount) && (closing1 == true)) {
    closeSlow1 = true;
  }

    //publish door position as door moves
  if ((close1 == true) || (closeSlow1 == true)) {
    pubStat1 = true;
  }

  if ((opening1 == false) && (close1 == true) && (closeSlow1 == false)) {
    ledcWriteChannel(pwmClose_1, fast);
    digitalWrite(direction_1, HIGH);
    client.publish(door1status, "Closing");
    close1 = false;
    closing1 = true;
    startTime_1 = millis();
    endTime_1 = startTime_1;
    timeOut1 == false;
  }

  if (closing1 == true) {
    endTime_1 = millis();
    sCount1 = String((int32_t)encoder1.getCount());
    iCount1 = sCount1.toInt();
    DEBUG_println(iCount1);

    door1_mA = ina219_door1.getCurrent_mA();

    if (door1_mA > closeCurrentLimit) {
      stopDoor_1();
      client.publish(door1status, "trouble(I)");
      DEBUG_println("Door closing stopped, over current!");
    }
  }

  if (closeSlow1 == true && opening1 == false) {
    ledcWriteChannel(pwmClose_1, slow);
    client.publish(door1status, "Closing slow");
    closeSlow1 = false;
  }

  if (digitalRead(closeLimit_1) == LOW && (closing1 == true)) {
    encoder1.setCount(0);
    DEBUG_println("counter 1 reset");
    stopDoor_1();
  }

  if ((digitalRead(stopButton_1) == LOW) || (digitalRead(beam) == LOW) || ((countPreset1 >= iCount1) && (closing1 == true))) {
    stopDoor_1();
  }

  if (((endTime_1 - startTime_1) >= closeTimeOut) && timeOut1 == false) {
    stopDoor_1();
    DEBUG_println("Door closing timed out!");
    client.publish(door1status, "trouble(T)");
    timeOut1 = true;
  }

  /**********************  Open Door 2  *******************************/
  if ((digitalRead(openButton_2) == LOW) && (iCount2 != fullCount)) {
    countPreset2 = fullCount;
    open2 = true;
  }

  if (iCount2 > (countPreset2 - slowDownCount) && (opening2 == true)) {
    openSlow2 = true;
  }

   //publish door position as door moves
  if ((open2 == true) || (openSlow2 == true)) {
    pubStat2 = true;
  }

  if ((closing2 == false) && (open2 == true) && (openSlow2 == false)) {
    ledcWriteChannel(pwmOpen_2, fast);
    client.publish(door2status, "Opening");
    open2 = false;
    opening2 = true;
    startTime_2 = millis();
    endTime_2 = startTime_2;
    timeOut2 == false;
  }

  if (opening2 == true) {
    endTime_2 = millis();
    sCount2 = String((int32_t)encoder2.getCount());
    iCount2 = sCount2.toInt();
    DEBUG_println(iCount2);

    door2_mA = ina219_door2.getCurrent_mA();

    if (door2_mA > openCurrentLimit) {
      stopDoor_2();
      client.publish(door2status, "trouble(I)");
      DEBUG_println("Door opening stopped, over current!");
    }
  }

  if (openSlow2 == true && closing2 == false) {
    ledcWriteChannel(pwmOpen_2, slow);
    openSlow2 = false;
    client.publish(door2status, "Opening slow");
  }

  if ((digitalRead(stopButton_2) == LOW) || (digitalRead(beam) == LOW) || (countPreset2 <= iCount2) && (opening2 == true)) {
    stopDoor_2();
  }

  if (((endTime_2 - startTime_2) >= openTimeOut) && timeOut2 == false) {
    stopDoor_2();
    DEBUG_println("Door opening timed out!");
    client.publish(door2status, "trouble(T)");
    timeOut2 = true;
  }

  /**********************  Close Door 2  *******************************/
  if ((digitalRead(closeButton_2) == LOW) && (digitalRead(closeLimit_2) == HIGH)) {
    countPreset2 = 0;
    close2 = true;
  }

  if (iCount2 < (countPreset2 + slowDownCount) && (closing2 == true)) {
    closeSlow2 = true;
  }

       //publish door position as door moves
  if ((close2 == true) || (closeSlow2 == true)) {
    pubStat2 = true;
  }
  
  if ((opening2 == false) && (close2 == true) && (closeSlow2 == false)) {
    ledcWriteChannel(pwmClose_2, fast);
    digitalWrite(direction_2, HIGH);
    client.publish(door2status, "Closing");
    close2 = false;
    closing2 = true;
    startTime_2 = millis();
    endTime_2 = startTime_1;
    timeOut2 == false;
  }

  if (closing2 == true) {
    endTime_2 = millis();
    sCount2 = String((int32_t)encoder2.getCount());
    iCount2 = sCount2.toInt();
    DEBUG_println(iCount2);

    door2_mA = ina219_door2.getCurrent_mA();

    if (door2_mA > closeCurrentLimit) {
      stopDoor_2();
      client.publish(door2status, "trouble(I)");
      DEBUG_println("Door closing stopped, over current!");
    }
  }

  if (closeSlow2 == true && opening2 == false) {
    ledcWriteChannel(pwmClose_2, slow);
    client.publish(door2status, "Closing slow");
    closeSlow2 = false;
  }

  if (digitalRead(closeLimit_2) == LOW && (closing2 == true)) {
    encoder2.setCount(0);
    DEBUG_println("counter 2 reset");
    stopDoor_2();
  }

  if ((digitalRead(stopButton_2) == LOW) || (digitalRead(beam) == LOW) || ((countPreset2 >= iCount2) && (closing2 == true))) {
    stopDoor_2();
  }

  if (((endTime_2 - startTime_2) >= closeTimeOut) && timeOut2 == false) {
    stopDoor_2();
    DEBUG_println("Door closing timed out!");
    client.publish(door2status, "trouble(T)");
    timeOut2 = true;
  }

  /************************  Publish data  *********************************/
  if (pubIP == true) {

    String ipaddress = WiFi.localIP().toString();
    char ipchar[ipaddress.length() + 1];
    ipaddress.toCharArray(ipchar, ipaddress.length() + 1);
    client.publish(ip, ipchar);

    pubIP = false;
  }

  if (pubStat1 == true) {

    sCount1 = String((int32_t)encoder1.getCount());
    iCount1 = sCount1.toInt();
    percentPosition1 = (((iCount1 * 100) / fullCount));

    DEBUG_print("Door 1 percent open ");
    DEBUG_println(percentPosition1);
    DEBUG_print("icount1 ");
    DEBUG_println(iCount1);
    DEBUG_print("fullCount ");
    DEBUG_println(fullCount);


    dtostrf(percentPosition1, 3, 0, cPercentPosition1);
    client.publish(door1position, cPercentPosition1);
    DEBUG_println("published Door 1 Position");

    pubStat1 = false;
  }

  if (pubStat2 == true) {

    sCount2 = String((int32_t)encoder2.getCount());
    iCount2 = sCount2.toInt();
    percentPosition2 = (((iCount2 * 100) / fullCount));

    DEBUG_print(" Door 2 percent open ");
    DEBUG_println(percentPosition2);
    DEBUG_print("icount2 ");
    DEBUG_println(iCount2);
    DEBUG_print("fullCount ");
    DEBUG_println(fullCount);


    dtostrf(percentPosition2, 3, 0, cPercentPosition2);
    client.publish(door2position, cPercentPosition2);
    DEBUG_println("published Door 2 Position");

    pubStat2 = false;
  }

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    ++value;

    busvoltageNew = ina219_door1.getBusVoltage_V();
    if (busvoltage != busvoltageNew) {
      char battery_voltage_str[5];
      dtostrf(busvoltageNew, 2, 1, battery_voltage_str);
      client.publish(Vbattery, battery_voltage_str);
      busvoltage = busvoltageNew;
    }
  }
}
//***********************  END MAIN LOOP  ********************************

void stopDoor_1() {
  ledcWriteChannel(pwmOpen_1, 0);
  ledcWriteChannel(pwmClose_1, 0);
  digitalWrite(direction_1, LOW);
  opening1 = false;
  closing1 = false;
  open1 = false;
  close1 = false;
  openSlow1 = false;
  closeSlow1 = false;
  client.publish(door1status, "Stopped");
  pubStat1 = true;
}

void stopDoor_2() {
  ledcWriteChannel(pwmOpen_2, 0);
  ledcWriteChannel(pwmClose_2, 0);
  digitalWrite(direction_2, LOW);
  opening2 = false;
  closing2 = false;
  open2 = false;
  close2 = false;
  openSlow2 = false;
  closeSlow2 = false;
  client.publish(door2status, "Stopped");
  pubStat2 = true;
}


//***************** OTA programming from inside Arduino IDE **************
void enableOTAfromIDE() {

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp32-[MAC]
  ArduinoOTA.setHostname("Overhead_Doors");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else  // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

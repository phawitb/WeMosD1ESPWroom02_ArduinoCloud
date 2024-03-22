/*
HC04
trig<--->D7
echo<--->D8

SCD41
SDA<--->D1
SCL<--->D2

*/



#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include <WiFiManager.h>
#include <TridentTD_LineNotify.h>
#include <bb_scd41.h>

#define LINE_TOKEN  "UYHD6lc2aghq35gegNnaqi8G86C5yUzp4iAuhFdJZ4P" 
#define SDA_PIN D1
#define SCL_PIN D2
#define BITBANG false
#define trig D7
#define echo D8
// const int trig = D7;
// const int echo = D8;

char *szAirQ[] = {(char *)"Good", (char *)"So-So", (char *)"Bad", (char *)"Danger", (char *)"Get out!"};
const char DEVICE_LOGIN_NAME[]  = "f8c14748-886d-4980-a607-cab20d077ced";
const char SSID[]               = "xBotan_2.4GHz";    // Network SSID (name)
const char PASS[]               = "Abcd1234";    // Network password (use for WPA, or use as key for WEP)
const char DEVICE_KEY[]  = "@ijw0dJ4mBX@s58V4vJeDd?MF";    // Secret device password

float distanceGroundToSensor;
float humidity;
float sealevel;
int batteryPercen;
int co2;
int sleeptime;

long duration, distance;
unsigned long startTime;
bool isReady = false;

CloudTemperature temperature;
SCD41 mySensor;
WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);
WiFiManager wifiManager;

void onDistanceGroundToSensorChange();
void onSleeptimeChange();

void initProperties(){

  ArduinoCloud.setBoardId(DEVICE_LOGIN_NAME);
  ArduinoCloud.setSecretDeviceKey(DEVICE_KEY);
  ArduinoCloud.addProperty(distanceGroundToSensor, READWRITE, ON_CHANGE, onDistanceGroundToSensorChange);
  ArduinoCloud.addProperty(humidity, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(sealevel, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(batteryPercen, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(co2, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(sleeptime, READWRITE, ON_CHANGE, onSleeptimeChange);
  ArduinoCloud.addProperty(temperature, READ, ON_CHANGE, NULL);

}

float calculateMean(float arr[], int size) {
    // Step 1: Sort the array
    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j < size; j++) {
            if (arr[i] > arr[j]) {
                // Swap elements if they are in the wrong order
                float temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
    }
    // Step 2: Remove first and last elements
    float sum = 0;
    for (int i = 1; i < size - 1; i++) {
        sum += arr[i];
    }
    // Step 3: Calculate the mean
    float mean = sum / (size - 2);
    return mean;
}

void setup() {
  
  //setup serial
  Serial.begin(9600);
  for(unsigned long const serialBeginTime = millis(); !Serial && (millis() - serialBeginTime > 5000); ) { }
  Serial.println("Wake up!!!......................");

  //setup wifi
  while(WiFi.status()!=WL_CONNECTED){
    Serial.println("-----------");
    if (!wifiManager.autoConnect("myIot01")) {
      Serial.println("NO");
      delay(3000);
      // ESP.reset();
      ESP.deepSleep(5*60*1e6); //if not connect wifi deepsleep 5min
      delay(5000);
    }
  }    
  Serial.print("WiFi.status()"); Serial.println(WiFi.status());
  
  //setup HC04
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  
  //setup arduino cloud
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::CONNECT, doThisOnConnect);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::SYNC, doThisOnSync);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::DISCONNECT, doThisOnDisconnect);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  //setup SCD41  
  if (mySensor.init(SDA_PIN, SCL_PIN, BITBANG, 100000) == SCD41_SUCCESS)
  {
    Serial.println("Found SCD41 sensor!");
    mySensor.start(); // start sampling mode
  } else { // can't find the sensor, stop
    Serial.println("SCD41 sensor not found");
    Serial.println("Check your connections");
    Serial.println("\nstopping...");
    while (1) {};
  }

  
  startTime = millis();

  //sent line notify
  LINE.setToken(LINE_TOKEN);
  LINE.notify("myIot01 online!!!!!!");
  
}

void loop() {
  //connect ArduinoCloud
  ArduinoCloud.update();
  if(ArduinoCloud.connected()!=1){
    if(millis()-startTime > 3*60*1000){
      Serial.print("Restart not connect Arduino!!ArduinoCloud.connected()");  Serial.println(ArduinoCloud.connected());
      ESP.restart();
      //ESP.deepSleep(5*60*1e6);
    }
  }

  if(isReady==true){
    // read setup variables from cloud
    Serial.print("distanceGroundToSensor"); Serial.println(distanceGroundToSensor);
    Serial.print("sleeptime"); Serial.println(sleeptime);

    float arrsealevel[10];
    float arrhumidity[10];
    float arrco2[10];
    float arrtemperature[10];
    float arrbatteryPercen[10];
    
    for(int ii = 0; ii < 10; ii++){
      //read SCD41
      char szTemp[64];
      int i, iCO2;
      mySensor.getSample();
      co2 = mySensor.co2();
      temperature = mySensor.temperature();
      humidity = mySensor.humidity();
    
      //read HC04
      digitalWrite(trig, LOW);
      delayMicroseconds(2);
      digitalWrite(trig, HIGH);
      delayMicroseconds(5);
      digitalWrite(trig, LOW);
      duration = pulseIn(echo, HIGH);
      distance = microsecondsToCentimeters(duration);
      sealevel = distanceGroundToSensor - distance;

      //batteryPercen
      int nVoltageRaw = analogRead(A0);
      // float fVoltage = (float)nVoltageRaw * 1.23;  // float fVoltage = (float)nVoltageRaw * 0.00486;
      batteryPercen = nVoltageRaw;
      Serial.print("nVoltageRaw"); Serial.println(nVoltageRaw);
      // Serial.print("fVoltage"); Serial.println(fVoltage);

      arrsealevel[ii]=sealevel;
      arrhumidity[ii]=humidity;
      arrco2[ii]=co2;
      arrtemperature[ii]=temperature;
      arrbatteryPercen[ii]=batteryPercen;
    
      Serial.print("distance"); Serial.println(distance);
      Serial.print("sealevel"); Serial.println(sealevel);
      Serial.print("humidity"); Serial.println(humidity);
      Serial.print("co2"); Serial.println(co2);
      Serial.print("temperature"); Serial.println(temperature);
      Serial.print("batteryPercen"); Serial.println(batteryPercen);
      
    }

    // Calculate mean
    int size;
    size = sizeof(arrsealevel) / sizeof(arrsealevel[0]);
    sealevel = calculateMean(arrsealevel, size);

    size = sizeof(arrhumidity) / sizeof(arrhumidity[0]);
    humidity = calculateMean(arrhumidity, size);

    size = sizeof(arrco2) / sizeof(arrco2[0]);
    co2 = calculateMean(arrco2, size);

    size = sizeof(arrtemperature) / sizeof(arrtemperature[0]);
    temperature = calculateMean(arrtemperature, size);

    size = sizeof(arrbatteryPercen) / sizeof(arrbatteryPercen[0]);
    batteryPercen = calculateMean(arrbatteryPercen, size);

    
    Serial.println("================");
    Serial.print("distance"); Serial.println(distance);
    Serial.print("sealevel"); Serial.println(sealevel);
    Serial.print("humidity"); Serial.println(humidity);
    Serial.print("co2"); Serial.println(co2);
    Serial.print("temperature"); Serial.println(temperature);
    Serial.print("batteryPercen"); Serial.println(batteryPercen);
    Serial.println("================");

    // sent to ArduinoCloud
    ArduinoCloud.update();
    delay(1000);

    // sleep
    Serial.print("Sleep!!.....sleeptime :");  Serial.println(sleeptime);

    // String txt = "temperature: " + String(temperature);
    // txt += "humidity: " + String(humidity);
    // txt += "sealevel: " + String(sealevel);
    // txt += "co2: " + String(co2);
    // txt += "batteryPercen: " + String(batteryPercen);
    // LINE.notify(txt);

    ESP.deepSleep(sleeptime*60*1e6);
    // delay(5000);
    // ESP.restart();

  }
  delay(1000);
  
  
}

long microsecondsToCentimeters(long microseconds){
  return microseconds / 29 / 2;
}
void doThisOnConnect(){
  Serial.println("Board successfully connected to Arduino IoT Cloud");
}
void doThisOnSync(){
  Serial.println("Thing Properties synchronised");
  isReady = true;
  
}
void doThisOnDisconnect(){
  Serial.println("Board disconnected from Arduino IoT Cloud");
}

void onDistanceGroundToSensorChange()  {
  // Add your code here to act upon DistanceGroundToSensor change
}

void onSleeptimeChange()  {
  // Add your code here to act upon Sleeptime change
}

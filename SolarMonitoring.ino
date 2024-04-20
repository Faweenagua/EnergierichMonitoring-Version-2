#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2CSen5x.h>
#include "INA226.h"
#include <HTTPClient.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "RTClib.h"
#include <LittleFS.h>

#define FORMAT_LITTLEFS_IF_FAILED true
int mydata;


// Your GPRS credentials (leave empty, if not needed)
const char apn[]      = "Internet"; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = ""; // GPRS User
const char gprsPass[] = ""; // GPRS Password

// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = ""; 

// Server details
// The server variable can be just a domain name or it can have a subdomain. It depends on the service you are using
const char server[] = "energierichsolar.com"; // domain name: example.com, maker.ifttt.com, etc
const char resource[] = "/connection.php";         // resource path, for example: /post-data.php
const int  port = 80;                             // server port number

// Keep this API Key value to be compatible with the PHP code provided in the project page. 
// If you change the apiKeyValue value, the PHP file /post-data.php also needs to have the same key 
String apiKeyValue = "tPmAT5Ab3j7F9";

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26


// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

// Define the serial console for debug prints, if needed
//#define DUMP_AT_COMMANDS

#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, Serial);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif



// TinyGSM Client for Internet connection
TinyGsmClient client(modem);


// LEDs
#define BLUELED 27
#define GREENLED 26


bool GSMConnectionStatus = false;
String localData = "";
bool localDataAvailable = false;


/// -------- RTC Module ----------///
RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

String localDateTime = "";
String timeHour = "";
int timeHourInt = 0;
String timeMinute = "";
String timeSecond = "";
String dateYear = "";
String dateMonth = "";
String dateDay = "";
String dateSend = "";
String timeSend = "";


volatile int greenState = 0;

////................SLEEP Mode...........//
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */ // SHOULD BE 15 MINS 900s
int sleepNow = 0;


String userID = "0";
String deviceID = "ASHESI";



////..........INA226............//

float panel1Current = 0.0;
float panel1Voltage = 0.0;
float panel1Power = 0.0;

float panel2Current = 0.0;
float panel2Voltage = 0.0;
float panel2Power = 0.0;

float panel3Current = 0.0;
float panel3Voltage = 0.0;
float panel3Power = 0.0;

float panel4Current = 0.0;
float panel4Voltage = 0.0;
float panel4Power = 0.0;

float panelCurrentTest = 0.0;
float chargeControlCurrentControl = 0.0;
float chargeControlCurrentTest = 0.0;
float panelVoltageControl = 0.0;
float panelVoltageTest = 0.0;
float chargeControlVoltageControl = 0.0;
float chargeControlVoltageTest = 0.0;
float panelPowerControl = 0.0;
float panelPowerTest = 0.0;
float chargeControlPowerControl = 0.0;
float chargeControlPowerTest = 0.0;

INA226 panelOne(0x40);
INA226 panelTwo(0x41);
INA226 panelThree(0x44);
INA226 panelFour(0x45);


////..........SD Card............//
// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;

// Variables to hold sensor readings
float temp = 1;
float hum = 2;
float pres = 4;
String dataMessage;
unsigned long epochTime = 122323;


////..........PM Sensor............//
// The used commands use up to 48 bytes. On some Arduino's the default buffer
// space is not large enough
#define MAXBUF_REQUIREMENT 48

#if (defined(I2C_BUFFER_LENGTH) &&                 \
     (I2C_BUFFER_LENGTH >= MAXBUF_REQUIREMENT)) || \
    (defined(BUFFER_LENGTH) && BUFFER_LENGTH >= MAXBUF_REQUIREMENT)
#define USE_PRODUCT_INFO
#endif

SensirionI2CSen5x sen5x;

float massConcentrationPm1p0;
float massConcentrationPm2p5;
float massConcentrationPm4p0;
float massConcentrationPm10p0;
float ambientHumidity;
float ambientTemperature;
float vocIndex;
float noxIndex;


int sendDataStatus = 0;

void setup() {
    
    Wire.begin();
    Serial.begin(9600); 
    while (!Serial) {
        delay(100);
    }
    delay(1000); // let serial console settle

    pinMode(BLUELED, OUTPUT);
    pinMode(GREENLED, OUTPUT);


    digitalWrite(BLUELED, HIGH);
    digitalWrite(GREENLED, LOW);


    

    ////..........RTC Module............//

      if (! rtc.begin()) {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        // while (1) delay(10);
      }

      if (rtc.lostPower()) {
        Serial.println("RTC lost power, let's set the time!");
        // When time needs to be set on a new device, or after a power loss, the
        // following line sets the RTC to the date & time this sketch was compiled
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        // This line sets the RTC with an explicit date & time, for example to set
        // January 21, 2014 at 3am you would call:
        // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
      }

    
    ////..........SD Card............//

    // initSDCard();
  
    if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
      Serial.println("LittleFS Mount Failed");
      return;
    }
   else{
      Serial.println("Little FS Mounted Successfully");
    } 

   // Check if the file already exists to prevent overwritting existing data
   bool fileexists = LittleFS.exists("/data.txt");
   Serial.print(fileexists);
   if(!fileexists) {
      Serial.println("File doesn’t exist");
      Serial.println("Creating file...");
      // Create File and add header
      writeFile(LittleFS, "/data.txt", "MY ESP32 DATA \r\n");
   }
   else {
      Serial.println("File already exists");
   }

    ////..........INA226............//
    // Initialize the INA226.
    if (!panelOne.begin() )
    {
      Serial.println("panelOne power sensor could not connect. Fix and Reboot");
    }
    if (!panelTwo.begin() )
    {
      Serial.println("panelTwo power sensor could not connect. Fix and Reboot");
    }
    if (!panelThree.begin() )
    {
      Serial.println("panelThree power sensor could not connect. Fix and Reboot");
    }
    if (!panelFour.begin() )
    {
      Serial.println("panelFour power sensor could not connect. Fix and Reboot");
    }
    panelOne.setMaxCurrentShunt(1, 0.002);
    panelTwo.setMaxCurrentShunt(1, 0.002);
    panelThree.setMaxCurrentShunt(1, 0.002);
    panelFour.setMaxCurrentShunt(1, 0.002);



    ////..........PM Sensor............//
    sen5x.begin(Wire);

    uint16_t error;
    char errorMessage[256];
    error = sen5x.deviceReset();
    if (error) {
        Serial.print("Error trying to execute deviceReset(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

    // Print SEN55 module information if i2c buffers are large enough
    #ifdef USE_PRODUCT_INFO
        printSerialNumber();
        printModuleVersions();
    #endif

    // set a temperature offset in degrees celsius
    // Note: supported by SEN54 and SEN55 sensors
    // By default, the temperature and humidity outputs from the sensor
    // are compensated for the modules self-heating. If the module is
    // designed into a device, the temperature compensation might need
    // to be adapted to incorporate the change in thermal coupling and
    // self-heating of other device components.
    //
    // A guide to achieve optimal performance, including references
    // to mechanical design-in examples can be found in the app note
    // “SEN5x – Temperature Compensation Instruction” at www.sensirion.com.
    // Please refer to those application notes for further information
    // on the advanced compensation settings used
    // in `setTemperatureOffsetParameters`, `setWarmStartParameter` and
    // `setRhtAccelerationMode`.
    //
    // Adjust tempOffset to account for additional temperature offsets
    // exceeding the SEN module's self heating.
    float tempOffset = 0.0;
    error = sen5x.setTemperatureOffsetSimple(tempOffset);
    if (error) {
        Serial.print("Error trying to execute setTemperatureOffsetSimple(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("Temperature Offset set to ");
        Serial.print(tempOffset);
        Serial.println(" deg. Celsius (SEN54/SEN55 only");
    }

    // Start Measurement
    error = sen5x.startMeasurement();
    if (error) {
        Serial.print("Error trying to execute startMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

  ///..........GSM Connection..........//
  
  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  // modem.init();
  GSMConnectionStatus = modem.restart();
  // use modem.init() if you don't need the complete restart

  delay(10000);

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }


  ///..........Sleep Mode Config.........//
  /*
  First we configure the wake up source
  We set our ESP32 to wake up every 5 seconds
  */
  // esp_sleep_enable_ext0_wakeup(GPIO_NUM_25,0); //1 = High, 0 = Low
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

  // Turn off network status lights to reduce current consumption
  turnOffNetlight();

}

unsigned long previousRuntime = 0; 
unsigned long previousLocalDataRuntime = 0;

void loop() {

  sendDataStatus = 0;

  onBlue();
  onGreen();

  ///..........Reconnect Wi-Fi..........//
  // reconnectWiFi();

  ///..........Transmit Local Data To Server..........//
  if(millis() - previousLocalDataRuntime >= 1200000){
    checkAndSendLocalDataToServer();
    previousLocalDataRuntime = millis();
  }
  
  // If GSM is connected send data to server else save it locally
  if (previousRuntime == 0 || millis() - previousRuntime > 600000)
    {

      // mydata = random (0, 1000);
      // appendFile(LittleFS, "/data.txt", (String(mydata)+ "\r\n").c_str()); //Append data to the file
      // readFile(LittleFS, "/data.txt"); // Read the contents of the file

      localData = "";
      writeFile(LittleFS,"/data.txt",localData.c_str());

      ///..........PM Sensor............//
      readPMSensorData();

      ///..........INA226............//
      readINA226SensorData();

      getTimeAndDate();


      String requestData = "&device_id="+ deviceID + "&panel1Current=" + String(panel1Current)+ "&panel1Voltage=" + String(panel1Voltage) + "&panel1Power=" + String(panel1Power) + 
                          "&panel2Current=" + String(panel2Current) + "&panel2Voltage=" + String(panel2Voltage) + "&panel2Power=" + 
                          String(panel2Power) + "&panel3Current=" + String(panel3Current) + "&panel3Voltage=" + String(panel3Voltage) + "&panel3Power=" + 
                          String(panel3Power) + "&panel4Current=" + String(panel4Current) + "&panel4Voltage=" + String(panel4Voltage) + "&panel4Power=" + String(panel4Power) + "&pm_1=" + String(massConcentrationPm1p0) +  
                          "&pm_2_5=" + String(massConcentrationPm2p5) + "&pm_4=" + String(massConcentrationPm4p0) + "&pm_10=" + String(massConcentrationPm10p0) + "&humidity=" + String(ambientHumidity) + "&temperature=" + String(ambientTemperature)+ "&VOC=" + String(vocIndex) + "&NOx=" + String(noxIndex) + 
                          "&deviceLocalTime=" + timeSend + "&deviceLocalDate=" + dateSend + "";

      if (GSMConnectionStatus)
        {
          Serial.println(requestData);
          //Check if upload was successful else store data locally
          if(!sendHTTPRequestViaGSM(requestData)){
            requestData = requestData + "|";
            appendFile(LittleFS,"/data.txt",requestData.c_str());
          }
        }else
        {
          requestData = requestData + "|";
          appendFile(LittleFS,"/data.txt",requestData.c_str());
        }


      ///...........Save to SD Card.............//
      //Concatenate all info separated by commas
      // dataMessage = String(epochTime) + "," + String(deviceID) + "," + String(ambientTemperature) + "," + String(ambientHumidity) + "," + String(panel1Current) + "," + String(panel1Voltage) + "," + String(panel1Power) + "," + String(panel2Current) + "," + String(panel2Voltage) + "," + String(panel2Power) + "," + String(panel3Current) + "," + String(panel3Voltage) + "," + String(panel3Power) + "," + String(panel4Current) + "," + String(panel4Voltage) + "," + String(panel4Power) + "," + String(massConcentrationPm1p0) + "," + String(massConcentrationPm2p5) + "," + String(massConcentrationPm4p0) + "," + String(massConcentrationPm10p0) + "," + String(vocIndex) + "," + String(noxIndex) + "\r\n";

      // Serial.print("Saving data: ");
      // Serial.println(dataMessage);

      //Append the data to file
      // appendFile(SD, "/data.txt", dataMessage.c_str());

      ///..........Transmit Local Data To Server..........//
      if(millis() - previousLocalDataRuntime >= 600000){
        checkAndSendLocalDataToServer();
        previousLocalDataRuntime = millis();
      }

      previousRuntime = millis();
    }

}




//----------- FUNCTIONS ---------//

void onGreen() {
  if (GSMConnectionStatus)
  {
    digitalWrite(GREENLED, HIGH);
  } else {
    digitalWrite(GREENLED, LOW);
  }
}

void onBlue() {
  if (!GSMConnectionStatus)
  {
    digitalWrite(BLUELED, LOW);
  } else {
    digitalWrite(BLUELED, HIGH);
  }
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void printModuleVersions() {
  uint16_t error;
  char errorMessage[256];

  unsigned char productName[32];
  uint8_t productNameSize = 32;

  error = sen5x.getProductName(productName, productNameSize);

  if (error) {
      Serial.print("Error trying to execute getProductName(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  } else {
      Serial.print("ProductName:");
      Serial.println((char*)productName);
  }

  uint8_t firmwareMajor;
  uint8_t firmwareMinor;
  bool firmwareDebug;
  uint8_t hardwareMajor;
  uint8_t hardwareMinor;
  uint8_t protocolMajor;
  uint8_t protocolMinor;

  error = sen5x.getVersion(firmwareMajor, firmwareMinor, firmwareDebug,
                            hardwareMajor, hardwareMinor, protocolMajor,
                            protocolMinor);
  if (error) {
      Serial.print("Error trying to execute getVersion(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  } else {
      Serial.print("Firmware: ");
      Serial.print(firmwareMajor);
      Serial.print(".");
      Serial.print(firmwareMinor);
      Serial.print(", ");

      Serial.print("Hardware: ");
      Serial.print(hardwareMajor);
      Serial.print(".");
      Serial.println(hardwareMinor);
  }
}

void printSerialNumber() {
    uint16_t error;
    char errorMessage[256];
    unsigned char serialNumber[32];
    uint8_t serialNumberSize = 32;

    error = sen5x.getSerialNumber(serialNumber, serialNumberSize);
    if (error) {
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("SerialNumber:");
        Serial.println((char*)serialNumber);
    }
}

void readPMSensorData(){
    Serial.print("**********PM Sensor**********\n");
    uint16_t error;
    char errorMessage[256];

    delay(1000);

    // Read Measurement
    
    error = sen5x.readMeasuredValues(
        massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
        massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex,
        noxIndex);

    if (error) {
        Serial.print("Error trying to execute readMeasuredValues(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("MassConcentrationPm1p0:");
        Serial.print(massConcentrationPm1p0);
        Serial.print("\t");
        Serial.print("MassConcentrationPm2p5:");
        Serial.print(massConcentrationPm2p5);
        Serial.print("\t");
        Serial.print("MassConcentrationPm4p0:");
        Serial.print(massConcentrationPm4p0);
        Serial.print("\t");
        Serial.print("MassConcentrationPm10p0:");
        Serial.print(massConcentrationPm10p0);
        Serial.print("\t");
        Serial.print("AmbientHumidity:");
        if (isnan(ambientHumidity)) {
            Serial.print("n/a");
        } else {
            Serial.print(ambientHumidity);
        }
        Serial.print("\t");
        Serial.print("AmbientTemperature:");
        if (isnan(ambientTemperature)) {
            Serial.print("n/a");
        } else {
            Serial.print(ambientTemperature);
        }
        Serial.print("\t");
        Serial.print("VocIndex:");
        if (isnan(vocIndex)) {
            Serial.print("n/a");
        } else {
            Serial.print(vocIndex);
        }
        Serial.print("\t");
        Serial.print("NoxIndex:");
        if (isnan(noxIndex)) {
            Serial.println("n/a");
        } else {
            Serial.println(noxIndex);
        }
    }
}

void readINA226SensorData(){
  Serial.print("**********INA226**********\n");

  Serial.println("\n#\tBUS\tSHUNT\tCURRENT\tPOWER");

  panel1Current = panelOne.getCurrent_mA();
  panel1Voltage = panelOne.getBusVoltage();
  panel1Power = panelOne.getPower_mW();

  panel2Current = panelTwo.getCurrent_mA();
  panel2Voltage = panelTwo.getBusVoltage();
  panel2Power = panelTwo.getPower_mW();

  panel3Current = panelThree.getCurrent_mA();
  panel3Voltage = panelThree.getBusVoltage();
  panel3Power = panelThree.getPower_mW();

  panel4Current = panelFour.getCurrent_mA();
  panel4Voltage = panelFour.getBusVoltage();
  panel4Power = panelFour.getPower_mW();

    Serial.print("Panel One");
    Serial.print("\t");
    Serial.print(panel1Voltage, 3);
    Serial.print("\t");
    Serial.print(panelOne.getShuntVoltage_mV(), 3);
    Serial.print("\t");
    Serial.print(panel1Current, 3);
    Serial.print("\t");
    Serial.print(panel1Power, 3);
    Serial.println();
    
    Serial.print("Panel Two");
    Serial.print("\t");
    Serial.print(panel2Voltage, 3);
    Serial.print("\t");
    Serial.print(panelTwo.getShuntVoltage_mV(), 3);
    Serial.print("\t");
    Serial.print(panel2Current, 3);
    Serial.print("\t");
    Serial.print(panel2Power, 3);
    Serial.println();

    Serial.print("Panel Three");
    Serial.print("\t");
    Serial.print(panel3Voltage, 3);
    Serial.print("\t");
    Serial.print(panelThree.getShuntVoltage_mV(), 3);
    Serial.print("\t");
    Serial.print(panel3Current, 3);
    Serial.print("\t");
    Serial.print(panel3Power, 3);
    Serial.println();

    Serial.print("Panel Four");
    Serial.print("\t");
    Serial.print(panel4Voltage, 3);
    Serial.print("\t");
    Serial.print(panelFour.getShuntVoltage_mV(), 3);
    Serial.print("\t");
    Serial.print(panel4Current, 3);
    Serial.print("\t");
    Serial.print(panel4Power, 3);
    Serial.println();
}

// Initialize SD card
void initSDCard(){
   if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

// Write to the SD card
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card
void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
      Serial.println("Failed to open file for appending");
      return;
  }
  if(file.print(message)){
      Serial.println("Message appended");
  } else {
      Serial.println("Append failed");
  }
  file.close();
}



String readFile(fs::FS &fs, const char * path){

  String fileData = "";

  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
      Serial.println("Failed to open file for reading");
      return String();
  }

  // Serial.print("Read from file: ");
  while(file.available()){
      fileData = file.readStringUntil('\n');
      break;
  }
  file.close();

  return fileData;
}

void getTimeAndDate(){
  Serial.println("------TIME------");

  DateTime now = rtc.now();
  unsigned long epochTime = now.unixtime();

  dateYear = String(now.year());
  dateMonth = String(now.month());
  dateDay = String(now.day());
  timeHour = String(now.hour());
  timeHourInt = now.hour();
  timeMinute = String(now.minute());
  timeSecond = String(now.second());

  dateSend = dateYear + "-" + dateMonth + "-" + dateDay;
  timeSend = timeHour + ":" + timeMinute + ":" + timeSecond;

  Serial.print(dateSend);
  Serial.print("  ");
  Serial.println(timeSend);

}

bool sendLocalData(String localData){

  int lengthOfData = localData.length();
  int numberOfDataRows = (lengthOfData/70)+2;

  String dataRows[numberOfDataRows];
  int StringCount = 0;

    // Split the string into substrings
  while (localData.length() > 0)
  {
    int index = localData.indexOf('|');
    if (index == -1) // No space found
    {
      dataRows[StringCount++] = localData;
      break;
    }
    else
    {
      dataRows[StringCount++] = localData.substring(0, index);
      localData = localData.substring(index+1);
    }
  }

  for (int i = 0; i < StringCount; i++)
  {
    Serial.print("Sending local data.................(");
    Serial.print(i+1);
    Serial.print("/");
    Serial.print(StringCount);
    Serial.println(")");


    String dataRow = dataRows[i];
    if(!sendHTTPRequestViaGSM(dataRow)){
      return false;
    }
    // Serial.println(dataRow);
  }
  return true;
}

bool sendHTTPRequestViaGSM(String httpRequestData){
  Serial.print("Connecting to APN: ");
  Serial.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(" fail");
  }
  else {
    Serial.println(" OK");
    
    Serial.print("Connecting to ");
    Serial.print(server);
    if (!client.connect(server, port)) {
      Serial.println(" fail");
      return false;
    }
    else {
      Serial.println(" OK");
    
      // Making an HTTP POST request
      Serial.println("Performing HTTP POST request...");
      client.print(String("POST ") + resource + " HTTP/1.1\r\n");
      client.print(String("Host: ") + server + "\r\n");
      client.println("Connection: close");
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.println(httpRequestData.length());
      client.println();
      client.println(httpRequestData);

      unsigned long timeout = millis();
      while (client.connected() && millis() - timeout < 10000L) {
        // Print available data (HTTP response from server)
        while (client.available()) {
          char c = client.read();
          Serial.print(c);
          timeout = millis();
        }
      }
      Serial.println();
      return true;
    }
  }
}

void checkAndSendLocalDataToServer(){
  ///..........Setup Local Data Storage..........//
    localData = readFile(LittleFS, "/data.txt");
    
    //check if there is data available for transmission
    if(localData.length()>10){
      localDataAvailable = true;
      Serial.println("Local data available for transmission.");
      
      //If data is available check for wifi connection
      if (GSMConnectionStatus)
      {

        // if wifi is connected send data to server
        if(sendLocalData(localData)){
          // if sending is successful, clear local data
          localData = "";
          writeFile(LittleFS,"/data.txt",localData.c_str());
          localDataAvailable = false;
          Serial.println("Local Data Transmitted Successfully.");
        }else
        {
          // if sending is unsuccessful update local data
          writeFile(LittleFS,"/data.txt",localData.c_str());
          Serial.print("Local Data Transmission Failed.");
        }
        
      }else
      {
        Serial.println("No Wi-Fi Connection -- Local Data Could not be Transmitted.");
      }
      
      // sendLocalData(localData);
      
    }else
    {
      localDataAvailable = false;
      Serial.println("No local data available for transmission.");
    }
}

void turnOffNetlight()
{
    Serial.println("Turning off SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=0");
}

void turnOnNetlight()
{
    Serial.println("Turning on SIM800 Red LED...");
    modem.sendAT("+CNETLIGHT=1");
}

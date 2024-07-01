#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>
#include <FlashStorage.h>
#include <TimeLib.h>

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// IMU sensor
Adafruit_BNO055 bno = Adafruit_BNO055(55);

// Software serial for WiFi module
SoftwareSerial wifiSerial(6, 7); // RX, TX pins for WiFi module

// Float switch pin
const int floatSwitchPin = 2;
bool floatSwitchState = false;

// Data structure for storing settings
struct Settings {
  char vesselName[32];
  char contactNumbers[3][16];
  float listAngleThreshold;
  float trimAngleThreshold;
};

FlashStorage(settingsStorage, Settings);
Settings settings;
float initialEuler[3];
float currentEuler[3];

void setup() {
  Serial.begin(9600);
  wifiSerial.begin(9600);

  pinMode(floatSwitchPin, INPUT);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  showLogo();
  countdown(30);

  if (!bno.begin()) {
    Serial.println("No BNO055 detected");
    while (1);
  }

  bno.setExtCrystalUse(true);
  calibrateIMU();

  WiFiManager wifiManager;
  wifiManager.autoConnect("Vessel_Setup_AP");

  loadSettings();

  display.clearDisplay();
  display.display();
}

void loop() {
  if (digitalRead(floatSwitchPin) == HIGH) {
    floatSwitchState = true;
    checkIMU();
    sendReports();
  } else {
    floatSwitchState = false;
  }

  delay(60000); // Check every minute
}

void showLogo() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Company");
  display.display();
  delay(5000);
}

void countdown(int seconds) {
  for (int i = seconds; i > 0; i--) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Countdown: ");
    display.print(i);
    display.display();
    delay(1000);
  }
  display.clearDisplay();
  display.display();
}

void calibrateIMU() {
  sensors_event_t event;
  bno.getEvent(&event);
  initialEuler[0] = event.orientation.x;
  initialEuler[1] = event.orientation.y;
  initialEuler[2] = event.orientation.z;
}

void checkIMU() {
  sensors_event_t event;
  bno.getEvent(&event);
  currentEuler[0] = event.orientation.x;
  currentEuler[1] = event.orientation.y;
  currentEuler[2] = event.orientation.z;

  for (int i = 0; i < 3; i++) {
    if (currentEuler[i] - initialEuler[i] > 180.0) currentEuler[i] -= 360.0;
    if (currentEuler[i] - initialEuler[i] < -180.0) currentEuler[i] += 360.0;
  }

  float listDiff = currentEuler[2] - initialEuler[2];
  float trimDiff = currentEuler[0] - initialEuler[0];

  if (abs(listDiff) >= settings.listAngleThreshold || abs(trimDiff) >= settings.trimAngleThreshold) {
    sendAlert(listDiff, trimDiff);
  }
}

void sendAlert(float listDiff, float trimDiff) {
  for (int i = 0; i < 3; i++) {
    wifiSerial.print("AT+CMGF=1\r");
    delay(100);
    wifiSerial.print("AT+CMGS=\"");
    wifiSerial.print(settings.contactNumbers[i]);
    wifiSerial.print("\"\r");
    delay(100);
    wifiSerial.print("Alert! Vessel List: ");
    wifiSerial.print(listDiff);
    wifiSerial.print(", Trim: ");
    wifiSerial.print(trimDiff);
    wifiSerial.write(26);
    delay(1000);
  }
}

void sendReports() {
  int currentHour = hour();
  if (currentHour == 8 || currentHour == 12 || currentHour == 18) {
    for (int i = 0; i < 3; i++) {
      wifiSerial.print("AT+CMGF=1\r");
      delay(100);
      wifiSerial.print("AT+CMGS=\"");
      wifiSerial.print(settings.contactNumbers[i]);
      wifiSerial.print("\"\r");
      delay(100);
      wifiSerial.print("Vessel Report at ");
      wifiSerial.print(currentHour);
      wifiSerial.print(":00\nList: ");
      wifiSerial.print(currentEuler[2] - initialEuler[2]);
      wifiSerial.print("\nTrim: ");
      wifiSerial.print(currentEuler[0] - initialEuler[0]);
      wifiSerial.write(26);
      delay(1000);
    }
  }
}

void loadSettings() {
  settings = settingsStorage.read();
  if (settings.vesselName[0] == '\0') {
    strcpy(settings.vesselName, "Default Vessel");
    strcpy(settings.contactNumbers[0], "+1234567890");
    strcpy(settings.contactNumbers[1], "+0987654321");
    strcpy(settings.contactNumbers[2], "+1122334455");
    settings.listAngleThreshold = 5.0;
    settings.trimAngleThreshold = 5.0;
    settingsStorage.write(settings);
  }
}

void updateSettings(const char* vesselName, const char* contact1, const char* contact2, const char* contact3, float listThreshold, float trimThreshold) {
  strcpy(settings.vesselName, vesselName);
  strcpy(settings.contactNumbers[0], contact1);
  strcpy(settings.contactNumbers[1], contact2);
  strcpy(settings.contactNumbers[2], contact3);
  settings.listAngleThreshold = listThreshold;
  settings.trimAngleThreshold = trimThreshold;
  settingsStorage.write(settings);
}

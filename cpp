#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LSM6DS3TRC.h>
#include <SoftwareSerial.h>

// OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// IMU sensor
Adafruit_LSM6DS3TRC lsm6ds3trc;

// WiFi and SIM800L modules
SoftwareSerial wifiSerial(2, 3); // RX, TX for WiFi module
SoftwareSerial sim800l(4, 5);    // RX, TX for SIM800L

// Company logo
const unsigned char company_logo [] PROGMEM = {
  // Add your company logo bitmap data here
};

// Calibration data
float calibRoll = 0, calibPitch = 0;

// Contact numbers and threshold angles
String vesselName;
String contact1, contact2, contact3;
float alertRollThreshold = 3.0;  // Default Roll Alert Threshold in degrees
float alertPitchThreshold = 3.0; // Default Pitch Alert Threshold in degrees

// Functions
void showLogo() {
  display.clearDisplay();
  display.drawBitmap(0, 0, company_logo, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
  display.display();
  delay(5000);
}

void countdown() {
  for (int i = 30; i > 0; i--) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("Countdown: ");
    display.print(i);
    display.display();
    delay(1000);
  }
  display.clearDisplay();
  display.display();
}

void calibrateIMU() {
  sensors_event_t accel;
  lsm6ds3trc.getEvent(&accel, NULL, NULL);
  // Convert to Euler angles
  calibRoll = atan2(accel.acceleration.y, accel.acceleration.z) * 180 / PI;
  calibPitch = atan2(-accel.acceleration.x, sqrt(accel.acceleration.y * accel.acceleration.y + accel.acceleration.z * accel.acceleration.z)) * 180 / PI;
}

void correctAngles(float &roll, float &pitch) {
  if (roll < 0) roll += 360;
  if (pitch < 0) pitch += 360;
  if (roll >= 360) roll -= 360;
  if (pitch >= 360) pitch -= 360;
}

void getInputFromWiFi() {
  Serial.println("Waiting for data from WiFi module...");

  // Assuming data is sent in the format: vesselName,alertRoll,alertPitch,contact1,contact2,contact3
  while (!wifiSerial.available());
  String data = wifiSerial.readStringUntil('\n');
  int index = 0;

  vesselName = data.substring(index, data.indexOf(',', index));
  index = data.indexOf(',', index) + 1;
  
  alertRollThreshold = data.substring(index, data.indexOf(',', index)).toFloat();
  if (alertRollThreshold == 0) alertRollThreshold = 3.0; // Default to 3 degrees
  index = data.indexOf(',', index) + 1;

  alertPitchThreshold = data.substring(index, data.indexOf(',', index)).toFloat();
  if (alertPitchThreshold == 0) alertPitchThreshold = 3.0; // Default to 3 degrees
  index = data.indexOf(',', index) + 1;

  contact1 = data.substring(index, data.indexOf(',', index));
  index = data.indexOf(',', index) + 1;

  contact2 = data.substring(index, data.indexOf(',', index));
  index = data.indexOf(',', index) + 1;

  contact3 = data.substring(index);

  Serial.println("Data received:");
  Serial.println("Vessel Name: " + vesselName);
  Serial.println("Roll Alert Set Point: " + String(alertRollThreshold));
  Serial.println("Pitch Alert Set Point: " + String(alertPitchThreshold));
  Serial.println("Contact Number 1: " + contact1);
  Serial.println("Contact Number 2: " + contact2);
  Serial.println("Contact Number 3: " + contact3);
}

void sendDataToGoogleSheets(float roll, float pitch, float batteryLevel) {
  // Your logic to send data to Google Sheets using GSM
}

void resetCalibration() {
  calibrateIMU();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  wifiSerial.begin(9600);
  sim800l.begin(9600);

  showLogo();
  countdown();

  if (!lsm6ds3trc.begin_I2C()) {
    Serial.println("Failed to initialize LSM6DS3TRC!");
    while (1);
  }

  calibrateIMU();

  getInputFromWiFi(); // Get vessel name, contact numbers, and alert thresholds
}

void loop() {
  sensors_event_t accel;
  lsm6ds3trc.getEvent(&accel, NULL, NULL);

  float roll = atan2(accel.acceleration.y, accel.acceleration.z) * 180 / PI;
  float pitch = atan2(-accel.acceleration.x, sqrt(accel.acceleration.y * accel.acceleration.y + accel.acceleration.z * accel.acceleration.z)) * 180 / PI;
  
  correctAngles(roll, pitch);

  float diffRoll = fabs(roll - calibRoll);
  float diffPitch = fabs(pitch - calibPitch);

  if (diffRoll > alertRollThreshold || diffPitch > alertPitchThreshold) {
    float batteryLevel = analogRead(A0) * (3.3 / 1023.0);
    sendDataToGoogleSheets(diffRoll, diffPitch, batteryLevel);
  }

  // Listen for a command to reset calibration (example logic)
  if (Serial.available()) {
    String command = Serial.readString();
    if (command == "RESET") {
      resetCalibration();
    }
  }

  // Example to turn off OLED after countdown
  static bool oledOff = false;
  if (!oledOff) {
    delay(30000); // Wait for 30 seconds
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    oledOff = true;
  }
}

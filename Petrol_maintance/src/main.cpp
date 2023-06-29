#include <WiFi.h>
#include <HTTPClient.h>
#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FirebaseESP32.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Wi-Fi credentials
const char* ssid = "khai";
const char* password = "Wimddddd";

// Firebase configuration
#define FIREBASE_HOST   "htnpetrol-491aa-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH   "jjV4zSjvIHeSKmdwNJZLP56YYdv2uDuOYirZWdUB"
const String firebasePath = "/id/";
// Firebase database path
const String TemperaturePath = "/temperature";
const String Fuel_levelPath = "/fuel_level";

// LCD configuration
const int lcdColumns = 16;
const int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows); 

// HC-SR04 pins
const int trigPin = 32;
const int echoPin = 13;

// MFRC522 pins
#define RST_PIN     4
#define SS_PIN      5
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Temperature sensor configuration
const int oneWireBus = 15;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// Servo configuration
const int servoPin = 33;
Servo myServo;

// Define sound speed in cm/uS
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

FirebaseData firebaseData;

void readTemperatureAndDistance(void* parameter) {
  while (true) {
    // Temperature measurement
    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);
    float temperatureF = sensors.getTempFByIndex(0);

    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.print("°C ");

    if (Firebase.setFloat(firebaseData, TemperaturePath, temperatureC)) {
      Serial.println("Temperature sent to Firebase successfully");
    } else {
      Serial.println("Failed to send Temperature to Firebase");
    }

    // Distance measurement
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH);
    float distanceCm = duration * SOUND_SPEED / 2;
    String fuel_level = "";

    Serial.print("Distance (cm): ");
    Serial.println(distanceCm);
    if(distanceCm>50) {
      fuel_level = "Da het nhien lieu trong thung chua";
    } else if(distanceCm>20 && distanceCm<=50) {
      fuel_level = "Thung chua sap het nhien lieu";
    } else {
      fuel_level = "Thung chua van con day nhien lieu";
    }

    if (Firebase.setString(firebaseData, Fuel_levelPath, fuel_level)) {
      Serial.println("Temperature sent to Firebase successfully");
    } else {
      Serial.println("Failed to send Temperature to Firebase");
    }
     // Display temperature and fuel level on LCD
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperatureC);
    lcd.print(" C");

    lcd.setCursor(0, 1);
    lcd.print("Fuel: ");
    lcd.print(distanceCm);

    delay(2000);

  }
}

void checkIdAndControlServo(void* parameter) {
  while (true) {
    // // Look for new RFID cards
    // Serial.println("Vao chuong trinh check ID:");
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    // Get the RFID UID
    String rfidUid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      rfidUid.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
      rfidUid.concat(String(mfrc522.uid.uidByte[i], HEX));
    }
    
    // Print the RFID UID
    Serial.print("Scanned RFID UID: ");
    Serial.println(rfidUid);

      // Build the Firebase path
      String firebasePath = "/delivery_person/" + rfidUid;

      // Check if the RFID UID exists in Firebase
      if (Firebase.get(firebaseData, firebasePath)) {
        Serial.println("RFID UID found in Firebase!");

        // Turn servo to 180 degrees
        myServo.write(180);
        delay(4000);

        // Return servo to 0 degrees after 1 second
        myServo.write(0);
      } else {
        Serial.println("RFID UID not found in Firebase.");
      }
    }

    delay(500); // Delay before checking again
  }
}

void setup() {
  Serial.begin(9600);
   // Initialize LCD
  lcd.begin(lcdColumns, lcdRows);
  // initialize LCD
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  Wire.setClock(400000); // Giảm tốc độ xung clock I2C xuống 400kHz

  // Initialize servo
  myServo.attach(servoPin);
  myServo.write(0); // Initialize servo position to 0 degrees

  // Initialize MFRC522
  SPI.begin();
  mfrc522.PCD_Init();

  // Initialize temperature sensor
  sensors.begin();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Create the two tasks
  xTaskCreatePinnedToCore(
    readTemperatureAndDistance,
    "TemperatureAndDistanceTask",
    10000,
    NULL,
    2,
    NULL,
    0
  );
// gia tri "2" thể hiện mức độ ưu tiên của tác vụ nhiệt độ và khoảng cách cao hơn so với tác vụ check id bơm nhiên liệu vào thùng
  xTaskCreatePinnedToCore(
    checkIdAndControlServo,
    "CheckIdAndControlServoTask",
    10000,
    NULL,
    1,
    NULL,
    1
  );
}

void loop() {
  // Empty loop as the tasks will handle the functionality
}


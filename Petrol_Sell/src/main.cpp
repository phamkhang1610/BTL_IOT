#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Wire.h>
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {12, 14, 27, 26}; // Các chân hàng kết nối với Arduino
byte colPins[COLS] = {25, 33, 32};    // Các chân cột kết nối với Arduino

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
int enteredAmount = 0; // Biến để lưu số tiền đã nhập
int selectedFuel = 0; // Biến để lưu loại xăng đã chọn (0: không có chọn, 1: Ron 92, 2: Ron 95)

Servo servo1;
Servo servo2;

#define SENSOR 4
#define ENA 15
#define IN1 5
#define IN2 2

#define RST_PIN 17  // Chân reset RFID
#define SS_PIN 16   // Chân chip select RFID

// Cấu hình kết nối WiFi
const char* ssid = "NHA";
const char* password = "11111111";

// Cấu hình kết nối Firebase
#define FIREBASE_HOST "htnpetrol-491aa-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "jjV4zSjvIHeSKmdwNJZLP56YYdv2uDuOYirZWdUB"

volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned long totalMilliLitres;
unsigned long previousMillis = 0;
int interval = 1000;
float calibrationFactor = 4.5;

bool paymentComplete  = false;
MFRC522 rfid(SS_PIN, RST_PIN); // Khởi tạo đối tượng RFID

FirebaseData firebaseData;

void startMotor();
void stopMotor();
float convertToLitres(int enteredAmount, int selectedFuel);
String readRFID();
bool checkRFIDExistence(const String& uid);

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void setup()
{
  Serial.begin(9600);
  SPI.begin(); // init SPI bus
  rfid.PCD_Init(); // init MFRC522
  Serial.println("Scan RFID Card");

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());

  // Kết nối tới Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  lcd.init();
  lcd.clear();
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print("1. Ron 92");
  
  lcd.setCursor(0, 1);
  lcd.print("2. Ron 95");


  Serial.println("Scan RFID Card");
  servo1.attach(13); // Chân kết nối servo1
  servo2.attach(3); // Chân kết nối servo2

  pinMode(SENSOR, INPUT_PULLUP);
  pulseCount = 0;
  flowRate = 0.0;
  totalMilliLitres = 0;
  previousMillis = 0;
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  digitalWrite(ENA, LOW);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}
void loop() {
  // String cardUID = readRFID();
  // // String cardUID = "b9413794";
  // Serial.println(cardUID);
  // Serial.println(Firebase.getInt(firebaseData, "/id/" + cardUID + "/money"));

  while (!paymentComplete) {
    char key = keypad.getKey();

    if (key) {
      if (key >= '0' && key <= '9') {
        enteredAmount = enteredAmount * 10 + (key - '0');
        lcd.setCursor(0, 1);
        lcd.print(enteredAmount);
      } else if (key == '#') {
        if (enteredAmount == 1) {
          selectedFuel = 1;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Ron 92");

          lcd.setCursor(0, 1);
          lcd.print("Nhap tien mua:");
          enteredAmount = 0;
        } else if (enteredAmount == 2) {
          selectedFuel = 2;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Ron 95");

          lcd.setCursor(0, 1);
          lcd.print("Nhap tien mua:");
          enteredAmount = 0;
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Loai xang: ");
          if (selectedFuel == 1) {
            lcd.print("Ron 92");
          } else if (selectedFuel == 2) {
            lcd.print("Ron 95");
          }

          lcd.setCursor(0, 1);
          lcd.print("Tien mua: ");
          lcd.print(enteredAmount);

          if (selectedFuel == 1) {
            servo1.write(180);
          } else if (selectedFuel == 2) {
            servo2.write(180);
          }
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Thanh toan: ");
          delay(2000);
          String cardUID = readRFID();
          // String cardUID = "b9413794";
          Serial.println(cardUID);
          Serial.println(Firebase.getInt(firebaseData, "/id/" + cardUID + "/money"));
          delay(1000);
          if (Firebase.getInt(firebaseData, "/id/" + cardUID + "/money")) {
            int currentMoney = firebaseData.intData();
            Serial.print("Current money: ");
            Serial.println(currentMoney);
            int updatedMoney=currentMoney;

            if(currentMoney>enteredAmount) {
              updatedMoney = currentMoney - enteredAmount;
            } else {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("SD khong du: ");
              break;
            }
            
            if (Firebase.setInt(firebaseData, "/id/" + cardUID + "/money", updatedMoney)) {
              Serial.println("Money deducted successfully.");
              lcd.setCursor(0, 1);
              lcd.print("Dang GD ");
            } else {
              Serial.println("So du khong du.");
            }
            startMotor();
            Serial.print("RFID card UID: ");
            Serial.println(cardUID);
            float totalLitres = 0;

            while (true) {
              unsigned long currentMillis = millis();
              if (currentMillis - previousMillis >= interval) {
                pulse1Sec = pulseCount;
                pulseCount = 0;
                flowRate = ((1000.0 / (currentMillis - previousMillis)) * pulse1Sec) / calibrationFactor;
                previousMillis = currentMillis;

                float flowLitresPerMinute = flowRate / 60.0 *1000 ;
                totalLitres += flowLitresPerMinute;

                Serial.print("Flow rate: ");
                Serial.print(flowLitresPerMinute, 2); // Print flow rate with 2 decimal places
                Serial.println(" L/min");

                Serial.print("Total liquid quantity: ");
                Serial.print(totalLitres, 3); // Print total liquid quantity with 3 decimal places
                Serial.println(" L");

                if (totalLitres >= convertToLitres(enteredAmount, selectedFuel)) {
                  stopMotor();
                  paymentComplete = true;
                  break;
                }
              }
            }
          } else {
            Serial.println("Failed to retrieve money data from Firebase.");
          }

          servo1.write(0);
          servo2.write(0);

          enteredAmount = 0;
          selectedFuel = 0;
          paymentComplete = false;

          delay(2000);

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("1. Ron 92");

          lcd.setCursor(0, 1);
          lcd.print("2. Ron 95");
        }
      }
    }
  }
}


void startMotor() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 255);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);
}

float convertToLitres(int enteredAmount, int selectedFuel) {
  float litres = 0.0;
  
  if (selectedFuel == 1) {
    litres = enteredAmount / 20.0;
  } else if (selectedFuel == 2) {
    litres = enteredAmount / 25.0;
  }
  
  return litres;
}

String readRFID() {
  // Kiểm tra xem có thẻ RFID nào được đặt gần đầu đọc hay không
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // Đọc mã thẻ RFID
    String rfidData = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      rfidData += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      rfidData += String(rfid.uid.uidByte[i], HEX);
    }
    
    // Tắt thẻ RFID
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    return rfidData;
  }

  return ""; // Trả về chuỗi rỗng nếu không có thẻ RFID được đọc
}
bool checkRFIDExistence(const String& uid) {
  if (Firebase.getBool(firebaseData, "/id/" + uid)) {
    return firebaseData.boolData();
  }
  return false;
}


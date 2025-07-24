#include <SPI.h>
#include <MFRC522.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(16, 17);  // RX, TXs

#ifdef ESP32
#include <WiFi.h>
#include <HTTPClient.h>
#endif

// ===== WiFi & Telegram =====
const char* ssid = "wifi-iot";
const char* password = "password-iot";
#define BOTtoken "7677233055:AAE9_XhKSiMq-msTKTRQCBB9tTk44nIe-BQ"
#define CHAT_ID "6353193463"


String UID = "";
String sensor = "";
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// ===== RC522 Pin =====
#define RST_PIN 4
#define SS_PIN 5
MFRC522 rfid(SS_PIN, RST_PIN);

// ===== Relay & LCD =====
#define RELAY_PIN 27
LiquidCrystal_I2C lcd(0x27, 16, 2);
#define pin_reset 25

// ===== Server =====
const char* server_url = "http://labrobotika.go-web.my.id/server.php?apikey=";
const char* apikey = "1d63b940dbab551d5da9ae0cec003134";
DynamicJsonDocument data(2048);

// UID valid

const int jumlah_max_id = 41;
String data_uid[jumlah_max_id];
String lastUID = "";

bool cardDetected = false;


void debug(String message, int row = 0, int clear = 1) {
  Serial.println(message);
  //tampilkan jika menggunakan lcd
  if (clear == 1) {
    lcd.clear();
  }
  lcd.setCursor(0, row);
  lcd.print(message);
}


void setupWiFi() {
  debug("Menghubungkan ... ");
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(1000);
    debug(".", 1, 0);
    attempts++;
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\n✅ WiFi Terhubung!" : "\n❌ Gagal konek WiFi");
}

void prosesData(String sensor = "", String uid = "") {
  debug("Memproses ... ");
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "";
  if (sensor == "") {
    url = String(server_url) + String(apikey);
  } else {
    url = String(server_url) + String(apikey) + "&rfid_" + sensor + "=" + uid;
  }
  url.replace(" ", "%20");
  Serial.println("Request ke: " + url);
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Proses Data Server ... ");
    String response = http.getString();
    DeserializationError err = deserializeJson(data, response);
    if (!err) {
      for (int a = 0; a < jumlah_max_id; a++) {
        String key = "out_" + String(a + 1);
        Serial.print(key + " : ");
        Serial.println(data[key].as<String>());
        data_uid[a] = data[key] | "0";
      }
    }
  }
  http.end();
}


void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  debug("Loading ... ");


  setupWiFi();
  client.setInsecure();
  bool status_pesan = bot.sendMessage(CHAT_ID, "✅ Sistem Aktif.");
  Serial.println("Pesan Telegram : " + String(status_pesan));
  prosesData();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(pin_reset, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, HIGH);
  delay(1000);
  digitalWrite(RELAY_PIN, LOW);
  delay(1000);
  digitalWrite(RELAY_PIN, HIGH);

  SPI.begin();
  rfid.PCD_Init();

  bot.sendMessage(CHAT_ID, "✅ RC522 aktif.");
  Serial.println("RC522 Siap.");
  debug("RC522 Siap.");
  delay(2000);

  while (1) {
    if (mySerial.available()) {
      String data = mySerial.readStringUntil('\n');
      data.trim();
      if (data == "READY") {
        debug("PN532 siap");
        bot.sendMessage(CHAT_ID, "✅ PN532 aktif.");
        delay(3000);
        break;
      }
    } else {
      debug("Loading ... ");
    }
  }
  lcd.clear();
  lcd.print("Ready");
}


void cek_UID() {
  Serial.println("CEK UID .... ");
  if (UID != "") {
    bool valid = false;

    for (int a = 0; a < jumlah_max_id; a++) {
      if (data_uid[a] == UID) {
        valid = true;
        break;
      }
    }

    if (valid) {
      Serial.println("✅ Kartu Valid - Relay ON");
      bot.sendMessage(CHAT_ID, "✅ " + sensor + " - Kartu Valid: " + UID);
      debug("Kartu Valid ", 1, 1);
      digitalWrite(RELAY_PIN, LOW);
      delay(3000);
      digitalWrite(RELAY_PIN, HIGH);
      prosesData(sensor, UID);
      delay(100);
      SPI.begin();
      rfid.PCD_Init();

    } else {
      Serial.println("❌ Kartu Tidak Dikenal");
      bot.sendMessage(CHAT_ID, "❌ " + sensor + " - Kartu Tidak Dikenal: " + UID);
      debug("Kartu Tidak Dikenal", 1, 1);
      delay(3000);
    }

    UID = "";
  }
  lcd.clear();
  lcd.print("Ready");
}


void loop() {
  if (!digitalRead(pin_reset)) {
    debug("RESTART ... ");
    delay(1000);
    ESP.restart();
  }
  sensor = "";
  if (mySerial.available()) {
    String data = mySerial.readStringUntil('\n');
    data.trim();
    if (data != "READY") {
      UID = data;
      sensor = "PN532";
      debug("UID: ");
      debug(UID,1,0);
    }
  }




  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uidStr = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) uidStr += "0";
      uidStr += String(rfid.uid.uidByte[i], HEX);
    }
    uidStr.toUpperCase();

    if (uidStr != lastUID) {
      lastUID = uidStr;
      cardDetected = true;
      UID = uidStr;
      sensor = "RC522";
      Serial.println("📡 UID Terdeteksi: " + uidStr);
      debug("UID: ");
      debug(UID,1,0);
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }

  // Deteksi apakah kartu sudah diangkat
  if (!rfid.PICC_IsNewCardPresent() && !rfid.PICC_ReadCardSerial() && cardDetected) {
    lastUID = "";
    cardDetected = false;
  } 

  cek_UID();
  delay(100);
}

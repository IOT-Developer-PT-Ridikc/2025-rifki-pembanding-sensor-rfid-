#include <SPI.h>
#include <Adafruit_PN532.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(16, 17);  // RX, TX
#define pin_reset 25
// =====================
// PN532 SPI Pin (HSPI)
// =====================
#define PN532_SCK 27
#define PN532_MISO 26
#define PN532_MOSI 13
#define PN532_SS 5
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);







// =====================
// Setup
// =====================
void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  pinMode(pin_reset, INPUT_PULLUP);


  // Init PN532
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("❌ PN532 tidak terdeteksi!");
    while (1)
      ;
  }

  Serial.print("PN532 Firmware: ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.');
  Serial.println((versiondata >> 8) & 0xFF);

  nfc.SAMConfig();  // Init card reader mode
  Serial.println("✅ PN532 siap membaca kartu.");


  delay(5000);
  Serial.println("READY");
  mySerial.println("READY");
  delay(100);
}

// =====================
// Loop
// =====================
void loop() {
  if (!digitalRead(pin_reset)) {
    Serial.println("RESTART");
    delay(1000);
    ESP.restart();
  }

  static String lastUID = "";

  uint8_t uid[7];
  uint8_t uidLength;

  // Coba baca kartu
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    String uidStr = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) uidStr += "0";
      uidStr += String(uid[i], HEX);
    }
    uidStr.toUpperCase();

    // Hindari pembacaan berulang jika kartu belum diangkat
    if (uidStr == lastUID) {
      delay(100);
      return;
    }

    lastUID = uidStr;
    Serial.println("📡 UID Terdeteksi: " + uidStr);

    mySerial.println(String(uidStr));
    mySerial.flush();

    // Tunggu kartu diangkat
    // while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    //   delay(100);
    // }
    delay(2000);
    lastUID = "";  // reset UID agar bisa terbaca ulang
  }
  mySerial.println("READY");
  Serial.println("READY");

  delay(100);
}

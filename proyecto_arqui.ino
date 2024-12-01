#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include "DHT.h"

// Credenciales de Wi-Fi
const char* ssid = "LauraNotFound";
const char* password = "12345678";

// Configuración de Firebase
#define FIREBASE_HOST "smart-casita-1ea25-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "TuSecretKey"             
FirebaseData firebaseData;

// Pines de sensores y LEDs
const int waterPin = 36;
const int pirPin = 34;
const int dhtPin = 14;
const int ledRFID = 13;
const int ledWater = 12;
const int ledMotion = 27;

// Tipo de sensor DHT
#define DHTTYPE DHT11
DHT dht(dhtPin, DHTTYPE);
// RFID RC522
#define RST_PIN 20
#define SS_PIN 22
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Variables globales
int waterLevel = 0;
int pirValue = 0;
float temperature = 0.0;
float humidity = 0.0;

// UID autorizado para el RFID
byte authorizedUID[] = {0x93, 0xF2, 0x79, 0x0F};

void setup() {
  Serial.begin(115200);

  // Conexión a Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConectado a WiFi.");


  // Inicializa Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  // Inicializar sensores y LEDs
  pinMode(pirPin, INPUT);
  pinMode(ledRFID, OUTPUT);
  pinMode(ledWater, OUTPUT);
  pinMode(ledMotion, OUTPUT);
  dht.begin();
  SPI.begin(5, 21, 19); // Pines SPI del ESP32
  mfrc522.PCD_Init();

  Serial.println("Sistema inicializado.");
}

void loop() {
  readWaterLevel();
  readPIR();
  readRFID();
  readDHT();

  delay(2000);
}

void readWaterLevel() {
  waterLevel = analogRead(waterPin);
  Serial.print("Nivel de agua: ");
  Serial.println(waterLevel);

  if (waterLevel > 500) { 
    blinkLED(ledWater, 5);
  }

  // Sube datos a Firebase
  Firebase.setInt(firebaseData, "/sensores/nivel_agua", waterLevel);
}

void readPIR() {
  pirValue = digitalRead(pirPin);
  if (pirValue == HIGH) {
    digitalWrite(ledMotion, HIGH);
    Serial.println("Movimiento detectado");
  } else {
    digitalWrite(ledMotion, LOW);
    Serial.println("Sin movimiento");
  }

  // Sube datos a Firebase
  String movimiento = (pirValue == HIGH) ? "Detectado" : "Sin movimiento";
  Firebase.setString(firebaseData, "/sensores/movimiento", movimiento);
}

void readRFID() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += String(mfrc522.uid.uidByte[i], HEX);
    if (i < mfrc522.uid.size - 1) uid += ":";
  }
  Serial.print("UID leída: ");
  Serial.println(uid);

  // Verifica si la tarjeta es válida
  if (isAuthorized(mfrc522.uid.uidByte, mfrc522.uid.size)) {
    Serial.println("Tarjeta Autorizada");
    digitalWrite(ledRFID, HIGH);
    delay(5000);
    digitalWrite(ledRFID, LOW);
  } else {
    Serial.println("Tarjeta No autorizada");
  }

  // Sube datos a Firebase

  String autorizacion = isAuthorized(mfrc522.uid.uidByte, mfrc522.uid.size) ? "Autorizada" : "No autorizada";
  Firebase.setString(firebaseData, "/sensores/rfid", autorizacion);

  mfrc522.PICC_HaltA();
}

void readDHT() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Error al leer el sensor DHT.");
    return;
  }

  Serial.print("Temperatura: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humedad: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Sube datos a Firebase
  Firebase.setFloat(firebaseData, "/sensores/temperatura", temperature);
  Firebase.setFloat(firebaseData, "/sensores/humedad", humidity);
}

bool isAuthorized(byte *uid, byte size) {
  if (size != sizeof(authorizedUID)) return false;
  for (byte i = 0; i < size; i++) {
    if (uid[i] != authorizedUID[i]) return false;
  }
  return true;
}

void blinkLED(int pin, int durationSeconds) {
  for (int i = 0; i < durationSeconds * 2; i++) {
    digitalWrite(pin, !digitalRead(pin));
    delay(500); 
  }
  digitalWrite(pin, LOW);
}

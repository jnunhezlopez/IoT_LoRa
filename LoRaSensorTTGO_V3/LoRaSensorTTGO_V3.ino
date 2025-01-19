#include <ArduinoJson.h>
#include "DHT.h"
#include "board_def.h"
#include <LoRa.h>

#define DHTPIN 4
#define DHTTYPE DHT22
#define BUTTON_PIN 26
#define OLED_DISPLAY_TIME 5000  // Tiempo en milisegundos que se muestra el OLED

int sendInterval = 5000; // Intervalo de envío por defecto (en milisegundos)
unsigned long lastSendTime = 0;

OLED_CLASS_OBJ display(OLED_ADDRESS, OLED_SDA, OLED_SCL);
DHT dht(DHTPIN, DHTTYPE);

// Estado del OLED
bool oledActive = false;
unsigned long oledStartTime = 0;

void printOLED(String msg) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, msg);
  display.display();
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Configuración OLED
  if (OLED_RST > 0) {
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, HIGH);
    delay(100);
    digitalWrite(OLED_RST, LOW);
    delay(100);
    digitalWrite(OLED_RST, HIGH);
  }

  display.init();
  display.flipScreenVertically();
  display.clear();
  display.setFont(ArialMT_Plain_10);

  // Inicializar botón
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Inicializar DHT
  dht.begin();

  // Inicializar LoRa
  SPI.begin(CONFIG_CLK, CONFIG_MISO, CONFIG_MOSI, CONFIG_NSS);
  LoRa.setPins(CONFIG_NSS, CONFIG_RST, CONFIG_DIO0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Error inicializando LoRa");
    while (true);
  }
  Serial.println("LoRa inicializado correctamente");
}

void handleIncomingMessage(String incomingMessage) {
  StaticJsonDocument<128> doc;  // Tamaño suficiente para manejar el comando JSON

  DeserializationError error = deserializeJson(doc, incomingMessage);
  if (error) {
    Serial.println("Error al analizar JSON: " + String(error.c_str()));
    return;
  }

  if (doc.containsKey("command") && doc["command"] == "set_interval") {
    if (doc.containsKey("value")) {
      int newInterval = doc["value"];
      sendInterval = newInterval;
      Serial.println("Nuevo intervalo configurado: " + String(sendInterval) + " ms");
    } else {
      Serial.println("Error: falta el valor de intervalo en el mensaje");
    }
  } else {
    Serial.println("Comando no reconocido");
  }
}

void loop() {
    // Recibir mensajes LoRa
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incomingMessage = "";
    while (LoRa.available()) {
      incomingMessage += (char)LoRa.read();
    }
    Serial.println("Mensaje recibido: " + incomingMessage);
    handleIncomingMessage(incomingMessage);

  }

  // Leer botón
  if (digitalRead(BUTTON_PIN) == HIGH && !oledActive) {
    oledActive = true;
    oledStartTime = millis();

    // Leer DHT
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println("Error leyendo del sensor DHT");
      printOLED("Error sensor DHT");
    } else {
      // Mostrar en OLED
      String displayMsg = "Temp: " + String(t) + " C\nHum: " + String(h) + " %" + " \nInt: " + String(sendInterval) + " ms";
      printOLED(displayMsg);
      Serial.println(displayMsg);
    }
  }

  // Apagar OLED después de OLED_DISPLAY_TIME
  if (oledActive && millis() - oledStartTime > OLED_DISPLAY_TIME) {
    oledActive = false;
    display.clear();
    display.display();
  }

  // Enviar datos LoRa regularmente
//  static unsigned long lastSendTime = 0; ya definido como variable global
  if (millis() - lastSendTime > sendInterval) {  // Cada 10 segundos
    lastSendTime = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println("Error leyendo del sensor DHT");
    } else {
      // Crear JSON
      StaticJsonDocument<200> doc;
      doc["temperature"] = t;
      doc["humidity"] = h;

      char buffer[200];
      size_t n = serializeJson(doc, buffer);

      // Enviar por LoRa
      LoRa.beginPacket();
      LoRa.write((uint8_t *)buffer, n);
      LoRa.endPacket();

      Serial.print("Enviado por LoRa: ");
      Serial.println(buffer);
    }
  }
}

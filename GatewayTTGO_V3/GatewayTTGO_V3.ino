#include <SPI.h> 
#include <LoRa.h>
#include "board_def.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Configuración OLED
OLED_CLASS_OBJ display(OLED_ADDRESS, OLED_SDA, OLED_SCL);
int width;
int height;

// Configuración WiFi
const char* ssid = "IoT_Network";
const char* password = "107_NetWork";

// Configuración MQTT
const char* mqtt_server = "192.168.12.1";
const int mqtt_port = 1883;
const char* mqtt_topic = "iot/lora";


WiFiClient espClient;
PubSubClient client(espClient);
void printOLED(String line1, String line2 = "", String line3 = "") {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 15, line1);
  display.drawString(0, 30, line2);
  display.drawString(0, 45, line3);
  display.display();
}
// Función para mostrar mensajes en OLED
void printOLEDStatus(String wifiStatus, String mqttStatus, String loraMsg) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  //display.setColor(BLACK);
  display.drawString(0, 0, "WiFi: " + wifiStatus);
  display.drawString(0, 10, "MQTT: " + mqttStatus);
  display.drawString(0, 20, "Último Msg:");
  display.drawString(0, 30, loraMsg);
  display.display();
}

// Conexión a WiFi
void connectToWiFi() {
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (millis() - startTime > 15000) {
      printOLEDStatus("Desconectado", "N/A", "Error WiFi");
      return; // Timeout después de 15 segundos
    }
  }
  printOLEDStatus("Conectado", "N/A", "...");
}
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido en el tópico: ");
  Serial.println(topic);

  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("Mensaje: " + message);

  // Enviar por LoRa
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  Serial.println("Mensaje enviado por LoRa");
}
// Reconexión a MQTT
void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect("LoRaGateway")) {
      client.subscribe("iot/sensor/commands");
      printOLEDStatus("Conectado", "MQTT OK", "...");
    } else {
      delay(5000);
    }
  }
}

// Configuración inicial
void setup() {
  Serial.begin(115200);

  // Configuración OLED
  display.init();
  width = display.getWidth() / 2;
  height = display.getHeight() / 2;
  display.flipScreenVertically();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // WiFi y MQTT
  connectToWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  // Configuración LoRa
  SPI.begin(CONFIG_CLK, CONFIG_MISO, CONFIG_MOSI, CONFIG_NSS);
  LoRa.setPins(CONFIG_NSS, CONFIG_RST, CONFIG_DIO0);
  if (!LoRa.begin(BAND)) {
    printOLEDStatus("Conectado", "N/A", "Error LoRa");
    while (true);
  }
  printOLEDStatus("Conectado", "MQTT N/A", "LoRa OK");
}

void loop() {
  // Verificar conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  // Verificar conexión MQTT
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Procesar paquetes LoRa
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String receivedData = "";
    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }
    // Formato JSON para MQTT
    //StaticJsonDocument<200> doc;
    //doc["device"] = "LoRaGateway";
    //doc["data"] = receivedData;
    //String jsonString;
    //serializeJson(doc, jsonString);
    Serial.print("Recibido: ");
    Serial.println(receivedData);

  // Parsear JSON 
    StaticJsonDocument<128> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, receivedData);

    if (error) {
      Serial.print("Error al parsear JSON: ");
      Serial.println(error.f_str());
      printOLED("Error JSON", error.f_str());
      return;
    }

    float temperature = jsonDoc["temperature"];
    float humidity = jsonDoc["humidity"];
    if (client.connected()) {
      //client.publish(mqtt_topic, jsonString.c_str());
      //client.publish(mqtt_topic_temp, String(temperature).c_str());
      //client.publish(mqtt_topic_hum, String(humidity).c_str());
      // Mostrar datos en OLED
      client.publish(mqtt_topic, receivedData.c_str());
      printOLED("Temp: " + String(temperature) + " C", "Hum: " + String(humidity) + " %");
      //printOLEDStatus("Conectado", "MQTT OK", receivedData);
    } else {
      printOLEDStatus("Conectado", "MQTT Error", receivedData);
    }
  }
}

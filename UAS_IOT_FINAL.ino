#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <Ultrasonic.h>
#include <DHT.h>

#define WIFI_SSID "nomz"
#define WIFI_PASSWORD "dimasarya"

#define MQTT_SERVER "broker.emqx.io"
#define MQTT_PORT 1883
#define MQTT_USER "dimas"
#define MQTT_PASSWORD "dimas"

WiFiClient espClient;
PubSubClient client(espClient);

#define SERVO_PIN D1
Servo servo;

#define BUZZER_PIN D2

#define TRIGGER_PIN D4
#define ECHO_PIN D4
Ultrasonic ultrasonic(TRIGGER_PIN, ECHO_PIN);

#define DHT_PIN D3
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

unsigned long lastPublishTime = 0;
const long interval = 5000; // 5 detik

bool isBuzzerOn = false;

void callback(char* receivedTopic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Pesan diterima [");
  Serial.print(receivedTopic);
  Serial.print("]: ");
  Serial.println(message);

  if (strcmp(receivedTopic, "smartcatfeeder/feeds") == 0) {
    if (message.startsWith("KasiMakan")) {
      int pos = message.substring(9).toInt();
      if (pos >= 0 && pos <= 180) {
        tone(BUZZER_PIN, 3000, 3000);
        servo.write(pos);
        client.publish("smartcatfeeder/response", "Pakan telah dibuka");
        tone(BUZZER_PIN, 2000, 100);
        delay(100);
        servo.write(90);
        delay(50);
        client.publish("smartcatfeeder/response", "Pakan ditutup");
        tone(BUZZER_PIN, 2500, 100);
      } else {
        client.publish("smartcatfeeder/response", "Error !!!");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  servo.attach(SERVO_PIN, 0, 2000);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.print("Menghubungkan ke WiFi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi terhubung. Alamat IP: ");
  Serial.println(WiFi.localIP());

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    if (client.connect("ESP8266Client", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("terhubung");
      client.subscribe("smartcatfeeder/feeds");
    } else {
      Serial.print("gagal, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }

  dht.begin();
}

void loop() {
  if (!client.connected()) {
    while (!client.connected()) {
      Serial.print("Menghubungkan ke MQTT...");
      if (client.connect("ESP8266Client", MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("terhubung");
        client.subscribe("smartcatfeeder/feeds");
      } else {
        Serial.print("gagal, rc=");
        Serial.print(client.state());
        delay(2000);
      }
    }
  }

  client.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - lastPublishTime >= interval) {
    lastPublishTime = currentMillis;

    // Baca dan kirimkan suhu dan kelembapan
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (!isnan(temperature) && !isnan(humidity)) {
      client.publish("smartcatfeeder/temperature", String(temperature).c_str());
      client.publish("smartcatfeeder/humidity", String(humidity).c_str());
      Serial.print("Temperature: ");
      Serial.println(temperature);
      Serial.print("Humidity: ");
      Serial.println(humidity);
    }

    // Baca dan kirimkan level pakan
    float distance_cm = ultrasonic.distanceRead(CM);
    String foodLevel;
    if (distance_cm >= 12) {
      foodLevel = "Habis";
      if (!isBuzzerOn) {
        tone(BUZZER_PIN, 2000);
        isBuzzerOn = true;
      }
    } else if (distance_cm < 12 && distance_cm >= 6) {
      foodLevel = "Setengah";
      if (isBuzzerOn) {
        noTone(BUZZER_PIN);
        isBuzzerOn = false;
      }
    } else {
      foodLevel = "Banyak";
      if (isBuzzerOn) {
        noTone(BUZZER_PIN);
        isBuzzerOn = false;
      }
    }
    client.publish("smartcatfeeder/foodlevel", foodLevel.c_str());
    Serial.print("Food Level: ");
    Serial.println(foodLevel);
  }
}

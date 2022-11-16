/**
   Tambahkan Library HTTPClient Lewat Manage Libraries
   Wiring Diagram ada di Folder
   Jika data tidak terbaca, Tambahkan reistor di pin A+ dan B- power meter
   Apabila ada yang kurang paham bisa di tanyakan
   M U P : 082122270198
*/


#include "SimpleModbusMaster.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

/**
   CODE RS485
   --------------------------------------
*/
#define baud 9600
#define timeout 1000
#define polling 200
#define retry_count 10

// used to toggle the receive/transmit pin on the driver
#define TxEnablePin 0

enum
{
  PACKET1,
  PACKET2,
  PACKET3,
  PACKET4,
  // leave this last entry
  TOTAL_NO_OF_PACKETS
};

// Create an array of Packets for modbus_update()
Packet packets[TOTAL_NO_OF_PACKETS];
packetPointer packet1 = &packets[PACKET1];
packetPointer packet2 = &packets[PACKET2];
packetPointer packet3 = &packets[PACKET3];
packetPointer packet4 = &packets[PACKET4];
unsigned int volt[2];
unsigned int a[2];
unsigned int pf[2];
unsigned int f[2];
unsigned int p[2];

unsigned long timer;

float tegangan = 0;
float arus = 0;
float cosphi = 0;
float freq = 0;
float power = 0;

float f_2uint_float(unsigned int uint1, unsigned int uint2) {    // reconstruct the float from 2 unsigned integers
  union f_2uint {
    float f;
    uint16_t i[2];
  };
  union f_2uint f_number;
  f_number.i[0] = uint1;
  f_number.i[1] = uint2;
  return f_number.f;
}

/**
   CODE KONEKSI
   ---------------------------------
*/
const char* ssid = "ISI NAMA AKSES POINT";  //NAMA AKSES POINT
const char* password = "ISI PASWORD"; // PASWORD AKSES POINT
const char* host = ""; //NAMA HOSTING ATAU KALAU LOCAL SERVER PAKAI IP LOCAL SERVER

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // TOPIK UNTUK KIRIM DARI NODEMCU KE DVICE LAIN
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("lukman98"); //TOPIK SUBCRIBER ATAU PENERIMA
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // CODE RS485
  //--------------------------------
  ////VOLTAGE
  packet1->id = 1;
  packet1->function = READ_HOLDING_REGISTERS;
  packet1->address = 3028;
  packet1->no_of_registers = 2;
  packet1->register_array = volt;

  ////ARUS
  packet2->id = 1;
  packet2->function = READ_HOLDING_REGISTERS;
  packet2->address = 3000;
  packet2->no_of_registers = 2;
  packet2->register_array = a;

  ////POWER FACTOR
  packet3->id = 1;
  packet3->function = READ_HOLDING_REGISTERS;
  packet3->address = 3084;
  packet3->no_of_registers = 2;
  packet3->register_array = pf;

  ////FREQ
  packet2->id = 1;
  packet2->function = READ_HOLDING_REGISTERS;
  packet2->address = 3110;
  packet2->no_of_registers = 2;
  packet2->register_array = f;

  ////POWER
  packet4->id = 1;
  packet4->function = READ_HOLDING_REGISTERS;
  packet4->address = 3054;
  packet4->no_of_registers = 2;
  packet4->register_array = p;

  modbus_configure(baud, timeout, polling, retry_count, TxEnablePin, packets, TOTAL_NO_OF_PACKETS);

  pinMode(LED_BUILTIN, OUTPUT);
  timer = millis();

  // CODE WEB
  // ------------------------------------------
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());



}

void loop() {
  unsigned int connection_status = modbus_update(packets);
  if (connection_status != TOTAL_NO_OF_PACKETS)
    digitalWrite(LED_BUILTIN, HIGH);
  else
    digitalWrite(LED_BUILTIN, LOW);

  long newTimer = millis();
  if (newTimer -  timer >= 100) {
    Serial.println();
    Serial.print("VOLTAGE : ");
    tegangan = f_2uint_float(volt[1], volt[0]);
    Serial.print(tegangan);

    Serial.print("  |  Cos Phi : ");
    cosphi = f_2uint_float(pf[1], pf[0]);
    Serial.println(cosphi);

    Serial.print("Frequensi : ");
    freq = f_2uint_float(f[1], f[0]);
    Serial.print(freq);

    Serial.print("   |  Power   : ");
    power = f_2uint_float(p[1], p[0]);
    Serial.println(power);

    Serial.print("Arus : ");
    arus = f_2uint_float(a[1], a[0]);
    if (arus == 0.00)
    {
      arus = ((power) / (tegangan * cosphi)) * 1000;

    }
    else {
      arus = arus;
      //Serial.println(arus);
    }
    Serial.println(arus);
    timer = newTimer;
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    char vString[8];
    char aString[8];
    char pString[8];
    char pfString[8];
    dtostrf(tegangan, 1, 2, vString);
    dtostrf(arus, 1, 2, aString);
    dtostrf(cosphi, 1, 2, pfString);
    dtostrf(power, 1, 2, pString);
    client.publish("esp32/volt", vString);
    client.publish("esp32/arus", aString);
    client.publish("esp32/cosphi", pfString);
    client.publish("esp32/power", pString);
  }

}

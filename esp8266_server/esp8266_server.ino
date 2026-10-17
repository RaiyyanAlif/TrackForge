#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>

const char* WIFI_SSID = "BeyonDer";
const char* WIFI_PASSWORD = "631254327";

// ESP8266 D5 = RX from Mega TX1 through voltage divider
// ESP8266 D6 = TX to Mega RX1
SoftwareSerial megaSerial(D5, D6);

IPAddress localIP(192, 168, 1, 104);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 1, 1);

WiFiServer server(8266);
WiFiClient client;
String commandBuffer = "";

void setup() {
  Serial.begin(115200);    // USB diagnostics
  megaSerial.begin(9600);  // Mega UART bridge

  WiFi.mode(WIFI_STA);
  //WiFi.config(localIP, gateway, subnet, dns);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println();
  Serial.println("Operation 1122 ESP bridge starting...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  server.begin();
  server.setNoDelay(true);

  Serial.println();
  Serial.print("WiFi connected. ESP IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("TCP server listening on port 8266");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(500);
    return;
  }

  if (!client || !client.connected()) {
    WiFiClient incoming = server.accept();
    if (incoming) {
      client.stop();
      client = incoming;
      client.setNoDelay(true);
      commandBuffer = "";
      Serial.println("TCP client connected");
    }
  }

  if (client && client.connected()) {
    while (client.available()) {
      char c = (char)client.read();

      if (c == '\r') continue;

      if (c == '\n') {
        commandBuffer.trim();
        if (commandBuffer.length() > 0) {
          megaSerial.println(commandBuffer);
          Serial.print("Forwarded to Mega: ");
          Serial.println(commandBuffer);
          commandBuffer = "";
        }
      } else if (commandBuffer.length() < 120) {
        commandBuffer += c;
      } else {
        commandBuffer = "";
        client.println("ERROR:COMMAND_TOO_LONG");
      }
    }

    while (megaSerial.available()) {
      char responseByte = (char)megaSerial.read();

      Serial.write(responseByte);

      if (client.connected()) {
        client.write((uint8_t)responseByte);
      }
    }
  }

  yield();
}
#include <WiFi.h>
#include "DFRobotDFPlayerMini.h"
#include "SoftwareSerial.h"

// Speaker pin connection
static const uint8_t PIN_MP3_RX = D1;
static const uint8_t PIN_MP3_TX = D0;

SoftwareSerial softwareSerial(PIN_MP3_RX, PIN_MP3_TX);
const char *ssid = "MAKERSPACE";
const char *password = "12345678";

WiFiServer server(80);
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  Serial.begin(115200);
  softwareSerial.begin(9600); // Init DFPlayer serial on custom pins

  pinMode(PIN_MP3_RX, INPUT);
  pinMode(PIN_MP3_TX, OUTPUT);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.begin();

  if (!myDFPlayer.begin(softwareSerial)) {
    Serial.println("DFPlayer Mini not found!");
    while (true);
  }

  Serial.println("DFPlayer Mini online.");
  myDFPlayer.volume(30); // Set volume level (0 to 30)
}

void loop() {
  WiFiClient client = server.available();
  
  if (client) {
    Serial.println("New Client.");
    String currentLine = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        currentLine += c;

        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Respond with webpage
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.println("<html><body><h1>DFPlayer Control</h1>");
            client.println("<button onclick=\"location.href='/play'\">Play</button>");
            client.println("<button onclick=\"location.href='/stop'\">Stop</button>");
            client.println("</body></html>");
            client.println();
            break;
          }
        }
      }
    }

    if (currentLine.indexOf("GET /play") >= 0) {
      myDFPlayer.play(1); // Play track 1
      Serial.println("Play command sent.");
    } else if (currentLine.indexOf("GET /stop") >= 0) {
      myDFPlayer.stop();
      Serial.println("Stop command sent.");
    }

    client.stop();
    Serial.println("Client Disconnected.");
  }
}

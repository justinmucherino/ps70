#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

// WiFi credentials
const char* ssid = "MAKERSPACE";
const char* password = "12345678";

// Create AsyncWebServer and WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// HTML page served to clients, with controls for circle and slip rate
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>ESP32 Mirror Control</title>
  <style>
    body { font-family: Helvetica; text-align:center; margin:20px; }
    #controls { max-width: 400px; margin: auto; }
    .row { margin: 15px 0; }
    label { display:block; margin-bottom:5px; }
    button { padding: 8px 16px; font-size:16px; }
  </style>
</head>
<body>
  <h1>ESP32 Mirror Servo</h1>
  <div id="controls">
    <div class="row">
      <input id="toggle-circle" type="checkbox">
      <label for="toggle-circle">Draw Dynamic Circle</label>
    </div>
    <div class="row">
      <button id="button-normal">Normal Circle</button>
    </div>
    <div class="row">
      <label for="slider-slip">Slip Increment: <span id="slip-value">0.005</span></label>
      <input id="slider-slip" type="range" min="0" max="0.1" step="0.0005" value="0.005">
    </div>
    <p>Status: <span id="state">Idle</span></p>
  </div>

  <script>
    const socket = new WebSocket(`ws://${window.location.hostname}/ws`);
    socket.onopen = () => console.log("WS connected");
    socket.onmessage = e => console.log("WS recv:", e.data);

    const toggle = document.getElementById('toggle-circle');
    const normalBtn = document.getElementById('button-normal');
    const slider = document.getElementById('slider-slip');
    const slipValue = document.getElementById('slip-value');
    const state = document.getElementById('state');

    toggle.addEventListener('change', () => {
      socket.send('toggle-circle');
      state.innerText = toggle.checked ? 'Dynamic ON' : 'Stopped';
    });

    normalBtn.addEventListener('click', () => {
      socket.send('normal-circle');
      state.innerText = 'Normal Circle';
    });

    slider.addEventListener('input', () => {
      const v = slider.value;
      slipValue.innerText = v;
      socket.send(`slip:${v}`);
    });
  </script>
</body>
</html>
)rawliteral";

// Servo objects
Servo xservo, yservo;

// Parametric draw settings
const int   N         = 200;     // steps per loop
const float A         = 20;      // total swing X (°)
const float B         = 20;      // total swing Y (°)
const float a         = 5.0;     // freq X
const float b         = 5.0;     // freq Y
const int   xoffset   = 60;      // servo midpoint X
const int   yoffset   = 0;       // servo midpoint Y

int xcentre, ycentre;
int  k          = 0;
float delta     = PI/2.0;       // initial phase for normal circle
float slipStep  = 0.005;        // default slip increment
bool  drawCircle= false;

// handle WS text messages
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->opcode==WS_TEXT && info->index==0 && info->final) {
    data[len]=0;
    String msg = (char*)data;
    Serial.println("WS in: " + msg);

    if (msg == "toggle-circle") {
      drawCircle = !drawCircle;
      Serial.println(drawCircle ? "Dynamic Circle ON" : "Circle OFF");
    }
    else if (msg == "normal-circle") {
      drawCircle = true;
      delta = PI/2.0;
      slipStep = 0.0;
      Serial.println("Normal Circle");
    }
    else if (msg.startsWith("slip:")) {
      float v = msg.substring(5).toFloat();
      slipStep = v;
      Serial.printf("SlipStep = %.4f\n", slipStep);
    }
  }
}

// WS event hookup
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) handleWebSocketMessage(arg, data, len);
}

void setup(){
  Serial.begin(115200);
  // attach servos (change pins if needed)
  xservo.attach(12);
  yservo.attach(27);
  xcentre = xoffset + (A/2.0);
  ycentre = yoffset + (B/2.0);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while(WiFi.status()!=WL_CONNECTED){
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi up: " + WiFi.localIP().toString());

  // WebSocket + server
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send_P(200,"text/html",index_html);
  });
  server.begin();
}

void loop(){
  if(drawCircle){
    float theta = TWO_PI * k / float(N);
    float x_raw = (A/2.0)*sin(a*theta + delta);
    float y_raw = (B/2.0)*sin(b*theta);

    int xpos = xcentre + int(x_raw);
    int ypos = ycentre + int(y_raw);

    xservo.write(xpos);
    yservo.write(ypos);

    k = (k+1) % N;
    delta += slipStep;
    if(delta > TWO_PI) delta -= TWO_PI;
  }
  ws.cleanupClients();
  delay(4);
}

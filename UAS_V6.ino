#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>

/* ================= WIFI ================= */
const char* ssid = "Espnada";
const char* password = "12345678";

/* ================= MQTT CLOUD ================= */
const char* mqtt_server = "b245d389086a4108a571b7c0a51eb83a.s1.eu.hivemq.cloud";
const int   mqtt_port   = 8883;
const char* mqtt_topic_status  = "iot/esp32/status";
const char* mqtt_topic_control = "iot/esp32/control";
const char* mqtt_user = "hivemq.webclient.1770393815655";
const char* mqtt_pass = "Lo#c1kzh;&05.dYXUGF3";

/* ================= OBJECT ================= */
WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
WebServer server(80);
Preferences prefs;

/* ================= HARDWARE ================= */
#define LED1_PIN 2
#define LED2_PIN 4
#define LED3_PIN 5

#define BUTTON_PIN 0
#define BUTTON2_PIN 27

/* ================= PWM ================= */
#define PWM_FREQ 5000
#define PWM_RES  8

int pwmLevels[3] = {5, 80, 255};   // redup → sedang → terang
int brightnessLevel = 0;

/* ================= STATE ================= */
bool ledPower = false;
int currentPwm = 0;

/* ================= FLAG ================= */
volatile bool buttonPressed = false;
volatile bool needPublish = false;
String publishSource = "";

/* ================= TIMER ================= */
unsigned long lastPublish = 0;
const unsigned long interval = 10000;

/* ================= HTML ================= */
const char page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<title>ESP32 Industrial Dashboard</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
:root{
  --sage:#9CAF88;--dark:#1f2933;--panel:#111827;
  --text:#e5e7eb;--accent:#7c9a6d;
}
body{
  margin:0;font-family:Segoe UI,Arial;
  background:linear-gradient(135deg,#0f172a,#1f2933);
  color:var(--text);
  display:flex;justify-content:center;align-items:center;
  min-height:100vh;
}
.card{
  width:360px;background:var(--panel);
  border-radius:16px;padding:25px;
  box-shadow:0 15px 40px rgba(0,0,0,.6);
  text-align:center;
}
h1{margin:0;font-size:20px;color:var(--sage);}
.sub{font-size:13px;opacity:.7;margin-bottom:20px;}
.status{margin:20px 0;padding:15px;border-radius:12px;background:#020617;}
.badge{padding:6px 14px;border-radius:999px;font-weight:bold;}
.on{background:rgba(124,154,109,.2);color:var(--sage);}
.off{background:rgba(220,38,38,.2);color:#ef4444;}
.btns{display:grid;grid-template-columns:1fr 1fr;gap:10px;}
button{
  padding:14px;border:none;border-radius:10px;
  font-weight:600;cursor:pointer;color:#fff;
  background:#334155;
  box-shadow:0 6px 0 #020617;
}
button:active{
  transform:translateY(4px);
  box-shadow:0 2px 0 #020617;
}
.toggle{grid-column:span 2;background:var(--accent)}
footer{margin-top:20px;font-size:12px;opacity:.6;}
</style>
</head>
<body>
<div class="card">
  <h1>ESP32 LED CONTROL</h1>
  <div class="sub">MQTT Cloud • TLS • RTOS</div>
  <div class="status">
    Status LED:
    <span id="stat" class="badge off">OFF</span>
  </div>
  <div class="btns">
    <button onclick="send('ON')">ON</button>
    <button onclick="send('OFF')">OFF</button>
    <button class="toggle" onclick="send('TOGGLE')">TOGGLE</button>
  </div>
  <footer>
    Nada Ismaya<br>NIM 23552011125
  </footer>
</div>

<script>
function send(c){ fetch('/cmd?c='+c); }
function update(){
  fetch('/status').then(r=>r.json()).then(d=>{
    let s=document.getElementById('stat');
    s.innerText=d.status;
    s.className='badge '+(d.status=='ON'?'on':'off');
  });
}
setInterval(update,1000); update();
</script>
</body>
</html>
)rawliteral";

/* ================= INTERRUPT ================= */
void IRAM_ATTR handleButton(){
  buttonPressed = true;
}

/* ================= APPLY LED ================= */
void applyLed(){
  currentPwm = ledPower ? pwmLevels[brightnessLevel] : 0;

  ledcWrite(LED1_PIN, currentPwm);
  ledcWrite(LED2_PIN, currentPwm);
  ledcWrite(LED3_PIN, currentPwm);
}

/* ================= MQTT CALLBACK ================= */
void mqttCallback(char* topic, byte* payload, unsigned int length){
  StaticJsonDocument<64> doc;
  if (deserializeJson(doc, payload, length)) return;

  const char* cmd = doc["cmd"];
  if (!cmd) return;

  if (!strcmp(cmd,"ON")) {
    ledPower = true;
    if (brightnessLevel == 0) brightnessLevel = 1;
  }
  else if (!strcmp(cmd,"OFF")) {
    ledPower = false;
  }
  else if (!strcmp(cmd,"TOGGLE")) {
    ledPower = !ledPower;
  }

  applyLed();

  prefs.putBool("power", ledPower);
  prefs.putInt("level", brightnessLevel);

  publishSource = "mqtt";
  needPublish = true;
}

/* ================= MQTT ================= */
void mqttReconnect(){
  while(!mqtt.connected()){
    mqtt.connect("ESP32-Nada", mqtt_user, mqtt_pass);
    mqtt.subscribe(mqtt_topic_control);
    delay(1000);
  }
}

/* ================= PUBLISH ================= */
void publishStatus(){
  StaticJsonDocument<64> doc;
  doc["status"] = (currentPwm > 0) ? "ON" : "OFF";
  doc["brightness"] = brightnessLevel;
  doc["source"] = publishSource;

  char buf[64];
  serializeJson(doc, buf);
  mqtt.publish(mqtt_topic_status, buf);

  serializeJson(doc, Serial);
  Serial.println();
}

/* ================= RTOS ================= */
void monitorTask(void* p){
  for(;;){
    if(millis() - lastPublish > interval){
      lastPublish = millis();
      publishSource = "rtos";
      needPublish = true;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/* ================= SETUP ================= */
void setup(){
  Serial.begin(115200);

  prefs.begin("cfg", false);
  ledPower = prefs.getBool("power", false);
  brightnessLevel = prefs.getInt("level", 0);

  ledcAttach(LED1_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(LED2_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(LED3_PIN, PWM_FREQ, PWM_RES);

  applyLed();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, handleButton, FALLING);
  attachInterrupt(BUTTON2_PIN, handleButton, FALLING);

/* ================= wifi conection ================= */
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) delay(500);

/* ================= MQTT DGN TLS ================= */
  secureClient.setInsecure();
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);

  server.on("/", [](){ server.send(200,"text/html",page); });

/* ================= HTTP WEB SERVER ================= */
  server.on("/cmd", [](){
    StaticJsonDocument<32> d;
    d["cmd"] = server.arg("c");
    char buf[32];
    serializeJson(d, buf);
    mqtt.publish(mqtt_topic_control, buf);
    server.send(200,"text/plain","OK");
  });

  server.on("/status", [](){
    StaticJsonDocument<32> d;
    d["status"] = (currentPwm > 0) ? "ON" : "OFF";
    String out;
    serializeJson(d, out);
    server.send(200,"application/json", out);
  });

  server.begin();
  xTaskCreate(monitorTask, "Monitor", 4096, NULL, 1, NULL);
}

/* ================= LOOP ================= */
void loop(){
  if(!mqtt.connected()) mqttReconnect();
  mqtt.loop();
  server.handleClient();

  if(buttonPressed){
    buttonPressed = false;

    ledPower = true;
    brightnessLevel++;
    if(brightnessLevel > 2) brightnessLevel = 0;

    applyLed();
    prefs.putBool("power", ledPower);
    prefs.putInt("level", brightnessLevel);

    publishSource = "button";
    needPublish = true;
  }

  if(needPublish){
    needPublish = false;
    publishStatus();
  }
}


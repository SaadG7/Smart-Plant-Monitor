#include <Arduino.h>

/*
  SMART PLANT MONITOR - ESP32 mit Web-Dashboard
  ================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- WLAN-ZUGANGSDATEN ----------
const char* WIFI_SSID     = "Smartiot";
const char* WIFI_PASSWORD = "Smart1234";

// ---------- PIN-BELEGUNG ----------
#define OLED_SDA      21
#define OLED_SCL      22
#define MOISTURE_PIN  35
#define DS18B20_PIN   13
#define LED_GRUEN     25
#define LED_GELB      26
#define LED_ROT       27

// ---------- OLED SETUP ----------
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- DS18B20 SETUP ----------
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
bool ds18b20Ok = false;

// ---------- KALIBRIERUNG FEUCHTIGKEITSSENSOR ----------
int TROCKEN_WERT = 3195;
int NASS_WERT    = 1090;

// ---------- WEBSERVER ----------
WebServer server(80);

// ---------- AKTUELLE MESSWERTE ----------
int   g_feuchtigkeit = 0;
float g_temperatur    = -999;
String g_status       = "startet...";

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Plant Monitor</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, Segoe UI, Roboto, sans-serif;
    background: #F5F5F0;
    color: #22291F;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 32px 20px;
    min-height: 100vh;
  }
  h1 { font-size: 22px; margin-bottom: 4px; }
  .sub { color: #6B7568; font-size: 13px; margin-bottom: 28px; }
  .card {
    background: white;
    border-radius: 20px;
    padding: 28px;
    width: 100%;
    max-width: 360px;
    box-shadow: 0 4px 18px rgba(0,0,0,0.08);
    text-align: center;
    margin-bottom: 18px;
  }
  .big { font-size: 56px; font-weight: 700; }
  .label { color: #6B7568; font-size: 13px; margin-top: 6px; }
  .status {
    display: inline-block;
    margin-top: 14px;
    padding: 8px 18px;
    border-radius: 999px;
    color: white;
    font-weight: 600;
    font-size: 14px;
  }
  .row { display: flex; gap: 14px; width: 100%; max-width: 360px; }
  .row .card { flex: 1; padding: 18px; }
  .row .big { font-size: 30px; }
  .dot { height: 8px; width: 8px; border-radius: 50%; display:inline-block; margin-right:6px; }
</style>
</head>
<body>
  <h1>&#127793; Smart Plant Monitor</h1>
  <div class="sub" id="conn"><span class="dot" style="background:#97BC62"></span>verbunden</div>

  <div class="card">
    <div class="big" id="feuchtigkeit">--%</div>
    <div class="label">Bodenfeuchte</div>
    <div class="status" id="status">lädt...</div>
  </div>

  <div class="row">
    <div class="card">
      <div class="big" id="temperatur">--</div>
      <div class="label">Temperatur (°C)</div>
    </div>
    <div class="card">
      <div class="big" id="zeit">0s</div>
      <div class="label">letzte Aktualisierung</div>
    </div>
  </div>

<script>
let sekunden = 0;
async function update() {
  try {
    const r = await fetch('/data');
    const d = await r.json();
    document.getElementById('feuchtigkeit').innerText = d.feuchtigkeit + '%';
    document.getElementById('temperatur').innerText = d.temperatur;
    const statusEl = document.getElementById('status');
    statusEl.innerText = d.status;
    const farben = { 'Pflanze OK': '#2E8B4A', 'Bald giessen': '#E8A33D', 'Braucht Wasser!': '#C0503D' };
    statusEl.style.background = farben[d.status] || '#6B7568';
    document.getElementById('conn').innerHTML = '<span class="dot" style="background:#97BC62"></span>verbunden';
    sekunden = 0;
  } catch (e) {
    document.getElementById('conn').innerHTML = '<span class="dot" style="background:#C0503D"></span>keine Verbindung';
  }
}
setInterval(() => {
  sekunden++;
  document.getElementById('zeit').innerText = sekunden + 's';
}, 1000);
update();
setInterval(update, 2000);
</script>
</body>
</html>
)HTML";

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleData() {
  String json = "{";
  json += "\"feuchtigkeit\":" + String(g_feuchtigkeit) + ",";
  json += "\"temperatur\":" + String(ds18b20Ok ? String(g_temperatur, 1) : String("n/a")) + ",";
  json += "\"status\":\"" + g_status + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED nicht gefunden!");
    while (true) delay(10);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  ds18b20.begin();
  ds18b20Ok = (ds18b20.getDeviceCount() > 0);

  pinMode(LED_GRUEN, OUTPUT);
  pinMode(LED_GELB, OUTPUT);
  pinMode(LED_ROT, OUTPUT);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Verbinde WLAN...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Verbinde mit WLAN");
  int versuche = 0;
  while (WiFi.status() != WL_CONNECTED && versuche < 30) {
    delay(500);
    Serial.print(".");
    versuche++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Verbunden! IP-Adresse: ");
    Serial.println(WiFi.localIP());
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WLAN verbunden!");
    display.println(WiFi.localIP().toString());
    display.display();
    delay(2500);
  } else {
    Serial.println();
    Serial.println("WLAN-Verbindung fehlgeschlagen.");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Kein WLAN!");
    display.println("Weiter ohne Web.");
    display.display();
    delay(2000);
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();

  static unsigned long letzteMessung = 0;
  if (millis() - letzteMessung < 2000) return;
  letzteMessung = millis();

  int rohwert = analogRead(MOISTURE_PIN);
  int feuchtigkeit = map(rohwert, TROCKEN_WERT, NASS_WERT, 0, 100);
  feuchtigkeit = constrain(feuchtigkeit, 0, 100);

  float temperatur = -999;
  if (ds18b20Ok) {
    ds18b20.requestTemperatures();
    temperatur = ds18b20.getTempCByIndex(0);
  }

  String status;
  if (feuchtigkeit > 60) {
    status = "Pflanze OK";
    digitalWrite(LED_GRUEN, HIGH);
    digitalWrite(LED_GELB, LOW);
    digitalWrite(LED_ROT, LOW);
  } else if (feuchtigkeit >= 30) {
    status = "Bald giessen";
    digitalWrite(LED_GRUEN, LOW);
    digitalWrite(LED_GELB, HIGH);
    digitalWrite(LED_ROT, LOW);
  } else {
    status = "Braucht Wasser!";
    digitalWrite(LED_GRUEN, LOW);
    digitalWrite(LED_GELB, LOW);
    digitalWrite(LED_ROT, HIGH);
  }

  g_feuchtigkeit = feuchtigkeit;
  g_temperatur = temperatur;
  g_status = status;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Smart Plant Monitor");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print(feuchtigkeit);
  display.println("%");

  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("Temp: ");
  if (ds18b20Ok) {
    display.print(temperatur);
    display.println(" C");
  } else {
    display.println("n/a");
  }

  display.setCursor(0, 54);
  display.println(status);
  display.display();

  Serial.print("Rohwert: ");
  Serial.print(rohwert);
  Serial.print(" | Feuchtigkeit: ");
  Serial.print(feuchtigkeit);
  Serial.print("% | Temp: ");
  if (ds18b20Ok) { Serial.print(temperatur); Serial.print(" C"); } else { Serial.print("n/a"); }
  Serial.print(" | Status: ");
  Serial.print(status);
  Serial.print(" | IP: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "-");
}

//====================================================================================================
//                  Software Sensor de temperatura/Umidade DHT11 e Sensor de corrente
//                  Autor: Sergio Levy Nobre dos Santos
//                  Departamento de Física - Universidade Federal do Ceará - UFC
//
//                  Placa: ideaspark ESP32 (ESP32-WROOM-32) com OLED 0,96" integrado, rede via Wi-Fi.
//                  Configurações em config.h. Credenciais em secrets.h (modelo: secrets.example.h).
//====================================================================================================

//********************************************************************************
//************************** INCLUDES ********************************************
//********************************************************************************

#include "config.h"

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <Wire.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#if LCD_ENABLED
#include <LiquidCrystal_I2C.h>  // Só é necessária com o LCD 20x4 opcional
#endif

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "firmware/secrets.h nao encontrado: usando as credenciais de exemplo de secrets.example.h"
#include "secrets.example.h"
#endif

//********************************************************************************
//************************** OBJETOS E VARIÁVEIS GLOBAIS *************************
//********************************************************************************

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences prefs;
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
#if LCD_ENABLED
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
#endif

DHT* dht[NUM_DHT];  // Criados no setup() a partir de DHT_PINS

// Leituras de temperatura/umidade (NAN = falha de leitura)
float temperature[NUM_DHT];
float humidity[NUM_DHT];
uint8_t dhtFailCount[NUM_DHT];
float tempMean = NAN, humMean = NAN;
uint8_t sensorsOk = 0;

// Ar-condicionado: corrente, potência estimada e energia acumulada desde o boot
float acCurrent[NUM_AC];
float acPower[NUM_AC];
double acEnergyKwh[NUM_AC];
unsigned long lastEnergyUpdate = 0;

// Alertas
float tempMax = TEMP_MAX_DEFAULT;
bool tempAlert = false;
bool acOffHoursAlert[NUM_AC];
bool dhtFailAlert[NUM_DHT];

// Estado
bool oledOk = false;
bool otaStarted = false;
bool bootReported = false;
uint8_t displayPage = 0;
bool displayRedraw = true;

// Temporização
unsigned long lastRead = 0, lastPublish = 0, lastInfo = 0, lastDisplayRefresh = 0, lastDisplayPage = 0;
unsigned long lastWifiAttempt = 0, lastMqttAttempt = 0, mqttLastConnected = 0;
unsigned long mqttRetryDelay = MQTT_RETRY_MIN_MS;

// Buffer único para os JSON publicados
char payload[512];

#define DISPLAY_PAGES 3
#define DISPLAY_LINES 4
#define LINE_CHARS    20
#define DEGREE_CHAR   '\x01'  // Marcador do símbolo de grau nas linhas de texto

// Conteúdo de uma tela: 4 linhas de 20 caracteres (mostradas no OLED e no LCD)
typedef char DisplayLines[DISPLAY_LINES][LINE_CHARS + 1];

#if LCD_ENABLED
byte degreeSymbol[8] = {
  0b00001100,
  0b00010010,
  0b00010010,
  0b00001100,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000};
#endif

const char* const WEEKDAYS[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
const char* const PAGE_TITLES[DISPLAY_PAGES] = {"Rede", "Clima", "Energia"};

//********************************************************************************
//************************** UTILITÁRIOS *****************************************
//********************************************************************************

float roundTo(float value, uint8_t decimals) {
  float factor = 1;
  while (decimals--) factor *= 10;
  return roundf(value * factor) / factor;
}

// Formata um float para o display ("--.-" quando a leitura falhou)
const char* formatFloat(char* buf, size_t size, float value, uint8_t decimals) {
  if (isnan(value)) {
    snprintf(buf, size, "--.-");
  } else {
    snprintf(buf, size, "%.*f", decimals, value);
  }
  return buf;
}

// Grava um número no JSON; leituras com falha viram null
void jsonSetNumber(JsonDocument& doc, const char* key, float value, uint8_t decimals) {
  if (isnan(value)) {
    doc[key] = nullptr;
  } else {
    doc[key] = roundTo(value, decimals);
  }
}

bool clockValid() {
  return time(nullptr) > 1700000000;  // Antes do NTP o relógio começa em 1970
}

void currentTime(struct tm& tm) {
  time_t now = time(nullptr);
  localtime_r(&now, &tm);
}

bool isAcOn(uint8_t i) {
  return acCurrent[i] >= AC_ON_THRESHOLD_A;
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

//********************************************************************************
//************************** WATCHDOG E REINÍCIO *********************************
//********************************************************************************

void watchdogStart() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfg = {
    .timeout_ms = WATCHDOG_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  // O core já inicializa o watchdog de tarefas; só ajusta o tempo (ou inicializa, se não estiver ativo)
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);  // Monitora a tarefa do loop()
}

const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "energizado";
    case ESP_RST_EXT:      return "reset externo";
    case ESP_RST_SW:       return "reinicio por software";
    case ESP_RST_PANIC:    return "falha (panic)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_BROWNOUT: return "queda de tensao";
    default:               return "desconhecida";
  }
}

//********************************************************************************
//************************** CONFIGURAÇÃO PERSISTENTE ****************************
//********************************************************************************

void loadSettings() {
  prefs.begin("clima", false);
  float value = prefs.getFloat("tempMax", TEMP_MAX_DEFAULT);
  if (value >= TEMP_MAX_MIN && value <= TEMP_MAX_MAX) tempMax = value;
}

void saveSettings() {
  prefs.putFloat("tempMax", tempMax);
}

//********************************************************************************
//************************** DISPLAYS (OLED E LCD) *******************************
//********************************************************************************


void buildPageNetwork(DisplayLines& l) {
  if (wifiConnected()) {
    snprintf(l[0], sizeof(l[0]), "IP %s", WiFi.localIP().toString().c_str());
  } else {
    snprintf(l[0], sizeof(l[0]), "Wi-Fi desconectado");
  }

  if (clockValid()) {
    struct tm tm;
    currentTime(tm);
    snprintf(l[1], sizeof(l[1]), "Hora: %02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    snprintf(l[2], sizeof(l[2]), "Data: %s %02d/%02d/%04d",
             WEEKDAYS[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
  } else {
    snprintf(l[1], sizeof(l[1]), "Hora: --:--:--");
    snprintf(l[2], sizeof(l[2]), "Data: aguardando NTP");
  }

  if (wifiConnected()) {
    snprintf(l[3], sizeof(l[3]), "MQTT:%s WiFi:%ddBm", mqttClient.connected() ? "ON " : "OFF", (int)WiFi.RSSI());
  } else {
    snprintf(l[3], sizeof(l[3]), "MQTT:OFF");
  }
}

void buildPageClimate(DisplayLines& l) {
  char value[12];
  snprintf(l[0], sizeof(l[0]), "Temp: %s%cC", formatFloat(value, sizeof(value), tempMean, 1), DEGREE_CHAR);
  snprintf(l[1], sizeof(l[1]), "Umid: %s %%", formatFloat(value, sizeof(value), humMean, 1));
  snprintf(l[2], sizeof(l[2]), "AR1  |  AR2  |  AR3");
  snprintf(l[3], sizeof(l[3]), "%s  |  %s  |  %s",
           isAcOn(0) ? "ON " : "OFF", isAcOn(1) ? "ON " : "OFF", isAcOn(2) ? "ON " : "OFF");
}

void buildPageEnergy(DisplayLines& l) {
  char value[16];
  float totalPower = 0;
  double totalEnergy = 0;
  for (uint8_t i = 0; i < NUM_AC; i++) {
    totalPower += acPower[i];
    totalEnergy += acEnergyKwh[i];
  }
  snprintf(l[0], sizeof(l[0]), "Potencia: %s W", formatFloat(value, sizeof(value), totalPower, 0));
  snprintf(l[1], sizeof(l[1]), "Energia: %s kWh", formatFloat(value, sizeof(value), totalEnergy, 2));
  snprintf(l[2], sizeof(l[2]), "Sensores OK: %u/%u", sensorsOk, NUM_DHT);

  bool acAlert = false, dhtAlert = false;
  for (uint8_t i = 0; i < NUM_AC; i++) acAlert |= acOffHoursAlert[i];
  for (uint8_t i = 0; i < NUM_DHT; i++) dhtAlert |= dhtFailAlert[i];
  if (tempAlert)      snprintf(l[3], sizeof(l[3]), "ALERTA: TEMP ALTA");
  else if (acAlert)   snprintf(l[3], sizeof(l[3]), "ALERTA: AR FORA HORA");
  else if (dhtAlert)  snprintf(l[3], sizeof(l[3]), "ALERTA: SENSOR FALHO");
  else                snprintf(l[3], sizeof(l[3]), "Sem alertas");
}

// OLED: barra de título (faixa amarela nos displays bicolores) + 4 linhas
void drawOled(const char* title, const DisplayLines& l) {
  if (!oledOk) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  char header[22];
  if (clockValid()) {
    struct tm tm;
    currentTime(tm);
    snprintf(header, sizeof(header), "%-10s %02d:%02d", title, tm.tm_hour, tm.tm_min);
  } else {
    snprintf(header, sizeof(header), "%s", title);
  }
  oled.setCursor(0, 2);
  oled.print(header);
  oled.drawFastHLine(0, 12, OLED_WIDTH, SSD1306_WHITE);

  for (uint8_t row = 0; row < DISPLAY_LINES; row++) {
    oled.setCursor(0, 17 + row * 12);
    for (const char* c = l[row]; *c; c++) {
      oled.write(*c == DEGREE_CHAR ? (char)0xF8 : *c);  // 0xF8 = ° na fonte padrão (cp437)
    }
  }
  oled.display();
}

// LCD 20x4: escreve as linhas inteiras completando com espaços (sem lcd.clear(), evita piscar)
void drawLcd(const DisplayLines& l) {
#if LCD_ENABLED
  for (uint8_t row = 0; row < DISPLAY_LINES; row++) {
    lcd.setCursor(0, row);
    uint8_t i = 0;
    for (; i < LCD_COLS && l[row][i]; i++) lcd.write(l[row][i] == DEGREE_CHAR ? 1 : l[row][i]);
    for (; i < LCD_COLS; i++) lcd.write(' ');
  }
#endif
}

void showLines(const char* title, const DisplayLines& l) {
  drawOled(title, l);
  drawLcd(l);
}

// Mensagem de status (boot, conexão, OTA)
void showMessage(const char* line1, const char* line2 = "", const char* line3 = "") {
  DisplayLines l = {};
  snprintf(l[0], sizeof(l[0]), "%s", line1);
  snprintf(l[1], sizeof(l[1]), "%s", line2);
  snprintf(l[2], sizeof(l[2]), "%s", line3);
  showLines("Clima UFC", l);
}

void updateDisplays() {
  DisplayLines l = {};
  switch (displayPage) {
    case 0: buildPageNetwork(l); break;
    case 1: buildPageClimate(l); break;
    default: buildPageEnergy(l); break;
  }
  showLines(PAGE_TITLES[displayPage], l);
}

void setupDisplays() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
#if OLED_ENABLED
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  if (oledOk) {
    oled.cp437(true);
  } else {
    Serial.println(F("OLED nao encontrado"));
  }
#endif
#if LCD_ENABLED
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.createChar(1, degreeSymbol);
#endif
}

//********************************************************************************
//************************** LEITURA DOS SENSORES ********************************
//********************************************************************************

void readClimateSensors() {
  float tSum = 0, hSum = 0;
  sensorsOk = 0;
  for (uint8_t i = 0; i < NUM_DHT; i++) {
    humidity[i] = dht[i]->readHumidity();
    temperature[i] = dht[i]->readTemperature();
    if (!isnan(temperature[i]) && !isnan(humidity[i])) {
      sensorsOk++;
      tSum += temperature[i];
      hSum += humidity[i];
      dhtFailCount[i] = 0;
    } else if (dhtFailCount[i] < 255) {
      dhtFailCount[i]++;
    }
  }
  // Média só dos sensores válidos; sem nenhum válido a média é "desconhecida" (null)
  tempMean = sensorsOk ? tSum / sensorsOk : NAN;
  humMean = sensorsOk ? hSum / sensorsOk : NAN;
}

// Corrente RMS: amostra o sinal por SCT_SAMPLE_MS e calcula o desvio padrão em torno
// do ponto médio. analogReadMilliVolts() usa a calibração de fábrica do ADC do ESP32.
float measureIrms(uint8_t pin) {
  double sum = 0, sumSq = 0;
  uint32_t n = 0;
  unsigned long start = millis();
  while (millis() - start < SCT_SAMPLE_MS) {
    double mv = analogReadMilliVolts(pin);
    sum += mv;
    sumSq += mv * mv;
    n++;
  }
  if (n == 0) return 0;
  double mean = sum / n;
  double variance = sumSq / n - mean * mean;
  double irms = variance > 0 ? sqrt(variance) / 1000.0 * SCT_CALIBRATION : 0;
  // Desconta o ruído do ADC (soma em quadratura)
  double noise = SCT_NOISE_FLOOR_A;
  return irms > noise ? sqrt(irms * irms - noise * noise) : 0;
}

// Mede cada sensor de corrente UMA vez por ciclo; display, MQTT e alertas usam o mesmo valor
void readCurrentSensors() {
  unsigned long now = millis();
  double hours = (now - lastEnergyUpdate) / 3600000.0;
  lastEnergyUpdate = now;

  for (uint8_t i = 0; i < NUM_AC; i++) {
    acCurrent[i] = measureIrms(AC_PINS[i]);
    // Abaixo do limite é ruído do sensor: não conta como consumo
    acPower[i] = isAcOn(i) ? acCurrent[i] * MAINS_VOLTAGE * POWER_FACTOR : 0;
    acEnergyKwh[i] += acPower[i] * hours / 1000.0;
  }
}

//********************************************************************************
//************************** MQTT ************************************************
//********************************************************************************

bool mqttPublishJson(const char* topic, JsonDocument& doc, bool retained, bool log) {
  if (!mqttClient.connected()) return false;
  size_t len = measureJson(doc);
  if (len >= sizeof(payload)) {
    Serial.printf("JSON grande demais para %s\n", topic);
    return false;
  }
  serializeJson(doc, payload, sizeof(payload));
  bool ok = mqttClient.beginPublish(topic, len, retained) &&
            mqttClient.write((const uint8_t*)payload, len) == len &&
            mqttClient.endPublish();
  if (!ok) {
    Serial.printf("Falha ao publicar em %s\n", topic);
  } else if (log && LOG_PAYLOADS) {
    Serial.printf("MQTT > %s: %s\n", topic, payload);
  }
  return ok;
}

void publishMeasurements() {
  char key[20];
  JsonDocument doc;

  for (uint8_t i = 0; i < NUM_DHT; i++) {
    snprintf(key, sizeof(key), "Temp%u", i + 1);
    jsonSetNumber(doc, key, temperature[i], 1);
  }
  jsonSetNumber(doc, "MeanValueTemp", tempMean, 1);
  doc["SensoresOK"] = sensorsOk;
  doc["AlertaTemp"] = tempAlert ? "ON" : "OFF";
  mqttPublishJson(TOPIC_TEMP, doc, false, true);

  doc.clear();
  for (uint8_t i = 0; i < NUM_DHT; i++) {
    snprintf(key, sizeof(key), "Hty%u", i + 1);
    jsonSetNumber(doc, key, humidity[i], 1);
  }
  jsonSetNumber(doc, "MeanValueHumidity", humMean, 1);
  mqttPublishJson(TOPIC_UMID, doc, false, true);

  doc.clear();
  for (uint8_t i = 0; i < NUM_AC; i++) {
    snprintf(key, sizeof(key), "Ar%u", i + 1);
    jsonSetNumber(doc, key, acCurrent[i], 2);
  }
  mqttPublishJson(TOPIC_AC, doc, false, true);

  doc.clear();
  float totalPower = 0;
  double totalEnergy = 0;
  for (uint8_t i = 0; i < NUM_AC; i++) {
    snprintf(key, sizeof(key), "Pot%u", i + 1);
    jsonSetNumber(doc, key, acPower[i], 0);
    snprintf(key, sizeof(key), "kWh%u", i + 1);
    jsonSetNumber(doc, key, acEnergyKwh[i], 3);
    snprintf(key, sizeof(key), "Estado%u", i + 1);
    doc[key] = isAcOn(i) ? "ON" : "OFF";
    totalPower += acPower[i];
    totalEnergy += acEnergyKwh[i];
  }
  jsonSetNumber(doc, "PotTotal", totalPower, 0);
  jsonSetNumber(doc, "kWhTotal", totalEnergy, 3);
  mqttPublishJson(TOPIC_ENERGY, doc, false, true);
}

void publishInfo() {
  JsonDocument doc;
  doc["RSSI"] = WiFi.RSSI();
  doc["IP"] = WiFi.localIP().toString();
  doc["Uptime"] = millis() / 1000;
  mqttPublishJson(TOPIC_INFO, doc, false, true);
}

void publishTempMax() {
  if (!mqttClient.connected()) return;
  char value[10];
  snprintf(value, sizeof(value), "%.1f", tempMax);
  mqttClient.publish(TOPIC_TEMPMAX, value, true);
}

// Alerta em TOPIC_WARNINGS: {"tipo":"TEMP_ALTA","ativo":true,"valor":28.1,"limite":27,"sensor":1}
void publishAlert(const char* type, bool active, float value, float limit, int index) {
  Serial.printf("ALERTA %s %s", type, active ? "ativo" : "normalizado");
  if (index > 0) Serial.printf(" (sensor %d)", index);
  Serial.println();

  JsonDocument doc;
  doc["tipo"] = type;
  doc["ativo"] = active;
  if (!isnan(value)) doc["valor"] = roundTo(value, 2);
  if (!isnan(limit)) doc["limite"] = roundTo(limit, 2);
  if (index > 0) doc["sensor"] = index;
  mqttPublishJson(TOPIC_WARNINGS, doc, false, true);
}

void publishBootInfo() {
  JsonDocument doc;
  doc["tipo"] = "BOOT";
  doc["versao"] = FIRMWARE_VERSION;
  doc["causa"] = resetReasonText();
  bootReported = mqttPublishJson(TOPIC_WARNINGS, doc, false, true);
}

// Comandos recebidos. Hoje: TOPIC_TEMPMAX_SET com o novo limite de temperatura (ex.: "26.5")
void mqttCallback(char* topic, byte* data, unsigned int length) {
  // Copia para um buffer próprio (o payload não termina em '\0')
  char msg[16];
  unsigned int n = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
  memcpy(msg, data, n);
  msg[n] = '\0';
  Serial.printf("MQTT < %s: %s\n", topic, msg);

  if (strcmp(topic, TOPIC_TEMPMAX_SET) == 0) {
    char* comma = strchr(msg, ',');
    if (comma) *comma = '.';  // aceita "26,5"
    char* end;
    float value = strtof(msg, &end);
    if (end != msg && value >= TEMP_MAX_MIN && value <= TEMP_MAX_MAX) {
      tempMax = value;
      saveSettings();
      Serial.printf("Novo limite de temperatura: %.1f\n", tempMax);
    } else {
      Serial.println(F("Limite de temperatura invalido, ignorado"));
    }
    publishTempMax();  // Confirma o valor em vigor (também desfaz valores inválidos no HA)
  }
}

#if HA_DISCOVERY_ENABLED
// Publica a configuração de uma entidade no MQTT Discovery do Home Assistant
void publishDiscovery(const char* component, const char* objectId, const char* name,
                      const char* stateTopic, const char* valueKey, const char* unit,
                      const char* deviceClass, const char* stateClass, bool diagnostic = false) {
  char topic[96], buf[48];
  snprintf(topic, sizeof(topic), HA_DISCOVERY_PREFIX "/%s/" DEVICE_ID "/%s/config", component, objectId);

  JsonDocument doc;
  doc["~"] = MQTT_BASE_TOPIC;
  doc["name"] = name;
  snprintf(buf, sizeof(buf), DEVICE_ID "_%s", objectId);
  doc["uniq_id"] = buf;
  doc["stat_t"] = stateTopic;
  if (valueKey) {
    snprintf(buf, sizeof(buf), "{{ value_json.%s }}", valueKey);
    doc["val_tpl"] = buf;
  }
  if (unit) doc["unit_of_meas"] = unit;
  if (deviceClass) doc["dev_cla"] = deviceClass;
  if (stateClass) doc["stat_cla"] = stateClass;
  if (diagnostic) doc["ent_cat"] = "diagnostic";
  doc["avty_t"] = "~/status";

  if (strcmp(component, "number") == 0) {
    doc["cmd_t"] = "~/TempMax/set";
    doc["min"] = TEMP_MAX_MIN;
    doc["max"] = TEMP_MAX_MAX;
    doc["step"] = 0.5;
    doc["mode"] = "box";
  }

  JsonObject dev = doc["dev"].to<JsonObject>();
  dev["ids"] = DEVICE_ID;
  dev["name"] = DEVICE_NAME;
  dev["mf"] = "UFC - Departamento de Fisica";
  dev["mdl"] = DEVICE_MODEL;
  dev["sw"] = FIRMWARE_VERSION;

  mqttPublishJson(topic, doc, true, false);
  esp_task_wdt_reset();
}

void publishAllDiscovery() {
  char objectId[16], name[32], key[20];
  const char* CELSIUS = "°C";

  for (uint8_t i = 0; i < NUM_DHT; i++) {
    snprintf(objectId, sizeof(objectId), "temp%u", i + 1);
    snprintf(name, sizeof(name), "Temperatura %u", i + 1);
    snprintf(key, sizeof(key), "Temp%u", i + 1);
    publishDiscovery("sensor", objectId, name, "~/Temp", key, CELSIUS, "temperature", "measurement");

    snprintf(objectId, sizeof(objectId), "umid%u", i + 1);
    snprintf(name, sizeof(name), "Umidade %u", i + 1);
    snprintf(key, sizeof(key), "Hty%u", i + 1);
    publishDiscovery("sensor", objectId, name, "~/Umid", key, "%", "humidity", "measurement");
  }
  publishDiscovery("sensor", "temp_media", "Temperatura média", "~/Temp", "MeanValueTemp",
                   CELSIUS, "temperature", "measurement");
  publishDiscovery("sensor", "umid_media", "Umidade média", "~/Umid", "MeanValueHumidity",
                   "%", "humidity", "measurement");
  publishDiscovery("sensor", "sensores_ok", "Sensores DHT OK", "~/Temp", "SensoresOK",
                   nullptr, nullptr, "measurement");
  publishDiscovery("binary_sensor", "alerta_temp", "Temperatura alta", "~/Temp", "AlertaTemp",
                   nullptr, "heat", nullptr);

  for (uint8_t i = 0; i < NUM_AC; i++) {
    snprintf(objectId, sizeof(objectId), "ar%u_corrente", i + 1);
    snprintf(name, sizeof(name), "Ar %u corrente", i + 1);
    snprintf(key, sizeof(key), "Ar%u", i + 1);
    publishDiscovery("sensor", objectId, name, "~/AC", key, "A", "current", "measurement");

    snprintf(objectId, sizeof(objectId), "ar%u_potencia", i + 1);
    snprintf(name, sizeof(name), "Ar %u potência", i + 1);
    snprintf(key, sizeof(key), "Pot%u", i + 1);
    publishDiscovery("sensor", objectId, name, "~/Energia", key, "W", "power", "measurement");

    snprintf(objectId, sizeof(objectId), "ar%u_energia", i + 1);
    snprintf(name, sizeof(name), "Ar %u energia", i + 1);
    snprintf(key, sizeof(key), "kWh%u", i + 1);
    publishDiscovery("sensor", objectId, name, "~/Energia", key, "kWh", "energy", "total_increasing");

    snprintf(objectId, sizeof(objectId), "ar%u_estado", i + 1);
    snprintf(name, sizeof(name), "Ar %u ligado", i + 1);
    snprintf(key, sizeof(key), "Estado%u", i + 1);
    publishDiscovery("binary_sensor", objectId, name, "~/Energia", key, nullptr, "power", nullptr);
  }
  publishDiscovery("sensor", "potencia_total", "Potência total", "~/Energia", "PotTotal",
                   "W", "power", "measurement");
  publishDiscovery("sensor", "energia_total", "Energia total", "~/Energia", "kWhTotal",
                   "kWh", "energy", "total_increasing");
  publishDiscovery("number", "temp_max", "Limite de temperatura", "~/TempMax", nullptr,
                   CELSIUS, "temperature", nullptr);
  publishDiscovery("sensor", "wifi_rssi", "Sinal Wi-Fi", "~/Info", "RSSI",
                   "dBm", "signal_strength", "measurement", true);
  publishDiscovery("sensor", "uptime", "Tempo ligado", "~/Info", "Uptime",
                   "s", "duration", "measurement", true);
}
#endif

bool mqttConnect() {
  char clientId[32];
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(clientId, sizeof(clientId), MQTT_CLIENT_ID_PREFIX "-%02X%02X%02X", mac[3], mac[4], mac[5]);
  const char* user = strlen(MQTT_USER) ? MQTT_USER : nullptr;
  const char* pass = strlen(MQTT_USER) ? MQTT_PASSWORD : nullptr;

  Serial.print(F("Conectando ao broker MQTT... "));
  // LWT: se a conexão cair, o broker publica "offline" no tópico de status
  bool ok = mqttClient.connect(clientId, user, pass, TOPIC_STATUS, 0, true, "offline");
  if (!ok) {
    Serial.printf("falhou, estado=%d\n", mqttClient.state());
    return false;
  }
  Serial.println(F("conectado"));

  mqttClient.publish(TOPIC_STATUS, "online", true);
  mqttClient.subscribe(TOPIC_TEMPMAX_SET);
#if HA_DISCOVERY_ENABLED
  publishAllDiscovery();
#endif
  publishTempMax();
  publishInfo();
  if (!bootReported) publishBootInfo();
  return true;
}

//********************************************************************************
//************************** REDE, OTA E RELÓGIO *********************************
//********************************************************************************

void setupOta() {
#if OTA_ENABLED
  ArduinoOTA.setHostname(WIFI_HOSTNAME);
  if (strlen(OTA_PASSWORD)) ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    Serial.println(F("OTA: atualizando firmware..."));
    showMessage("Atualizando", "firmware (OTA)...");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    esp_task_wdt_reset();  // A atualização roda dentro do loop e pode passar do tempo do watchdog
    static uint8_t lastPct = 255;
    uint8_t pct = total ? done * 100 / total : 0;
    if (pct != lastPct && pct % 10 == 0) {
      lastPct = pct;
      char line[21];
      snprintf(line, sizeof(line), "%u%%", pct);
      showMessage("Atualizando", "firmware (OTA)...", line);
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: erro %u\n", error);
    displayRedraw = true;
  });
  ArduinoOTA.begin();
  otaStarted = true;
#endif
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt = millis();

  Serial.printf("Conectando ao Wi-Fi \"%s\"...\n", WIFI_SSID);
  showMessage("Inicializando", "Conectando Wi-Fi...", WIFI_SSID);
  unsigned long start = millis();
  while (!wifiConnected() && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }

  if (wifiConnected()) {
    Serial.printf("Wi-Fi conectado! IP: %s, sinal: %d dBm\n", WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
    showMessage("Wi-Fi conectado!", WiFi.localIP().toString().c_str());
  } else {
    // Sem rede o sistema continua lendo os sensores; a conexão é refeita no loop
    Serial.println(F("Wi-Fi indisponivel, seguindo sem rede"));
    showMessage("Wi-Fi indisponivel!", "Tentando no fundo...");
  }
  delay(1000);
}

// Mantém Wi-Fi, OTA e MQTT; reinicia a placa se o MQTT ficar fora por muito tempo
void maintainNetwork(unsigned long now) {
  if (!wifiConnected()) {
    // O autoReconnect do ESP32 nem sempre se recupera; força uma nova tentativa periodicamente
    if (now - lastWifiAttempt >= WIFI_RETRY_MS) {
      lastWifiAttempt = now;
      Serial.println(F("Wi-Fi desconectado, tentando reconectar..."));
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  } else if (!otaStarted) {
    setupOta();
  }

#if OTA_ENABLED
  if (otaStarted) ArduinoOTA.handle();
#endif

  if (mqttClient.connected()) {
    mqttClient.loop();
    mqttLastConnected = now;
    return;
  }

#if NETWORK_REBOOT_AFTER_MS > 0
  if (now - mqttLastConnected >= NETWORK_REBOOT_AFTER_MS) {
    Serial.println(F("MQTT desconectado ha muito tempo, reiniciando..."));
    showMessage("Sem rede/MQTT...", "Reiniciando");
    delay(500);
    ESP.restart();
  }
#endif

  if (!wifiConnected() || now - lastMqttAttempt < mqttRetryDelay) return;
  bool ok = mqttConnect();
  lastMqttAttempt = millis();
  // Espera exponencial para não insistir com o broker fora do ar
  mqttRetryDelay = ok ? MQTT_RETRY_MIN_MS : min(mqttRetryDelay * 2, MQTT_RETRY_MAX_MS);
  displayRedraw = true;
}

//********************************************************************************
//************************** ALERTAS *********************************************
//********************************************************************************

void checkAlerts() {
  // Temperatura média acima do limite (com histerese para não oscilar)
  if (!isnan(tempMean)) {
    if (!tempAlert && tempMean > tempMax) {
      tempAlert = true;
      publishAlert("TEMP_ALTA", true, tempMean, tempMax, 0);
    } else if (tempAlert && tempMean < tempMax - TEMP_ALERT_HYSTERESIS) {
      tempAlert = false;
      publishAlert("TEMP_ALTA", false, tempMean, tempMax, 0);
    }
  }

  // Sensor DHT sem responder por várias leituras seguidas
  for (uint8_t i = 0; i < NUM_DHT; i++) {
    if (!dhtFailAlert[i] && dhtFailCount[i] >= DHT_FAIL_ALERT_COUNT) {
      dhtFailAlert[i] = true;
      publishAlert("SENSOR_FALHA", true, NAN, NAN, i + 1);
    } else if (dhtFailAlert[i] && dhtFailCount[i] == 0) {
      dhtFailAlert[i] = false;
      publishAlert("SENSOR_FALHA", false, NAN, NAN, i + 1);
    }
  }

#if AC_ALERT_ENABLED
  // Ar-condicionado ligado fora do expediente
  if (!clockValid()) return;
  struct tm tm;
  currentTime(tm);
  bool weekend = tm.tm_wday == 0 || tm.tm_wday == 6;  // 0 = domingo, 6 = sábado
  bool offHours = (AC_ALERT_WEEKENDS && weekend) || tm.tm_hour < WORK_START_HOUR || tm.tm_hour >= WORK_END_HOUR;
  for (uint8_t i = 0; i < NUM_AC; i++) {
    bool alert = offHours && isAcOn(i);
    if (alert != acOffHoursAlert[i]) {
      acOffHoursAlert[i] = alert;
      publishAlert("AR_FORA_DO_HORARIO", alert, acCurrent[i], AC_ON_THRESHOLD_A, i + 1);
    }
  }
#endif
}

//********************************************************************************
//************************** SETUP ***********************************************
//********************************************************************************

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println(F("Monitor de Climatizacao e Energia v" FIRMWARE_VERSION));
  Serial.printf("Causa do reset: %s\n", resetReasonText());

  setupDisplays();
  showMessage("Inicializando", "v" FIRMWARE_VERSION);
  loadSettings();

  // Sensores DHT
  for (uint8_t i = 0; i < NUM_DHT; i++) {
    dht[i] = new DHT(DHT_PINS[i], DHT_TYPE);
    dht[i]->begin();
  }

  // Sensores de corrente: ADC de 12 bits com atenuação de 11 dB (faixa de ~0 a 3,1 V)
  analogReadResolution(12);
  for (uint8_t i = 0; i < NUM_AC; i++) analogSetPinAttenuation(AC_PINS[i], ADC_11db);
  lastEnergyUpdate = millis();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);  // Sincronização NTP em segundo plano

  setupWifi();

  readClimateSensors();
  readCurrentSensors();
  lastRead = millis();

  if (wifiConnected()) {
    setupOta();
    mqttConnect();
  }
  lastMqttAttempt = mqttLastConnected = millis();

  watchdogStart();
}

//********************************************************************************
//************************** LOOP ************************************************
//********************************************************************************

void loop() {
  esp_task_wdt_reset();
  unsigned long now = millis();

  maintainNetwork(now);

  if (now - lastRead >= READ_INTERVAL_MS) {
    lastRead = now;
    readClimateSensors();
    readCurrentSensors();
    checkAlerts();
  }

  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    publishMeasurements();
  }

  if (now - lastInfo >= INFO_INTERVAL_MS) {
    lastInfo = now;
    publishInfo();
  }

  if (now - lastDisplayPage >= DISPLAY_PAGE_MS) {
    lastDisplayPage = now;
    displayPage = (displayPage + 1) % DISPLAY_PAGES;
    displayRedraw = true;
  }
  if (displayRedraw || now - lastDisplayRefresh >= DISPLAY_REFRESH_MS) {
    lastDisplayRefresh = now;
    displayRedraw = false;
    updateDisplays();
  }
}

//====================================================================================================
//                  Software Sensor de temperatura/Umidade DHT11 e Sensor de corrente
//                  Autor: Sergio Levy Nobre dos Santos
//                  Departamento de Física - Universidade Federal do Ceará - UFC
//
//                  Configurações em config.h. Credenciais em secrets.h (modelo: secrets.example.h).
//====================================================================================================

//********************************************************************************
//************************** INCLUDES ********************************************
//********************************************************************************

#include <UIPEthernet.h>        // ENC28J60 (também fornece EthernetClient e EthernetUDP)
#include <SPI.h>
#include <Wire.h>               // Comunicação I2C com o display
#include <DHT.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <LiquidCrystal_I2C.h>  // Display 20x4 I2C
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EmonLib.h>            // Sensores de corrente
#include <EEPROM.h>
#include <avr/wdt.h>

#include "config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "firmware/secrets.h nao encontrado: usando as credenciais de exemplo de secrets.example.h"
#include "secrets.example.h"
#endif

//********************************************************************************
//************************** WATCHDOG E CAUSA DO RESET ***************************
//********************************************************************************

// Executada antes da inicialização do Arduino (seção .init3, recomendação da avr-libc):
// guarda a causa do último reset e desliga o watchdog, evitando reinícios em sequência.
uint8_t resetCause __attribute__((section(".noinit")));
void captureResetCause(void) __attribute__((naked, used, section(".init3")));
void captureResetCause(void) {
  resetCause = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

bool watchdogArmed = false;

void watchdogArm() {
#if WATCHDOG_ENABLED
  wdt_enable(WDTO_8S);
  watchdogArmed = true;
#endif
}

// DHCP, DNS e conexões TCP da UIPEthernet podem bloquear por dezenas de segundos quando a
// rede não responde (não há timeout configurável), então o watchdog é pausado nelas.
void watchdogPause() {
  if (watchdogArmed) wdt_disable();
}

void watchdogResume() {
  if (watchdogArmed) wdt_enable(WDTO_8S);
}

void rebootNow() {
#if WATCHDOG_ENABLED
  wdt_enable(WDTO_15MS);
  while (true) {}
#else
  asm volatile("jmp 0");
#endif
}

//********************************************************************************
//************************** OBJETOS E VARIÁVEIS GLOBAIS *************************
//********************************************************************************

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
EthernetUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET_SECONDS, NTP_UPDATE_MS);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

DHT* dht[NUM_DHT];      // Criados no setup() a partir de DHT_PINS
EnergyMonitor sct[NUM_AC];

// Leituras de temperatura/umidade (NAN = falha de leitura)
float temperature[NUM_DHT];
float humidity[NUM_DHT];
uint8_t dhtFailCount[NUM_DHT];
float tempMean = NAN, humMean = NAN;
uint8_t sensorsOk = 0;

// Ar-condicionado: corrente, potência estimada e energia acumulada desde o boot
float acCurrent[NUM_AC];
float acPower[NUM_AC];
uint32_t acEnergyWh[NUM_AC];    // Parte inteira (Wh)
float acEnergyWs[NUM_AC];       // Resto ainda menor que 1 Wh (W·s), evita perda de precisão do float
unsigned long lastEnergyUpdate = 0;

// Alertas
float tempMax = TEMP_MAX_DEFAULT;
bool tempAlert = false;
bool acOffHoursAlert[NUM_AC];
bool dhtFailAlert[NUM_DHT];

// Temporização
unsigned long lastRead = 0, lastPublish = 0, lastLcdRefresh = 0, lastLcdPage = 0;
unsigned long lastNtpAttempt = 0, lastMqttAttempt = 0, mqttLastConnected = 0;
unsigned long mqttRetryDelay = MQTT_RETRY_MIN_MS;
bool bootReported = false;
uint8_t lcdPage = 0;
bool lcdRedraw = true;

// Buffer único para os JSON publicados (enviados com beginPublish, sem cópia no PubSubClient)
char payload[512];

#define LCD_PAGES 3

byte degreeSymbol[8] = {
  B00001100,
  B00010010,
  B00010010,
  B00001100,
  B00000000,
  B00000000,
  B00000000,
  B00000000};

const char WEEKDAYS[7][4] PROGMEM = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};

// Configuração persistida na EEPROM
struct Settings {
  uint16_t magic;
  float tempMax;
};
#define SETTINGS_MAGIC 0xC1A7

//********************************************************************************
//************************** UTILITÁRIOS *****************************************
//********************************************************************************

float roundTo(float value, uint8_t decimals) {
  float factor = 1;
  while (decimals--) factor *= 10;
  return roundf(value * factor) / factor;
}

// Formata um float para o display ("--.-" quando a leitura falhou)
char* formatFloat(char* buf, float value, uint8_t decimals) {
  if (isnan(value)) {
    strcpy_P(buf, PSTR("--.-"));
  } else {
    dtostrf(value, 0, decimals, buf);
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
  return timeClient.isTimeSet();
}

void currentTime(tmElements_t& tm) {
  breakTime(timeClient.getEpochTime(), tm);
}

bool isAcOn(uint8_t i) {
  return acCurrent[i] >= AC_ON_THRESHOLD_A;
}

float acEnergyKwh(uint8_t i) {
  return acEnergyWh[i] / 1000.0 + acEnergyWs[i] / 3600000.0;
}

//********************************************************************************
//************************** CONFIGURAÇÃO PERSISTENTE ****************************
//********************************************************************************

void loadSettings() {
  Settings s;
  EEPROM.get(0, s);
  if (s.magic == SETTINGS_MAGIC && s.tempMax >= TEMP_MAX_MIN && s.tempMax <= TEMP_MAX_MAX) {
    tempMax = s.tempMax;
  }
}

void saveSettings() {
  Settings s = {SETTINGS_MAGIC, tempMax};
  EEPROM.put(0, s);  // put() só grava os bytes que mudaram
}

//********************************************************************************
//************************** DISPLAY LCD *****************************************
//********************************************************************************

// Escreve uma linha inteira, completando com espaços (sem lcd.clear(), evita piscar)
void lcdLine(uint8_t row, const char* text) {
  lcd.setCursor(0, row);
  uint8_t i = 0;
  for (; i < LCD_COLS && text[i]; i++) lcd.write(text[i]);
  for (; i < LCD_COLS; i++) lcd.write(' ');
}

void lcdLine_P(uint8_t row, PGM_P text) {
  char line[LCD_COLS + 1];
  strncpy_P(line, text, LCD_COLS);
  line[LCD_COLS] = '\0';
  lcdLine(row, line);
}

void lcdPageNetwork() {
  char line[LCD_COLS + 1];
  IPAddress ip = Ethernet.localIP();
  snprintf_P(line, sizeof(line), PSTR("IP: %u.%u.%u.%u"), ip[0], ip[1], ip[2], ip[3]);
  lcdLine(0, line);

  if (clockValid()) {
    tmElements_t tm;
    currentTime(tm);
    snprintf_P(line, sizeof(line), PSTR("Hora: %02u:%02u:%02u"), tm.Hour, tm.Minute, tm.Second);
    lcdLine(1, line);
    snprintf_P(line, sizeof(line), PSTR("Data: %S %02u/%02u/%04u"),
               WEEKDAYS[tm.Wday - 1], tm.Day, tm.Month, tmYearToCalendar(tm.Year));
    lcdLine(2, line);
  } else {
    lcdLine_P(1, PSTR("Hora: --:--:--"));
    lcdLine_P(2, PSTR("Data: aguardando NTP"));
  }

  lcdLine_P(3, mqttClient.connected() ? PSTR("Status MQTT: ON") : PSTR("Status MQTT: OFF"));
}

void lcdPageClimate() {
  char line[LCD_COLS + 1], value[10];
  snprintf_P(line, sizeof(line), PSTR("Temp: %s\001C"), formatFloat(value, tempMean, 1));
  lcdLine(0, line);
  snprintf_P(line, sizeof(line), PSTR("Umid: %s %%"), formatFloat(value, humMean, 1));
  lcdLine(1, line);
  lcdLine_P(2, PSTR("AR1  |  AR2  |  AR3"));
  snprintf_P(line, sizeof(line), PSTR("%S  |  %S  |  %S"),
             isAcOn(0) ? PSTR("ON ") : PSTR("OFF"),
             isAcOn(1) ? PSTR("ON ") : PSTR("OFF"),
             isAcOn(2) ? PSTR("ON ") : PSTR("OFF"));
  lcdLine(3, line);
}

void lcdPageEnergy() {
  char line[LCD_COLS + 1], value[12];
  float totalPower = 0, totalEnergy = 0;
  for (uint8_t i = 0; i < NUM_AC; i++) {
    totalPower += acPower[i];
    totalEnergy += acEnergyKwh(i);
  }
  snprintf_P(line, sizeof(line), PSTR("Potencia: %s W"), formatFloat(value, totalPower, 0));
  lcdLine(0, line);
  snprintf_P(line, sizeof(line), PSTR("Energia: %s kWh"), formatFloat(value, totalEnergy, 2));
  lcdLine(1, line);
  snprintf_P(line, sizeof(line), PSTR("Sensores OK: %u/%u"), sensorsOk, NUM_DHT);
  lcdLine(2, line);

  bool acAlert = false, dhtAlert = false;
  for (uint8_t i = 0; i < NUM_AC; i++) acAlert |= acOffHoursAlert[i];
  for (uint8_t i = 0; i < NUM_DHT; i++) dhtAlert |= dhtFailAlert[i];
  if (tempAlert)      lcdLine_P(3, PSTR("ALERTA: TEMP ALTA"));
  else if (acAlert)   lcdLine_P(3, PSTR("ALERTA: AR FORA HORA"));
  else if (dhtAlert)  lcdLine_P(3, PSTR("ALERTA: SENSOR FALHO"));
  else                lcdLine_P(3, PSTR("Sem alertas"));
}

void updateLcd() {
  switch (lcdPage) {
    case 0: lcdPageNetwork(); break;
    case 1: lcdPageClimate(); break;
    default: lcdPageEnergy(); break;
  }
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

// Mede cada sensor de corrente UMA vez por ciclo; LCD, MQTT e alertas usam o mesmo valor
void readCurrentSensors() {
  unsigned long now = millis();
  float dtSeconds = (now - lastEnergyUpdate) / 1000.0;
  lastEnergyUpdate = now;

  for (uint8_t i = 0; i < NUM_AC; i++) {
    acCurrent[i] = sct[i].calcIrms(SCT_SAMPLES);
    // Abaixo do limite é ruído do sensor: não conta como consumo
    acPower[i] = isAcOn(i) ? acCurrent[i] * MAINS_VOLTAGE * POWER_FACTOR : 0;

    acEnergyWs[i] += acPower[i] * dtSeconds;
    uint32_t wh = acEnergyWs[i] / 3600.0;
    acEnergyWh[i] += wh;
    acEnergyWs[i] -= wh * 3600.0;
  }
}

//********************************************************************************
//************************** MQTT ************************************************
//********************************************************************************

bool mqttPublishJson(const char* topic, JsonDocument& doc, bool retained, bool log) {
  if (!mqttClient.connected()) return false;
  size_t len = measureJson(doc);
  if (len >= sizeof(payload)) {
    Serial.print(F("JSON grande demais para "));
    Serial.println(topic);
    return false;
  }
  serializeJson(doc, payload, sizeof(payload));
  bool ok = mqttClient.beginPublish(topic, len, retained) &&
            mqttClient.write((const uint8_t*)payload, len) == len &&
            mqttClient.endPublish();
  if (!ok) {
    Serial.print(F("Falha ao publicar em "));
    Serial.println(topic);
  } else if (log && LOG_PAYLOADS) {
    Serial.print(F("MQTT > "));
    Serial.print(topic);
    Serial.print(F(": "));
    Serial.println(payload);
  }
  return ok;
}

void publishMeasurements() {
  char key[20];
  JsonDocument doc;

  for (uint8_t i = 0; i < NUM_DHT; i++) {
    snprintf_P(key, sizeof(key), PSTR("Temp%u"), i + 1);
    jsonSetNumber(doc, key, temperature[i], 1);
  }
  jsonSetNumber(doc, "MeanValueTemp", tempMean, 1);
  doc["SensoresOK"] = sensorsOk;
  doc["AlertaTemp"] = tempAlert ? "ON" : "OFF";
  mqttPublishJson(TOPIC_TEMP, doc, false, true);

  doc.clear();
  for (uint8_t i = 0; i < NUM_DHT; i++) {
    snprintf_P(key, sizeof(key), PSTR("Hty%u"), i + 1);
    jsonSetNumber(doc, key, humidity[i], 1);
  }
  jsonSetNumber(doc, "MeanValueHumidity", humMean, 1);
  mqttPublishJson(TOPIC_UMID, doc, false, true);

  doc.clear();
  for (uint8_t i = 0; i < NUM_AC; i++) {
    snprintf_P(key, sizeof(key), PSTR("Ar%u"), i + 1);
    jsonSetNumber(doc, key, acCurrent[i], 2);
  }
  mqttPublishJson(TOPIC_AC, doc, false, true);

  doc.clear();
  float totalPower = 0, totalEnergy = 0;
  for (uint8_t i = 0; i < NUM_AC; i++) {
    snprintf_P(key, sizeof(key), PSTR("Pot%u"), i + 1);
    jsonSetNumber(doc, key, acPower[i], 0);
    snprintf_P(key, sizeof(key), PSTR("kWh%u"), i + 1);
    jsonSetNumber(doc, key, acEnergyKwh(i), 3);
    snprintf_P(key, sizeof(key), PSTR("Estado%u"), i + 1);
    doc[key] = isAcOn(i) ? "ON" : "OFF";
    totalPower += acPower[i];
    totalEnergy += acEnergyKwh(i);
  }
  jsonSetNumber(doc, "PotTotal", totalPower, 0);
  jsonSetNumber(doc, "kWhTotal", totalEnergy, 3);
  mqttPublishJson(TOPIC_ENERGY, doc, false, true);
}

void publishTempMax() {
  if (!mqttClient.connected()) return;
  char value[10];
  dtostrf(tempMax, 0, 1, value);
  mqttClient.publish(TOPIC_TEMPMAX, value, true);
}

// Alerta em TOPIC_WARNINGS: {"tipo":"TEMP_ALTA","ativo":true,"valor":28.1,"limite":27,"sensor":1}
void publishAlert(const __FlashStringHelper* type, bool active, float value, float limit, int8_t index) {
  Serial.print(F("ALERTA "));
  Serial.print(type);
  Serial.print(active ? F(" ativo") : F(" normalizado"));
  if (index > 0) {
    Serial.print(F(" (sensor "));
    Serial.print(index);
    Serial.print(')');
  }
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
  if (resetCause & _BV(WDRF))       doc["causa"] = "watchdog";
  else if (resetCause & _BV(BORF))  doc["causa"] = "queda de tensao";
  else if (resetCause & _BV(EXTRF)) doc["causa"] = "reset externo";
  else if (resetCause & _BV(PORF))  doc["causa"] = "energizado";
  else                              doc["causa"] = "desconhecida";
  bootReported = mqttPublishJson(TOPIC_WARNINGS, doc, false, true);
}

// Comandos recebidos. Hoje: TOPIC_TEMPMAX_SET com o novo limite de temperatura (ex.: "26.5")
void mqttCallback(char* topic, byte* data, unsigned int length) {
  // Copia para um buffer próprio: escrever em data[length] passaria do fim do buffer
  char msg[16];
  unsigned int n = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
  memcpy(msg, data, n);
  msg[n] = '\0';

  Serial.print(F("MQTT < "));
  Serial.print(topic);
  Serial.print(F(": "));
  Serial.println(msg);

  if (strcmp_P(topic, PSTR(TOPIC_TEMPMAX_SET)) == 0) {
    char* comma = strchr(msg, ',');
    if (comma) *comma = '.';  // aceita "26,5"
    char* end;
    float value = strtod(msg, &end);
    if (end != msg && value >= TEMP_MAX_MIN && value <= TEMP_MAX_MAX) {
      tempMax = value;
      saveSettings();
      Serial.print(F("Novo limite de temperatura: "));
      Serial.println(tempMax);
    } else {
      Serial.println(F("Limite de temperatura invalido, ignorado"));
    }
    publishTempMax();  // Confirma o valor em vigor (também desfaz valores inválidos no HA)
  }
}

#if HA_DISCOVERY_ENABLED
// Publica a configuração de uma entidade no MQTT Discovery do Home Assistant
void publishDiscovery(const __FlashStringHelper* component, const char* objectId, const char* name,
                      const __FlashStringHelper* stateTopic, const char* valueKey,
                      const __FlashStringHelper* unit, const __FlashStringHelper* deviceClass,
                      const __FlashStringHelper* stateClass) {
  char topic[96], buf[48];
  snprintf_P(topic, sizeof(topic), PSTR(HA_DISCOVERY_PREFIX "/%S/" DEVICE_ID "/%s/config"),
             (PGM_P)component, objectId);

  JsonDocument doc;
  doc["~"] = MQTT_BASE_TOPIC;
  doc["name"] = name;
  snprintf_P(buf, sizeof(buf), PSTR(DEVICE_ID "_%s"), objectId);
  doc["uniq_id"] = buf;
  doc["stat_t"] = stateTopic;
  if (valueKey) {
    snprintf_P(buf, sizeof(buf), PSTR("{{ value_json.%s }}"), valueKey);
    doc["val_tpl"] = buf;
  }
  if (unit) doc["unit_of_meas"] = unit;
  if (deviceClass) doc["dev_cla"] = deviceClass;
  if (stateClass) doc["stat_cla"] = stateClass;
  doc["avty_t"] = "~/status";

  if (strcmp_P("number", (PGM_P)component) == 0) {
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
  dev["mdl"] = "Arduino Mega 2560 + ENC28J60";
  dev["sw"] = FIRMWARE_VERSION;

  mqttPublishJson(topic, doc, true, false);
  wdt_reset();
}

void publishAllDiscovery() {
  char objectId[16], name[32], key[20];
  const __FlashStringHelper* CELSIUS = F("°C");

  for (uint8_t i = 0; i < NUM_DHT; i++) {
    snprintf_P(objectId, sizeof(objectId), PSTR("temp%u"), i + 1);
    snprintf_P(name, sizeof(name), PSTR("Temperatura %u"), i + 1);
    snprintf_P(key, sizeof(key), PSTR("Temp%u"), i + 1);
    publishDiscovery(F("sensor"), objectId, name, F("~/Temp"), key, CELSIUS, F("temperature"), F("measurement"));

    snprintf_P(objectId, sizeof(objectId), PSTR("umid%u"), i + 1);
    snprintf_P(name, sizeof(name), PSTR("Umidade %u"), i + 1);
    snprintf_P(key, sizeof(key), PSTR("Hty%u"), i + 1);
    publishDiscovery(F("sensor"), objectId, name, F("~/Umid"), key, F("%"), F("humidity"), F("measurement"));
  }
  publishDiscovery(F("sensor"), "temp_media", "Temperatura média", F("~/Temp"), "MeanValueTemp",
                   CELSIUS, F("temperature"), F("measurement"));
  publishDiscovery(F("sensor"), "umid_media", "Umidade média", F("~/Umid"), "MeanValueHumidity",
                   F("%"), F("humidity"), F("measurement"));
  publishDiscovery(F("sensor"), "sensores_ok", "Sensores DHT OK", F("~/Temp"), "SensoresOK",
                   nullptr, nullptr, F("measurement"));
  publishDiscovery(F("binary_sensor"), "alerta_temp", "Temperatura alta", F("~/Temp"), "AlertaTemp",
                   nullptr, F("heat"), nullptr);

  for (uint8_t i = 0; i < NUM_AC; i++) {
    snprintf_P(objectId, sizeof(objectId), PSTR("ar%u_corrente"), i + 1);
    snprintf_P(name, sizeof(name), PSTR("Ar %u corrente"), i + 1);
    snprintf_P(key, sizeof(key), PSTR("Ar%u"), i + 1);
    publishDiscovery(F("sensor"), objectId, name, F("~/AC"), key, F("A"), F("current"), F("measurement"));

    snprintf_P(objectId, sizeof(objectId), PSTR("ar%u_potencia"), i + 1);
    snprintf_P(name, sizeof(name), PSTR("Ar %u potência"), i + 1);
    snprintf_P(key, sizeof(key), PSTR("Pot%u"), i + 1);
    publishDiscovery(F("sensor"), objectId, name, F("~/Energia"), key, F("W"), F("power"), F("measurement"));

    snprintf_P(objectId, sizeof(objectId), PSTR("ar%u_energia"), i + 1);
    snprintf_P(name, sizeof(name), PSTR("Ar %u energia"), i + 1);
    snprintf_P(key, sizeof(key), PSTR("kWh%u"), i + 1);
    publishDiscovery(F("sensor"), objectId, name, F("~/Energia"), key, F("kWh"), F("energy"), F("total_increasing"));

    snprintf_P(objectId, sizeof(objectId), PSTR("ar%u_estado"), i + 1);
    snprintf_P(name, sizeof(name), PSTR("Ar %u ligado"), i + 1);
    snprintf_P(key, sizeof(key), PSTR("Estado%u"), i + 1);
    publishDiscovery(F("binary_sensor"), objectId, name, F("~/Energia"), key, nullptr, F("power"), nullptr);
  }
  publishDiscovery(F("sensor"), "potencia_total", "Potência total", F("~/Energia"), "PotTotal",
                   F("W"), F("power"), F("measurement"));
  publishDiscovery(F("sensor"), "energia_total", "Energia total", F("~/Energia"), "kWhTotal",
                   F("kWh"), F("energy"), F("total_increasing"));
  publishDiscovery(F("number"), "temp_max", "Limite de temperatura", F("~/TempMax"), nullptr,
                   CELSIUS, F("temperature"), nullptr);
}
#endif

bool mqttConnect() {
  char clientId[32];
  snprintf_P(clientId, sizeof(clientId), PSTR(MQTT_CLIENT_ID_PREFIX "-%02X%02X%02X"),
             MAC_ADDRESS[3], MAC_ADDRESS[4], MAC_ADDRESS[5]);
  const char* user = strlen(MQTT_USER) ? MQTT_USER : nullptr;
  const char* pass = strlen(MQTT_USER) ? MQTT_PASSWORD : nullptr;

  Serial.print(F("Conectando ao broker MQTT... "));
  lcdLine_P(3, PSTR("MQTT: conectando..."));

  watchdogPause();
  // LWT: se a conexão cair, o broker publica "offline" no tópico de status
  bool ok = mqttClient.connect(clientId, user, pass, TOPIC_STATUS, 0, true, "offline");
  watchdogResume();

  if (!ok) {
    Serial.print(F("falhou, estado="));
    Serial.println(mqttClient.state());
    return false;
  }
  Serial.println(F("conectado"));

  mqttClient.publish(TOPIC_STATUS, "online", true);
  mqttClient.subscribe(TOPIC_TEMPMAX_SET);
#if HA_DISCOVERY_ENABLED
  publishAllDiscovery();
#endif
  publishTempMax();
  if (!bootReported) publishBootInfo();
  return true;
}

//********************************************************************************
//************************** REDE E RELÓGIO **************************************
//********************************************************************************

void setupEthernet() {
  Ethernet.init(ETHERNET_CS_PIN);
  lcdLine_P(0, PSTR("Inicializando"));
  lcdLine_P(1, PSTR("Ethernet (DHCP)..."));
  Serial.println(F("Inicializando Ethernet (DHCP)..."));

  while (Ethernet.begin(MAC_ADDRESS) == 0) {
    lcdLine_P(1, PSTR("Erro de conexao!"));
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      lcdLine_P(2, PSTR("ENC28J60 ausente!"));
      Serial.println(F("Modulo ENC28J60 nao encontrado!"));
    } else if (Ethernet.linkStatus() == LinkOFF) {
      lcdLine_P(2, PSTR("Cabo desconectado!"));
      Serial.println(F("Cabo desconectado!"));
    } else {
      lcdLine_P(2, PSTR("DHCP sem resposta!"));
      Serial.println(F("DHCP sem resposta!"));
    }
    delay(5000);
    lcdLine_P(1, PSTR("Tentando novamente.."));
  }

  Serial.print(F("Conectado! IP: "));
  Serial.println(Ethernet.localIP());
  lcdLine_P(1, PSTR("Conectado!"));
  lcdLine_P(2, PSTR(""));
}

// Mantém DHCP e MQTT; reinicia a placa se o MQTT ficar fora por muito tempo
void maintainNetwork(unsigned long now) {
  watchdogPause();  // A renovação do DHCP pode bloquear até 60 s se o servidor não responder
  Ethernet.maintain();
  watchdogResume();

  if (mqttClient.connected()) {
    mqttClient.loop();
    mqttLastConnected = now;
    return;
  }

#if NETWORK_REBOOT_AFTER_MS > 0
  if (now - mqttLastConnected >= NETWORK_REBOOT_AFTER_MS) {
    Serial.println(F("MQTT desconectado ha muito tempo, reiniciando..."));
    lcd.clear();
    lcdLine_P(1, PSTR("Sem rede/MQTT..."));
    lcdLine_P(2, PSTR("Reiniciando"));
    delay(500);
    rebootNow();
  }
#endif

  if (now - lastMqttAttempt < mqttRetryDelay) return;
  bool ok = mqttConnect();
  lastMqttAttempt = millis();
  // Espera exponencial para não travar o display repetidamente com o broker fora do ar
  mqttRetryDelay = ok ? MQTT_RETRY_MIN_MS : min(mqttRetryDelay * 2, MQTT_RETRY_MAX_MS);
  lcdRedraw = true;
}

void maintainClock(unsigned long now) {
  unsigned long interval = clockValid() ? NTP_UPDATE_MS : 10000UL;
  if (now - lastNtpAttempt < interval && lastNtpAttempt != 0) return;
  lastNtpAttempt = now;
  watchdogPause();  // A consulta DNS do servidor NTP pode levar até 15 s
  bool ok = timeClient.forceUpdate();
  watchdogResume();
  if (!ok) {
    Serial.println(F("Falha ao sincronizar NTP"));
  }
}

//********************************************************************************
//************************** ALERTAS *********************************************
//********************************************************************************

void checkAlerts() {
  // Temperatura média acima do limite (com histerese para não oscilar)
  if (!isnan(tempMean)) {
    if (!tempAlert && tempMean > tempMax) {
      tempAlert = true;
      publishAlert(F("TEMP_ALTA"), true, tempMean, tempMax, 0);
    } else if (tempAlert && tempMean < tempMax - TEMP_ALERT_HYSTERESIS) {
      tempAlert = false;
      publishAlert(F("TEMP_ALTA"), false, tempMean, tempMax, 0);
    }
  }

  // Sensor DHT sem responder por várias leituras seguidas
  for (uint8_t i = 0; i < NUM_DHT; i++) {
    if (!dhtFailAlert[i] && dhtFailCount[i] >= DHT_FAIL_ALERT_COUNT) {
      dhtFailAlert[i] = true;
      publishAlert(F("SENSOR_FALHA"), true, NAN, NAN, i + 1);
    } else if (dhtFailAlert[i] && dhtFailCount[i] == 0) {
      dhtFailAlert[i] = false;
      publishAlert(F("SENSOR_FALHA"), false, NAN, NAN, i + 1);
    }
  }

#if AC_ALERT_ENABLED
  // Ar-condicionado ligado fora do expediente
  if (!clockValid()) return;
  tmElements_t tm;
  currentTime(tm);
  bool weekend = tm.Wday == 1 || tm.Wday == 7;  // 1 = domingo, 7 = sábado
  bool offHours = (AC_ALERT_WEEKENDS && weekend) || tm.Hour < WORK_START_HOUR || tm.Hour >= WORK_END_HOUR;
  for (uint8_t i = 0; i < NUM_AC; i++) {
    bool alert = offHours && isAcOn(i);
    if (alert != acOffHoursAlert[i]) {
      acOffHoursAlert[i] = alert;
      publishAlert(F("AR_FORA_DO_HORARIO"), alert, acCurrent[i], AC_ON_THRESHOLD_A, i + 1);
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

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.createChar(1, degreeSymbol);

  loadSettings();

  // Sensores DHT
  for (uint8_t i = 0; i < NUM_DHT; i++) {
    dht[i] = new DHT(DHT_PINS[i], DHT_TYPE);
    dht[i]->begin();
  }

  // Sensores de corrente; as primeiras medições são descartadas até o filtro da EmonLib estabilizar
  for (uint8_t i = 0; i < NUM_AC; i++) {
    sct[i].current(AC_PINS[i], SCT_CALIBRATION);
    for (uint8_t k = 0; k < 3; k++) sct[i].calcIrms(SCT_SAMPLES);
  }
  lastEnergyUpdate = millis();

  setupEthernet();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  timeClient.begin();

  maintainClock(millis());
  readClimateSensors();
  readCurrentSensors();
  lastRead = millis();

  mqttConnect();
  lastMqttAttempt = mqttLastConnected = millis();

  lcd.clear();
  watchdogArm();
}

//********************************************************************************
//************************** LOOP ************************************************
//********************************************************************************

void loop() {
  wdt_reset();
  unsigned long now = millis();

  maintainNetwork(now);
  maintainClock(now);

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

  if (now - lastLcdPage >= LCD_PAGE_MS) {
    lastLcdPage = now;
    lcdPage = (lcdPage + 1) % LCD_PAGES;
    lcdRedraw = true;
  }
  if (lcdRedraw || now - lastLcdRefresh >= LCD_REFRESH_MS) {
    lastLcdRefresh = now;
    lcdRedraw = false;
    updateLcd();
  }
}

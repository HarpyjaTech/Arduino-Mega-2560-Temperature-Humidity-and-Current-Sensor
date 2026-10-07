//====================================================================================================
//                  Configuração do Monitor de Climatização e Energia (ESP32)
//                  Placa: ideaspark ESP32 (ESP32-WROOM-32, 30 pinos) com OLED 0,96" integrado.
//                  Ajuste aqui pinos, intervalos, limites e tópicos MQTT.
//                  Credenciais (Wi-Fi, broker, senhas) ficam em secrets.h.
//====================================================================================================
#pragma once

#include <Arduino.h>

//********************************************************************************
//************************** IDENTIFICAÇÃO ***************************************
//********************************************************************************

#define FIRMWARE_VERSION "4.0.0-esp32"
#define DEVICE_ID        "ufc_fisica_clima"          // Usado nos IDs do Home Assistant
#define DEVICE_NAME      "Monitor Climatizacao UFC"  // Nome exibido no Home Assistant
#define DEVICE_MODEL     "ideaspark ESP32 OLED"

//********************************************************************************
//************************** WI-FI ***********************************************
//********************************************************************************

#define WIFI_HOSTNAME          "monitor-clima-ufc"  // Nome na rede e no OTA
#define WIFI_CONNECT_TIMEOUT_MS 20000UL  // Espera no boot; depois o sistema segue sem rede
#define WIFI_RETRY_MS           30000UL  // Nova tentativa se o Wi-Fi cair

//********************************************************************************
//************************** NTP (DATA/HORA) *************************************
//********************************************************************************

#define NTP_SERVER_1 "a.st1.ntp.br"
#define NTP_SERVER_2 "pool.ntp.org"
#define TIMEZONE     "<-03>3"  // UTC-3 (Fortaleza, sem horário de verão), formato POSIX

//********************************************************************************
//************************** MQTT ************************************************
//********************************************************************************

#define MQTT_CLIENT_ID_PREFIX "ESP32Clima"  // O final do MAC é anexado para evitar colisões
#define MQTT_BASE_TOPIC       "sensores/ArduinoDHT11AC"  // Mantido para compatibilidade

// Tópicos publicados (mantidos compatíveis com a versão do Arduino Mega)
#define TOPIC_TEMP     MQTT_BASE_TOPIC "/Temp"
#define TOPIC_UMID     MQTT_BASE_TOPIC "/Umid"
#define TOPIC_AC       MQTT_BASE_TOPIC "/AC"
#define TOPIC_ENERGY   MQTT_BASE_TOPIC "/Energia"
#define TOPIC_INFO     MQTT_BASE_TOPIC "/Info"      // Sinal Wi-Fi, IP e tempo ligado
#define TOPIC_STATUS   MQTT_BASE_TOPIC "/status"    // "online"/"offline" (LWT, retido)
#define TOPIC_WARNINGS MQTT_BASE_TOPIC "/warnings"  // Alertas em JSON
#define TOPIC_TEMPMAX  MQTT_BASE_TOPIC "/TempMax"   // Limite de temperatura atual (retido)
#define TOPIC_TEMPMAX_SET MQTT_BASE_TOPIC "/TempMax/set"  // Comando para alterar o limite

// Publica as configurações de MQTT Discovery do Home Assistant ao conectar.
#define HA_DISCOVERY_ENABLED 1
#define HA_DISCOVERY_PREFIX  "homeassistant"

#define MQTT_RETRY_MIN_MS    5000UL    // Primeira espera entre tentativas de reconexão
#define MQTT_RETRY_MAX_MS    60000UL   // A espera dobra a cada falha até este limite
#define MQTT_SOCKET_TIMEOUT_S 5

// Se o MQTT ficar desconectado por este tempo, o ESP32 reinicia sozinho. Use 0 para desativar.
#define NETWORK_REBOOT_AFTER_MS (10UL * 60UL * 1000UL)

//********************************************************************************
//************************** SENSORES DHT ****************************************
//********************************************************************************

// DHT11 (±2 °C / ±5 %UR) ou DHT22 (±0,5 °C / ±2 %UR) - basta trocar aqui.
#define DHT_TYPE DHT11

// GPIOs bidirecionais livres da placa. Evitados: 0, 2, 5, 12 e 15 (pinos de boot -
// o pull-up do DHT no GPIO12 impede o ESP32 de iniciar), 6-11 (memória flash),
// 21/22 (I2C do OLED) e 34-39 (somente entrada, usados pelos SCT).
#define NUM_DHT 9
const uint8_t DHT_PINS[NUM_DHT] = {4, 13, 14, 16, 17, 18, 19, 23, 25};

// Leituras consecutivas com falha até um sensor ser reportado como defeituoso.
#define DHT_FAIL_ALERT_COUNT 5

//********************************************************************************
//************************** SENSORES DE CORRENTE (SCT-013) **********************
//********************************************************************************

// Somente pinos do ADC1: o ADC2 não funciona com o Wi-Fi ligado.
#define NUM_AC 3
const uint8_t AC_PINS[NUM_AC] = {36, 39, 34};  // VP, VN, D34

// Ampères por volt RMS no pino. SCT-013-000 (2000 espiras) com resistor de carga de
// 33 Ω -> 2000 / 33 ≈ 60,6. Ajuste comparando com um alicate amperímetro.
#define SCT_CALIBRATION 60.6

// Tempo de amostragem de cada sensor. 200 ms = 12 ciclos completos de 60 Hz.
#define SCT_SAMPLE_MS 200

// Ruído do ADC do ESP32 medido com o ar desligado (A). É descontado das leituras.
// Para medir: publique com o ar desligado e copie aqui o valor de "Ar1".
#define SCT_NOISE_FLOOR_A 0.0

// Corrente a partir da qual o ar-condicionado é considerado ligado.
#define AC_ON_THRESHOLD_A 1.0

// Estimativa de potência: P = Irms × tensão × fator de potência.
#define MAINS_VOLTAGE 220.0
#define POWER_FACTOR  0.90

//********************************************************************************
//************************** ALERTAS *********************************************
//********************************************************************************

// Limite padrão de temperatura média. Pode ser alterado via MQTT (TOPIC_TEMPMAX_SET)
// e fica salvo na memória flash.
#define TEMP_MAX_DEFAULT    27.0
#define TEMP_MAX_MIN        10.0  // Faixa aceita para comandos via MQTT
#define TEMP_MAX_MAX        40.0
#define TEMP_ALERT_HYSTERESIS 0.5

// Alerta de ar-condicionado ligado fora do expediente.
#define AC_ALERT_ENABLED    1
#define WORK_START_HOUR     7    // Início do expediente (inclusive)
#define WORK_END_HOUR       22   // Fim do expediente (exclusive)
#define AC_ALERT_WEEKENDS   1    // 1 = sábado e domingo contam como fora do expediente

//********************************************************************************
//************************** DISPLAYS ********************************************
//********************************************************************************

// I2C da placa (o OLED integrado está ligado nestes pinos)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// OLED 0,96" SSD1306 128x64 integrado na placa
#define OLED_ENABLED 1
#define OLED_ADDRESS 0x3C
#define OLED_WIDTH   128
#define OLED_HEIGHT  64

// LCD 20x4 I2C opcional, no mesmo barramento do OLED (veja docs/ligacoes.md:
// o módulo I2C do LCD é de 5 V e precisa de conversor de nível lógico).
#define LCD_ENABLED 0
#define LCD_ADDRESS 0x27
#define LCD_COLS    20
#define LCD_ROWS    4

//********************************************************************************
//************************** TEMPORIZAÇÃO E DIAGNÓSTICO **************************
//********************************************************************************

#define READ_INTERVAL_MS    2500UL  // Leitura dos sensores (DHT11 >= 1 s, DHT22 >= 2 s)
#define PUBLISH_INTERVAL_MS 5000UL  // Publicação MQTT
#define INFO_INTERVAL_MS    60000UL // Publicação do sinal Wi-Fi / tempo ligado
#define DISPLAY_REFRESH_MS  1000UL  // Atualização do conteúdo dos displays
#define DISPLAY_PAGE_MS     4000UL  // Troca de tela

#define SERIAL_BAUD   115200
#define LOG_PAYLOADS  1   // 1 = imprime os JSON publicados no Serial

#define WATCHDOG_TIMEOUT_S 30  // Reinicia o ESP32 se o loop travar por este tempo

// Atualização do firmware pelo Wi-Fi (Arduino IDE / PlatformIO). Senha em secrets.h.
#define OTA_ENABLED 1

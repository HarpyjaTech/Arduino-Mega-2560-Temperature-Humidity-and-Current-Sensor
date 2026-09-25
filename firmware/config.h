//====================================================================================================
//                  Configuração do Monitor de Climatização e Energia
//                  Ajuste aqui pinos, intervalos, limites e tópicos MQTT.
//                  Credenciais (broker, usuário, senha) ficam em secrets.h.
//====================================================================================================
#pragma once

#include <Arduino.h>

//********************************************************************************
//************************** IDENTIFICAÇÃO ***************************************
//********************************************************************************

#define FIRMWARE_VERSION "3.0.0"
#define DEVICE_ID        "ufc_fisica_clima"          // Usado nos IDs do Home Assistant
#define DEVICE_NAME      "Monitor Climatizacao UFC"  // Nome exibido no Home Assistant

//********************************************************************************
//************************** REDE ************************************************
//********************************************************************************

#define ETHERNET_CS_PIN 53  // Pino CS do módulo ENC28J60

// Se houver mais de um equipamento na mesma rede, altere o último byte.
const uint8_t MAC_ADDRESS[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

//********************************************************************************
//************************** NTP (DATA/HORA) *************************************
//********************************************************************************

#define NTP_SERVER          "a.st1.ntp.br"
#define NTP_OFFSET_SECONDS  (-3L * 3600L)  // UTC-3 (Fortaleza, sem horário de verão)
#define NTP_UPDATE_MS       60000UL

//********************************************************************************
//************************** MQTT ************************************************
//********************************************************************************

#define MQTT_CLIENT_ID_PREFIX "ArduinoClima"  // O final do MAC é anexado para evitar colisões
#define MQTT_BASE_TOPIC       "sensores/ArduinoDHT11AC"

// Tópicos publicados (mantidos compatíveis com a versão anterior)
#define TOPIC_TEMP     MQTT_BASE_TOPIC "/Temp"
#define TOPIC_UMID     MQTT_BASE_TOPIC "/Umid"
#define TOPIC_AC       MQTT_BASE_TOPIC "/AC"
// Tópicos novos
#define TOPIC_ENERGY   MQTT_BASE_TOPIC "/Energia"
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

// Se o MQTT ficar desconectado por este tempo, o Arduino reinicia sozinho
// (recupera travamentos do ENC28J60). Use 0 para desativar.
#define NETWORK_REBOOT_AFTER_MS (10UL * 60UL * 1000UL)

//********************************************************************************
//************************** SENSORES DHT ****************************************
//********************************************************************************

// DHT11 (±2 °C / ±5 %UR) ou DHT22 (±0,5 °C / ±2 %UR) - basta trocar aqui.
#define DHT_TYPE DHT11

#define NUM_DHT 9
const uint8_t DHT_PINS[NUM_DHT] = {A0, A1, A2, A3, A4, A5, A6, A7, A8};

// Leituras consecutivas com falha até um sensor ser reportado como defeituoso.
#define DHT_FAIL_ALERT_COUNT 5

//********************************************************************************
//************************** SENSORES DE CORRENTE (SCT-013) **********************
//********************************************************************************

#define NUM_AC 3
// Ar1 -> A13, Ar2 -> A14, Ar3 -> A15. A versão anterior tinha comentários indicando
// que A13 seria fisicamente o "ar 2" e A14 o "ar 1": confira a instalação.
const uint8_t AC_PINS[NUM_AC] = {A13, A14, A15};

// Calibração EmonLib = relação de espiras / resistor de carga.
// SCT-013-000 (100 A : 50 mA = 2000 espiras) com resistor de 33 Ω -> 2000 / 33 ≈ 60,6.
// Ajuste comparando com um alicate amperímetro.
#define SCT_CALIBRATION 60.0
#define SCT_SAMPLES     1480   // Amostras por medição (~170 ms por sensor)

// Corrente a partir da qual o ar-condicionado é considerado ligado.
// Um split consome tipicamente 3-10 A; abaixo de ~0,5 A é ruído do sensor.
#define AC_ON_THRESHOLD_A 1.0

// Estimativa de potência: P = Irms × tensão × fator de potência.
// Não há medição de tensão, então o valor é uma estimativa.
#define MAINS_VOLTAGE 220.0
#define POWER_FACTOR  0.90

//********************************************************************************
//************************** ALERTAS *********************************************
//********************************************************************************

// Limite padrão de temperatura média. Pode ser alterado via MQTT (TOPIC_TEMPMAX_SET)
// e fica salvo na EEPROM.
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
//************************** DISPLAY LCD *****************************************
//********************************************************************************

#define LCD_ADDRESS 0x27
#define LCD_COLS    20
#define LCD_ROWS    4

//********************************************************************************
//************************** TEMPORIZAÇÃO E DIAGNÓSTICO **************************
//********************************************************************************

#define READ_INTERVAL_MS    2500UL  // Leitura dos sensores (DHT11 >= 1 s, DHT22 >= 2 s)
#define PUBLISH_INTERVAL_MS 5000UL  // Publicação MQTT
#define LCD_REFRESH_MS      1000UL  // Atualização do conteúdo do display
#define LCD_PAGE_MS         4000UL  // Troca de tela

#define SERIAL_BAUD   115200
#define LOG_PAYLOADS  1   // 1 = imprime os JSON publicados no Serial

#define WATCHDOG_ENABLED 1  // Reinicia o Arduino se o programa travar por ~8 s

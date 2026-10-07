//====================================================================================================
//  Modelo de credenciais.
//  Copie este arquivo para "secrets.h" (na mesma pasta) e preencha com os dados reais.
//  O secrets.h está no .gitignore e nunca deve ser enviado ao repositório.
//====================================================================================================
#pragma once

// Wi-Fi (somente 2,4 GHz)
#define WIFI_SSID     "nome_da_rede"
#define WIFI_PASSWORD "senha_da_rede"

// Broker MQTT
#define MQTT_SERVER   "192.168.0.10"  // IP ou nome do broker MQTT
#define MQTT_PORT     1883
#define MQTT_USER     "mqtt_user"     // Deixe "" se o broker não exige autenticação
#define MQTT_PASSWORD "mqtt_password"

// Senha para atualização do firmware pelo Wi-Fi (OTA). Deixe "" para não exigir senha.
#define OTA_PASSWORD  "troque_esta_senha"

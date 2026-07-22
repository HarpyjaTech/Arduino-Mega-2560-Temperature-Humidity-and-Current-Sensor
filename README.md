
# Sistema de Monitoramento de Climatização e Energia - UFC

<a href="https://buymeacoffee.com/hapyjatech" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>

Este projeto consiste em um sistema de monitoramento ambiental e de consumo elétrico desenvolvido para o **Departamento de Física da Universidade Federal do Ceará (UFC)**. O software realiza a leitura de 9 sensores de temperatura e umidade (DHT11) e 3 sensores de corrente para ar-condicionado, exibindo os dados em um display LCD e transmitindo-os via protocolo MQTT.

## 🚀 Funcionalidades

* **Monitoramento Ambiental Robusto**: Leitura de 9 sensores DHT11 para uma cobertura ampla da área.
* **Cálculo de Média Inteligente**: O sistema calcula automaticamente a média de temperatura e umidade, filtrando sensores que apresentem erro de leitura (NaN).
* **Medição de Consumo AC**: Monitoramento de 3 unidades de ar-condicionado através de sensores de corrente (EmonLib).
* **Integração IoT (MQTT)**: Publicação dos dados em formato JSON em tópicos específicos para integração com brokers externos ou Home Assistant.
* **Interface LCD Dinâmica**: Exibição alternada de informações de rede (IP e Status MQTT), data/hora e valores lidos dos sensores.
* **Sincronização NTP**: Atualização automática de horário via servidor NTP (a.st1.ntp.br) com ajuste de fuso horário.

## 🛠️ Hardware Necessário

* **Microcontrolador**: Arduino Mega 2560 (compatível com Shield Ethernet).
* **Comunicação**: Módulo Ethernet W5100/ENC28J60 (Biblioteca UIPEthernet).
* **Sensores de Ambiente**: 9x DHT11.
* **Sensores de Corrente**: 3x Sensores (SCT-013 ou similar).
* **Display**: LCD 20x4 I2C (Endereço 0x27).

## 📌 Pinagem e Conexões

### Sensores DHT11 (Pinos Analógicos)
| Sensor | Pino | Sensor | Pino | Sensor | Pino |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Sensor 1 | A0 | Sensor 4 | A3 | Sensor 7 | A6 |
| Sensor 2 | A1 | Sensor 5 | A4 | Sensor 8 | A7 |
| Sensor 3 | A2 | Sensor 6 | A5 | Sensor 9 | A8 |

### Monitoramento de Corrente (AC)
* **Ar 1**: Pino A13.
* **Ar 2**: Pino A14.
* **Ar 3**: Pino A15.

### Comunicação e Display
* **Ethernet CS**: Pino 53.
* **Display I2C**: Pinos SDA/SCL padrão do Arduino.

## 📦 Bibliotecas Requeridas

As seguintes bibliotecas são necessárias para a compilação do projeto:

1.  `UIPEthernet` 
2.  `DHT` (Adafruit) 
3.  `PubSubClient`
4.  `ArduinoJson` 
5.  `EmonLib` 
6.  `LiquidCrystal_I2C` 
7.  `NTPClient` e `TimeLib`

   
## 🔧 Configuração e Instalação
1. Conexões de Pinos
   
* Sensores DHT11: Pinos analógicos de `A0` a `A8`.
* Sensores de Corrente: Pinos `A13`, `A14` e `A15`.
* Ethernet Shield: Pino CS configurado no `53`.

## 🔧 Configuração de Software

No arquivo principal, certifique-se de configurar as credenciais da sua rede e do Broker MQTT:

```cpp
// Configuração de Rede
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; // [cite: 2]

// Configuração MQTT
const char* mqtt_server ="SEU_BROKER_IP"; // [cite: 3]
const char* mqtt_user = "seu_usuario"; // [cite: 3]
const char* mqtt_password = "sua_senha"; // [cite: 4]
```
## 3. Tópicos MQTT

O sistema publica dados nos seguintes tópicos:

* `sensores/ArduinoDHT11AC/Temp`: Dados de temperatura.
* `sensores/ArduinoDHT11AC/Umid`: Dados de umidade.
* `sensores/ArduinoDHT11AC/AC`: Consumo dos ares-condicionados.

# 📊 Estrutura dos Dados (JSON)

Os dados são enviados no seguinte formato:

* Temperatura:
```json
{
  "Temp1": 25.5, ..., "Temp9": 26.0,
  "MeanValueTemp": 25.8
}
```
* Umidade:
```json
{
  "Hty1": 25.5, ..., "Hty9": 26.0,
  "MeanValueUmid": 25.8
}
```
* Ar-Condicionado (Corrente):
* 
```json
{
  "Ar1": 12.5,
  "Ar2": 0.0,
  "Ar3": 11.2
}
```

## 📝 Autor

* Sergio Levy Nobre dos Santos - Departamento de Física - UFC

## Imagens do projeto


![Image_20250118_130733_568](https://github.com/user-attachments/assets/2aacaf27-2c63-4971-8011-f1e249f647d8)


![Image_20250118_130733_665](https://github.com/user-attachments/assets/e4878627-8e44-4ed8-ac58-fbc1abc976ce)




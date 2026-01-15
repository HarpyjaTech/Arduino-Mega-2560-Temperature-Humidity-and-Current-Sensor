
# Sistema de Monitoramento de Climatização e Energia - UFC

[cite_start]Este projeto consiste em um sistema de monitoramento ambiental e de consumo elétrico desenvolvido para o **Departamento de Física da Universidade Federal do Ceará (UFC)**. [cite_start]O software utiliza um Arduino para realizar a leitura de múltiplos sensores de temperatura/umidade e sensores de corrente AC, transmitindo os dados via protocolo MQTT e exibindo-os em uma interface local via LCD[cite: 15, 30, 39].

## 🚀 Funcionalidades

* [cite_start]**Monitoramento Ambiental Robusto**: Leitura de 9 sensores DHT11 para uma cobertura ampla da área[cite: 8, 49, 54].
* [cite_start]**Cálculo de Média Inteligente**: O sistema calcula automaticamente a média de temperatura e umidade, filtrando sensores que apresentem erro de leitura (NaN)[cite: 58, 59, 60].
* [cite_start]**Medição de Consumo AC**: Monitoramento de 3 unidades de ar-condicionado através de sensores de corrente (EmonLib)[cite: 11, 29].
* [cite_start]**Integração IoT (MQTT)**: Publicação dos dados em formato JSON em tópicos específicos para integração com brokers externos ou Home Assistant[cite: 3, 15, 61, 62].
* [cite_start]**Interface LCD Dinâmica**: Exibição alternada de informações de rede (IP e Status MQTT), data/hora e valores lidos dos sensores[cite: 30, 36, 38].
* [cite_start]**Sincronização NTP**: Atualização automática de horário via servidor NTP (a.st1.ntp.br) com ajuste de fuso horário[cite: 22, 52].

## 🛠️ Hardware Necessário

* [cite_start]**Microcontrolador**: Arduino (compatível com Shield Ethernet)[cite: 2, 39].
* [cite_start]**Comunicação**: Módulo Ethernet W5100/ENC28J60 (Biblioteca UIPEthernet)[cite: 2].
* [cite_start]**Sensores de Ambiente**: 9x DHT11[cite: 7, 8].
* [cite_start]**Sensores de Corrente**: 3x Sensores (SCT-013 ou similar)[cite: 5, 11].
* [cite_start]**Display**: LCD 20x4 I2C (Endereço 0x27)[cite: 12, 13].

## 📌 Pinagem e Conexões

### Sensores DHT11 (Pinos Analógicos)
| Sensor | Pino | Sensor | Pino | Sensor | Pino |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Sensor 1 | [cite_start]A0 [cite: 5] | Sensor 4 | [cite_start]A3 [cite: 5] | Sensor 7 | [cite_start]A6 [cite: 5] |
| Sensor 2 | [cite_start]A1 [cite: 5] | Sensor 5 | [cite_start]A4 [cite: 5] | Sensor 8 | [cite_start]A7 [cite: 5] |
| Sensor 3 | [cite_start]A2 [cite: 5] | Sensor 6 | [cite_start]A5 [cite: 5] | Sensor 9 | [cite_start]A8 [cite: 5] |

### Monitoramento de Corrente (AC)
* [cite_start]**Ar 1**: Pino A13[cite: 5].
* [cite_start]**Ar 2**: Pino A14[cite: 6].
* [cite_start]**Ar 3**: Pino A15[cite: 6].

### Comunicação e Display
* [cite_start]**Ethernet CS**: Pino 53[cite: 39].
* [cite_start]**Display I2C**: Pinos SDA/SCL padrão do Arduino[cite: 13].

## 📦 Bibliotecas Requeridas

As seguintes bibliotecas são necessárias para a compilação do projeto:

1.  [cite_start]`UIPEthernet` [cite: 2]
2.  [cite_start]`DHT` (Adafruit) [cite: 2]
3.  [cite_start]`PubSubClient` [cite: 2]
4.  [cite_start]`ArduinoJson` [cite: 2, 14]
5.  [cite_start]`EmonLib` [cite: 2, 11]
6.  [cite_start]`LiquidCrystal_I2C` [cite: 2]
7.  [cite_start]`NTPClient` e `TimeLib` [cite: 2]

## 🔧 Configuração de Software

No arquivo principal, certifique-se de configurar as credenciais da sua rede e do Broker MQTT:

```cpp
// Configuração de Rede
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; [cite_start]// [cite: 2]

// Configuração MQTT
const char* mqtt_server ="SEU_BROKER_IP"; // [cite: 3]
const char* mqtt_user = "seu_usuario"; [cite_start]// [cite: 3]
const char* mqtt_password = "sua_senha"; [cite_start]// [cite: 4]



![Image_20250118_130733_568](https://github.com/user-attachments/assets/2aacaf27-2c63-4971-8011-f1e249f647d8)


![Image_20250118_130733_665](https://github.com/user-attachments/assets/e4878627-8e44-4ed8-ac58-fbc1abc976ce)

Comunicação feita via módulo enc28j60

![ENC28J60-Pinout](https://github.com/user-attachments/assets/4565f1c4-005e-4c5c-a028-7a823bafc77d)

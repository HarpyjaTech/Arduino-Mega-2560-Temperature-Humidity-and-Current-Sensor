# Sistema de Monitoramento de Climatização e Energia - UFC

[![Compilar firmware](https://github.com/HarpyjaTech/Arduino-Mega-2560-Temperature-Humidity-and-Current-Sensor/actions/workflows/build.yml/badge.svg)](https://github.com/HarpyjaTech/Arduino-Mega-2560-Temperature-Humidity-and-Current-Sensor/actions/workflows/build.yml)

<a href="https://buymeacoffee.com/hapyjatech" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>

Sistema de monitoramento ambiental e de consumo elétrico desenvolvido para o **Departamento de Física da Universidade Federal do Ceará (UFC)**. Um **ESP32 com display OLED integrado** lê 9 sensores de temperatura e umidade (DHT11) e 3 sensores de corrente de ar-condicionado. Os dados aparecem no display e são enviados via **Wi-Fi** e MQTT, com integração automática ao Home Assistant.

> A versão para **Arduino Mega 2560 + ENC28J60** (rede cabeada) está no branch [`claude/project-improvements-hp5sxg`](https://github.com/HarpyjaTech/Arduino-Mega-2560-Temperature-Humidity-and-Current-Sensor/tree/claude/project-improvements-hp5sxg).

## 🚀 Funcionalidades

* **Monitoramento ambiental**: 9 sensores DHT11 (ou DHT22) com média de temperatura e umidade. Sensores com falha ficam de fora da média.
* **Ar-condicionado**: corrente RMS (ADC calibrado do ESP32), estado ligado/desligado, potência estimada e energia acumulada (kWh) de 3 aparelhos.
* **MQTT + Home Assistant**: dados em JSON e **MQTT Discovery**, então as entidades aparecem sozinhas no Home Assistant.
* **Alertas**: temperatura acima do limite, ar ligado fora do expediente e sensor DHT com defeito.
* **Limite ajustável remotamente**: o limite de temperatura pode ser alterado via MQTT/Home Assistant e fica salvo na memória flash.
* **Display OLED integrado**: 3 telas alternadas (rede/data/hora, clima e ar-condicionado, energia/alertas). Um LCD 20x4 I2C pode ser ligado em paralelo, se quiser.
* **Wi-Fi com atualização remota (OTA)**: o firmware pode ser regravado pela rede, sem cabo USB.
* **Operação contínua**: loop sem bloqueios, reconexão de Wi-Fi e MQTT com espera progressiva, watchdog, reinício automático após queda prolongada da rede e aviso de status online/offline (LWT).
* **Horário via NTP** (`a.st1.ntp.br`, UTC-3), sincronizado em segundo plano pelo próprio ESP32.

## 🛠️ Hardware

| Item | Observação |
| :--- | :--- |
| **ideaspark ESP32 OLED** | ESP32-WROOM-32, 30 pinos, USB CH340, OLED 0,96" SSD1306 128x64 integrado |
| 9x DHT11 | Alimentados em 3,3 V. DHT22 é recomendado para maior precisão |
| 3x SCT-013-000 | Com resistor de carga de 33 Ω e divisor de polarização em **3,3 V** |
| LCD 20x4 I2C (opcional) | Requer conversor de nível lógico 5 V ↔ 3,3 V |

O firmware também funciona em qualquer ESP32 DevKit V1 (ESP32-WROOM-32): sem o OLED integrado, basta ligar um OLED SSD1306 I2C nos mesmos pinos.

> ⚠️ O ESP32 trabalha com **3,3 V** e seus pinos **não toleram 5 V**. Sensores e módulos que forem alimentados em 5 V precisam de atenção. Veja o guia de ligações.

Esquemas e detalhes de montagem: **[docs/ligacoes.md](docs/ligacoes.md)**.

### 📌 Pinagem

| Função | GPIOs |
| :--- | :--- |
| DHT11 1 a 9 | 4, 13, 14, 16, 17, 18, 19, 23, 25 |
| SCT Ar 1, Ar 2, Ar 3 | 36 (VP), 39 (VN), 34 (somente ADC1, que funciona com o Wi-Fi ligado) |
| OLED integrado / LCD opcional (I2C) | SDA 21, SCL 22 |

## 📁 Estrutura

```
firmware/
  firmware.ino         Programa principal
  config.h             Pinos, intervalos, limites, tópicos
  secrets.example.h    Modelo de credenciais (copie para secrets.h)
docs/
  ligacoes.md                   Ligações elétricas
  home-assistant-dashboard.yaml Exemplo de painel do Home Assistant
platformio.ini         Build via PlatformIO (opcional)
```

## 🔧 Instalação

### 1. Credenciais

Copie `firmware/secrets.example.h` para `firmware/secrets.h` e preencha:

```cpp
#define WIFI_SSID     "nome_da_rede"  // Somente redes de 2,4 GHz
#define WIFI_PASSWORD "senha_da_rede"
#define MQTT_SERVER   "192.168.0.10"  // IP ou nome do broker
#define MQTT_PORT     1883
#define MQTT_USER     "usuario"       // "" se o broker não exige autenticação
#define MQTT_PASSWORD "senha"
#define OTA_PASSWORD  "outra_senha"   // Para atualizar o firmware pelo Wi-Fi
```

O `secrets.h` está no `.gitignore` e não é enviado ao GitHub. Sem ele, o firmware compila com as credenciais de exemplo e mostra um aviso.

### 2a. Arduino IDE

1. Em *Arquivo → Preferências → URLs adicionais*, adicione `https://espressif.github.io/arduino-esp32/package_esp32_index.json` e instale **esp32 by Espressif Systems** (versão testada: 3.3.2) no Gerenciador de Placas.
2. Abra `firmware/firmware.ino` (a pasta precisa ter o mesmo nome do sketch).
3. Instale pelo Gerenciador de Bibliotecas (versões testadas):

   | Biblioteca | Versão |
   | :--- | :--- |
   | DHT sensor library (Adafruit) | 1.4.6 |
   | Adafruit Unified Sensor | 1.1.14 |
   | PubSubClient | 2.8 |
   | ArduinoJson | 7.4.2 |
   | Adafruit SSD1306 | 2.5.17 |
   | Adafruit GFX Library | 1.12.6 |
   | Adafruit BusIO | 1.17.4 |
   | LiquidCrystal I2C (só com `LCD_ENABLED 1`) | 1.1.2 |

4. Selecione a placa **DOIT ESP32 DEVKIT V1** e a porta USB. Se o computador não reconhecer a placa, instale o driver do **CH340**.
5. Grave. Se o upload não começar, segure o botão **BOOT** até aparecer "Connecting...".
6. Monitor serial em **115200 baud**.

Depois da primeira gravação, o firmware pode ser atualizado pelo Wi-Fi: a placa aparece como porta de rede **monitor-clima-ufc** na Arduino IDE (senha `OTA_PASSWORD`).

### 2b. PlatformIO

```bash
pio run -t upload          # compila e grava via USB (plataforma e bibliotecas baixadas automaticamente)
pio run -e ota -t upload   # grava pelo Wi-Fi (ajuste a senha em platformio.ini)
pio device monitor         # monitor serial a 115200
```

## ⚙️ Configuração (`firmware/config.h`)

| Parâmetro | Padrão | Descrição |
| :--- | :--- | :--- |
| `DHT_TYPE` | `DHT11` | Troque para `DHT22` se substituir os sensores |
| `SCT_CALIBRATION` | `60.6` | Ampères por volt RMS: `2000 / resistor de carga` para SCT-013-000 |
| `SCT_NOISE_FLOOR_A` | `0.0` | Ruído do ADC com o ar desligado, descontado das leituras |
| `AC_ON_THRESHOLD_A` | `1.0` | Corrente mínima para considerar o ar ligado |
| `MAINS_VOLTAGE` / `POWER_FACTOR` | `220` / `0.90` | Usados na estimativa de potência |
| `TEMP_MAX_DEFAULT` | `27.0` | Limite inicial do alerta de temperatura |
| `WORK_START_HOUR` / `WORK_END_HOUR` | `7` / `22` | Expediente (alerta de ar ligado fora do horário) |
| `AC_ALERT_WEEKENDS` | `1` | Sábado e domingo contam como fora do expediente |
| `READ_INTERVAL_MS` / `PUBLISH_INTERVAL_MS` | `2500` / `5000` | Intervalos de leitura e publicação |
| `NETWORK_REBOOT_AFTER_MS` | 10 min | Reinicia a placa se o MQTT ficar fora por esse tempo (`0` desativa) |
| `HA_DISCOVERY_ENABLED` | `1` | Publica o MQTT Discovery do Home Assistant |
| `WATCHDOG_TIMEOUT_S` | `30` | Reinicia o ESP32 se o loop travar |
| `OLED_ENABLED` / `LCD_ENABLED` | `1` / `0` | Displays em uso |
| `OTA_ENABLED` | `1` | Atualização do firmware pelo Wi-Fi |
| `WIFI_HOSTNAME` | `monitor-clima-ufc` | Nome da placa na rede |

### Calibração dos sensores de corrente

1. Ligue um ar-condicionado e meça a corrente com um alicate amperímetro no mesmo condutor.
2. Compare com o valor publicado em `sensores/ArduinoDHT11AC/AC`.
3. Ajuste: `SCT_CALIBRATION_novo = SCT_CALIBRATION_atual × (corrente_alicate / corrente_lida)`.

O ADC do ESP32 tem mais ruído que o do Arduino. Com os ares **desligados**, anote o valor de `Ar1`/`Ar2`/`Ar3` e coloque-o em `SCT_NOISE_FLOOR_A`. Um capacitor de 100 nF entre o pino do ADC e o GND também ajuda.

A potência é uma **estimativa** (`corrente × tensão × fator de potência`), porque não há medição de tensão. A energia acumulada volta a zero quando a placa reinicia. No Home Assistant isso é tratado automaticamente (`total_increasing`).

## 📡 MQTT

### Tópicos

| Tópico | Direção | Conteúdo |
| :--- | :--- | :--- |
| `sensores/ArduinoDHT11AC/Temp` | publica | Temperaturas (JSON) |
| `sensores/ArduinoDHT11AC/Umid` | publica | Umidades (JSON) |
| `sensores/ArduinoDHT11AC/AC` | publica | Corrente dos ares (JSON) |
| `sensores/ArduinoDHT11AC/Energia` | publica | Potência, energia e estado dos ares (JSON) |
| `sensores/ArduinoDHT11AC/Info` | publica (a cada 60 s) | Sinal Wi-Fi (dBm), IP e tempo ligado (JSON) |
| `sensores/ArduinoDHT11AC/status` | publica (retido) | `online` / `offline` (LWT) |
| `sensores/ArduinoDHT11AC/warnings` | publica | Alertas e aviso de reinício (JSON) |
| `sensores/ArduinoDHT11AC/TempMax` | publica (retido) | Limite de temperatura em vigor |
| `sensores/ArduinoDHT11AC/TempMax/set` | **assina** | Novo limite, ex.: `26.5` (aceito entre 10 e 40) |

### 📊 Formato dos dados

Sensores com falha de leitura aparecem como `null`. A média também é `null` se nenhum sensor responder.

`Temp`:
```json
{"Temp1": 25.0, "...": 0, "Temp9": 26.0, "MeanValueTemp": 25.8, "SensoresOK": 9, "AlertaTemp": "OFF"}
```

`Umid`:
```json
{"Hty1": 60.0, "...": 0, "Hty9": 58.0, "MeanValueHumidity": 59.1}
```

`AC` (corrente em A):
```json
{"Ar1": 5.42, "Ar2": 0.08, "Ar3": 4.97}
```

`Energia` (potência em W, energia em kWh desde o último reinício):
```json
{"Pot1": 1073, "kWh1": 2.315, "Estado1": "ON", "Pot2": 0, "kWh2": 0.0, "Estado2": "OFF", "Pot3": 984, "kWh3": 1.902, "Estado3": "ON", "PotTotal": 2057, "kWhTotal": 4.217}
```

`Info`:
```json
{"RSSI": -61, "IP": "192.168.0.42", "Uptime": 86400}
```

`warnings`:
```json
{"tipo": "TEMP_ALTA", "ativo": true, "valor": 27.6, "limite": 27.0}
{"tipo": "AR_FORA_DO_HORARIO", "ativo": true, "valor": 5.1, "limite": 1.0, "sensor": 2}
{"tipo": "SENSOR_FALHA", "ativo": true, "sensor": 4}
{"tipo": "BOOT", "versao": "4.0.0-esp32", "causa": "watchdog"}
```

Cada alerta é publicado ao **começar** (`"ativo": true`) e ao **terminar** (`"ativo": false`), não a cada leitura.

## 🏠 Home Assistant

Com `HA_DISCOVERY_ENABLED`, o dispositivo **Monitor Climatizacao UFC** aparece em *Configurações → Dispositivos e serviços → MQTT* com todas as entidades: temperaturas, umidades, médias, corrente/potência/energia e estado de cada ar, alerta de temperatura, o limite de temperatura (editável) e, como diagnóstico, o sinal Wi-Fi e o tempo ligado.

- As entidades de energia podem ser adicionadas ao **Painel de Energia**.
- Um painel pronto está em [docs/home-assistant-dashboard.yaml](docs/home-assistant-dashboard.yaml).
- Exemplo de automação para notificar alertas:

```yaml
automation:
  - alias: "Alerta do monitor de climatização"
    trigger:
      - platform: mqtt
        topic: sensores/ArduinoDHT11AC/warnings
    condition:
      - condition: template
        value_template: "{{ trigger.payload_json.ativo | default(false) }}"
    action:
      - service: notify.notify
        data:
          title: "Climatização - {{ trigger.payload_json.tipo }}"
          message: "{{ trigger.payload }}"
```

## 🖥️ Display

O OLED mostra uma barra de título com o nome da tela e a hora, e 4 linhas de conteúdo. O LCD opcional mostra as mesmas 4 linhas.

| Tela | Conteúdo |
| :--- | :--- |
| 1 | IP, hora, data, status do MQTT e sinal do Wi-Fi |
| 2 | Temperatura e umidade médias, estado dos 3 ares |
| 3 | Potência total, energia acumulada, sensores funcionando e alerta ativo |

## 🛡️ Robustez

* **Watchdog**: se o loop travar por 30 s, o ESP32 reinicia. A causa do reinício é publicada em `warnings` (`BOOT`).
* **Wi-Fi**: se a rede cair, as leituras e o display continuam funcionando, e a placa tenta reconectar a cada 30 s. No boot, ela espera até 20 s pelo Wi-Fi e depois segue sem rede.
* **Reconexão MQTT sem bloqueio**: as tentativas começam a cada 5 s e dobram até 60 s.
* **Reinício por falta de rede**: se o MQTT ficar desconectado por 10 min, a placa reinicia.

## 🔄 Mudanças em relação à versão Arduino Mega

* **Placa**: Arduino Mega 2560 + ENC28J60 → ESP32 (ideaspark com OLED). A rede passa a ser Wi-Fi.
* **Pinos**: todos mudaram (veja a pinagem). Os sensores agora trabalham em 3,3 V.
* **Display**: o OLED integrado substitui o LCD 20x4, que continua disponível como opção (`LCD_ENABLED`).
* **Corrente**: medida com o ADC calibrado do ESP32 em vez da EmonLib (que assume o ADC de 10 bits do Arduino). `SCT_CALIBRATION` mantém o mesmo significado.
* **Bibliotecas removidas**: UIPEthernet, EmonLib, NTPClient e Time. O ESP32 já tem Wi-Fi e NTP nativos.
* **Novidades**: atualização do firmware pelo Wi-Fi (OTA) e tópico `Info` com sinal do Wi-Fi e tempo ligado.
* **Mantidos**: tópicos MQTT, chaves JSON, entidades do Home Assistant (mesmos IDs), alertas e o limite de temperatura ajustável.

## 💡 Melhorias de hardware sugeridas

* **DHT22** (±0,5 °C) ou **SHT31** (±0,3 °C, I2C) no lugar do DHT11 (±2 °C, resolução de 1 °C). O DHT22 exige apenas trocar `DHT_TYPE`.
* Um **transformador de tensão** (ex.: ZMPT101B) em um pino livre do ADC1 (GPIO 32, 33 ou 35) permitiria medir a potência real em vez de estimá-la.

## 📝 Autor

* Sergio Levy Nobre dos Santos - Departamento de Física - UFC

## Imagens do projeto

![Image_20250118_130733_568](https://github.com/user-attachments/assets/2aacaf27-2c63-4971-8011-f1e249f647d8)

![Image_20250118_130733_665](https://github.com/user-attachments/assets/e4878627-8e44-4ed8-ac58-fbc1abc976ce)

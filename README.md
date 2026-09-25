# Sistema de Monitoramento de Climatização e Energia - UFC

[![Compilar firmware](https://github.com/HarpyjaTech/Arduino-Mega-2560-Temperature-Humidity-and-Current-Sensor/actions/workflows/build.yml/badge.svg)](https://github.com/HarpyjaTech/Arduino-Mega-2560-Temperature-Humidity-and-Current-Sensor/actions/workflows/build.yml)

<a href="https://buymeacoffee.com/hapyjatech" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>

Sistema de monitoramento ambiental e de consumo elétrico desenvolvido para o **Departamento de Física da Universidade Federal do Ceará (UFC)**. Um Arduino Mega 2560 lê 9 sensores de temperatura e umidade (DHT11) e 3 sensores de corrente de ar-condicionado. Os dados aparecem em um display LCD e são enviados via MQTT, com integração automática ao Home Assistant.

## 🚀 Funcionalidades

* **Monitoramento ambiental**: 9 sensores DHT11 (ou DHT22) com média de temperatura e umidade. Sensores com falha ficam de fora da média.
* **Ar-condicionado**: corrente (EmonLib), estado ligado/desligado, potência estimada e energia acumulada (kWh) de 3 aparelhos.
* **MQTT + Home Assistant**: dados em JSON e **MQTT Discovery**, então as entidades aparecem sozinhas no Home Assistant.
* **Alertas**: temperatura acima do limite, ar ligado fora do expediente e sensor DHT com defeito.
* **Limite ajustável remotamente**: o limite de temperatura pode ser alterado via MQTT/Home Assistant e fica salvo na EEPROM.
* **Display LCD 20x4**: 3 telas alternadas (rede/data/hora, clima e ar-condicionado, energia/alertas), atualizadas sem piscar.
* **Operação contínua**: loop sem bloqueios, reconexão MQTT com espera progressiva, watchdog, reinício automático após queda prolongada da rede e aviso de status online/offline (LWT).
* **Horário via NTP** (`a.st1.ntp.br`, UTC-3).

## 🛠️ Hardware

| Item | Observação |
| :--- | :--- |
| Arduino Mega 2560 | |
| Módulo Ethernet **ENC28J60** | A biblioteca UIPEthernet **não** suporta W5100/W5500 |
| 9x DHT11 | DHT22 é recomendado para maior precisão (veja abaixo) |
| 3x SCT-013-000 | Com resistor de carga de 33 Ω e divisor de polarização |
| LCD 20x4 com módulo I2C | Endereço 0x27 |

Esquemas e detalhes de montagem: **[docs/ligacoes.md](docs/ligacoes.md)**.

### 📌 Pinagem

| Função | Pinos |
| :--- | :--- |
| DHT11 1 a 9 | A0, A1, A2, A3, A4, A5, A6, A7, A8 |
| SCT Ar 1, Ar 2, Ar 3 | A13, A14, A15 |
| ENC28J60 (SPI) | MISO 50, MOSI 51, SCK 52, CS 53 |
| LCD I2C | SDA 20, SCL 21 |

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
#define MQTT_SERVER   "192.168.0.10"  // IP ou nome do broker
#define MQTT_PORT     1883
#define MQTT_USER     "usuario"       // "" se o broker não exige autenticação
#define MQTT_PASSWORD "senha"
```

O `secrets.h` está no `.gitignore` e não é enviado ao GitHub. Sem ele, o firmware compila com as credenciais de exemplo e mostra um aviso.

### 2a. Arduino IDE

1. Abra `firmware/firmware.ino` (a pasta precisa ter o mesmo nome do sketch).
2. Instale pelo Gerenciador de Bibliotecas (versões testadas):

   | Biblioteca | Versão |
   | :--- | :--- |
   | UIPEthernet | 2.0.12 |
   | DHT sensor library (Adafruit) | 1.4.6 |
   | Adafruit Unified Sensor | 1.1.14 |
   | PubSubClient | 2.8 |
   | ArduinoJson | 7.4.2 |
   | EmonLib | 1.1.0 |
   | LiquidCrystal I2C (Frank de Brabander) | 1.1.2 |
   | NTPClient | 3.2.1 |
   | Time (Michael Margolis / Paul Stoffregen) | 1.6.1 |

   > Não é necessário instalar a biblioteca `Ethernet`: a UIPEthernet já fornece `EthernetClient` e `EthernetUDP`.

3. Selecione a placa **Arduino Mega or Mega 2560** e grave.
4. Monitor serial em **115200 baud**.

### 2b. PlatformIO

```bash
pio run -t upload     # compila e grava (bibliotecas baixadas automaticamente)
pio device monitor    # monitor serial a 115200
```

## ⚙️ Configuração (`firmware/config.h`)

| Parâmetro | Padrão | Descrição |
| :--- | :--- | :--- |
| `DHT_TYPE` | `DHT11` | Troque para `DHT22` se substituir os sensores |
| `SCT_CALIBRATION` | `60.0` | `2000 / resistor de carga` para SCT-013-000 |
| `AC_ON_THRESHOLD_A` | `1.0` | Corrente mínima para considerar o ar ligado |
| `MAINS_VOLTAGE` / `POWER_FACTOR` | `220` / `0.90` | Usados na estimativa de potência |
| `TEMP_MAX_DEFAULT` | `27.0` | Limite inicial do alerta de temperatura |
| `WORK_START_HOUR` / `WORK_END_HOUR` | `7` / `22` | Expediente (alerta de ar ligado fora do horário) |
| `AC_ALERT_WEEKENDS` | `1` | Sábado e domingo contam como fora do expediente |
| `READ_INTERVAL_MS` / `PUBLISH_INTERVAL_MS` | `2500` / `5000` | Intervalos de leitura e publicação |
| `NETWORK_REBOOT_AFTER_MS` | 10 min | Reinicia a placa se o MQTT ficar fora por esse tempo (`0` desativa) |
| `HA_DISCOVERY_ENABLED` | `1` | Publica o MQTT Discovery do Home Assistant |
| `WATCHDOG_ENABLED` | `1` | Watchdog de 8 s |
| `MAC_ADDRESS` | `DE:AD:BE:EF:FE:ED` | Altere se houver mais de um equipamento na rede |

### Calibração dos sensores de corrente

1. Ligue um ar-condicionado e meça a corrente com um alicate amperímetro no mesmo condutor.
2. Compare com o valor publicado em `sensores/ArduinoDHT11AC/AC`.
3. Ajuste: `SCT_CALIBRATION_novo = SCT_CALIBRATION_atual × (corrente_alicate / corrente_lida)`.

A potência é uma **estimativa** (`corrente × tensão × fator de potência`), porque não há medição de tensão. A energia acumulada volta a zero quando a placa reinicia. No Home Assistant isso é tratado automaticamente (`total_increasing`).

## 📡 MQTT

### Tópicos

| Tópico | Direção | Conteúdo |
| :--- | :--- | :--- |
| `sensores/ArduinoDHT11AC/Temp` | publica | Temperaturas (JSON) |
| `sensores/ArduinoDHT11AC/Umid` | publica | Umidades (JSON) |
| `sensores/ArduinoDHT11AC/AC` | publica | Corrente dos ares (JSON) |
| `sensores/ArduinoDHT11AC/Energia` | publica | Potência, energia e estado dos ares (JSON) |
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

`warnings`:
```json
{"tipo": "TEMP_ALTA", "ativo": true, "valor": 27.6, "limite": 27.0}
{"tipo": "AR_FORA_DO_HORARIO", "ativo": true, "valor": 5.1, "limite": 1.0, "sensor": 2}
{"tipo": "SENSOR_FALHA", "ativo": true, "sensor": 4}
{"tipo": "BOOT", "versao": "3.0.0", "causa": "watchdog"}
```

Cada alerta é publicado ao **começar** (`"ativo": true`) e ao **terminar** (`"ativo": false`), não a cada leitura.

## 🏠 Home Assistant

Com `HA_DISCOVERY_ENABLED`, o dispositivo **Monitor Climatizacao UFC** aparece em *Configurações → Dispositivos e serviços → MQTT* com todas as entidades: temperaturas, umidades, médias, corrente/potência/energia e estado de cada ar, alerta de temperatura e o limite de temperatura (editável).

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

| Tela | Conteúdo |
| :--- | :--- |
| 1 | IP, hora, data e status do MQTT |
| 2 | Temperatura e umidade médias, estado dos 3 ares |
| 3 | Potência total, energia acumulada, sensores funcionando e alerta ativo |

## 🛡️ Robustez

* **Watchdog**: se o programa travar por ~8 s, a placa reinicia. A causa do reinício é publicada em `warnings` (`BOOT`).
  > Placas Mega (principalmente clones) com bootloader muito antigo podem ficar presas em reinícios após um reset do watchdog. Se isso acontecer, regrave o bootloader atual pela Arduino IDE ou use `WATCHDOG_ENABLED 0`.
* **Reconexão MQTT sem bloqueio**: as leituras e o display continuam funcionando com o broker fora do ar. As tentativas começam a cada 5 s e dobram até 60 s.
* **Reinício por falta de rede**: o ENC28J60 às vezes trava. Se o MQTT ficar desconectado por 10 min, a placa reinicia.
* **DHCP**: a concessão é renovada automaticamente (`Ethernet.maintain()`).

## 🔄 Mudanças em relação à versão anterior

* O sketch foi movido para `firmware/firmware.ino` e as credenciais para `firmware/secrets.h`.
* Monitor serial agora em **115200 baud** (antes 9600, o que deixava o loop lento).
* Dados publicados a cada 5 s (configurável).
* Tópicos e chaves JSON antigos foram mantidos. `Temp` ganhou `SensoresOK` e `AlertaTemp`, e há novos tópicos (`Energia`, `status`, `warnings`, `TempMax`).
* A média sem nenhum sensor válido agora é `null` em vez de `0`.
* A antiga mensagem de discovery em `sensores/dht11/config` e `sensores/ArCondicionado/config`, que o Home Assistant não reconhecia, foi substituída pelo MQTT Discovery padrão (`homeassistant/...`).
* O limite de ar ligado passou de 30 A para 1 A. Com 30 A, o display mostrava sempre "OFF".

## 💡 Melhorias de hardware sugeridas

* **DHT22** (±0,5 °C) ou **SHT31** (±0,3 °C, I2C) no lugar do DHT11 (±2 °C, resolução de 1 °C). O DHT22 exige apenas trocar `DHT_TYPE`.
* Um **transformador de tensão** (ex.: ZMPT101B) permitiria medir potência real com a EmonLib (`calcVI`) em vez de estimá-la.

## 📝 Autor

* Sergio Levy Nobre dos Santos - Departamento de Física - UFC

## Imagens do projeto

![Image_20250118_130733_568](https://github.com/user-attachments/assets/2aacaf27-2c63-4971-8011-f1e249f647d8)

![Image_20250118_130733_665](https://github.com/user-attachments/assets/e4878627-8e44-4ed8-ac58-fbc1abc976ce)

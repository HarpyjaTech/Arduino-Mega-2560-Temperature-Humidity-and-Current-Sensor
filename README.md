
Monitoramento de Sensores DHT11 e Corrente AC com Arduino

Este projeto consiste em um sistema de monitoramento ambiental e de consumo elétrico desenvolvido para o Departamento de Física da Universidade Federal do Ceará (UFC). O software realiza a leitura de 9 sensores de temperatura e umidade (DHT11) e 3 sensores de corrente para ar-condicionado, exibindo os dados em um display LCD e transmitindo-os via protocolo MQTT para integração com sistemas de automação como o Home Assistant.
🚀 Funcionalidades

    Monitoramento Multissensorial: Leitura simultânea de 9 sensores DHT11 .

    Cálculo de Médias: Processamento em tempo real da média de temperatura e umidade, ignorando leituras falhas (NaN) .

    Medição de Corrente: Monitoramento de 3 unidades de ar-condicionado através de sensores SCT-013 (EmonLib).

    Interface Local: Exibição de status de rede, IP, data/hora e valores dos sensores em um Display LCD 20x4 I2C .

    Conectividade IoT: Publicação de dados em formato JSON via MQTT .

    Sincronização de Tempo: Atualização de data e hora via servidor NTP.

🛠️ Hardware Necessário

    Microcontrolador: Arduino (compatível com Shield Ethernet).

    Módulo de Rede: Shield Ethernet (UIPEthernet).

    Sensores de Ambiente: 9x DHT11.

    Sensores de Corrente: 3x Sensores de corrente (ex: SCT-013) conectados aos pinos A13, A14 e A15.

    Display: LCD 20x4 com módulo I2C (Endereço 0x27).

📦 Bibliotecas Utilizadas

As seguintes bibliotecas devem ser instaladas no seu Arduino IDE:

    UIPEthernet.h: Controle do módulo Ethernet.

    DHT.h: Leitura dos sensores de temperatura e umidade.

    NTPClient.h: Sincronização de horário via rede.

    PubSubClient.h: Cliente MQTT para publicação de dados.

    ArduinoJson.h: Manipulação e criação de objetos JSON.

    EmonLib.h: Cálculos de energia e corrente AC.

    LiquidCrystal_I2C.h: Controle do display via protocolo I2C.

🔧 Configuração e Instalação
1. Conexões de Pinos

    Sensores DHT11: Pinos analógicos de A0 a A8.

    Sensores de Corrente: Pinos A13, A14 e A15.

    Ethernet Shield: Pino CS configurado no 53.

2. Configuração de Rede e MQTT

No código, localize e altere as variáveis de rede para o seu ambiente:
C++

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; // Endereço MAC [cite: 3]
const char* mqtt_server ="BOKER MQTT HOST";       // Endereço do seu Broker [cite: 4]
const char* mqtt_user = "mqtt_user";              // Usuário MQTT [cite: 4]
const char* mqtt_password = "mqtt_user";          // Senha MQTT [cite: 4]

3. Tópicos MQTT

O sistema publica dados nos seguintes tópicos:

    sensores/ArduinoDHT11AC/Temp: Dados de temperatura.

    sensores/ArduinoDHT11AC/Umid: Dados de umidade.

    sensores/ArduinoDHT11AC/AC: Consumo dos ares-condicionados.

📊 Estrutura dos Dados (JSON)

Os dados são enviados no seguinte formato:

Temperatura:
JSON

{
  "Temp1": 25.5, ..., "Temp9": 26.0,
  "MeanValueTemp": 25.8
}

Ar-Condicionado (Corrente):
JSON

{
  "Ar1": 12.5,
  "Ar2": 0.0,
  "Ar3": 11.2
}

📝 Autor

    Sergio Levy Nobre dos Santos - Departamento de Física - UFC



![Image_20250118_130733_568](https://github.com/user-attachments/assets/2aacaf27-2c63-4971-8011-f1e249f647d8)


![Image_20250118_130733_665](https://github.com/user-attachments/assets/e4878627-8e44-4ed8-ac58-fbc1abc976ce)

Comunicação feita via módulo enc28j60

![ENC28J60-Pinout](https://github.com/user-attachments/assets/4565f1c4-005e-4c5c-a028-7a823bafc77d)

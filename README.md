
Monitoramento de Sensores DHT11 e Corrente AC com Arduino

Este projeto consiste em um sistema robusto de monitoramento ambiental e de consumo elétrico desenvolvido para o Departamento de Física da Universidade Federal do Ceará (UFC). O software realiza a leitura de múltiplos sensores de temperatura/umidade e corrente, integrando os dados localmente via Display LCD e remotamente via protocolo MQTT.
🚀 Funcionalidades

    Monitoramento Ambiental: Leitura de 9 sensores DHT11 distribuídos pelo ambiente.

    Gestão de Energia: Monitoramento de corrente de 3 unidades de ar-condicionado usando sensores SCT.

    Cálculo de Médias: Algoritmo para calcular a média de temperatura e umidade, filtrando automaticamente leituras inválidas (NaN).

    Interface I2C: Exibição dinâmica de dados em um Display LCD 20x4, alternando entre telas de status de rede e dados dos sensores.

    Integração IoT: Envio de dados em formato JSON para um Broker MQTT.

    Sincronização Temporal: Uso de NTP (Network Time Protocol) para manter data e hora atualizadas via rede.

🛠️ Hardware Utilizado

    Microcontrolador: Arduino Mega (ou similar com suporte a múltiplos pinos analógicos).

    Conectividade: Shield Ethernet (UIPEthernet).

    Sensores:

        9x Sensores de Temperatura e Umidade DHT11.

        3x Sensores de Corrente Não Invasivo (SCT-013).

    Display: LCD 20x4 com adaptador I2C (Endereço 0x27).

📋 Pinagem Sugerida
Componente	Pino Arduino
Sensores DHT11 (1 a 9)	

A0 até A8
Corrente Ar 1	

A13
Corrente Ar 2	

A14
Corrente Ar 3	

A15
Ethernet CS	

53
💻 Configurações de Software

O código utiliza bibliotecas essenciais para comunicação e processamento de dados:

    Rede: Define um endereço MAC estático e busca IP via DHCP.

    MQTT: Conecta-se a um Broker na porta 1883 com autenticação de usuário.

    JSON: Organiza as leituras em objetos dataTemp, dataUmid e dataAC antes do envio.

Estrutura de Tópicos MQTT

    sensores/ArduinoDHT11AC/Temp

    sensores/ArduinoDHT11AC/Umid

    sensores/ArduinoDHT11AC/AC 

🔧 Como Usar

    Instale as bibliotecas listadas no cabeçalho do código (UIPEthernet, DHT, NTPClient, PubSubClient, ArduinoJson, EmonLib).

    Atualize as variáveis mqtt_server, mqtt_user e mqtt_password com as credenciais do seu Broker.

    Carregue o código no seu Arduino.

    Acompanhe o status de conexão diretamente no display LCD.

Autor: Sergio Levy Nobre dos Santos

Instituição: Universidade Federal do Ceará (UFC)



![Image_20250118_130733_568](https://github.com/user-attachments/assets/2aacaf27-2c63-4971-8011-f1e249f647d8)


![Image_20250118_130733_665](https://github.com/user-attachments/assets/e4878627-8e44-4ed8-ac58-fbc1abc976ce)

Comunicação feita via módulo enc28j60

![ENC28J60-Pinout](https://github.com/user-attachments/assets/4565f1c4-005e-4c5c-a028-7a823bafc77d)

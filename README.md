# Sistema de Monitoramento de Qualidade da Água com ESP32 e MQTT

Este repositório contém o código-fonte e os arquivos de simulação para o projeto de monitoramento de qualidade da água, desenvolvido como parte da disciplina de [NOME DA DISCIPLINA] da Universidade Presbiteriana Mackenzie.

## Descrição do Projeto

O sistema simula um dispositivo IoT baseado no microcontrolador ESP32 que monitora em tempo real os níveis de pH e turbidez da água. Os dados coletados são publicados via protocolo MQTT, permitindo o monitoramento remoto. O sistema também inclui um atuador (simulado por um LED) que representa uma válvula de segurança, acionada quando os parâmetros de qualidade da água estão fora dos limites seguros pré-definidos.

## Hardware (Simulado)

* Placa de Desenvolvimento: **ESP32 DevKit**
* Sensor de pH (Simulado por Potenciômetro)
* Sensor de Turbidez (Simulado por Potenciômetro)
* Atuador: **LED Vermelho** (Representando uma Válvula Solenoide)

## Software e Ferramentas

* **Framework:** ESP-IDF (Espressif IoT Development Framework)
* **Linguagem:** C
* **Plataforma de Simulação:** Wokwi
* **Protocolo de Comunicação:** MQTT
* **Cliente de Teste MQTT:** MQTT Explorer

## Como Executar a Simulação

1.  Abra o site [Wokwi.com](https://wokwi.com/).
2.  Crie um novo projeto ESP32 usando o framework ESP-IDF.
3.  Copie o conteúdo do arquivo `main.c` deste repositório para o arquivo `main.c` do seu projeto no Wokwi.
4.  Copie o conteúdo do arquivo `diagram.json` deste repositório para a aba `diagram.json` do seu projeto no Wokwi.
5.  Inicie a simulação.
6.  Use um cliente MQTT como o MQTT Explorer, conectado ao broker `test.mosquitto.org`, para visualizar os dados sendo publicados nos tópicos `agua/sim/idf/#`.

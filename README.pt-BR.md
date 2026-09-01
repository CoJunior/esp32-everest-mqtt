🌐 **Idioma:** [English](README.md) | Português (Brasil)

---

# ESP32 + EVerest + MQTT — Monitoramento e Simulação de Carregamento de Veículos Elétricos

Este projeto implementa uma plataforma experimental para monitoramento e simulação de sessões de carregamento de veículos elétricos utilizando **EVerest Open Source**, **ESP32**, **MQTT**, **Mosquitto** e uma interface web desenvolvida com **Vue.js**.

O sistema permite acompanhar dados de telemetria do processo de carregamento em tempo real, visualizar grandezas elétricas, acompanhar gráficos de potência e energia, enviar comandos de conexão e desconexão e gerar cenários de falha através de uma interface web executada diretamente no ESP32.

---

# 1. Visão Geral

A arquitetura atual do projeto é:

```text
                         EVerest
                            │
                            │ MQTT
                            │
              session_info / powermeter
                            │
                            ▼
                       Mosquitto
                      MQTT Broker
                     /            \
                    /              \
                   ▼                ▼
                ESP32         Dashboard Vue
                   │
                   │
             Servidor HTTP
                   │
          Interface de Cenários
             /             \
            ▼               ▼
       Falha MQTT      Falha do Veículo
```

O EVerest publica informações da sessão de carregamento através do MQTT.

O ESP32 se comunica com o broker MQTT e também executa um servidor HTTP próprio, que fornece uma interface web para simulação de cenários operacionais e de falha.

O frontend Vue se conecta ao Mosquitto através de MQTT sobre WebSocket e apresenta as informações recebidas em um dashboard em tempo real.

---

# 2. Funcionalidades Implementadas

A versão atual possui:

- comunicação entre EVerest e Mosquitto;
- comunicação entre ESP32 e Mosquitto;
- comunicação entre Vue.js e Mosquitto através de WebSocket;
- aquisição da telemetria de carregamento do EVerest;
- aquisição das grandezas elétricas do powermeter;
- dashboard em tempo real;
- aba de telemetria;
- aba de gráficos;
- gráfico de potência ao longo do tempo;
- gráfico de energia ao longo do tempo;
- controle de conexão do veículo;
- controle de desconexão do veículo;
- watchdog para disponibilidade do EVerest;
- servidor HTTP embarcado no ESP32;
- interface web própria do ESP32;
- simulação de falha MQTT;
- restauração da comunicação MQTT;
- simulação de falha do veículo;
- restauração da falha do veículo;
- publicação da falha simulada através do MQTT;
- visualização da falha simulada no dashboard Vue.

---

# 3. Tecnologias Utilizadas

## Hardware

- ESP32
- Cabo USB para programação e monitoramento serial
- Computador com Ubuntu/Linux

## Software

- EVerest Open Source
- ESP-IDF v5.5
- Mosquitto MQTT Broker
- MQTT
- MQTT sobre WebSocket
- Vue.js
- Vite
- Node.js
- npm
- Chart.js
- vue-chartjs
- Docker

---

# 4. Estrutura do Projeto

```text
esp32-everest-mqtt/
│
├── CMakeLists.txt
├── sdkconfig
│
├── main/
│   ├── CMakeLists.txt
│   └── main.c
│
├── frontend/
│   ├── package.json
│   ├── package-lock.json
│   ├── vite.config.js
│   ├── index.html
│   │
│   ├── public/
│   │
│   └── src/
│       ├── App.vue
│       ├── main.js
│       └── services/
│           └── mqtt.js
│
├── README.md
└── README.pt-BR.md
```

A pasta `build/` é gerada automaticamente pelo ESP-IDF e não é armazenada no repositório.

---

# 5. Instalação do Ambiente

As instruções abaixo consideram Ubuntu/Linux.

Atualize o sistema:

```bash
sudo apt update
sudo apt upgrade
```

---

# 6. Ferramentas Básicas

Instale:

```bash
sudo apt install -y \
git \
wget \
flex \
bison \
gperf \
python3 \
python3-pip \
python3-venv \
cmake \
ninja-build \
ccache \
libffi-dev \
libssl-dev \
dfu-util \
libusb-1.0-0
```

Verifique:

```bash
git --version
python3 --version
cmake --version
```

---

# 7. ESP-IDF

O projeto foi desenvolvido e testado utilizando:

```text
ESP-IDF v5.5
```

Na máquina original de desenvolvimento, o ambiente é ativado com:

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

Verifique:

```bash
idf.py --version
```

Resultado esperado:

```text
ESP-IDF v5.5
```

## Instalação do ESP-IDF v5.5 em Outra Máquina

```bash
cd ~

git clone --recursive --branch v5.5 \
https://github.com/espressif/esp-idf.git

cd esp-idf

./install.sh esp32

source ~/esp-idf/export.sh
```

Verifique:

```bash
idf.py --version
```

> Evite utilizar versões diferentes do ESP-IDF para compilar o mesmo projeto.

---

# 8. Instalação do Mosquitto

```bash
sudo apt install -y mosquitto mosquitto-clients
```

Verifique:

```bash
mosquitto -h
```

---

# 9. Configuração do Mosquitto

O projeto utiliza:

```text
MQTT TCP        → ESP32 / EVerest
MQTT WebSocket  → Frontend Vue
```

Exemplo de configuração para desenvolvimento:

```conf
listener 1883
protocol mqtt

listener 9001
protocol websockets

allow_anonymous true
```

A porta `1883` é utilizada pelo MQTT convencional.

A porta `9001` é utilizada pelo navegador através de WebSocket.

> `allow_anonymous true` deve ser utilizado apenas em ambiente local e controlado.

Reinicie:

```bash
sudo systemctl restart mosquitto
```

Verifique:

```bash
sudo systemctl status mosquitto
```

---

# 10. Comandos do Mosquitto

Iniciar:

```bash
sudo systemctl start mosquitto
```

Parar:

```bash
sudo systemctl stop mosquitto
```

Reiniciar:

```bash
sudo systemctl restart mosquitto
```

Monitorar todas as mensagens:

```bash
mosquitto_sub -h localhost -t '#' -v
```

---

# 11. Node.js e npm

Instale:

```bash
sudo apt install -y nodejs npm
```

Verifique:

```bash
node --version
npm --version
```

---

# 12. Preparação do Frontend

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend
```

Instale:

```bash
npm install
```

Caso necessário:

```bash
npm install mqtt chart.js vue-chartjs
```

---

# 13. Configuração MQTT do Frontend

Arquivo:

```text
frontend/src/services/mqtt.js
```

Conexão:

```javascript
const MQTT_URL = 'ws://localhost:9001'
```

Principais tópicos:

```text
everest_api/connector_1/var/session_info
everest_api/connector_1/var/powermeter
esp32/ev/falha
```

---

# 14. Tópicos MQTT

## Informações da Sessão

```text
everest_api/connector_1/var/session_info
```

Exemplo:

```json
{
  "charged_energy_wh": 4339,
  "charging_duration_s": 1468,
  "latest_total_w": 0.0,
  "permanent_fault": false,
  "state": "Unplugged"
}
```

O dashboard utiliza essas informações para:

- estado do carregamento;
- potência;
- energia;
- duração;
- falha permanente.

---

## Powermeter

```text
everest_api/connector_1/var/powermeter
```

Exemplo:

```json
{
  "meter_id": "YETI_POWERMETER",
  "phase_seq_error": false,
  "power_W": {
    "total": 0.0
  },
  "voltage_V": {
    "L1": 229.08,
    "L2": 229.08,
    "L3": 229.08
  },
  "current_A": {
    "L1": 0.0,
    "L2": 0.0,
    "L3": 0.0,
    "N": 0.0
  },
  "frequency_Hz": {
    "L1": 50.05,
    "L2": 50.05,
    "L3": 50.05
  }
}
```

---

## Comandos do Veículo

O dashboard publica comandos em:

```text
esp32/ev/comando
```

Comandos:

```text
CONECTAR
DESCONECTAR
```

---

## Falha Simulada do Veículo

Tópico:

```text
esp32/ev/falha
```

Falha ativa:

```json
{
  "origem": "ESP32",
  "tipo": "falha_veiculo_simulada",
  "ativa": true
}
```

Veículo restaurado:

```json
{
  "origem": "ESP32",
  "tipo": "falha_veiculo_simulada",
  "ativa": false
}
```

---

# 15. Configuração Wi-Fi do ESP32

O ESP32 precisa estar conectado à mesma rede do computador onde estão executando:

- Mosquitto;
- EVerest;
- frontend.

O repositório público utiliza:

```c
#define WIFI_SSID      "Nome_da_rede"
#define WIFI_PASSWORD  "Senha_da_rede"
```

Substitua localmente pelas credenciais da rede antes da compilação.

> Nunca envie senhas reais, tokens ou chaves para um repositório público.

---

# 16. Endereço do Broker para o ESP32

O frontend pode utilizar:

```text
localhost
```

O ESP32 precisa utilizar o IP do computador.

Descubra com:

```bash
hostname -I
```

Exemplo:

```text
192.168.1.40
```

Então:

```text
mqtt://192.168.1.40:1883
```

O endereço pode mudar dependendo da rede.

---

# 17. Compilando o ESP32

```bash
cd ~/Projetos/esp32_everest_mqtt
```

Ative o ESP-IDF:

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

Compile:

```bash
idf.py build
```

Resultado esperado:

```text
Project build complete.
```

---

# 18. Porta Serial

Identifique o ESP32:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Normalmente:

```text
/dev/ttyUSB0
```

Caso esteja ocupada:

```bash
lsof /dev/ttyUSB0
```

Feche outros monitores seriais.

---

# 19. Gravando o Firmware

```bash
idf.py -p /dev/ttyUSB0 flash
```

Para gravar e abrir o monitor:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Sair:

```text
Ctrl + ]
```

---

# 20. Servidor Web do ESP32

Depois da conexão Wi-Fi, o endereço IP aparece no monitor serial.

Exemplo:

```text
192.168.1.180
```

Abra:

```text
http://192.168.1.180
```

O IP pode mudar por DHCP.

---

# 21. Interface Web do ESP32

A interface apresenta:

- estado do Wi-Fi;
- servidor HTTP;
- estado MQTT;
- estado do EVerest;
- falha MQTT simulada;
- falha simulada do veículo;
- potência do EVerest;
- falha permanente do EVerest.

Também possui controles de simulação e restauração de falhas.

---

# 22. Cenário de Falha MQTT

Controles:

```text
SIMULAR FALHA MQTT
RESTAURAR MQTT
```

Ao ativar a falha, somente o cliente MQTT do ESP32 é interrompido.

Resultado esperado:

```text
Wi-Fi          → CONECTADO
Servidor HTTP  → ATIVO
MQTT ESP32     → FALHA SIMULADA
```

O Mosquitto e o EVerest continuam ativos.

---

# 23. Cenário de Falha do Veículo

Controles:

```text
SIMULAR FALHA DO VEÍCULO
RESTAURAR VEÍCULO
```

A falha é publicada em:

```text
esp32/ev/falha
```

O dashboard Vue recebe automaticamente essa informação.

---

# 24. Falha Simulada vs. Falha Permanente

A falha simulada pelo ESP32 não altera:

```text
permanent_fault
```

do EVerest.

Portanto:

```text
Falha permanente EVerest
          ≠
Falha simulada ESP32
```

Isso permite distinguir uma falha reportada pelo EVerest de uma falha injetada propositalmente pelo simulador.

---

# 25. Executando o Frontend

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend
npm run dev
```

Normalmente:

```text
http://localhost:5173
```

---

# 26. Dashboard

O dashboard possui:

```text
Telemetria
Gráficos
```

A telemetria apresenta:

- status;
- potência;
- energia;
- tempo;
- falha permanente;
- falha simulada do ESP32;
- tensão L1/L2/L3;
- corrente L1/L2/L3;
- corrente do neutro;
- frequência L1/L2/L3;
- potência do medidor;
- sequência de fases.

---

# 27. Gráficos

São apresentados:

```text
Potência × Tempo
Energia × Tempo
```

Durante o carregamento:

- novos pontos são adicionados;
- a potência é atualizada;
- a energia aumenta;
- o tempo avança.

Quando a sessão termina:

- é registrado um último ponto de potência zero;
- o gráfico para de receber novos pontos;
- mensagens repetidas de `Unplugged` não geram pontos duplicados.

Uma nova sessão limpa o histórico anterior.

---

# 28. Estados do EVerest

Estados observados:

```text
Unplugged
Preparing
Charging
Finished
Faulted
```

Fluxo típico:

```text
Unplugged
    ↓
Preparing
    ↓
Charging
    ↓
Finished / Unplugged
```

---

# 29. Controle do Carregamento

O dashboard possui:

```text
CONECTAR
DESCONECTAR
```

Os botões dependem de:

- conexão MQTT;
- disponibilidade do EVerest;
- estado atual da sessão;
- falha permanente;
- comando em andamento.

---

# 30. Watchdog do EVerest

O frontend monitora:

```text
everest_api/connector_1/var/session_info
```

Se nenhuma mensagem for recebida por aproximadamente cinco segundos, o EVerest é considerado indisponível.

Nesse caso, os comandos do dashboard são desativados até que a comunicação seja restaurada.

---

# 31. Executando o EVerest

O EVerest é atualmente executado dentro do ambiente Docker/SIL configurado.

Dentro desse ambiente:

```bash
./run-scripts/run-sil.sh
```

A configuração utilizada está associada a:

```text
/workspace/config/config-sil.yaml
```

> O comando exato utilizado no sistema hospedeiro para criar ou iniciar o container depende da instalação local do EVerest e ainda não está documentado neste repositório. O comando acima é o comando validado dentro do ambiente de desenvolvimento.

---

# 32. Testando as Mensagens do EVerest

Sessão:

```bash
mosquitto_sub \
-h localhost \
-t 'everest_api/connector_1/var/session_info' \
-v
```

Powermeter:

```bash
mosquitto_sub \
-h localhost \
-t 'everest_api/connector_1/var/powermeter' \
-v
```

Todos os tópicos:

```bash
mosquitto_sub -h localhost -t '#' -v
```

---

# 33. Comportamento ao Parar o Mosquitto

Durante os testes foi observado que parar completamente o Mosquitto pode provocar o encerramento do EVerest após a perda da conexão MQTT.

Assim, após iniciar novamente o Mosquitto, pode ocorrer:

```text
MQTT Broker → ONLINE
EVerest     → OFFLINE
```

Nesse caso é necessário iniciar novamente o EVerest:

```bash
./run-scripts/run-sil.sh
```

Por esse motivo, a simulação de falha MQTT do ESP32 interrompe somente o cliente MQTT do ESP32.

---

# 34. Ordem Recomendada de Inicialização

```text
1. Mosquitto
      ↓
2. EVerest
      ↓
3. ESP32
      ↓
4. Frontend Vue
      ↓
5. Dashboard
      ↓
6. Interface web ESP32
```

---

# 35. Terminal 1 — Mosquitto

```bash
sudo systemctl status mosquitto
```

Caso esteja parado:

```bash
sudo systemctl start mosquitto
```

---

# 36. Terminal 2 — EVerest

Dentro do ambiente Docker/SIL:

```bash
./run-scripts/run-sil.sh
```

---

# 37. Terminal 3 — ESP32

```bash
cd ~/Projetos/esp32_everest_mqtt

source /home/junior/.espressif/v5.5/esp-idf/export.sh

idf.py --version
```

Monitor:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Recompilar, gravar e monitorar:

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

# 38. Terminal 4 — Frontend

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend
npm run dev
```

Abra:

```text
http://localhost:5173
```

---

# 39. Monitor MQTT Opcional

```bash
mosquitto_sub -h localhost -t '#' -v
```

É útil para acompanhar e depurar a comunicação entre os componentes.

---

# 40. Teste Completo

## Teste 1 — Comunicação

Confirme:

```text
MQTT conectado
EVerest online
```

## Teste 2 — Telemetria

Confirme:

```text
tensão
corrente
frequência
potência
energia
estado
tempo
```

## Teste 3 — Carregamento

Conecte o veículo simulado.

Fluxo esperado:

```text
Unplugged
    ↓
Preparing
    ↓
Charging
```

Durante o carregamento:

- potência aumenta;
- corrente aumenta;
- energia aumenta;
- tempo avança;
- gráficos são atualizados.

## Teste 4 — Desconexão

Desconecte o veículo.

Resultado:

- potência retorna para zero;
- corrente retorna para zero;
- energia final permanece;
- o gráfico registra o final da sessão.

## Teste 5 — Falha MQTT

Abra:

```text
http://IP_DO_ESP32
```

Ative:

```text
SIMULAR FALHA MQTT
```

Resultado:

```text
MQTT ESP32 → FALHA SIMULADA
Wi-Fi      → CONECTADO
HTTP       → ATIVO
```

Depois:

```text
RESTAURAR MQTT
```

## Teste 6 — Falha do Veículo

Ative:

```text
SIMULAR FALHA DO VEÍCULO
```

No dashboard:

```text
⚠ FALHA ATIVA
```

Depois:

```text
RESTAURAR VEÍCULO
```

Resultado:

```text
NORMAL
```

---

# 41. Problemas Comuns

## `idf.py: command not found`

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

## Versão Incorreta do ESP-IDF

```bash
idf.py --version
```

Esperado:

```text
ESP-IDF v5.5
```

## Porta do ESP32 Não Encontrada

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

## Porta Serial Ocupada

```bash
lsof /dev/ttyUSB0
```

## MQTT Desconectado

```bash
sudo systemctl status mosquitto
```

Verifique também a porta WebSocket `9001`.

## MQTT Online, mas EVerest Offline

Reinicie o EVerest:

```bash
./run-scripts/run-sil.sh
```

## Falha do ESP32 Não Chega ao Dashboard

```bash
mosquitto_sub \
-h localhost \
-t 'esp32/ev/falha' \
-v
```

## Dados do Powermeter Não Aparecem

```bash
mosquitto_sub \
-h localhost \
-t 'everest_api/connector_1/var/powermeter' \
-v
```

## `session_info` Não Aparece

```bash
mosquitto_sub \
-h localhost \
-t 'everest_api/connector_1/var/session_info' \
-v
```

---

# 42. Tensão com o Veículo Desconectado

O powermeter pode continuar apresentando aproximadamente:

```text
230 V
```

mesmo quando o veículo está desconectado.

Isso é esperado porque o medidor continua medindo a tensão disponível da rede.

Sem carregamento:

```text
Corrente ≈ 0 A
Potência ≈ 0 W
```

enquanto a tensão pode continuar presente.

---

# 43. Estado Atual do Desenvolvimento

Fluxo de monitoramento:

```text
EVerest
   ↓
Mosquitto
   ↓
Dashboard Vue
```

Fluxo do ESP32:

```text
EVerest
   ↓
Mosquitto
   ↓
ESP32
   ↓
Servidor Web ESP32
```

Fluxo da falha simulada:

```text
Interface Web ESP32
        ↓
Falha Simulada
        ↓
ESP32
        ↓
MQTT
        ↓
Dashboard Vue
        ↓
⚠ FALHA ATIVA
```

---

# 44. Próximas Etapas

Possíveis evoluções:

- novos cenários de falha;
- parâmetros configuráveis do veículo;
- implementação do Digital Twin;
- State of Charge (SOC);
- capacidade da bateria;
- temperatura da bateria;
- perfis de carregamento;
- persistência de sessões;
- histórico de carregamentos;
- armazenamento de eventos e logs;
- geração de relatórios;
- exportação de dados;
- testes experimentais;
- simulação dos estados IEC 61851;
- integração do Control Pilot;
- integração com PCB dedicada, conforme definição final do escopo.

---

# 45. Contexto Acadêmico

Este projeto faz parte do desenvolvimento de uma plataforma experimental para estudo de sistemas de carregamento de veículos elétricos.

A arquitetura combina:

```text
EVerest → plataforma de carregamento
ESP32   → simulação e comunicação
MQTT    → troca de mensagens
Vue.js  → monitoramento e visualização
```

A plataforma permite estudar carregamento, telemetria, comunicação e cenários de falha sem depender inicialmente de um veículo elétrico real.

O projeto poderá evoluir posteriormente para um Digital Twin de veículo elétrico e uma plataforma experimental de carregamento mais completa.

---

# 46. Execução Rápida

## Iniciar Mosquitto

```bash
sudo systemctl start mosquitto
```

## Iniciar EVerest

Dentro do ambiente configurado:

```bash
./run-scripts/run-sil.sh
```

## Iniciar ESP32

```bash
cd ~/Projetos/esp32_everest_mqtt

source /home/junior/.espressif/v5.5/esp-idf/export.sh

idf.py -p /dev/ttyUSB0 monitor
```

Caso seja necessário recompilar:

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Iniciar Frontend

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend
npm run dev
```

## Interfaces

Dashboard Vue:

```text
http://localhost:5173
```

ESP32:

```text
http://IP_DO_ESP32
```

---

# 47. Segurança

Não envie para o repositório:

- senhas Wi-Fi;
- chaves de API;
- tokens;
- chaves privadas SSH;
- credenciais pessoais;
- arquivos `.env` com segredos.

Utilize placeholders:

```c
#define WIFI_SSID      "Nome_da_rede"
#define WIFI_PASSWORD  "Senha_da_rede"
```

---

# 48. Licença

Nenhuma licença foi definida para este projeto até o momento.

Enquanto uma licença não for adicionada, aplicam-se as regras padrão de direitos autorais.
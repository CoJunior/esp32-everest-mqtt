# ESP32 + EVerest + MQTT — Monitoramento e Simulação de Carregamento de Veículos Elétricos

Este projeto implementa uma plataforma experimental para monitoramento e simulação de sessões de carregamento de veículos elétricos utilizando **EVerest Open Source**, **ESP32**, **MQTT**, **Mosquitto** e uma interface web desenvolvida com **Vue.js**.

O sistema permite acompanhar dados de telemetria do processo de carregamento em tempo real, visualizar grandezas elétricas, acompanhar gráficos de potência e energia, enviar comandos de conexão e desconexão e gerar cenários de falha através de uma interface web executada diretamente no ESP32.

---

# 1. Visão geral

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
          Interface de cenários
             /             \
            ▼               ▼
      Falha MQTT      Falha do veículo
```

O EVerest disponibiliza informações da sessão de carregamento através do MQTT.

O ESP32 se comunica com o broker MQTT e também possui um servidor HTTP próprio, utilizado para acessar uma interface de simulação de cenários.

O frontend Vue se conecta ao Mosquitto através de MQTT sobre WebSocket e apresenta as informações recebidas em um dashboard.

---

# 2. Funcionalidades implementadas

Atualmente o projeto possui:

- comunicação entre EVerest e Mosquitto;
- comunicação entre ESP32 e Mosquitto;
- comunicação entre Vue.js e Mosquitto através de WebSocket;
- leitura da telemetria do EVerest;
- leitura das grandezas elétricas do powermeter;
- dashboard em tempo real;
- aba de telemetria;
- aba de gráficos;
- gráfico de potência ao longo do tempo;
- gráfico de energia ao longo do tempo;
- controle de conexão do veículo;
- controle de desconexão do veículo;
- watchdog para detectar indisponibilidade do EVerest;
- servidor HTTP embarcado no ESP32;
- interface web própria do ESP32;
- simulação de falha MQTT do ESP32;
- restauração da comunicação MQTT;
- simulação de falha do veículo;
- restauração da falha do veículo;
- envio da falha simulada através do MQTT;
- exibição da falha simulada no dashboard Vue.

---

# 3. Tecnologias utilizadas

## Hardware

- ESP32
- Cabo USB para programação e monitor serial
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

# 4. Estrutura do projeto

A estrutura principal utilizada atualmente é:

```text
esp32_everest_mqtt/
│
├── CMakeLists.txt
├── sdkconfig
├── sdkconfig.old
│
├── main/
│   ├── CMakeLists.txt
│   ├── main.c
│   └── main_backup.c
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
└── build/
```

> A pasta `build/` é gerada automaticamente pelo ESP-IDF e não precisa ser criada manualmente.

---

# 5. Instalação do ambiente

As instruções abaixo consideram uma máquina Ubuntu/Linux.

Atualize os pacotes:

```bash
sudo apt update
sudo apt upgrade
```

---

# 6. Instalação das ferramentas básicas

Instale as ferramentas necessárias:

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

# 7. ESP-IDF utilizado no projeto

O projeto foi desenvolvido e validado com:

```text
ESP-IDF v5.5
```

O ambiente utilizado está localizado em:

```text
/home/junior/.espressif/v5.5/esp-idf
```

Sempre que abrir um terminal novo para trabalhar com o ESP32, ative o ambiente com:

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

Depois confirme:

```bash
idf.py --version
```

O esperado é:

```text
ESP-IDF v5.5
```

> É importante não misturar versões diferentes do ESP-IDF no mesmo projeto.

---

# 8. Problema de versão do ESP-IDF

Durante o desenvolvimento ocorreu um erro porque o projeto havia sido configurado com ESP-IDF v5.5 e foi tentado um build com ESP-IDF v6.1.

O erro estava relacionado ao cache do CMake apontando para diretórios diferentes.

Por isso, para este projeto, utilize sempre:

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

---

# 9. Instalação do Mosquitto

Instale o broker MQTT:

```bash
sudo apt install -y mosquitto mosquitto-clients
```

Verifique:

```bash
mosquitto -h
```

---

# 10. Configuração do Mosquitto

O projeto utiliza duas formas de comunicação:

```text
MQTT TCP        → ESP32 / EVerest
MQTT WebSocket  → Frontend Vue
```

O frontend utiliza:

```text
ws://localhost:9001
```

Portanto, o Mosquitto deve possuir:

```text
listener 1883
protocol mqtt

listener 9001
protocol websockets

allow_anonymous true
```

A porta `1883` é utilizada pelo MQTT tradicional.

A porta `9001` é utilizada pelo navegador através de WebSocket.

> `allow_anonymous true` é adequado apenas para ambiente de desenvolvimento local.

Depois de alterar a configuração:

```bash
sudo systemctl restart mosquitto
```

Verifique:

```bash
sudo systemctl status mosquitto
```

---

# 11. Iniciar e parar o Mosquitto

Para iniciar:

```bash
sudo systemctl start mosquitto
```

Para parar:

```bash
sudo systemctl stop mosquitto
```

Para reiniciar:

```bash
sudo systemctl restart mosquitto
```

Para verificar:

```bash
sudo systemctl status mosquitto
```

---

# 12. Testando o Mosquitto

Para visualizar todas as mensagens MQTT:

```bash
mosquitto_sub -h localhost -t '#' -v
```

Para encerrar:

```text
Ctrl + C
```

---

# 13. Node.js e npm

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

# 14. Preparando o frontend

Entre na pasta:

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend
```

Instale as dependências:

```bash
npm install
```

Caso seja necessário instalar manualmente as principais bibliotecas:

```bash
npm install mqtt chart.js vue-chartjs
```

---

# 15. Configuração MQTT do frontend

O arquivo responsável pela conexão MQTT é:

```text
frontend/src/services/mqtt.js
```

O frontend utiliza:

```javascript
const MQTT_URL = 'ws://localhost:9001'
```

Os tópicos utilizados atualmente são:

```text
everest_api/connector_1/var/session_info
everest_api/connector_1/var/powermeter
esp32/ev/falha
```

---

# 16. Tópicos MQTT utilizados

## Telemetria da sessão

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

Informações utilizadas:

- estado da sessão;
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

## Comandos do veículo

O dashboard publica comandos em:

```text
esp32/ev/comando
```

Comandos utilizados:

```text
CONECTAR
DESCONECTAR
```

---

## Falha simulada do veículo

O ESP32 publica:

```text
esp32/ev/falha
```

Quando a falha é ativada:

```json
{
  "origem": "ESP32",
  "tipo": "falha_veiculo_simulada",
  "ativa": true
}
```

Quando é restaurada:

```json
{
  "origem": "ESP32",
  "tipo": "falha_veiculo_simulada",
  "ativa": false
}
```

---

# 17. Configuração do Wi-Fi do ESP32

O ESP32 precisa estar conectado à mesma rede local utilizada pelo computador onde estão executando:

- Mosquitto;
- EVerest;
- frontend.

As credenciais Wi-Fi estão configuradas no firmware do ESP32.

> Não é recomendado publicar senha real de Wi-Fi em repositórios públicos.

---

# 18. IP do broker MQTT para o ESP32

O frontend pode usar:

```text
localhost
```

porque ele roda no mesmo computador do Mosquitto.

O ESP32 não pode utilizar `localhost` para acessar o computador.

Para descobrir o IP do Ubuntu:

```bash
hostname -I
```

Exemplo:

```text
192.168.1.40
```

No firmware, o broker deve apontar para:

```text
mqtt://192.168.1.40:1883
```

O endereço pode mudar dependendo da rede.

---

# 19. Compilando o ESP32

Entre no projeto:

```bash
cd ~/Projetos/esp32_everest_mqtt
```

Ative o ESP-IDF:

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

Confira:

```bash
idf.py --version
```

Compile:

```bash
idf.py build
```

Quando estiver correto:

```text
Project build complete.
```

---

# 20. Identificando a porta USB

Com o ESP32 conectado:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Normalmente:

```text
/dev/ttyUSB0
```

---

# 21. Erro de porta USB

Caso apareça:

```text
Could not open /dev/ttyUSB0
```

verifique:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Durante o desenvolvimento foi observado que conectar o ESP32 em outra porta USB pode alterar o dispositivo disponível.

---

# 22. Porta serial ocupada

Caso a porta exista, mas esteja ocupada:

```bash
lsof /dev/ttyUSB0
```

Feche programas como:

- outro monitor serial;
- CuteCom;
- Arduino IDE;
- outro processo do ESP-IDF.

---

# 23. Gravando o firmware

Para gravar:

```bash
idf.py -p /dev/ttyUSB0 flash
```

Para gravar e abrir o monitor:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Para sair do monitor:

```text
Ctrl + ]
```

---

# 24. Servidor web do ESP32

Depois que o ESP32 conecta ao Wi-Fi, o monitor apresenta o IP.

Exemplo:

```text
Wi-Fi conectado. IP do ESP32: 192.168.1.180
```

Abra:

```text
http://192.168.1.180
```

O IP pode mudar por DHCP.

---

# 25. Interface web do ESP32

A interface apresenta:

- Wi-Fi;
- servidor HTTP;
- estado MQTT do ESP32;
- estado EVerest;
- falha MQTT simulada;
- falha simulada do veículo;
- potência recebida do EVerest;
- falha permanente recebida do EVerest.

---

# 26. Cenário MQTT

A interface possui:

```text
SIMULAR FALHA MQTT
RESTAURAR MQTT
```

---

# 27. Simulação de falha MQTT

Ao clicar em:

```text
SIMULAR FALHA MQTT
```

somente a comunicação MQTT do ESP32 é interrompida.

Resultado esperado:

```text
Wi-Fi          → CONECTADO
Servidor HTTP  → ATIVO
MQTT ESP32     → FALHA SIMULADA
```

O Mosquitto continua funcionando.

O EVerest continua funcionando.

O servidor HTTP continua acessível.

---

# 28. Restaurando o MQTT

Clique:

```text
RESTAURAR MQTT
```

Resultado esperado:

```text
MQTT ESP32     → CONECTADO
Falha simulada → Nenhuma
```

---

# 29. Cenário do veículo

A interface possui:

```text
SIMULAR FALHA DO VEÍCULO
RESTAURAR VEÍCULO
```

---

# 30. Simulação de falha do veículo

Clique:

```text
SIMULAR FALHA DO VEÍCULO
```

Resultado esperado:

```text
Falha do veículo simulada → FALHA ATIVA
```

O MQTT continua conectado.

A falha é publicada em:

```text
esp32/ev/falha
```

---

# 31. Diferença entre falha simulada e falha do EVerest

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

Isso permite diferenciar uma falha real reportada pelo EVerest de uma falha injetada pelo simulador.

---

# 32. Restaurando o veículo

Clique:

```text
RESTAURAR VEÍCULO
```

Resultado:

```text
Falha do veículo simulada → NORMAL
```

O dashboard Vue também retorna automaticamente para o estado normal.

---

# 33. Executando o frontend

Entre na pasta:

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend
```

Execute:

```bash
npm run dev
```

O Vite normalmente exibirá:

```text
Local: http://localhost:5173/
```

Abra:

```text
http://localhost:5173
```

---

# 34. Dashboard Vue

O dashboard possui duas abas:

```text
Telemetria
Gráficos
```

---

# 35. Aba Telemetria

São apresentados:

```text
Status
Potência
Energia
Tempo
Falha permanente
Falha simulada ESP32
```

Também são apresentadas:

```text
Tensão L1
Tensão L2
Tensão L3

Corrente L1
Corrente L2
Corrente L3

Corrente do neutro

Frequência L1
Frequência L2
Frequência L3

Potência do medidor

Sequência de fases
```

---

# 36. Falha simulada no dashboard

Em funcionamento normal:

```text
FALHA SIMULADA ESP32
NORMAL
```

Quando a falha é ativada no ESP32:

```text
FALHA SIMULADA ESP32
⚠ FALHA ATIVA
```

O dashboard apresenta também um alerta visual.

Ao restaurar o veículo:

```text
NORMAL
```

sem necessidade de atualizar a página.

---

# 37. Aba Gráficos

São apresentados:

```text
Potência × Tempo
Energia × Tempo
```

Os gráficos são atualizados durante uma sessão de carregamento.

Ao iniciar uma nova sessão, o histórico anterior é limpo.

Durante o carregamento, novos pontos são registrados.

Quando a sessão termina:

- é adicionado um último ponto com potência zero;
- o gráfico é congelado;
- mensagens posteriores em `Unplugged` não criam pontos repetidos.

---

# 38. Estados principais do EVerest

Estados observados:

```text
Unplugged
Preparing
Charging
Finished
Faulted
```

No dashboard:

```text
Unplugged → Desconectado
Preparing → Preparando
Charging  → Carregando
Finished  → Finalizado
Faulted   → Falha
```

---

# 39. Controle de carregamento

O dashboard possui:

```text
CONECTAR
DESCONECTAR
```

Os comandos são enviados para:

```text
esp32/ev/comando
```

Os botões dependem de:

- conexão MQTT;
- disponibilidade do EVerest;
- estado atual da sessão;
- falha permanente;
- comando em andamento.

---

# 40. Watchdog do EVerest

O frontend monitora a chegada de:

```text
session_info
```

Se o EVerest deixar de publicar por aproximadamente 5 segundos, o dashboard mostra:

```text
Aguardando EVerest
```

e os comandos são bloqueados.

---

# 41. Executando o EVerest

O EVerest é executado no ambiente Docker configurado para o simulador SIL.

Dentro do ambiente/container do EVerest, execute:

```bash
./run-scripts/run-sil.sh
```

O simulador utiliza a configuração:

```text
/workspace/config/config-sil.yaml
```

Durante a execução, os logs do EVerest aparecem no terminal.

---

# 42. Testando a publicação do EVerest

Para verificar `session_info`:

```bash
mosquitto_sub \
-h localhost \
-t 'everest_api/connector_1/var/session_info' \
-v
```

Para verificar `powermeter`:

```bash
mosquitto_sub \
-h localhost \
-t 'everest_api/connector_1/var/powermeter' \
-v
```

Para visualizar tudo:

```bash
mosquitto_sub -h localhost -t '#' -v
```

---

# 43. Atenção ao desligar o Mosquitto

Durante os testes foi observado que interromper completamente o Mosquitto pode fazer o processo do EVerest encerrar por perda da conexão MQTT.

Nesse caso pode ocorrer:

```text
MQTT → conectado
EVerest → indisponível
```

mesmo depois de iniciar o broker novamente.

Nesse caso é necessário iniciar novamente:

```bash
./run-scripts/run-sil.sh
```

Por esse motivo, o botão:

```text
SIMULAR FALHA MQTT
```

não desliga o broker.

Ele interrompe somente o cliente MQTT do ESP32.

---

# 44. Ordem recomendada para executar o sistema

A ordem utilizada é:

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

# 45. Terminal 1 — Mosquitto

Verifique:

```bash
sudo systemctl status mosquitto
```

Caso esteja parado:

```bash
sudo systemctl start mosquitto
```

---

# 46. Terminal 2 — EVerest

Com o ambiente Docker do EVerest aberto:

```bash
./run-scripts/run-sil.sh
```

Confirme que aparecem mensagens de:

```text
session_info
powermeter
```

no MQTT.

---

# 47. Terminal 3 — ESP32

Entre no projeto:

```bash
cd ~/Projetos/esp32_everest_mqtt
```

Ative o ESP-IDF:

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

Confira:

```bash
idf.py --version
```

Para apenas monitorar:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Para recompilar e gravar:

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

# 48. Terminal 4 — Frontend

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend
npm run dev
```

Abra:

```text
http://localhost:5173
```

---

# 49. Terminal opcional — Monitor MQTT

```bash
mosquitto_sub -h localhost -t '#' -v
```

Esse terminal ajuda bastante na depuração.

---

# 50. Teste completo do sistema

## Teste 1 — Conexões

No dashboard confirme:

```text
MQTT conectado
EVerest online
```

---

## Teste 2 — Telemetria

Confirme a atualização de:

```text
tensão
corrente
frequência
potência
energia
estado
tempo
```

---

## Teste 3 — Conectar veículo

Clique:

```text
CONECTAR
```

O estado deve evoluir, por exemplo:

```text
Unplugged
   ↓
Preparing
   ↓
Charging
```

Durante `Charging`:

- potência aumenta;
- corrente aumenta;
- energia cresce;
- tempo avança;
- gráficos recebem pontos.

---

## Teste 4 — Desconectar

Clique:

```text
DESCONECTAR
```

Resultado esperado:

- potência retorna para zero;
- corrente retorna para zero;
- energia permanece com valor final;
- gráfico registra o fim;
- gráfico deixa de adicionar pontos.

---

## Teste 5 — Falha MQTT do ESP32

Abra:

```text
http://IP_DO_ESP32
```

Clique:

```text
SIMULAR FALHA MQTT
```

Confirme:

```text
MQTT ESP32 → FALHA SIMULADA
Wi-Fi      → CONECTADO
HTTP       → ATIVO
```

Depois:

```text
RESTAURAR MQTT
```

Resultado:

```text
MQTT ESP32 → CONECTADO
```

---

## Teste 6 — Falha simulada do veículo

Com o dashboard aberto:

```text
http://localhost:5173
```

Na interface do ESP32 clique:

```text
SIMULAR FALHA DO VEÍCULO
```

No dashboard deve aparecer:

```text
⚠ FALHA ATIVA
```

A falha permanente do EVerest deve continuar separada.

Depois clique:

```text
RESTAURAR VEÍCULO
```

Resultado:

```text
NORMAL
```

---

# 51. Problemas comuns

## `idf.py: command not found`

Execute:

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

---

## Versão errada do ESP-IDF

Verifique:

```bash
idf.py --version
```

Utilize:

```text
ESP-IDF v5.5
```

---

## `/dev/ttyUSB0` não existe

Execute:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

---

## Porta serial ocupada

Execute:

```bash
lsof /dev/ttyUSB0
```

Feche outros monitores seriais.

---

## Frontend mostra MQTT desconectado

Verifique:

```bash
sudo systemctl status mosquitto
```

Confira também se a porta WebSocket `9001` está configurada.

---

## MQTT conectado, mas EVerest indisponível

Verifique se o EVerest está em execução.

Caso tenha sido encerrado:

```bash
./run-scripts/run-sil.sh
```

---

## Dashboard não recebe falha do ESP32

Teste:

```bash
mosquitto_sub \
-h localhost \
-t 'esp32/ev/falha' \
-v
```

Depois clique:

```text
SIMULAR FALHA DO VEÍCULO
```

Também é possível abrir o Console do navegador e filtrar:

```text
esp32/ev/falha
```

---

## Dashboard não recebe powermeter

Teste:

```bash
mosquitto_sub \
-h localhost \
-t 'everest_api/connector_1/var/powermeter' \
-v
```

---

## Dashboard não recebe session_info

Teste:

```bash
mosquitto_sub \
-h localhost \
-t 'everest_api/connector_1/var/session_info' \
-v
```

---

# 52. Observação sobre tensão com veículo desconectado

Mesmo com o veículo desconectado, o powermeter pode continuar apresentando aproximadamente:

```text
230 V
```

Isso representa a tensão da rede medida pelo powermeter.

Quando não existe carregamento:

```text
Corrente ≈ 0 A
Potência ≈ 0 W
```

---

# 53. Estado atual do desenvolvimento

Atualmente estão funcionando:

```text
EVerest
   ↓
Mosquitto
   ↓
ESP32
   ↓
Servidor Web ESP32
```

Também:

```text
EVerest
   ↓
Mosquitto
   ↓
Dashboard Vue
```

E a integração de falha:

```text
Interface ESP32
      ↓
Falha do veículo
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

# 54. Próximas etapas

Possíveis evoluções:

- ampliar cenários de falha;
- adicionar parâmetros configuráveis do veículo;
- implementar Digital Twin;
- adicionar SOC;
- adicionar capacidade da bateria;
- adicionar temperatura;
- adicionar perfis de carregamento;
- persistir sessões;
- armazenar histórico;
- armazenar eventos e logs;
- gerar relatórios;
- exportar dados;
- executar testes experimentais;
- integrar hardware Control Pilot e estados IEC 61851, conforme definição do escopo.

---

# 55. Objetivo acadêmico

O projeto faz parte do desenvolvimento de uma plataforma experimental para estudo do carregamento de veículos elétricos.

A proposta utiliza:

```text
EVerest → plataforma de carregamento
ESP32   → simulação e comunicação
MQTT    → troca de mensagens
Vue.js  → monitoramento
```

A plataforma permite estudar funcionamento normal e cenários de falha sem depender inicialmente de um veículo elétrico real.

---

# 56. Resumo rápido para executar

Depois que o ambiente estiver instalado:

## Mosquitto

```bash
sudo systemctl start mosquitto
```

## EVerest

Dentro do ambiente Docker:

```bash
./run-scripts/run-sil.sh
```

## ESP32

```bash
cd ~/Projetos/esp32_everest_mqtt

source /home/junior/.espressif/v5.5/esp-idf/export.sh

idf.py -p /dev/ttyUSB0 monitor
```

Caso precise recompilar:

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Frontend

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend

npm run dev
```

## Interfaces

Dashboard:

```text
http://localhost:5173
```

Interface do ESP32:

```text
http://IP_DO_ESP32
```

Com Mosquitto, EVerest, ESP32 e frontend ativos, o sistema estará pronto para monitoramento, controle e simulação de cenários.
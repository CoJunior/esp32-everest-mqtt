🌐 **Language:** English | [Português (Brasil)](README.pt-BR.md)

---

# ESP32 + EVerest + MQTT — Electric Vehicle Charging Monitoring and Simulation

This project implements an experimental platform for monitoring and simulating Electric Vehicle (EV) charging sessions using **EVerest Open Source**, **ESP32**, **MQTT**, **Mosquitto**, and a web interface developed with **Vue.js**.

The system provides real-time charging telemetry, electrical measurements, power and energy charts, vehicle connection/disconnection controls, and fault simulation scenarios through a web interface hosted directly on the ESP32.

---

# 1. Project Overview

The current system architecture is:

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
                ESP32         Vue Dashboard
                   │
                   │
              HTTP Server
                   │
           Scenario Interface
             /             \
            ▼               ▼
       MQTT Fault      Vehicle Fault
```

EVerest publishes charging session information through MQTT.

The ESP32 communicates with the MQTT broker and also runs its own HTTP server, which provides a web interface for operational and fault scenario simulation.

The Vue frontend connects to Mosquitto using MQTT over WebSocket and displays the received information in a real-time dashboard.

---

# 2. Implemented Features

The current version includes:

- communication between EVerest and Mosquitto;
- communication between ESP32 and Mosquitto;
- communication between Vue.js and Mosquitto using WebSocket;
- EVerest charging telemetry acquisition;
- powermeter electrical measurement acquisition;
- real-time monitoring dashboard;
- telemetry tab;
- charts tab;
- power over time chart;
- energy over time chart;
- vehicle connection control;
- vehicle disconnection control;
- EVerest availability watchdog;
- embedded HTTP server on the ESP32;
- ESP32 web interface;
- ESP32 MQTT fault simulation;
- MQTT communication restoration;
- simulated vehicle fault;
- vehicle fault restoration;
- simulated fault publication through MQTT;
- simulated fault visualization on the Vue dashboard.

---

# 3. Technologies

## Hardware

- ESP32
- USB cable for programming and serial monitoring
- Ubuntu/Linux computer

## Software

- EVerest Open Source
- ESP-IDF v5.5
- Mosquitto MQTT Broker
- MQTT
- MQTT over WebSocket
- Vue.js
- Vite
- Node.js
- npm
- Chart.js
- vue-chartjs
- Docker

---

# 4. Project Structure

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

The `build/` directory is generated automatically by ESP-IDF and is not stored in the repository.

---

# 5. Environment Installation

The instructions below assume an Ubuntu/Linux environment.

Update the system:

```bash
sudo apt update
sudo apt upgrade
```

---

# 6. Basic Development Tools

Install the required packages:

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

Verify:

```bash
git --version
python3 --version
cmake --version
```

---

# 7. ESP-IDF

This project was developed and tested using:

```text
ESP-IDF v5.5
```

On the original development machine, ESP-IDF is activated with:

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

Verify:

```bash
idf.py --version
```

Expected result:

```text
ESP-IDF v5.5
```

## Installing ESP-IDF v5.5 on Another Machine

```bash
cd ~

git clone --recursive --branch v5.5 \
https://github.com/espressif/esp-idf.git

cd esp-idf

./install.sh esp32

source ~/esp-idf/export.sh
```

Verify:

```bash
idf.py --version
```

> Do not mix different ESP-IDF versions when building this project.

---

# 8. Mosquitto Installation

Install Mosquitto:

```bash
sudo apt install -y mosquitto mosquitto-clients
```

Verify:

```bash
mosquitto -h
```

---

# 9. Mosquitto Configuration

The project uses:

```text
MQTT TCP        → ESP32 / EVerest
MQTT WebSocket  → Vue frontend
```

Example Mosquitto development configuration:

```conf
listener 1883
protocol mqtt

listener 9001
protocol websockets

allow_anonymous true
```

Port `1883` is used for standard MQTT communication.

Port `9001` is used by the browser through WebSocket.

> `allow_anonymous true` should only be used in a controlled local development environment.

Restart Mosquitto:

```bash
sudo systemctl restart mosquitto
```

Check:

```bash
sudo systemctl status mosquitto
```

---

# 10. Mosquitto Commands

Start:

```bash
sudo systemctl start mosquitto
```

Stop:

```bash
sudo systemctl stop mosquitto
```

Restart:

```bash
sudo systemctl restart mosquitto
```

Monitor all MQTT messages:

```bash
mosquitto_sub -h localhost -t '#' -v
```

---

# 11. Node.js and npm

Install:

```bash
sudo apt install -y nodejs npm
```

Verify:

```bash
node --version
npm --version
```

---

# 12. Frontend Setup

Enter the frontend directory:

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend
```

Install dependencies:

```bash
npm install
```

If necessary:

```bash
npm install mqtt chart.js vue-chartjs
```

---

# 13. Frontend MQTT Configuration

The MQTT service is located at:

```text
frontend/src/services/mqtt.js
```

The frontend connects using:

```javascript
const MQTT_URL = 'ws://localhost:9001'
```

Main subscribed topics:

```text
everest_api/connector_1/var/session_info
everest_api/connector_1/var/powermeter
esp32/ev/falha
```

---

# 14. MQTT Topics

## Session Information

```text
everest_api/connector_1/var/session_info
```

Example:

```json
{
  "charged_energy_wh": 4339,
  "charging_duration_s": 1468,
  "latest_total_w": 0.0,
  "permanent_fault": false,
  "state": "Unplugged"
}
```

The dashboard uses this information for:

- charging state;
- charging power;
- charged energy;
- charging duration;
- permanent fault status.

---

## Powermeter

```text
everest_api/connector_1/var/powermeter
```

Example:

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

## Vehicle Commands

The dashboard publishes commands to:

```text
esp32/ev/comando
```

Current commands:

```text
CONECTAR
DESCONECTAR
```

---

## Simulated Vehicle Fault

Topic:

```text
esp32/ev/falha
```

Fault active:

```json
{
  "origem": "ESP32",
  "tipo": "falha_veiculo_simulada",
  "ativa": true
}
```

Fault restored:

```json
{
  "origem": "ESP32",
  "tipo": "falha_veiculo_simulada",
  "ativa": false
}
```

The payload field names remain in Portuguese because they are part of the currently implemented MQTT message format.

---

# 15. ESP32 Wi-Fi Configuration

The ESP32 must be connected to the same local network as:

- Mosquitto;
- EVerest;
- the development computer.

The public repository uses placeholders:

```c
#define WIFI_SSID      "Nome_da_rede"
#define WIFI_PASSWORD  "Senha_da_rede"
```

Replace these values locally before compiling.

> Never commit real Wi-Fi passwords, tokens, API keys, or other credentials.

---

# 16. MQTT Broker Address for ESP32

The frontend can use:

```text
localhost
```

The ESP32 must use the computer's network IP.

Find it with:

```bash
hostname -I
```

Example:

```text
192.168.1.40
```

The ESP32 broker URI would then be:

```text
mqtt://192.168.1.40:1883
```

The actual IP address may change depending on the network.

---

# 17. Building the ESP32 Firmware

Enter the project:

```bash
cd ~/Projetos/esp32_everest_mqtt
```

Activate ESP-IDF:

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

Build:

```bash
idf.py build
```

Successful output should include:

```text
Project build complete.
```

---

# 18. Serial Port

Identify the ESP32:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Typical result:

```text
/dev/ttyUSB0
```

If the port is busy:

```bash
lsof /dev/ttyUSB0
```

Close other serial monitors before continuing.

---

# 19. Flashing the ESP32

Flash:

```bash
idf.py -p /dev/ttyUSB0 flash
```

Flash and monitor:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Exit the monitor with:

```text
Ctrl + ]
```

---

# 20. ESP32 Web Server

After connecting to Wi-Fi, the ESP32 displays its IP address in the serial monitor.

Example:

```text
192.168.1.180
```

Open:

```text
http://192.168.1.180
```

The IP may change because of DHCP.

---

# 21. ESP32 Web Interface

The ESP32 web interface displays:

- Wi-Fi status;
- HTTP server status;
- MQTT status;
- EVerest state;
- MQTT simulated fault;
- vehicle simulated fault;
- EVerest power;
- EVerest permanent fault.

It also provides controls for fault injection and recovery.

---

# 22. MQTT Fault Scenario

Available controls:

```text
SIMULAR FALHA MQTT
RESTAURAR MQTT
```

English meaning:

```text
SIMULATE MQTT FAULT
RESTORE MQTT
```

When the fault is activated, only the ESP32 MQTT client is stopped.

Expected behavior:

```text
Wi-Fi       → CONNECTED
HTTP Server → ACTIVE
ESP32 MQTT  → SIMULATED FAULT
```

Mosquitto and EVerest remain active.

---

# 23. Vehicle Fault Scenario

Available controls:

```text
SIMULAR FALHA DO VEÍCULO
RESTAURAR VEÍCULO
```

English meaning:

```text
SIMULATE VEHICLE FAULT
RESTORE VEHICLE
```

When activated, the ESP32 publishes the fault state through:

```text
esp32/ev/falha
```

The Vue dashboard automatically displays the simulated fault.

---

# 24. Simulated Fault vs. EVerest Permanent Fault

The ESP32 simulated fault does not modify:

```text
permanent_fault
```

reported by EVerest.

Therefore:

```text
EVerest permanent fault
          ≠
ESP32 simulated vehicle fault
```

This makes it possible to distinguish between actual EVerest faults and deliberately injected simulation faults.

---

# 25. Running the Vue Frontend

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend
npm run dev
```

Vite normally provides:

```text
http://localhost:5173
```

---

# 26. Dashboard

The current dashboard contains two tabs:

```text
Telemetria
Gráficos
```

English equivalents:

```text
Telemetry
Charts
```

The telemetry view includes:

- status;
- power;
- energy;
- charging time;
- permanent fault;
- ESP32 simulated fault;
- L1/L2/L3 voltage;
- L1/L2/L3 current;
- neutral current;
- L1/L2/L3 frequency;
- meter power;
- phase sequence.

---

# 27. Charts

The dashboard displays:

```text
Power × Time
Energy × Time
```

During charging:

- new points are added;
- power changes in real time;
- energy increases throughout the session.

When the session ends:

- a final zero-power point is added;
- the chart stops receiving new session points;
- repeated `Unplugged` messages do not create duplicate points.

A new charging session clears the previous session chart.

---

# 28. Main EVerest States

Observed states include:

```text
Unplugged
Preparing
Charging
Finished
Faulted
```

Typical sequence:

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

# 29. Charging Controls

The dashboard provides:

```text
CONECTAR
DESCONECTAR
```

English equivalents:

```text
CONNECT
DISCONNECT
```

The buttons depend on:

- MQTT connection;
- EVerest availability;
- current charging state;
- permanent fault state;
- whether another command is in progress.

---

# 30. EVerest Watchdog

The frontend monitors:

```text
everest_api/connector_1/var/session_info
```

If no session information is received for approximately five seconds, EVerest is considered unavailable.

The dashboard then disables charging controls until communication returns.

This prevents the application from considering EVerest online only because the MQTT broker is available.

---

# 31. Running EVerest

EVerest is currently executed inside the configured Docker/SIL environment.

Inside the EVerest environment:

```bash
./run-scripts/run-sil.sh
```

The current configuration uses:

```text
/workspace/config/config-sil.yaml
```

> The exact host-side Docker container creation/start command depends on the local EVerest installation and is not included yet. The command above is the validated command used inside the configured development environment.

---

# 32. Testing EVerest MQTT Messages

Session information:

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

All topics:

```bash
mosquitto_sub -h localhost -t '#' -v
```

---

# 33. Important Mosquitto Behavior

During development, completely stopping Mosquitto was observed to cause EVerest to terminate after losing its MQTT connection.

After restarting Mosquitto, the system may therefore show:

```text
MQTT Broker → ONLINE
EVerest     → OFFLINE
```

In this case, EVerest must also be restarted:

```bash
./run-scripts/run-sil.sh
```

The ESP32 MQTT fault simulation intentionally stops only the ESP32 MQTT client and does not stop the broker.

---

# 34. Recommended Startup Order

```text
1. Mosquitto
      ↓
2. EVerest
      ↓
3. ESP32
      ↓
4. Vue frontend
      ↓
5. Dashboard
      ↓
6. ESP32 web interface
```

---

# 35. Terminal 1 — Mosquitto

```bash
sudo systemctl status mosquitto
```

If necessary:

```bash
sudo systemctl start mosquitto
```

---

# 36. Terminal 2 — EVerest

Inside the configured EVerest Docker/SIL environment:

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

Rebuild, flash, and monitor:

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

Open:

```text
http://localhost:5173
```

---

# 39. Optional MQTT Monitor

```bash
mosquitto_sub -h localhost -t '#' -v
```

This is useful for debugging communication between components.

---

# 40. Complete System Test

## Test 1 — Communication

Confirm:

```text
MQTT connected
EVerest online
```

## Test 2 — Telemetry

Confirm updates for:

```text
voltage
current
frequency
power
energy
state
charging time
```

## Test 3 — Charging

Connect the simulated vehicle.

Expected sequence:

```text
Unplugged
    ↓
Preparing
    ↓
Charging
```

During charging:

- power increases;
- current increases;
- energy increases;
- charging time advances;
- charts update.

## Test 4 — Disconnect

Disconnect the vehicle.

Expected behavior:

- power returns to zero;
- current returns to zero;
- final energy remains available;
- the graph records the end of the session.

## Test 5 — MQTT Fault

Open:

```text
http://IP_OF_ESP32
```

Activate:

```text
SIMULAR FALHA MQTT
```

Expected:

```text
ESP32 MQTT → SIMULATED FAULT
Wi-Fi      → CONNECTED
HTTP       → ACTIVE
```

Restore using:

```text
RESTAURAR MQTT
```

## Test 6 — Vehicle Fault

Activate:

```text
SIMULAR FALHA DO VEÍCULO
```

The Vue dashboard should display:

```text
⚠ FAULT ACTIVE
```

Restore with:

```text
RESTAURAR VEÍCULO
```

The dashboard should return to:

```text
NORMAL
```

---

# 41. Troubleshooting

## `idf.py: command not found`

```bash
source /home/junior/.espressif/v5.5/esp-idf/export.sh
```

## Wrong ESP-IDF Version

```bash
idf.py --version
```

Expected:

```text
ESP-IDF v5.5
```

## ESP32 Serial Port Not Found

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

## Serial Port Busy

```bash
lsof /dev/ttyUSB0
```

## MQTT Disconnected

```bash
sudo systemctl status mosquitto
```

Also verify that WebSocket port `9001` is configured.

## MQTT Online but EVerest Offline

Restart EVerest:

```bash
./run-scripts/run-sil.sh
```

## ESP32 Fault Not Reaching Dashboard

```bash
mosquitto_sub \
-h localhost \
-t 'esp32/ev/falha' \
-v
```

## Powermeter Data Missing

```bash
mosquitto_sub \
-h localhost \
-t 'everest_api/connector_1/var/powermeter' \
-v
```

## Session Information Missing

```bash
mosquitto_sub \
-h localhost \
-t 'everest_api/connector_1/var/session_info' \
-v
```

---

# 42. Voltage While Disconnected

The powermeter may continue reporting approximately:

```text
230 V
```

when the vehicle is disconnected.

This is expected because the powermeter is measuring the available supply voltage.

Without charging:

```text
Current ≈ 0 A
Power   ≈ 0 W
```

while voltage may remain present.

---

# 43. Current Development Status

Monitoring path:

```text
EVerest
   ↓
Mosquitto
   ↓
Vue Dashboard
```

ESP32 path:

```text
EVerest
   ↓
Mosquitto
   ↓
ESP32
   ↓
ESP32 Web Server
```

Fault simulation path:

```text
ESP32 Web Interface
        ↓
Simulated Vehicle Fault
        ↓
ESP32
        ↓
MQTT
        ↓
Vue Dashboard
        ↓
⚠ FAULT ACTIVE
```

---

# 44. Future Work

Possible future improvements include:

- additional fault scenarios;
- configurable vehicle parameters;
- Digital Twin implementation;
- battery State of Charge (SOC);
- battery capacity;
- battery temperature;
- charging profiles;
- persistent charging session storage;
- charging history;
- event and log storage;
- report generation;
- data export;
- experimental validation;
- IEC 61851 state simulation;
- Control Pilot integration;
- dedicated PCB integration, depending on the final project scope.

---

# 45. Academic Context

This project is part of the development of an experimental platform for studying Electric Vehicle charging systems.

The current architecture combines:

```text
EVerest → EV charging platform
ESP32   → simulation and communication
MQTT    → message exchange
Vue.js  → monitoring and visualization
```

The platform allows charging behavior, telemetry, communication, and fault scenarios to be studied without initially requiring a real electric vehicle.

The project can later evolve toward a more complete Electric Vehicle Digital Twin and experimental charging platform.

---

# 46. Quick Start

## Start Mosquitto

```bash
sudo systemctl start mosquitto
```

## Start EVerest

Inside the configured EVerest environment:

```bash
./run-scripts/run-sil.sh
```

## Start ESP32

```bash
cd ~/Projetos/esp32_everest_mqtt

source /home/junior/.espressif/v5.5/esp-idf/export.sh

idf.py -p /dev/ttyUSB0 monitor
```

If rebuilding is necessary:

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Start Frontend

```bash
cd ~/Projetos/esp32_everest_mqtt/frontend
npm run dev
```

## Interfaces

Vue dashboard:

```text
http://localhost:5173
```

ESP32:

```text
http://IP_OF_ESP32
```

---

# 47. Security Notes

Do not commit:

- Wi-Fi passwords;
- API keys;
- access tokens;
- SSH private keys;
- personal credentials;
- `.env` files containing secrets.

Use placeholders such as:

```c
#define WIFI_SSID      "Nome_da_rede"
#define WIFI_PASSWORD  "Senha_da_rede"
```

---

# 48. License

No license has been defined for this project yet.

Until a license is added, standard copyright rules apply.
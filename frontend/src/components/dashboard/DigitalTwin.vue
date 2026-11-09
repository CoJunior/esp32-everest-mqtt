<script setup>
import { computed } from 'vue'

const props = defineProps({
  telemetria: {
    type: Object,
    required: true
  },
  controlPilot: {
    type: Object,
    required: true
  },
  mqttConectado: {
    type: Boolean,
    default: false
  },
  everestDisponivel: {
    type: Boolean,
    default: false
  },
  falhaVeiculoSimulada: {
    type: Boolean,
    default: false
  }
})

const carregando = computed(() =>
  props.telemetria.status === 'Charging'
)

const falhaAtiva = computed(() =>
  props.falhaVeiculoSimulada ||
  props.telemetria.falhaPermanente
)

const conectado = computed(() =>
  carregando.value ||
  ['B1', 'B2', 'C1', 'C2', 'D1', 'D2'].includes(
    props.controlPilot.estado
  )
)

const estadoVisual = computed(() => {
  if (!props.mqttConectado || !props.everestDisponivel) {
    return 'Comunicação indisponível'
  }

  if (falhaAtiva.value) {
    return 'Falha detectada'
  }

  if (carregando.value) {
    return 'Carregando'
  }

  if (conectado.value) {
    return 'Veículo conectado'
  }

  return 'Veículo desconectado'
})
</script>

<template>
  <section class="digital-twin">

    <header class="dt-header">
      <div>
        <span class="dt-eyebrow">GÊMEO DIGITAL · TEMPO REAL</span>
        <h1>Digital Twin do Sistema de Carregamento</h1>
        <p>
          Representação visual da integração ESP32,
          EVerest e MQTT.
        </p>
      </div>

      <span class="dt-status" :class="{ active: carregando }">
        {{ estadoVisual }}
      </span>
    </header>

    <div class="dt-scene">

      <div class="dt-device">
        <div class="dt-icon">⚡</div>
        <h2>Estação EVSE</h2>
        <p>EVerest</p>
        <strong>
          {{ everestDisponivel ? 'ONLINE' : 'OFFLINE' }}
        </strong>
      </div>

      <div
        class="dt-connection"
        :class="{
          charging: carregando,
          connected: conectado,
          fault: falhaAtiva
        }"
      >
        <span class="dt-flow-label">
          {{ carregando ? 'ENERGIA EM FLUXO' : 'SEM FLUXO DE ENERGIA' }}
        </span>

        <div class="dt-cable">
          <span class="dt-energy-dot"></span>
        </div>

        <span class="dt-power">
          {{ telemetria.potencia }} kW
        </span>
      </div>

      <div class="dt-device">
        <div class="dt-icon">🚘</div>
        <h2>Veículo elétrico</h2>
        <p>Estado IEC: {{ controlPilot.estado }}</p>
        <strong>{{ estadoVisual }}</strong>
      </div>

    </div>

    <div v-if="falhaAtiva" class="dt-alert">
      Atenção: falha detectada no sistema de carregamento.
    </div>

    <div class="dt-metrics">
      <article>
        <span>Potência</span>
        <strong>{{ telemetria.potencia }} kW</strong>
      </article>

      <article>
        <span>Energia acumulada</span>
        <strong>{{ telemetria.energia }} Wh</strong>
      </article>

      <article>
        <span>Tempo da sessão</span>
        <strong>{{ telemetria.tempo }}</strong>
      </article>

      <article>
        <span>Control Pilot</span>
        <strong>{{ controlPilot.estado }}</strong>
      </article>

      <article>
        <span>PWM Duty Cycle</span>
        <strong>{{ controlPilot.pwmDuty }}%</strong>
      </article>

      <article>
        <span>Limite PWM</span>
        <strong>{{ controlPilot.correnteMaxima }} A</strong>
      </article>
    </div>

  </section>
</template>

<style scoped>
.digital-twin {
  padding: 28px;
  color: #f2f8ff;
}

.dt-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 20px;
  margin-bottom: 28px;
}

.dt-eyebrow {
  color: #2ee6a6;
  font-size: 11px;
  letter-spacing: 2px;
}

.dt-header h1 {
  margin: 10px 0;
  font-size: 27px;
}

.dt-header p,
.dt-device p {
  color: #8da9c8;
}

.dt-status {
  padding: 12px 18px;
  background: #223247;
  border-radius: 30px;
  color: #c7d5e5;
}

.dt-status.active {
  color: #2ee6a6;
  background: #123e38;
}

.dt-scene {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 20px;
  padding: 45px 25px;
  background: #14243a;
  border: 1px solid #294563;
  border-radius: 18px;
}

.dt-device {
  flex: 1;
  text-align: center;
  padding: 30px 15px;
  background: #0e1d31;
  border: 1px solid #294563;
  border-radius: 16px;
}

.dt-icon {
  font-size: 52px;
  margin-bottom: 15px;
}

.dt-device h2 {
  font-size: 19px;
}

.dt-device strong {
  color: #2ee6a6;
  font-size: 13px;
}

.dt-connection {
  flex: 1;
  text-align: center;
}

.dt-flow-label {
  font-size: 11px;
  color: #8da9c8;
}

.dt-cable {
  height: 8px;
  margin: 24px 0;
  background: #45566a;
  border-radius: 10px;
  position: relative;
  overflow: hidden;
}

.dt-connection.connected .dt-cable {
  background: #276e61;
}

.dt-connection.charging .dt-cable {
  background: #155e4d;
}

.dt-energy-dot {
  display: none;
  position: absolute;
  width: 25px;
  height: 8px;
  background: #2ee6a6;
  box-shadow: 0 0 15px #2ee6a6;
  border-radius: 10px;
}

.dt-connection.charging .dt-energy-dot {
  display: block;
  animation: energy-flow 1.5s linear infinite;
}

.dt-connection.fault .dt-cable {
  background: #ef4444;
}

.dt-power {
  color: #2ee6a6;
  font-size: 22px;
  font-weight: bold;
}

.dt-alert {
  margin-top: 20px;
  padding: 18px;
  background: #48212b;
  border: 1px solid #ef4444;
  border-radius: 12px;
  color: #ffb4b4;
}

.dt-metrics {
  margin-top: 24px;
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 16px;
}

.dt-metrics article {
  background: #14243a;
  border: 1px solid #294563;
  border-radius: 14px;
  padding: 22px;
}

.dt-metrics span {
  display: block;
  color: #8da9c8;
  font-size: 12px;
  margin-bottom: 12px;
}

.dt-metrics strong {
  font-size: 22px;
}

@keyframes energy-flow {
  from { left: -25px; }
  to { left: 100%; }
}

@media (max-width: 900px) {
  .dt-scene {
    flex-direction: column;
  }

  .dt-device,
  .dt-connection {
    width: 100%;
    box-sizing: border-box;
  }

  .dt-connection {
    padding: 20px 0;
  }

  .dt-metrics {
    grid-template-columns: repeat(2, 1fr);
  }

  .dt-header {
    flex-direction: column;
    align-items: flex-start;
  }
}

@media (max-width: 500px) {
  .dt-metrics {
    grid-template-columns: 1fr;
  }
}
</style>

<style>
.ev-app-layout .digital-twin .dt-header h1 {
  color: #f2f8ff;
  opacity: 1;
  text-shadow: none;
}
</style>

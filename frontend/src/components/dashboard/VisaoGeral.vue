<script setup>
defineProps({
  telemetria: {
    type: Object,
    required: true
  },
  controlPilot: {
    type: Object,
    required: true
  },
  eletrica: {
    type: Object,
    required: true
  },
  statusApresentacao: {
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
  },
  podeConectar: {
    type: Boolean,
    default: false
  },
  podeDesconectar: {
    type: Boolean,
    default: false
  },
  comandoEmAndamento: {
    type: Boolean,
    default: false
  },
  comandoEnviado: {
    type: String,
    default: ''
  },
  mensagemControle: {
    type: String,
    default: ''
  }
})

const emit = defineEmits(['conectar', 'desconectar'])
</script>

<template>
  <div class="ev-overview">
    <div class="ev-overview-heading">
      <div>
        <span class="ev-eyebrow">PAINEL DE MONITORAMENTO</span>
        <h1>Visão Geral</h1>
        <p>Monitoramento em tempo real do sistema de carregamento.</p>
      </div>

      <div class="ev-connection-status">
        <span :class="mqttConectado ? 'online' : 'offline'">
          <i></i>
          MQTT {{ mqttConectado ? 'conectado' : 'desconectado' }}
        </span>

        <span :class="everestDisponivel ? 'online' : 'offline'">
          <i></i>
          EVerest {{ everestDisponivel ? 'online' : 'offline' }}
        </span>
      </div>
    </div>

    <section class="ev-control-section">
      <div class="ev-section-heading">
        <div>
          <span class="ev-eyebrow">ESTAÇÃO DE CARREGAMENTO</span>
          <h2>Controle do carregamento</h2>
        </div>

        <span class="ev-state-badge" :class="statusApresentacao.classe">
          {{ statusApresentacao.texto }}
        </span>
      </div>

      <div class="ev-control-actions">
        <button
          type="button"
          class="ev-action-connect"
          :disabled="!podeConectar"
          @click="emit('conectar')"
        >
          {{
            comandoEmAndamento && comandoEnviado === 'CONECTAR'
              ? 'CONECTANDO...'
              : 'CONECTAR VEÍCULO'
          }}
        </button>

        <button
          type="button"
          class="ev-action-disconnect"
          :disabled="!podeDesconectar"
          @click="emit('desconectar')"
        >
          {{
            comandoEmAndamento && comandoEnviado === 'DESCONECTAR'
              ? 'DESCONECTANDO...'
              : 'DESCONECTAR'
          }}
        </button>
      </div>

      <p v-if="mensagemControle" class="ev-control-message">
        {{ mensagemControle }}
      </p>
    </section>

    <section class="ev-metrics-grid">
      <article class="ev-metric-card">
        <span>POTÊNCIA ATUAL</span>
        <strong>{{ telemetria.potencia }} <small>kW</small></strong>
        <p>Potência de carregamento</p>
      </article>

      <article class="ev-metric-card">
        <span>ENERGIA ACUMULADA</span>
        <strong>{{ telemetria.energia }} <small>Wh</small></strong>
        <p>Energia da sessão</p>
      </article>

      <article class="ev-metric-card">
        <span>TEMPO DE CARREGAMENTO</span>
        <strong>{{ telemetria.tempo }}</strong>
        <p>Duração da sessão</p>
      </article>

      <article class="ev-metric-card">
        <span>ESTADO DA SESSÃO</span>
        <strong class="ev-metric-state">
          {{ statusApresentacao.texto }}
        </strong>
        <p>Estado reportado pelo EVerest</p>
      </article>
    </section>

    <div class="ev-overview-columns">
      <section class="ev-detail-panel">
        <div class="ev-section-heading">
          <h2>Control Pilot</h2>
          <span class="ev-panel-tag">IEC 61851</span>
        </div>

        <div class="ev-detail-row">
          <span>Estado IEC</span>
          <strong>{{ controlPilot.estado }}</strong>
        </div>

        <div class="ev-detail-row">
          <span>Estado EVerest</span>
          <strong>{{ telemetria.status }}</strong>
        </div>

        <div class="ev-detail-row">
          <span>PWM Duty Cycle</span>
          <strong>{{ Number(controlPilot.pwmDuty).toFixed(1) }} %</strong>
        </div>

        <div class="ev-detail-row">
          <span>Limite PWM</span>
          <strong>{{ Number(controlPilot.correnteMaxima).toFixed(1) }} A</strong>
        </div>

        <div class="ev-detail-row">
          <span>Corrente medida máxima</span>
          <strong>
            {{
              Math.max(
                eletrica.correnteL1,
                eletrica.correnteL2,
                eletrica.correnteL3
              ).toFixed(2)
            }} A
          </strong>
        </div>
      </section>

      <section class="ev-detail-panel">
        <div class="ev-section-heading">
          <h2>Diagnóstico do sistema</h2>
          <span class="ev-panel-tag">TEMPO REAL</span>
        </div>

        <div class="ev-detail-row">
          <span>Comunicação MQTT</span>
          <strong :class="mqttConectado ? 'ev-ok' : 'ev-alert'">
            {{ mqttConectado ? 'Conectado' : 'Desconectado' }}
          </strong>
        </div>

        <div class="ev-detail-row">
          <span>EVerest</span>
          <strong :class="everestDisponivel ? 'ev-ok' : 'ev-alert'">
            {{ everestDisponivel ? 'Disponível' : 'Indisponível' }}
          </strong>
        </div>

        <div class="ev-detail-row">
          <span>Falha permanente</span>
          <strong :class="telemetria.falhaPermanente ? 'ev-alert' : 'ev-ok'">
            {{ telemetria.falhaPermanente ? 'ATIVA' : 'NORMAL' }}
          </strong>
        </div>

        <div class="ev-detail-row">
          <span>Falha simulada ESP32</span>
          <strong :class="falhaVeiculoSimulada ? 'ev-alert' : 'ev-ok'">
            {{ falhaVeiculoSimulada ? 'ATIVA' : 'NORMAL' }}
          </strong>
        </div>

        <div class="ev-detail-row">
          <span>Sequência de fases</span>
          <strong :class="eletrica.erroSequenciaFases ? 'ev-alert' : 'ev-ok'">
            {{ eletrica.erroSequenciaFases ? 'ERRO' : 'OK' }}
          </strong>
        </div>
      </section>
    </div>
  </div>
</template>

<style scoped>
.ev-overview {
  display: flex;
  flex-direction: column;
  gap: 24px;
  color: #eaf2ff;
}

.ev-overview-heading,
.ev-section-heading {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 20px;
}

.ev-overview-heading h1 {
  margin: 7px 0;
  font-size: 30px;
  color: #ffffff;
}

.ev-overview-heading p,
.ev-metric-card p {
  color: #8fa4bb;
  font-size: 13px;
}

.ev-eyebrow {
  color: #5fe0b3;
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 1.7px;
}

.ev-connection-status {
  display: flex;
  gap: 10px;
  flex-wrap: wrap;
}

.ev-connection-status span {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 13px;
  border-radius: 20px;
  background: #14263a;
  font-size: 12px;
}

.ev-connection-status i {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: currentColor;
}

.online,
.ev-ok {
  color: #5fe0a7;
}

.offline,
.ev-alert {
  color: #ff818b;
}

.ev-control-section,
.ev-detail-panel,
.ev-metric-card {
  background: #142236;
  border: 1px solid #253a50;
  border-radius: 16px;
}

.ev-control-section {
  padding: 24px;
}

.ev-section-heading h2 {
  color: #ffffff;
  font-size: 18px;
  margin: 7px 0 0;
}

.ev-state-badge,
.ev-panel-tag {
  padding: 8px 12px;
  border-radius: 20px;
  background: #1d3447;
  font-size: 11px;
  font-weight: 700;
}

.ev-control-actions {
  display: flex;
  justify-content: flex-start;
  gap: 12px;
  margin-top: 22px;
  flex-wrap: wrap;
}

.ev-control-actions button {
  padding: 13px 20px;
  border: none;
  border-radius: 9px;
  font-weight: 700;
  font-size: 12px;
  cursor: pointer;
}

.ev-action-connect {
  background: #4fe0a0;
  color: #09251e;
}

.ev-action-disconnect {
  background: #34485d;
  color: #ffffff;
}

.ev-control-actions button:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.ev-control-message {
  margin-top: 14px;
  color: #a9bdd0;
  font-size: 12px;
}

.ev-metrics-grid {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 16px;
}

.ev-metric-card {
  padding: 23px;
}

.ev-metric-card > span {
  color: #91a8c0;
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 1px;
}

.ev-metric-card strong {
  display: block;
  margin: 18px 0 10px;
  font-size: 27px;
  color: #ffffff;
}

.ev-metric-card small {
  color: #8fa4bb;
  font-size: 13px;
}

.ev-metric-card .ev-metric-state {
  font-size: 19px;
}

.ev-overview-columns {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 16px;
}

.ev-detail-panel {
  padding: 24px;
}

.ev-detail-row {
  display: flex;
  justify-content: space-between;
  gap: 12px;
  padding: 15px 0;
  border-bottom: 1px solid #26394d;
  font-size: 13px;
}

.ev-detail-row:last-child {
  border-bottom: none;
}

.ev-detail-row > span {
  color: #9bb0c6;
}

.ev-detail-row strong {
  color: #f0f6ff;
  text-align: right;
}

.ev-detail-row strong.ev-ok {
  color: #5fe0a7;
}

.ev-detail-row strong.ev-alert {
  color: #ff818b;
}

@media (max-width: 1150px) {
  .ev-metrics-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (max-width: 700px) {
  .ev-overview-heading,
  .ev-section-heading {
    align-items: flex-start;
    flex-direction: column;
  }

  .ev-metrics-grid,
  .ev-overview-columns {
    grid-template-columns: 1fr;
  }

  .ev-overview-heading h1 {
    font-size: 25px;
  }
}
</style>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue'

const emit = defineEmits(['status-alterado'])

defineProps({
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
  potencia: {
    type: [String, Number],
    default: '0.000'
  },
  falhaPermanente: {
    type: Boolean,
    default: false
  }
})

const statusConsultado = ref(false)
const statusESP32 = ref(null)
let ultimoStatusRegistrado = null
let intervaloStatus = null

async function consultarStatusESP32() {
  try {
    const resposta = await fetch('/api/esp32/api/status', {
      method: 'GET',
      cache: 'no-store',
      signal: AbortSignal.timeout(5000)
    })

    if (!resposta.ok) {
      throw new Error(`HTTP ${resposta.status}`)
    }

    const dados = await resposta.json()

    if (
      typeof dados.mqtt_conectado !== 'boolean' ||
      typeof dados.falha_mqtt_simulada !== 'boolean' ||
      typeof dados.falha_veiculo_simulada !== 'boolean' ||
      typeof dados.estado_everest !== 'string'
    ) {
      throw new Error('Resposta de status inválida')
    }

    const estadoAtual = {
      falha_mqtt_simulada: dados.falha_mqtt_simulada,
      falha_veiculo_simulada: dados.falha_veiculo_simulada
    }

    if (ultimoStatusRegistrado !== null) {
      for (const campo of Object.keys(estadoAtual)) {
        if (estadoAtual[campo] !== ultimoStatusRegistrado[campo]) {
          emit('status-alterado', {
            campo,
            ativo: estadoAtual[campo]
          })
        }
      }
    }

    ultimoStatusRegistrado = estadoAtual

    statusESP32.value = dados
    httpDisponivel.value = true
    statusConsultado.value = true
  } catch (erro) {
    httpDisponivel.value = false
    statusESP32.value = null
    statusConsultado.value = true
    console.warn('Não foi possível consultar o status do ESP32:', erro)
  }
}

onMounted(() => {
  consultarStatusESP32()

  intervaloStatus = setInterval(() => {
    consultarStatusESP32()
  }, 3000)
})

onUnmounted(() => {
  if (intervaloStatus !== null) {
    clearInterval(intervaloStatus)
  }
})

const httpDisponivel = ref(false)
const comandoEmExecucao = ref(false)
const mensagemOperacao = ref('')

async function executarCenario(caminho, descricao) {
  if (comandoEmExecucao.value) return

  const confirmado = window.confirm(
    `Deseja realmente executar: ${descricao}?`
  )

  if (!confirmado) return

  comandoEmExecucao.value = true
  mensagemOperacao.value = `Executando: ${descricao}...`

  try {
    const resposta = await fetch(
      `/api/esp32${caminho}`,
      {
        method: 'GET',
        signal: AbortSignal.timeout(8000)
      }
    )

    if (!resposta.ok) {
      throw new Error(`HTTP ${resposta.status}`)
    }

    httpDisponivel.value = true
    mensagemOperacao.value = `${descricao}: solicitação enviada ao ESP32.`
  } catch (erro) {
    console.error('Erro na comunicação com ESP32:', erro)

    httpDisponivel.value = false
    mensagemOperacao.value =
      `Não foi possível executar ${descricao}: ${erro.message}`
  } finally {
    comandoEmExecucao.value = false
  }
}
</script>

<template>
  <div class="esp32-page">
    <header class="esp32-header">
      <div>
        <span class="esp32-eyebrow">INTERFACE DE SIMULAÇÃO</span>
        <h1>ESP32 + EVerest</h1>
        <p>Monitoramento e simulação de cenários do sistema de carregamento.</p>
      </div>

      <span class="esp32-header-badge">
        PROJETO TCC · UFSC
      </span>
    </header>

    <section class="esp32-panel">
      <h2>⚡ Status da Conexão e Sistema</h2>

      <div class="esp32-status-grid">
        <div class="esp32-status-card">
          <span>Comunicação MQTT do dashboard</span>
          <strong :class="mqttConectado ? 'ok' : 'erro'">
            {{ mqttConectado ? 'CONECTADO' : 'DESCONECTADO' }}
          </strong>
        </div>

        <div class="esp32-status-card">
          <span>Servidor HTTP do ESP32</span>
          <strong :class="httpDisponivel ? 'ok' : 'erro'">
            {{ httpDisponivel ? 'ONLINE' : (statusConsultado ? 'INDISPONÍVEL' : 'VERIFICANDO') }}
          </strong>
        </div>

        <div class="esp32-status-card">
          <span>Estado EVerest</span>
          <strong :class="everestDisponivel ? 'ok' : 'erro'">
            {{ everestDisponivel ? 'ONLINE' : 'OFFLINE' }}
          </strong>
        </div>

        <div class="esp32-status-card">
          <span>Falha simulada do veículo</span>
          <strong :class="statusESP32?.falha_veiculo_simulada ? 'erro' : 'ok'">
            {{ !httpDisponivel ? 'SEM DADOS' : (statusESP32.falha_veiculo_simulada ? 'FALHA ATIVA' : 'NORMAL') }}
          </strong>
        </div>

        <div class="esp32-status-card">
          <span>MQTT do ESP32</span>
          <strong :class="httpDisponivel && statusESP32?.mqtt_conectado && !statusESP32?.falha_mqtt_simulada ? 'ok' : 'erro'">
            {{ !httpDisponivel
              ? 'SEM DADOS'
              : statusESP32.falha_mqtt_simulada
                ? 'FALHA SIMULADA'
                : statusESP32.mqtt_conectado
                  ? 'CONECTADO'
                  : 'DESCONECTADO' }}
          </strong>
        </div>

        <div class="esp32-status-card">
          <span>Estado EVerest recebido pelo ESP32</span>
          <strong :class="httpDisponivel ? 'ok' : 'erro'">
            {{ httpDisponivel ? statusESP32.estado_everest : 'SEM DADOS' }}
          </strong>
        </div>

        <div class="esp32-status-card">
          <span>Potência EVerest</span>
          <strong>{{ potencia }} kW</strong>
        </div>

        <div class="esp32-status-card">
          <span>Falha permanente EVerest</span>
          <strong :class="falhaPermanente ? 'erro' : 'ok'">
            {{ falhaPermanente ? 'SIM' : 'NÃO' }}
          </strong>
        </div>
      </div>
    </section>

    <div class="esp32-scenarios">
      <section class="esp32-panel">
        <div class="esp32-scenario-title">
          <span class="esp32-scenario-icon">◉</span>
          <div>
            <h2>Cenário MQTT</h2>
            <p>Simule falhas na comunicação MQTT do ESP32.</p>
          </div>
        </div>

        <div class="esp32-actions">
          <button type="button" class="esp32-btn esp32-btn-danger" :disabled="comandoEmExecucao" @click="executarCenario('/mqtt/falha', 'Simular falha MQTT')">
            SIMULAR FALHA MQTT
            <span>›</span>
          </button>

          <button type="button" class="esp32-btn esp32-btn-success" :disabled="comandoEmExecucao" @click="executarCenario('/mqtt/restaurar', 'Restaurar MQTT')">
            RESTAURAR MQTT
            <span>›</span>
          </button>
        </div>

        <div class="esp32-info">
          A falha MQTT interrompe somente a comunicação do ESP32.
          O broker Mosquitto e o EVerest permanecem em execução.
        </div>
      </section>

      <section class="esp32-panel">
        <div class="esp32-scenario-title">
          <span class="esp32-scenario-icon">⚙</span>
          <div>
            <h2>Cenário do Veículo</h2>
            <p>Simule falhas no veículo por meio do ESP32.</p>
          </div>
        </div>

        <div class="esp32-actions">
          <button type="button" class="esp32-btn esp32-btn-danger" :disabled="comandoEmExecucao" @click="executarCenario('/veiculo/falha', 'Simular falha do veículo')">
            SIMULAR FALHA DO VEÍCULO
            <span>›</span>
          </button>

          <button type="button" class="esp32-btn esp32-btn-success" :disabled="comandoEmExecucao" @click="executarCenario('/veiculo/restaurar', 'Restaurar veículo')">
            RESTAURAR VEÍCULO
            <span>›</span>
          </button>
        </div>

        <div class="esp32-info">
          Esta é uma falha injetada pelo simulador ESP32.
          Ela é independente da falha permanente real informada pelo EVerest.
        </div>
      </section>
    </div>
  </div>
</template>

<style scoped>
.esp32-page {
  color: #eaf4ff;
}

.esp32-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 20px;
  margin-bottom: 26px;
}

.esp32-eyebrow {
  color: #35e4b0;
  font-size: 11px;
  font-weight: 800;
  letter-spacing: 2px;
}

.esp32-header h1 {
  margin: 8px 0;
  font-size: clamp(26px, 3vw, 36px);
}

.esp32-header p,
.esp32-scenario-title p {
  color: #9cb8d4;
  margin: 0;
}

.esp32-header-badge {
  border: 1px solid #235476;
  background: #0d2740;
  border-radius: 30px;
  padding: 11px 17px;
  color: #65d9f8;
  font-size: 11px;
  font-weight: 700;
  white-space: nowrap;
}

.esp32-panel {
  background: linear-gradient(145deg, #10263d, #091b30);
  border: 1px solid #24415d;
  border-radius: 18px;
  padding: 26px;
  margin-bottom: 22px;
  box-shadow: 0 14px 30px rgba(0, 0, 0, 0.15);
}

.esp32-panel h2 {
  color: #f2f8ff;
  font-size: 20px;
  margin: 0 0 22px;
}

.esp32-status-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 13px;
}

.esp32-status-card {
  display: flex;
  flex-direction: column;
  gap: 13px;
  padding: 19px;
  background: #0b2036;
  border: 1px solid #24415d;
  border-radius: 12px;
}

.esp32-status-card span {
  color: #a2bdd7;
  font-size: 12px;
}

.esp32-status-card strong {
  font-size: 17px;
  overflow-wrap: anywhere;
}

.esp32-status-card .ok {
  color: #27e99b;
}

.esp32-status-card .erro {
  color: #ff6179;
}

.esp32-scenarios {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 22px;
}

.esp32-scenario-title {
  display: flex;
  align-items: flex-start;
  gap: 15px;
}

.esp32-scenario-title h2 {
  margin-bottom: 7px;
}

.esp32-scenario-icon {
  font-size: 31px;
  color: #49bbff;
}

.esp32-actions {
  display: grid;
  gap: 12px;
  margin-top: 25px;
}

.esp32-btn {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  border: 0;
  border-radius: 11px;
  padding: 17px 20px;
  color: #ffffff;
  font-weight: 800;
  text-align: left;
}

.esp32-btn span {
  font-size: 26px;
}

.esp32-btn-danger {
  background: linear-gradient(90deg, #c71f48, #e43843);
}

.esp32-btn-success {
  background: linear-gradient(90deg, #087a4e, #0ca65d);
}

.esp32-btn:disabled {
  opacity: 0.65;
  cursor: not-allowed;
}

.esp32-info {
  margin-top: 17px;
  padding: 17px;
  border: 1px solid #24415d;
  background: #0b2036;
  border-radius: 11px;
  color: #b2cbe5;
  line-height: 1.6;
  font-size: 13px;
}

@media (max-width: 800px) {
  .esp32-status-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }

  .esp32-scenarios {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 650px) {
  .esp32-status-grid {
    grid-template-columns: 1fr;
  }

  .esp32-header {
    flex-direction: column;
    align-items: flex-start;
  }

  .esp32-panel {
    padding: 19px;
  }
}
</style>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue'

const estacao = ref(null)
const conector = ref(null)
const transacoes = ref([])
const carregando = ref(true)
const erro = ref('')
const ultimaAtualizacao = ref('')

let intervalo = null

async function carregarDados() {
  try {
    erro.value = ''

    const respostas = await Promise.all([
      fetch('/api/ocpp/estacao'),
      fetch('/api/ocpp/conector'),
      fetch('/api/ocpp/transacoes')
    ])

    if (respostas.some(resposta => !resposta.ok)) {
      throw new Error('Falha ao consultar a API OCPP')
    }

    const [dadosEstacao, dadosConector, dadosTransacoes] =
      await Promise.all(respostas.map(resposta => resposta.json()))

    estacao.value = dadosEstacao
    conector.value = dadosConector
    transacoes.value = dadosTransacoes.transacoes || []

    ultimaAtualizacao.value = new Date().toLocaleTimeString('pt-BR')
  } catch (e) {
    erro.value = e.message
  } finally {
    carregando.value = false
  }
}

function formatarData(valor) {
  if (!valor) return 'Em andamento'

  const data = new Date(valor)

  return Number.isNaN(data.getTime())
    ? 'Data indisponível'
    : data.toLocaleString('pt-BR')
}


// Calcula a energia consumida a partir das leituras OCPP em Wh
function calcularEnergiaKWh(transacao) {
  if (transacao.stop_value == null) {
    return 'Em andamento'
  }

  if (transacao.start_value == null) {
    return '---'
  }

  const inicial = Number(transacao.start_value)
  const final = Number(transacao.stop_value)

  if (!Number.isFinite(inicial) || !Number.isFinite(final)) {
    return '---'
  }

  const diferenca = final - inicial

  if (diferenca < 0) {
    return '---'
  }

  return (diferenca / 1000).toLocaleString('pt-BR', {
    minimumFractionDigits: 3,
    maximumFractionDigits: 3
  })
}

onMounted(() => {
  carregarDados()
  intervalo = setInterval(carregarDados, 10000)
})

onUnmounted(() => {
  if (intervalo) clearInterval(intervalo)
})
</script>

<template>
  <main class="ocpp-page">
    <header class="ocpp-header">
      <div>
        <span class="ocpp-eyebrow">PROTOCOLO OCPP 1.6J</span>
        <h1>OCPP / SteVe</h1>
        <p>Monitoramento da estação de carregamento e histórico de transações.</p>
      </div>

      <span class="ocpp-badge">Estação {{ estacao?.charge_box_id || '---' }}</span>
    </header>

    <p v-if="carregando" class="ocpp-message">
      Carregando dados do SteVe...
    </p>

    <p v-if="erro" class="ocpp-error">
      Erro de comunicação: {{ erro }}
    </p>

    <section class="ocpp-grid">
      <article class="ocpp-card">
        <h2>Estação de carregamento</h2>

        <div class="ocpp-info">
          <span>ChargeBox ID</span>
          <strong>{{ estacao?.charge_box_id || '---' }}</strong>
        </div>

        <div class="ocpp-info">
          <span>Protocolo</span>
          <strong>{{ estacao?.ocpp_protocol || '---' }}</strong>
        </div>

        <div class="ocpp-info">
          <span>Fabricante</span>
          <strong>{{ estacao?.charge_point_vendor || '---' }}</strong>
        </div>

        <div class="ocpp-info">
          <span>Modelo</span>
          <strong>{{ estacao?.charge_point_model || '---' }}</strong>
        </div>

        <div class="ocpp-info">
          <span>Firmware</span>
          <strong>{{ estacao?.fw_version || '---' }}</strong>
        </div>

        <div class="ocpp-info">
          <span>Cadastro</span>
          <strong>{{ estacao?.registration_status || '---' }}</strong>
        </div>
      </article>

      <article class="ocpp-card">
        <h2>Estado do conector</h2>

        <div class="ocpp-info">
          <span>Conector</span>
          <strong>{{ conector?.connector_id ?? '---' }}</strong>
        </div>

        <div class="ocpp-info">
          <span>Último estado registrado</span>
          <strong class="ocpp-highlight">
            {{ conector?.status || '---' }}
          </strong>
        </div>

        <div class="ocpp-info">
          <span>Código de erro</span>
          <strong>{{ conector?.error_code || '---' }}</strong>
        </div>

        <div class="ocpp-info">
          <span>Último registro</span>
          <strong>{{ formatarData(conector?.status_timestamp) }}</strong>
        </div>

        <div class="ocpp-info">
          <span>Último heartbeat</span>
          <strong>{{ formatarData(estacao?.last_heartbeat_timestamp) }}</strong>
        </div>

        <p class="ocpp-note">
          Os estados são registros do SteVe e não garantem conexão em tempo real.
        </p>
      </article>
    </section>

    <section class="ocpp-card ocpp-history">
      <div class="ocpp-history-header">
        <h2>Histórico de transações</h2>
        <span>{{ transacoes.length }} registros</span>
      </div>

      <div class="ocpp-table-wrapper">
        <table>
          <thead>
            <tr>
              <th>ID</th>
              <th>Conector</th>
              <th>Início</th>
              <th>Fim</th>
              <th>Leitura inicial</th>
              <th>Leitura final</th>
              <th>Energia (kWh)</th>
              <th>Motivo de encerramento</th>
            </tr>
          </thead>

          <tbody>
            <tr v-for="transacao in transacoes" :key="transacao.transaction_id">
              <td>#{{ transacao.transaction_id }}</td>
              <td>{{ transacao.connector_id }}</td>
              <td>{{ formatarData(transacao.start_timestamp) }}</td>
              <td>{{ formatarData(transacao.stop_timestamp) }}</td>
              <td>{{ transacao.start_value ?? '---' }}</td>
              <td>{{ transacao.stop_value ?? '---' }}</td>
              <td>{{ calcularEnergiaKWh(transacao) }}</td>
              <td>{{ transacao.stop_reason || 'Em andamento' }}</td>
            </tr>

            <tr v-if="!transacoes.length">
              <td colspan="8">Nenhuma transação disponível.</td>
            </tr>
          </tbody>
        </table>
      </div>

      <p class="ocpp-note">
        Leituras originais em Wh. Energia consumida calculada pela diferença entre as leituras final e inicial.
      </p>
    </section>

    <footer class="ocpp-footer">
      Última consulta à API: {{ ultimaAtualizacao || '---' }}
    </footer>
  </main>
</template>

<style scoped>
.ocpp-page {
  padding: 34px;
  color: #f2f8ff;
  max-width: 1500px;
  margin: 0 auto;
}

.ocpp-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 20px;
  margin-bottom: 28px;
}

.ocpp-eyebrow {
  color: #2ee6a6;
  font-size: 11px;
  letter-spacing: 2px;
  font-weight: 700;
}

.ocpp-header h1 {
  font-size: 30px;
  margin: 8px 0;
}

.ocpp-header p,
.ocpp-note,
.ocpp-footer {
  color: #8eb4d9;
}

.ocpp-badge {
  background: #103c38;
  color: #2ee6a6;
  border: 1px solid #176b5b;
  padding: 12px 18px;
  border-radius: 30px;
  white-space: nowrap;
}

.ocpp-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 20px;
  margin-bottom: 20px;
}

.ocpp-card {
  background: #14263e;
  border: 1px solid #294867;
  border-radius: 16px;
  padding: 24px;
  min-width: 0;
}

.ocpp-card h2 {
  font-size: 19px;
  margin: 0 0 22px;
}

.ocpp-info {
  display: flex;
  justify-content: space-between;
  gap: 16px;
  padding: 12px 0;
  border-bottom: 1px solid #28415d;
}

.ocpp-info span {
  color: #8eb4d9;
}

.ocpp-info strong {
  text-align: right;
  overflow-wrap: anywhere;
}

.ocpp-highlight {
  color: #2ee6a6;
}

.ocpp-note {
  font-size: 12px;
  line-height: 1.6;
  margin-top: 18px;
}

.ocpp-history-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 15px;
}

.ocpp-history-header span {
  color: #2ee6a6;
}

.ocpp-table-wrapper {
  overflow-x: auto;
}

.ocpp-history table {
  width: 100%;
  border-collapse: collapse;
  font-size: 13px;
}

.ocpp-history th,
.ocpp-history td {
  text-align: left;
  padding: 14px 12px;
  border-bottom: 1px solid #28415d;
  white-space: nowrap;
}

.ocpp-history th {
  color: #8eb4d9;
  font-weight: 600;
}

.ocpp-history tbody tr:hover {
  background: #1b344f;
}

.ocpp-error {
  color: #ff7d88;
  background: #47202b;
  padding: 14px;
  border-radius: 10px;
}

.ocpp-message {
  color: #8eb4d9;
}

.ocpp-footer {
  margin-top: 18px;
  text-align: right;
  font-size: 12px;
}

@media (max-width: 900px) {
  .ocpp-grid {
    grid-template-columns: 1fr;
  }

  .ocpp-page {
    padding: 20px;
  }

  .ocpp-header {
    flex-direction: column;
    align-items: flex-start;
  }
}
</style>

<style>
.ev-app-layout .ocpp-page .ocpp-header h1 {
  color: #f2f8ff;
  opacity: 1;
  text-shadow: none;
}
</style>

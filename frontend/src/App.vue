<script setup>
import {
  ref,
  onMounted,
  onUnmounted,
  computed
} from 'vue'

import { Line } from 'vue-chartjs'
import mqttClient from './services/mqtt'

import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
} from 'chart.js'

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
)

/* ============================================================
 * CONFIGURAÇÕES
 * ============================================================ */

const TOPICO_SESSION_INFO =
  'everest_api/connector_1/var/session_info'

const TOPICO_POWERMETER =
  'everest_api/connector_1/var/powermeter'

const TOPICO_COMANDO =
  'esp32/ev/comando'

const TOPICO_FALHA_ESP32 =
  'esp32/ev/falha'

const LIMITE_PONTOS = 60
const TIMEOUT_EVEREST_MS = 5000

/* ============================================================
 * ESTADO GERAL
 * ============================================================ */

const abaAtiva = ref('telemetria')

const mqttConectado = ref(false)
const everestDisponivel = ref(false)

let watchdogEverest = null

/* ============================================================
 * TELEMETRIA DA SESSÃO
 * session_info
 * ============================================================ */

const telemetria = ref({
  status: 'Aguardando EVerest...',
  potencia: '0.000',
  energia: 0,
  tempo: '00:00',
  falhaPermanente: false
})

/* ============================================================
 * FALHA SIMULADA PELO ESP32
 * ============================================================ */

const falhaVeiculoSimulada = ref(false)

/* ============================================================
 * GRANDEZAS ELÉTRICAS
 * powermeter
 * ============================================================ */

const eletrica = ref({
  meterId: '---',
  timestamp: '---',

  potenciaTotalW: 0,

  tensaoL1: 0,
  tensaoL2: 0,
  tensaoL3: 0,

  correnteL1: 0,
  correnteL2: 0,
  correnteL3: 0,
  correnteN: 0,

  frequenciaL1: 0,
  frequenciaL2: 0,
  frequenciaL3: 0,

  energiaImportada: 0,

  erroSequenciaFases: false
})

/* ============================================================
 * CONTROLE
 * ============================================================ */

const comandoEnviado = ref('')
const comandoEmAndamento = ref(false)
const mensagemControle = ref('')

/* ============================================================
 * HISTÓRICO DOS GRÁFICOS
 * ============================================================ */

const tempos = ref([])
const potencias = ref([])
const energias = ref([])

/*
 * Controle da sessão exibida nos gráficos.
 * O histórico é registrado somente durante
 * uma sessão ativa de carregamento.
 */
let sessaoGraficoAtiva = false
let ultimoTempoGrafico = null

const dadosPotencia = ref({
  labels: [],
  datasets: [
    {
      label: 'Potência (kW)',
      data: [],
      borderWidth: 2,
      tension: 0.3
    }
  ]
})

const dadosEnergia = ref({
  labels: [],
  datasets: [
    {
      label: 'Energia (Wh)',
      data: [],
      borderWidth: 2,
      tension: 0.3
    }
  ]
})

const opcoesGrafico = {
  responsive: true,
  maintainAspectRatio: false,

  animation: {
    duration: 300
  }
}

/* ============================================================
 * STATUS
 * ============================================================ */

const statusApresentacao = computed(() => {
  if (!everestDisponivel.value) {
    return {
      texto: 'Aguardando EVerest...',
      classe: 'status-default'
    }
  }

  if (telemetria.value.falhaPermanente) {
    return {
      texto: 'Falha permanente',
      classe: 'status-fault'
    }
  }

  switch (telemetria.value.status) {
    case 'Charging':
      return {
        texto: 'Carregando',
        classe: 'status-charging'
      }

    case 'Preparing':
      return {
        texto: 'Preparando',
        classe: 'status-preparing'
      }

    case 'Unplugged':
      return {
        texto: 'Desconectado',
        classe: 'status-unplugged'
      }

    case 'Finished':
      return {
        texto: 'Finalizado',
        classe: 'status-finished'
      }

    case 'Faulted':
      return {
        texto: 'Falha',
        classe: 'status-fault'
      }

    default:
      return {
        texto: telemetria.value.status,
        classe: 'status-default'
      }
  }
})

/* ============================================================
 * CONTROLE DOS BOTÕES
 * ============================================================ */

const podeConectar = computed(() => {
  return (
    mqttConectado.value &&
    everestDisponivel.value &&
    telemetria.value.status === 'Unplugged' &&
    !telemetria.value.falhaPermanente &&
    !comandoEmAndamento.value
  )
})

const podeDesconectar = computed(() => {
  return (
    mqttConectado.value &&
    everestDisponivel.value &&
    (
      telemetria.value.status === 'Preparing' ||
      telemetria.value.status === 'Charging' ||
      telemetria.value.status === 'Finished'
    ) &&
    !comandoEmAndamento.value
  )
})

/* ============================================================
 * FORMATAÇÃO
 * ============================================================ */

function formatarTempo(segundos) {
  const total = Number(segundos) || 0

  const minutos = Math.floor(total / 60)
  const segundosRestantes = Math.floor(total % 60)

  return `${String(minutos).padStart(2, '0')}:${String(
    segundosRestantes
  ).padStart(2, '0')}`
}

function numero(valor) {
  return Number(valor) || 0
}

/* ============================================================
 * WATCHDOG EVEREST
 * ============================================================ */

function reiniciarWatchdogEverest() {
  everestDisponivel.value = true

  if (watchdogEverest !== null) {
    clearTimeout(watchdogEverest)
  }

  watchdogEverest = setTimeout(() => {
    everestDisponivel.value = false
    comandoEmAndamento.value = false

    console.warn(
      'Watchdog: EVerest sem telemetria por mais de 5 segundos'
    )
  }, TIMEOUT_EVEREST_MS)
}

function pararWatchdogEverest() {
  if (watchdogEverest !== null) {
    clearTimeout(watchdogEverest)
    watchdogEverest = null
  }

  everestDisponivel.value = false
}

/* ============================================================
 * GRÁFICOS POR SESSÃO
 * ============================================================ */

function reconstruirGraficos() {
  dadosPotencia.value = {
    labels: [...tempos.value],

    datasets: [
      {
        label: 'Potência (kW)',
        data: [...potencias.value],
        borderWidth: 2,
        tension: 0.3
      }
    ]
  }

  dadosEnergia.value = {
    labels: [...tempos.value],

    datasets: [
      {
        label: 'Energia (Wh)',
        data: [...energias.value],
        borderWidth: 2,
        tension: 0.3
      }
    ]
  }
}

function limparGraficos() {
  tempos.value = []
  potencias.value = []
  energias.value = []

  ultimoTempoGrafico = null

  reconstruirGraficos()

  console.log(
    'Gráficos limpos para nova sessão'
  )
}

function registrarPontoGrafico(
  tempo,
  potencia,
  energia
) {
  /*
   * O EVerest pode publicar mais de uma atualização
   * dentro do mesmo segundo.
   *
   * Nesse caso atualizamos o último ponto
   * em vez de criar labels repetidas.
   */
  if (
    ultimoTempoGrafico === tempo &&
    tempos.value.length > 0
  ) {
    const ultimoIndice =
      tempos.value.length - 1

    potencias.value[ultimoIndice] =
      potencia

    energias.value[ultimoIndice] =
      energia

    reconstruirGraficos()

    return
  }

  tempos.value.push(`${tempo}s`)
  potencias.value.push(potencia)
  energias.value.push(energia)

  ultimoTempoGrafico = tempo

  if (
    tempos.value.length >
    LIMITE_PONTOS
  ) {
    tempos.value.shift()
    potencias.value.shift()
    energias.value.shift()
  }

  reconstruirGraficos()
}

function registrarPontoFinal(
  tempo,
  energia
) {
  /*
   * Registra somente um ponto final
   * com potência igual a zero.
   *
   * Depois disso o gráfico fica congelado
   * até o início de uma nova sessão.
   */

  tempos.value.push(
    `${tempo}s (fim)`
  )

  potencias.value.push(0)
  energias.value.push(energia)

  if (
    tempos.value.length >
    LIMITE_PONTOS
  ) {
    tempos.value.shift()
    potencias.value.shift()
    energias.value.shift()
  }

  reconstruirGraficos()

  console.log(
    'Gráfico congelado no fim da sessão:',
    {
      tempo,
      energia
    }
  )
}

function atualizarGraficosDaSessao(
  status,
  tempo,
  potencia,
  energia
) {
  const carregando =
    status === 'Preparing' ||
    status === 'Charging'

  /*
   * ==========================================================
   * INÍCIO DE UMA NOVA SESSÃO
   * ==========================================================
   *
   * Se o primeiro estado recebido for Preparing ou
   * diretamente Charging, limpamos o histórico anterior.
   */

  if (
    carregando &&
    !sessaoGraficoAtiva
  ) {
    limparGraficos()

    sessaoGraficoAtiva = true

    console.log(
      'Nova sessão iniciada nos gráficos'
    )
  }

  /*
   * ==========================================================
   * SESSÃO EM ANDAMENTO
   * ==========================================================
   */

  if (
    carregando &&
    sessaoGraficoAtiva
  ) {
    registrarPontoGrafico(
      tempo,
      potencia,
      energia
    )

    return
  }

  /*
   * ==========================================================
   * FIM DA SESSÃO
   * ==========================================================
   *
   * Finished ou Unplugged encerram a coleta.
   *
   * Como sessaoGraficoAtiva passa para false,
   * mensagens posteriores de Unplugged não
   * acrescentam novos pontos.
   */

  if (
    sessaoGraficoAtiva &&
    (
      status === 'Finished' ||
      status === 'Unplugged'
    )
  ) {
    registrarPontoFinal(
      tempo,
      energia
    )

    sessaoGraficoAtiva = false
    ultimoTempoGrafico = null
  }
}

/* ============================================================
 * COMANDOS MQTT
 * ============================================================ */

function enviarComando(comando) {
  if (!mqttConectado.value) {
    console.warn(
      'Comando não enviado: MQTT desconectado'
    )

    return
  }

  if (!everestDisponivel.value) {
    console.warn(
      'Comando não enviado: EVerest indisponível'
    )

    return
  }

  if (comandoEmAndamento.value) {
    return
  }

  comandoEmAndamento.value = true

  if (comando === 'CONECTAR') {
    mensagemControle.value =
      'Solicitando conexão...'
  } else {
    mensagemControle.value =
      'Solicitando desconexão...'
  }

  mqttClient.publish(
    TOPICO_COMANDO,
    comando,
    {
      qos: 1
    },
    (error) => {
      if (error) {
        console.error(
          'Erro ao enviar comando:',
          error
        )

        mensagemControle.value =
          'Erro ao enviar comando'

        comandoEmAndamento.value = false

        return
      }

      comandoEnviado.value = comando

      console.log(
        `Comando enviado para ${TOPICO_COMANDO}:`,
        comando
      )
    }
  )
}

function conectar() {
  if (!podeConectar.value) {
    return
  }

  enviarComando('CONECTAR')
}

function desconectar() {
  if (!podeDesconectar.value) {
    return
  }

  enviarComando('DESCONECTAR')
}

/* ============================================================
 * ESTADO DO COMANDO
 * ============================================================ */

function atualizarEstadoDoComando(status) {
  if (
    comandoEnviado.value === 'CONECTAR' &&
    (
      status === 'Preparing' ||
      status === 'Charging'
    )
  ) {
    comandoEmAndamento.value = false

    if (status === 'Preparing') {
      mensagemControle.value =
        'Preparando carregamento...'
    }

    if (status === 'Charging') {
      mensagemControle.value =
        'Veículo carregando'
    }
  }

  if (
    comandoEnviado.value === 'DESCONECTAR' &&
    (
      status === 'Finished' ||
      status === 'Unplugged'
    )
  ) {
    comandoEmAndamento.value = false

    if (status === 'Finished') {
      mensagemControle.value =
        'Finalizando carregamento...'
    }

    if (status === 'Unplugged') {
      mensagemControle.value =
        'Veículo desconectado'
    }
  }
}

/* ============================================================
 * PROCESSAR SESSION_INFO
 * ============================================================ */

function processarSessionInfo(dados) {
  reiniciarWatchdogEverest()

  const status =
    dados.state ?? 'Desconhecido'

  const potenciaW =
    numero(dados.latest_total_w)

  const potenciaKW =
    potenciaW / 1000

  const energiaWH =
    numero(dados.charged_energy_wh)

  const tempoS =
    numero(dados.charging_duration_s)

  const falhaPermanente =
    Boolean(dados.permanent_fault)

  telemetria.value = {
    status,
    potencia: potenciaKW.toFixed(3),
    energia: energiaWH,
    tempo: formatarTempo(tempoS),
    falhaPermanente
  }

  atualizarEstadoDoComando(status)

  atualizarGraficosDaSessao(
    status,
    tempoS,
    potenciaKW,
    energiaWH
  )

  console.log(
    'session_info → Dashboard:',
    {
      status,
      potenciaW,
      energiaWH,
      tempoS,
      falhaPermanente
    }
  )
}

/* ============================================================
 * PROCESSAR POWERMETER
 * ============================================================ */

function processarPowermeter(dados) {
  const voltage =
    dados.voltage_V ?? {}

  const current =
    dados.current_A ?? {}

  const frequency =
    dados.frequency_Hz ?? {}

  const power =
    dados.power_W ?? {}

  const energyImport =
    dados.energy_Wh_import ?? {}

  eletrica.value = {
    meterId:
      dados.meter_id ?? '---',

    timestamp:
      dados.timestamp ?? '---',

    potenciaTotalW:
      numero(power.total),

    tensaoL1:
      numero(voltage.L1),

    tensaoL2:
      numero(voltage.L2),

    tensaoL3:
      numero(voltage.L3),

    correnteL1:
      numero(current.L1),

    correnteL2:
      numero(current.L2),

    correnteL3:
      numero(current.L3),

    correnteN:
      numero(current.N),

    frequenciaL1:
      numero(frequency.L1),

    frequenciaL2:
      numero(frequency.L2),

    frequenciaL3:
      numero(frequency.L3),

    energiaImportada:
      numero(energyImport.total),

    erroSequenciaFases:
      Boolean(dados.phase_seq_error)
  }

  console.log(
    'powermeter → Dashboard:',
    eletrica.value
  )
}

/* ============================================================
 * PROCESSAR FALHA SIMULADA DO ESP32
 * ============================================================ */

function processarFalhaESP32(dados) {
  falhaVeiculoSimulada.value =
    Boolean(dados.ativa)

  console.log(
    'falha ESP32 → Dashboard:',
    {
      origem: dados.origem ?? 'ESP32',
      tipo: dados.tipo ?? 'falha_veiculo_simulada',
      ativa: falhaVeiculoSimulada.value
    }
  )
}

/* ============================================================
 * RECEBER MQTT
 * ============================================================ */

function receberMensagemMQTT(
  topic,
  message
) {
  try {
    const dados =
      JSON.parse(
        message.toString()
      )

    if (
      topic ===
      TOPICO_SESSION_INFO
    ) {
      processarSessionInfo(dados)
      return
    }

    if (
      topic ===
      TOPICO_POWERMETER
    ) {
      processarPowermeter(dados)
      return
    }

    if (
      topic ===
      TOPICO_FALHA_ESP32
    ) {
      processarFalhaESP32(dados)
    }
  } catch (error) {
    console.error(
      `Erro ao interpretar mensagem MQTT do tópico ${topic}:`,
      error
    )
  }
}

/* ============================================================
 * MQTT
 * ============================================================ */

function aoConectarMQTT() {
  mqttConectado.value = true

  console.log(
    'Dashboard detectou MQTT conectado'
  )
}

function aoDesconectarMQTT() {
  mqttConectado.value = false
  comandoEmAndamento.value = false

  pararWatchdogEverest()

  console.log(
    'Dashboard detectou MQTT desconectado'
  )
}

/* ============================================================
 * MONTAGEM
 * ============================================================ */

onMounted(() => {
  mqttConectado.value =
    mqttClient.connected

  mqttClient.on(
    'connect',
    aoConectarMQTT
  )

  mqttClient.on(
    'close',
    aoDesconectarMQTT
  )

  mqttClient.on(
    'message',
    receberMensagemMQTT
  )
})

/* ============================================================
 * DESMONTAGEM
 * ============================================================ */

onUnmounted(() => {
  pararWatchdogEverest()

  mqttClient.off(
    'connect',
    aoConectarMQTT
  )

  mqttClient.off(
    'close',
    aoDesconectarMQTT
  )

  mqttClient.off(
    'message',
    receberMensagemMQTT
  )
})
</script>

<template>
  <div class="dashboard">

    <header>
      <h1>
        Sistema de Monitoramento EVerest
      </h1>

      <p>
        Telemetria do processo de carregamento
      </p>

      <div
        class="mqtt-status"
        :class="
          mqttConectado
            ? 'mqtt-online'
            : 'mqtt-offline'
        "
      >
        <span class="mqtt-dot"></span>

        {{
          mqttConectado
            ? 'MQTT conectado'
            : 'MQTT desconectado'
        }}
      </div>

      <div
        class="mqtt-status"
        :class="
          everestDisponivel
            ? 'mqtt-online'
            : 'everest-waiting'
        "
      >
        <span class="mqtt-dot"></span>

        {{
          everestDisponivel
            ? 'EVerest online'
            : 'Aguardando EVerest'
        }}
      </div>
    </header>

    <nav class="tabs">
      <button
        :class="{
          active: abaAtiva === 'telemetria'
        }"
        @click="abaAtiva = 'telemetria'"
      >
        Telemetria
      </button>

      <button
        :class="{
          active: abaAtiva === 'graficos'
        }"
        @click="abaAtiva = 'graficos'"
      >
        Gráficos
      </button>
    </nav>

    <template
      v-if="abaAtiva === 'telemetria'"
    >

      <!-- =========================
           SESSÃO
           ========================= -->

      <main class="telemetry-grid">

        <div class="card">
          <span class="label">
            Status
          </span>

          <strong
            class="status-value"
            :class="
              statusApresentacao.classe
            "
          >
            <span
              class="status-dot"
            ></span>

            {{
              statusApresentacao.texto
            }}
          </strong>
        </div>

        <div class="card">
          <span class="label">
            Potência
          </span>

          <strong>
            {{ telemetria.potencia }} kW
          </strong>
        </div>

        <div class="card">
          <span class="label">
            Energia
          </span>

          <strong>
            {{ telemetria.energia }} Wh
          </strong>
        </div>

        <div class="card">
          <span class="label">
            Tempo
          </span>

          <strong>
            {{ telemetria.tempo }}
          </strong>
        </div>

        <div
          class="card fault-card"
        >
          <span class="label">
            Falha permanente
          </span>

          <strong>
            {{
              telemetria.falhaPermanente
                ? 'SIM'
                : 'NÃO'
            }}
          </strong>
        </div>

        <div class="card">
          <span class="label">
            Falha simulada ESP32
          </span>

          <strong
            :style="{
              color: falhaVeiculoSimulada
                ? '#c62828'
                : '#16813b'
            }"
          >
            {{
              falhaVeiculoSimulada
                ? '⚠ FALHA ATIVA'
                : 'NORMAL'
            }}
          </strong>
        </div>

      </main>

      <section
        v-if="falhaVeiculoSimulada"
        class="control-panel"
        style="border: 2px solid #c62828;"
      >
        <h2 style="color: #c62828;">
          ⚠ Falha simulada do veículo
        </h2>

        <p class="command-status">
          O ESP32 informou uma falha injetada pelo simulador.
          Esta indicação é independente da falha permanente
          reportada pelo EVerest.
        </p>
      </section>

      <!-- =========================
           GRANDEZAS ELÉTRICAS
           ========================= -->

      <section class="control-panel">

        <h2>
          Grandezas elétricas
        </h2>

        <div class="telemetry-grid">

          <div class="card">
            <span class="label">
              Tensão L1
            </span>

            <strong>
              {{ eletrica.tensaoL1.toFixed(2) }} V
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Tensão L2
            </span>

            <strong>
              {{ eletrica.tensaoL2.toFixed(2) }} V
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Tensão L3
            </span>

            <strong>
              {{ eletrica.tensaoL3.toFixed(2) }} V
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Corrente L1
            </span>

            <strong>
              {{ eletrica.correnteL1.toFixed(2) }} A
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Corrente L2
            </span>

            <strong>
              {{ eletrica.correnteL2.toFixed(2) }} A
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Corrente L3
            </span>

            <strong>
              {{ eletrica.correnteL3.toFixed(2) }} A
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Frequência L1
            </span>

            <strong>
              {{ eletrica.frequenciaL1.toFixed(2) }} Hz
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Frequência L2
            </span>

            <strong>
              {{ eletrica.frequenciaL2.toFixed(2) }} Hz
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Frequência L3
            </span>

            <strong>
              {{ eletrica.frequenciaL3.toFixed(2) }} Hz
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Corrente do neutro
            </span>

            <strong>
              {{ eletrica.correnteN.toFixed(2) }} A
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Potência do medidor
            </span>

            <strong>
              {{
                (
                  eletrica.potenciaTotalW /
                  1000
                ).toFixed(3)
              }}
              kW
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Sequência de fases
            </span>

            <strong>
              {{
                eletrica.erroSequenciaFases
                  ? 'ERRO'
                  : 'OK'
              }}
            </strong>
          </div>

        </div>

      </section>

      <!-- =========================
           CONTROLE
           ========================= -->

      <section
        class="control-panel"
      >
        <h2>
          Controle do carregamento
        </h2>

        <div
          class="control-buttons"
        >

          <button
            class="connect-button"
            :disabled="!podeConectar"
            @click="conectar"
          >
            {{
              comandoEmAndamento &&
              comandoEnviado === 'CONECTAR'
                ? 'CONECTANDO...'
                : 'CONECTAR'
            }}
          </button>

          <button
            class="disconnect-button"
            :disabled="!podeDesconectar"
            @click="desconectar"
          >
            {{
              comandoEmAndamento &&
              comandoEnviado === 'DESCONECTAR'
                ? 'DESCONECTANDO...'
                : 'DESCONECTAR'
            }}
          </button>

        </div>

        <p
          v-if="mensagemControle"
          class="command-status"
        >
          {{ mensagemControle }}
        </p>

      </section>

    </template>

    <!-- =========================
         GRÁFICOS
         ========================= -->

    <main
      v-else
      class="charts-container"
    >

      <h2>
        Gráficos de carregamento
      </h2>

      <div class="chart-card">

        <h3>
          Potência × Tempo
        </h3>

        <div
          class="chart-wrapper"
        >
          <Line
            :data="dadosPotencia"
            :options="opcoesGrafico"
          />
        </div>

      </div>

      <div class="chart-card">

        <h3>
          Energia × Tempo
        </h3>

        <div
          class="chart-wrapper"
        >
          <Line
            :data="dadosEnergia"
            :options="opcoesGrafico"
          />
        </div>

      </div>

    </main>

  </div>
</template>
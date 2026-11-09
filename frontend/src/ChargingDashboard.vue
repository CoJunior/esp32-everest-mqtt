<script setup>
import {
  ref,
  onMounted,
  onUnmounted,
  computed
} from 'vue'

import { Line } from 'vue-chartjs'
import mqttClient from './services/mqtt'
import Sidebar from './components/dashboard/Sidebar.vue'
import VisaoGeral from './components/dashboard/VisaoGeral.vue'
import CenariosESP32 from './components/dashboard/CenariosESP32.vue'
import DigitalTwin from './components/dashboard/DigitalTwin.vue'
import OcppSteve from './components/dashboard/OcppSteve.vue'
import './dashboard.css'

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

const TOPICO_CONTROL_PILOT =
  'esp32/ev/control_pilot'

const TOPICO_COMANDO =
  'esp32/ev/comando'

const TOPICO_FALHA_ESP32 =
  'esp32/ev/falha'

const LIMITE_PONTOS = 60
const TIMEOUT_EVEREST_MS = 5000

/* ============================================================
 * ESTADO GERAL
 * ============================================================ */

const abaAtiva = ref('visao-geral')

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
 * CONTROL PILOT
 * Dados recebidos do ESP32
 * ============================================================ */

const controlPilot = ref({
  estado: '---',
  pwmDuty: 0,
  correnteMaxima: 0
})

/* ============================================================
 * HISTÓRICO DE EVENTOS
 * ============================================================ */

const historicoEventos = ref([])

const LIMITE_EVENTOS = 50

// Exportar o histórico de eventos para CSV
function exportarHistoricoCSV() {
  if (historicoEventos.value.length === 0) {
    window.alert('Não há eventos para exportar.')
    return
  }

  const escaparCSV = (valor) => {
    const texto = String(valor ?? '')
    return '"' + texto.replace(/"/g, '""') + '"'
  }

  const linhas = [
    ['Data e horário', 'Origem', 'Tipo', 'Descrição'],
    ...historicoEventos.value.map(evento => [
      evento.horario,
      evento.origem,
      evento.tipo,
      evento.descricao
    ])
  ]

  const conteudo = linhas
    .map(linha => linha.map(escaparCSV).join(';'))
    .join('\r\n')

  const blob = new Blob(
    ['\uFEFF' + conteudo],
    { type: 'text/csv;charset=utf-8;' }
  )

  const url = URL.createObjectURL(blob)
  const link = document.createElement('a')

  const data = new Date()
    .toISOString()
    .slice(0, 10)

  link.href = url
  link.download = `historico_eventos_${data}.csv`

  document.body.appendChild(link)
  link.click()
  link.remove()

  setTimeout(() => URL.revokeObjectURL(url), 1000)
}

const CHAVE_HISTORICO = 'everest_digital_twin_historico_v1'

let ultimoEstadoIEC = null
let ultimoEstadoEverest = null
let ultimaFalhaESP32 = null

// Recuperar os eventos salvos no navegador
function recuperarHistorico() {
  try {
    const salvo = localStorage.getItem(CHAVE_HISTORICO)

    if (!salvo) return

    const eventos = JSON.parse(salvo)

    if (!Array.isArray(eventos)) return

    historicoEventos.value = eventos
      .filter(evento =>
        evento &&
        typeof evento.horario === 'string' &&
        typeof evento.origem === 'string' &&
        typeof evento.tipo === 'string' &&
        typeof evento.descricao === 'string'
      )
      .slice(0, LIMITE_EVENTOS)

    console.log(
      'Histórico recuperado:',
      historicoEventos.value.length,
      'eventos'
    )
  } catch (erro) {
    console.warn('Erro ao recuperar histórico:', erro)
  }
}

// Salvar automaticamente os eventos
function salvarHistorico() {
  try {
    localStorage.setItem(
      CHAVE_HISTORICO,
      JSON.stringify(historicoEventos.value)
    )
  } catch (erro) {
    console.warn('Erro ao salvar histórico:', erro)
  }
}

function registrarEvento(origem, tipo, descricao) {
  const agora = new Date().toLocaleString('pt-BR', {
    day: '2-digit',
    month: '2-digit',
    year: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false
  })

  historicoEventos.value.unshift({
    horario: agora,
    origem,
    tipo,
    descricao
  })

  if (historicoEventos.value.length > LIMITE_EVENTOS) {
    historicoEventos.value.pop()
  }

  salvarHistorico()

  console.log('Evento registrado:', {
    horario: agora,
    origem,
    tipo,
    descricao
  })
}

// Carregar registros anteriores antes dos novos eventos
recuperarHistorico()

// Recuperar o último estado conhecido de cada componente
function recuperarUltimoEstado(origem, tipo, prefixoInicial, prefixoMudanca) {
  const evento = historicoEventos.value.find(item =>
    item.origem === origem &&
    item.tipo === tipo &&
    (
      item.descricao.startsWith(prefixoInicial) ||
      item.descricao.startsWith(prefixoMudanca)
    )
  )

  if (!evento) return null

  if (evento.descricao.startsWith(prefixoInicial)) {
    return evento.descricao.slice(prefixoInicial.length).trim()
  }

  const partes = evento.descricao
    .slice(prefixoMudanca.length)
    .split('→')

  if (partes.length !== 2) return null

  return partes[1].trim()
}

ultimoEstadoEverest = recuperarUltimoEstado(
  'EVerest',
  'Sessão',
  'Estado inicial EVerest: ',
  'Estado EVerest: '
)

ultimoEstadoIEC = recuperarUltimoEstado(
  'ESP32',
  'Control Pilot',
  'Estado inicial IEC: ',
  'Estado IEC: '
)

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
const correntes = ref([])
const limitesCorrente = ref([])
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
      borderColor: '#2EE6A6',
      backgroundColor: '#2EE6A6',
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
      borderColor: '#38BDF8',
      backgroundColor: '#38BDF8',
      borderWidth: 2,
      tension: 0.3
    }
  ]
})

const dadosCorrente = ref({
  labels: [],
  datasets: [
    {
      label: 'Corrente medida (A)',
      data: [],
      borderColor: '#22C55E',
      backgroundColor: '#22C55E',
      borderWidth: 2,
      tension: 0.3
    },
    {
      label: 'Limite PWM (A)',
      data: [],
      borderColor: '#F59E0B',
      backgroundColor: '#F59E0B',
      borderDash: [8, 5],
      borderWidth: 2,
      tension: 0
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
        borderColor: '#2EE6A6',
        backgroundColor: '#2EE6A6',
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
        borderColor: '#38BDF8',
        backgroundColor: '#38BDF8',
        borderWidth: 2,
        tension: 0.3
      }
    ]
  }

  dadosCorrente.value = {
    labels: [...tempos.value],
    datasets: [
      {
        label: 'Corrente medida (A)',
        data: [...correntes.value],
        borderColor: '#22C55E',
        backgroundColor: '#22C55E',
        borderWidth: 2,
        tension: 0.3
      },
      {
        label: 'Limite PWM (A)',
        data: [...limitesCorrente.value],
        borderColor: '#F59E0B',
        backgroundColor: '#F59E0B',
        borderDash: [8, 5],
        borderWidth: 2,
        tension: 0
      }
    ]
  }
}


function limparGraficos() {
  tempos.value = []
  potencias.value = []
  energias.value = []
  correntes.value = []
  limitesCorrente.value = []

  ultimoTempoGrafico = null

  reconstruirGraficos()

  console.log(
    'Gráficos limpos para nova sessão'
  )
}

function registrarPontoGrafico(
  tempo,
  potencia,
  energia,
  corrente
)

{
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

    correntes.value[ultimoIndice] =
      corrente

    limitesCorrente.value[ultimoIndice] =
      controlPilot.value.correnteMaxima

    reconstruirGraficos()

    return
  }

  tempos.value.push(`${tempo}s`)
  potencias.value.push(potencia)
  energias.value.push(energia)
  correntes.value.push(corrente)

  limitesCorrente.value.push(
    controlPilot.value.correnteMaxima
  )


  ultimoTempoGrafico = tempo

  if (
    tempos.value.length >
    LIMITE_PONTOS
  ) {
    tempos.value.shift()
    potencias.value.shift()
    energias.value.shift()
    correntes.value.shift()
    limitesCorrente.value.shift()
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
  correntes.value.push(0)

  limitesCorrente.value.push(
   controlPilot.value.correnteMaxima
)

  if (
    tempos.value.length >
    LIMITE_PONTOS
  ) {
    tempos.value.shift()
    potencias.value.shift()
    energias.value.shift()
    correntes.value.shift()
    limitesCorrente.value.shift()
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
      energia,
    Math.max(
      eletrica.value.correnteL1,
      eletrica.value.correnteL2,
      eletrica.value.correnteL3
     )
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

  if (
  status !==
  ultimoEstadoEverest
) {
  if (ultimoEstadoEverest === null) {
    registrarEvento(
      'EVerest',
      'Sessão',
      `Estado inicial EVerest: ${status}`
    )
  } else {
    registrarEvento(
      'EVerest',
      'Sessão',
      `Estado EVerest: ${ultimoEstadoEverest} → ${status}`
    )
  }

  ultimoEstadoEverest = status
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
 * PROCESSAR CONTROL PILOT
 * ============================================================ */

function processarControlPilot(dados) {
  controlPilot.value = {
    estado:
      dados.estado ?? '---',

    pwmDuty:
      numero(dados.pwm_duty),

    correnteMaxima:
      numero(dados.corrente_maxima_a)
  }

if (
  controlPilot.value.estado !==
  ultimoEstadoIEC
) {
  if (ultimoEstadoIEC === null) {
    registrarEvento(
      'ESP32',
      'Control Pilot',
      `Estado inicial IEC: ${controlPilot.value.estado}`
    )
  } else {
    registrarEvento(
      'ESP32',
      'Control Pilot',
      `Estado IEC: ${ultimoEstadoIEC} → ${controlPilot.value.estado}`
    )
  }

  ultimoEstadoIEC =
    controlPilot.value.estado
}

  console.log(
    'Control Pilot → Dashboard:',
    controlPilot.value
  )
}

/* ============================================================
 * PROCESSAR FALHA SIMULADA DO ESP32
 * ============================================================ */

function registrarMudancaESP32({ campo, ativo }) {
  const mensagens = {
    falha_mqtt_simulada: ativo
      ? 'Falha MQTT simulada — ATIVADA'
      : 'Comunicação MQTT do ESP32 — RESTAURADA',

    falha_veiculo_simulada: ativo
      ? 'Falha do veículo simulada — ATIVADA'
      : 'Falha do veículo simulada — RESTAURADA'
  }

  const descricao = mensagens[campo]

  if (!descricao) return

  registrarEvento(
    'ESP32',
    ativo ? 'Falha' : 'Restauração',
    descricao
  )
}

function processarFalhaESP32(dados) {
  const falhaAtual =
    Boolean(dados.ativa)

  const cenarioAtual =
    dados.cenario ?? 'nenhuma'

  falhaVeiculoSimulada.value =
    falhaAtual

  console.log(
    'falha ESP32 → Dashboard:',
    {
      origem: dados.origem ?? 'ESP32',
      tipo: dados.tipo ?? 'falha_veiculo_simulada',
      cenario: cenarioAtual,
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
      TOPICO_CONTROL_PILOT
    ) {
      processarControlPilot(dados)
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
  <div class="ev-app-layout">
  <Sidebar :pagina-ativa="abaAtiva" @navegar="abaAtiva = $event" />
  <div class="dashboard">

    <VisaoGeral
      v-show="abaAtiva === 'visao-geral'"
      :telemetria="telemetria"
      :control-pilot="controlPilot"
      :eletrica="eletrica"
      :status-apresentacao="statusApresentacao"
      :mqtt-conectado="mqttConectado"
      :everest-disponivel="everestDisponivel"
      :falha-veiculo-simulada="falhaVeiculoSimulada"
      :pode-conectar="podeConectar"
      :pode-desconectar="podeDesconectar"
      :comando-em-andamento="comandoEmAndamento"
      :comando-enviado="comandoEnviado"
      :mensagem-controle="mensagemControle"
      @conectar="conectar"
      @desconectar="desconectar"
    />

    <CenariosESP32
      @status-alterado="registrarMudancaESP32"
      v-show="abaAtiva === 'esp32'"
      :mqtt-conectado="mqttConectado"
      :everest-disponivel="everestDisponivel"
      :falha-veiculo-simulada="falhaVeiculoSimulada"
      :potencia="telemetria.potencia"
      :falha-permanente="telemetria.falhaPermanente"
    />

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


    </template>

    <template v-else-if="abaAtiva === 'control-pilot'">
      <!--=========================
           CONTROL PILOT
           ========================= -->

      <section  class="control-panel">

        <h2>
          Control Pilot
        </h2>

        <div class="telemetry-grid">

          <div class="card">
            <span class="label">
              Estado IEC
            </span>

            <strong>
              {{ controlPilot.estado }}
            </strong>
          </div>

          <div class="card">
            <span class="label">
            Estado EVerest
           </span>

           <strong>
             {{ telemetria.status }}
           </strong>
          </div>

          <div class="card">
            <span class="label">
              PWM Duty Cycle
            </span>

            <strong>
              {{ controlPilot.pwmDuty.toFixed(1) }} %
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Limite PWM
            </span>

            <strong>
              {{ controlPilot.correnteMaxima.toFixed(1) }} A
            </strong>
          </div>

          <div class="card">
            <span class="label">
              Corrente medida
            </span>

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

        </div>

      </section>



    </template>

    <template v-else-if="abaAtiva === 'historico'">
      <!-- =========================
     HISTÓRICO DE EVENTOS
     ========================= -->

<section class="control-panel">

  <h2>
    Histórico de Eventos do Digital Twin
  </h2>

  <button
    type="button"
    @click="exportarHistoricoCSV"
    :disabled="historicoEventos.length === 0"
    style="
      background: #116b57;
      color: white;
      border: none;
      border-radius: 8px;
      padding: 12px 20px;
      margin: 12px 0 22px;
      font-weight: bold;
      cursor: pointer;
    "
  >
    Exportar Histórico (CSV)
  </button>

  <div
    v-if="historicoEventos.length === 0"
    class="card"
  >
    Nenhum evento registrado.
  </div>

  <div
    v-else
    class="historico-eventos"
  >

    <div
      v-for="(evento, index) in historicoEventos"
      :key="index"
      class="card"
    >
      <span class="label">
        {{ evento.horario }}
      </span>

      <strong>
        {{ evento.descricao }}
      </strong>

      <span class="label">
        {{ evento.origem }} · {{ evento.tipo }}
      </span>
    </div>

  </div>

</section>


    </template>


    <!-- =========================
         DIGITAL TWIN
         ========================= -->

    <template v-else-if="abaAtiva === 'digital-twin'">
      <DigitalTwin
        :telemetria="telemetria"
        :control-pilot="controlPilot"
        :mqtt-conectado="mqttConectado"
        :everest-disponivel="everestDisponivel"
        :falha-veiculo-simulada="falhaVeiculoSimulada"
      />
    </template>

    <!-- =========================
         OCPP / STEVE
         ========================= -->

    <template v-else-if="abaAtiva === 'ocpp'">
      <OcppSteve />
    </template>

    <!-- =========================
         GRÁFICOS
         ========================= -->

    <main
      v-else-if="abaAtiva === 'graficos'"
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

          <div class="chart-card">

      <h3>
        Corrente medida × Limite PWM
      </h3>

      <div
        class="chart-wrapper"
      >
        <Line
          :data="dadosCorrente"
          :options="opcoesGrafico"
        />
      </div>

    </div>

    </main>

  </div>
  </div>
</template>
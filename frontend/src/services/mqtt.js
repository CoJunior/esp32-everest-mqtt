import mqtt from 'mqtt'

const MQTT_URL = 'ws://localhost:9001'

const TOPICOS = [
  'everest_api/connector_1/var/session_info',
  'everest_api/connector_1/var/powermeter',
  'esp32/ev/control_pilot',
  'esp32/ev/falha',
  'esp32/ev/status'
]

const client = mqtt.connect(MQTT_URL, {
  reconnectPeriod: 2000,
  connectTimeout: 5000
})

client.on('connect', () => {
  console.log('MQTT conectado via WebSocket')

  client.subscribe(TOPICOS, (error) => {
    if (error) {
      console.error(
        'Erro ao assinar tópicos:',
        error
      )

      return
    }

    console.log('Tópicos MQTT assinados:')

    TOPICOS.forEach((topico) => {
      console.log(`- ${topico}`)
    })
  })
})

client.on('message', (topic, message) => {
  console.log('Mensagem MQTT recebida:')
  console.log('Tópico:', topic)
  console.log('Mensagem:', message.toString())
})

client.on('reconnect', () => {
  console.log(
    'Tentando reconectar ao MQTT...'
  )
})

client.on('close', () => {
  console.log(
    'Conexão MQTT encerrada'
  )
})

client.on('error', (error) => {
  console.error(
    'Erro MQTT:',
    error
  )
})

export default client
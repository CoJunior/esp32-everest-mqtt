<script setup>
defineProps({
  paginaAtiva: {
    type: String,
    default: 'visao-geral'
  }
})

const emit = defineEmits(['navegar'])

const paginas = [
  { id: 'visao-geral', nome: 'Visão Geral', icone: '◈' },
  { id: 'telemetria', nome: 'Telemetria', icone: '▤' },
  { id: 'graficos', nome: 'Gráficos', icone: '▥' },
  { id: 'control-pilot', nome: 'Control Pilot', icone: '⌁' },
  { id: 'digital-twin', nome: 'Digital Twin', icone: '◇' },
  { id: 'historico', nome: 'Histórico de Eventos', icone: '◷' },
  { id: 'ocpp', nome: 'OCPP / SteVe', icone: '⇄' },
  { id: 'esp32', nome: 'Cenários ESP32', icone: '⚙' }
]
</script>

<template>
  <aside class="ev-sidebar">
    <div class="ev-sidebar-brand">
      <div class="ev-brand-icon">ϟ</div>

      <div>
        <strong>EV Charging</strong>
        <span>Monitor</span>
      </div>
    </div>

    <div class="ev-sidebar-caption">
      MONITORAMENTO
    </div>

    <nav class="ev-sidebar-nav" aria-label="Navegação principal">
      <button
        v-for="pagina in paginas"
        :key="pagina.id"
        type="button"
        class="ev-nav-item"
        :class="{ 'ev-nav-active': paginaAtiva === pagina.id }"
        :aria-current="paginaAtiva === pagina.id ? 'page' : undefined"
        @click="emit('navegar', pagina.id)"
      >
        <span class="ev-nav-icon" aria-hidden="true">
          {{ pagina.icone }}
        </span>

        <span>{{ pagina.nome }}</span>
      </button>
    </nav>

    <div class="ev-sidebar-footer">
      <span class="ev-footer-indicator"></span>
      <div>
        <strong>EV Charging System</strong>
        <small>ESP32 · EVerest · MQTT</small>
      </div>
    </div>
  </aside>
</template>

<style scoped>
.ev-sidebar {
  width: 252px;
  min-height: 100vh;
  padding: 28px 15px 20px;
  background: #0b1424;
  border-right: 1px solid #22334a;
  color: #e8f1fb;
  display: flex;
  flex-direction: column;
  gap: 28px;
}

.ev-sidebar-brand {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 0 11px;
}

.ev-brand-icon {
  width: 43px;
  height: 43px;
  display: grid;
  place-items: center;
  border-radius: 12px;
  background: #123d39;
  color: #5cf2b4;
  font-size: 30px;
}

.ev-sidebar-brand strong,
.ev-sidebar-brand span {
  display: block;
}

.ev-sidebar-brand strong {
  font-size: 20px;
}

.ev-sidebar-brand span {
  margin-top: 3px;
  font-size: 11px;
  color: #91a4ba;
}

.ev-sidebar-caption {
  padding: 0 13px;
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 1.7px;
  color: #70869e;
}

.ev-sidebar-nav {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.ev-nav-item {
  width: 100%;
  min-height: 45px;
  padding: 11px 13px;
  border: 1px solid transparent;
  border-radius: 10px;
  background: transparent;
  color: #9aacc1;
  display: flex;
  align-items: center;
  gap: 13px;
  text-align: left;
  font-size: 13px;
  font-weight: 500;
  cursor: pointer;
  transition: background 0.2s, color 0.2s;
}

.ev-nav-item:hover {
  background: #16283e;
  color: #ffffff;
}

.ev-nav-active {
  background: #123d39;
  border-color: #216759;
  color: #67f2b6;
}

.ev-nav-icon {
  width: 22px;
  text-align: center;
  font-size: 19px;
}

.ev-sidebar-footer {
  margin-top: auto;
  padding: 15px 10px 0;
  border-top: 1px solid #22334a;
  display: flex;
  align-items: center;
  gap: 10px;
}

.ev-footer-indicator {
  width: 9px;
  height: 9px;
  border-radius: 50%;
  background: #49d6a2;
}

.ev-sidebar-footer strong,
.ev-sidebar-footer small {
  display: block;
}

.ev-sidebar-footer strong {
  font-size: 11px;
}

.ev-sidebar-footer small {
  margin-top: 5px;
  color: #8196ad;
  font-size: 10px;
}

@media (max-width: 900px) {
  .ev-sidebar {
    width: 100%;
    min-height: auto;
    padding: 16px;
    gap: 15px;
  }

  .ev-sidebar-nav {
    flex-direction: row;
    overflow-x: auto;
    padding-bottom: 5px;
  }

  .ev-nav-item {
    width: auto;
    flex: 0 0 auto;
    white-space: nowrap;
  }

  .ev-sidebar-caption,
  .ev-sidebar-footer {
    display: none;
  }
}
</style>

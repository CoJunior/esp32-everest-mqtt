import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],

  server: {
    proxy: {
      '/api/ocpp': {
        target: 'http://127.0.0.1:3001',
        changeOrigin: true
      },
      '/api/esp32': {
        target: 'http://192.168.1.174',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/api\/esp32/, '')
      }
    }
  }
})

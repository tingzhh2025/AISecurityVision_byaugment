import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import AutoImport from 'unplugin-auto-import/vite'
import Components from 'unplugin-vue-components/vite'
import { ElementPlusResolver } from 'unplugin-vue-components/resolvers'
import { resolve } from 'path'

export default defineConfig({
  plugins: [
    vue(),
    AutoImport({
      resolvers: [ElementPlusResolver()],
      imports: ['vue', 'vue-router', 'pinia']
    }),
    Components({
      resolvers: [ElementPlusResolver()]
    })
  ],
  define: {
    // 设置环境变量绕过代理
    'process.env.NO_PROXY': JSON.stringify('127.0.0.1,localhost')
  },
  resolve: {
    alias: {
      '@': resolve(__dirname, 'src')
    }
  },
  server: {
    host: '0.0.0.0',
    port: 3000,
    proxy: {
      '/api': {
        target: 'http://192.168.1.199:8080',
        changeOrigin: true,
        secure: false,
        configure: (proxy, options) => {
          proxy.on('error', (err, req, res) => {
            console.log('proxy error', err);
          });
          proxy.on('proxyReq', (proxyReq, req, res) => {
            console.log('Sending Request to the Target:', req.method, req.url);
          });
          proxy.on('proxyRes', (proxyRes, req, res) => {
            console.log('Received Response from the Target:', proxyRes.statusCode, req.url);
          });
        }
      },
      '/stream': {
        target: 'http://127.0.0.1:8161',
        changeOrigin: true,
        secure: false
      },
      // MJPEG streams for different cameras - dynamic port routing
      '/mjpeg': {
        target: 'http://192.168.1.199:8161', // Default target
        changeOrigin: true,
        secure: false,
        ws: false, // Disable WebSocket upgrade
        router: (req) => {
          // Extract port from query parameter
          try {
            const url = new URL(req.url, 'http://localhost')
            const port = url.searchParams.get('port')
            if (port && /^\d+$/.test(port)) {
              const target = `http://192.168.1.199:${port}`
              console.log(`[MJPEG Proxy] Routing ${req.url} to ${target}`)
              return target
            }
          } catch (error) {
            console.error('[MJPEG Proxy] URL parsing error:', error)
          }
          console.log(`[MJPEG Proxy] Using default target for ${req.url}`)
          return 'http://192.168.1.199:8161' // Default fallback
        },
        pathRewrite: {
          '^/mjpeg/stream.mjpg': '/stream.mjpg'
        },
        configure: (proxy, options) => {
          proxy.on('error', (err, req, res) => {
            console.log('[MJPEG Proxy] Error:', err.message, 'for', req.url)
          })
          proxy.on('proxyRes', (proxyRes, req, res) => {
            // Handle MJPEG stream headers
            if (proxyRes.headers['content-type'] &&
                proxyRes.headers['content-type'].includes('multipart/x-mixed-replace')) {
              console.log('[MJPEG Proxy] Detected MJPEG stream, preserving headers')
              // Don't buffer the response for streaming
              res.writeHead(proxyRes.statusCode, proxyRes.headers)
              proxyRes.pipe(res, { end: true })
              return false // Prevent default handling
            }
          })
        }
      }
    }
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets'
  }
})

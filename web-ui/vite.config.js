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
        target: 'http://127.0.0.1:8080',
        changeOrigin: true,
        secure: false,
        timeout: 30000, // 30秒超时
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
        target: 'http://127.0.0.1:8161', // Default target
        changeOrigin: true,
        secure: false,
        ws: false, // Disable WebSocket upgrade
        selfHandleResponse: true, // Handle response manually
        router: (req) => {
          // Extract port from query parameter
          try {
            const url = new URL(req.url, 'http://localhost')
            const port = url.searchParams.get('port')
            if (port && /^\d+$/.test(port)) {
              const target = `http://127.0.0.1:${port}`
              console.log(`[MJPEG Proxy] Routing ${req.url} to ${target}`)
              return target
            }
          } catch (error) {
            console.error('[MJPEG Proxy] URL parsing error:', error)
          }
          console.log(`[MJPEG Proxy] Using default target for ${req.url}`)
          return 'http://127.0.0.1:8161' // Default fallback
        },
        pathRewrite: {
          '^/mjpeg/stream.mjpg': '/stream.mjpg'
        },
        configure: (proxy, options) => {
          proxy.on('error', (err, req, res) => {
            console.log('[MJPEG Proxy] Error:', err.message, 'for', req.url)
            if (!res.headersSent) {
              res.writeHead(500, { 'Content-Type': 'text/plain' })
              res.end('MJPEG Proxy Error')
            }
          })
          proxy.on('proxyRes', (proxyRes, req, res) => {
            console.log('[MJPEG Proxy] Response received:', proxyRes.statusCode, req.url)
            // Set CORS headers
            res.setHeader('Access-Control-Allow-Origin', '*')
            res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS')
            res.setHeader('Access-Control-Allow-Headers', 'Content-Type')

            // Copy all headers from the response
            Object.keys(proxyRes.headers).forEach(key => {
              res.setHeader(key, proxyRes.headers[key])
            })

            // Set status code
            res.statusCode = proxyRes.statusCode

            // Pipe the response directly without parsing
            proxyRes.pipe(res, { end: true })
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

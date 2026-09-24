import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { VitePWA } from 'vite-plugin-pwa';

// base: './' -> los assets usan rutas relativas (funciona dentro del exe)
export default defineConfig({
  plugins: [
    react(),
    // ---------- PWA ----------
    // La web se puede INSTALAR en el celular (icono propio, pantalla completa,
    // sin barra del navegador) y queda un shell cacheado para abrir al toque.
    // OJO: la instalacion y el service worker SOLO funcionan en contexto
    // seguro (https o localhost), igual que el Bluetooth y el microfono.
    VitePWA({
      // build nuevo => service worker nuevo (nada de quedarse con la version
      // vieja cacheada: mismo espiritu que el no-cache del index.html)
      registerType: 'autoUpdate',
      injectRegister: 'auto',
      includeAssets: ['fondo-doodle.svg', 'icono.svg', 'apple-touch-icon.png', 'favicon.ico'],
      manifest: {
        name: 'Kira — Cerebro de la feria',
        short_name: 'Kira',
        description:
          'Charlando con Kira, una IA que se cría conversando, con carita de micro:bit.',
        lang: 'es-AR',
        start_url: '/',
        scope: '/',
        display: 'standalone',
        orientation: 'any',
        background_color: '#212121',
        theme_color: '#212121',
        categories: ['education', 'entertainment'],
        icons: [
          { src: 'icono-192.png', sizes: '192x192', type: 'image/png' },
          { src: 'icono-512.png', sizes: '512x512', type: 'image/png' },
          // maskable: full-bleed, el SO le pone su mascara (circulo/squircle)
          { src: 'maskable-512.png', sizes: '512x512', type: 'image/png', purpose: 'maskable' },
        ],
      },
      workbox: {
        globPatterns: ['**/*.{js,css,html,svg,png,ico,webmanifest}'],
        cleanupOutdatedCaches: true,
        // sin red, cualquier ruta cae en el shell de la app
        navigateFallback: '/index.html',
        navigateFallbackDenylist: [/^\/api\//, /^\/grabaciones\//],
        runtimeCaching: [
          {
            // LA API NUNCA SE CACHEA: la IA, la voz y los sensores tienen que
            // ser SIEMPRE en vivo (una respuesta vieja seria una mentira).
            urlPattern: ({ url }: { url: URL }) => url.pathname.startsWith('/api/'),
            handler: 'NetworkOnly',
          },
        ],
      },
      // en `npm run dev` no se mete el service worker (no molestar al desarrollo)
      devOptions: { enabled: false },
    }),
  ],
  base: './',
  server: {
    port: 5173,
    proxy: {
      '/api': 'http://127.0.0.1:8000',
    },
  },
});

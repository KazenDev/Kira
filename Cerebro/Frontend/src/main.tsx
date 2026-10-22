import React from 'react';
import ReactDOM from 'react-dom/client';
import { MotionConfig } from 'motion/react';
import AuthGate from './componentes/AuthGate';
import './estilos.css';

// Deploy nuevo: el service worker nuevo toma el control, pero la pestaña (o
// la PWA instalada) seguia mostrando la version vieja hasta recargar DOS
// veces. Se recarga sola una vez. La primera instalacion no recarga (no
// habia version vieja que reemplazar).
if ('serviceWorker' in navigator) {
  const habiaVersion = Boolean(navigator.serviceWorker.controller);
  let recargando = false;
  navigator.serviceWorker.addEventListener('controllerchange', () => {
    if (!habiaVersion || recargando) return;
    recargando = true;
    window.location.reload();
  });
}

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    {/* reducedMotion="user": Motion (que anima por JS, fuera del alcance del
        media query de CSS) también respeta la preferencia del Sistema ->
        quien pide poco movimiento, ve poca movimiento en TODA la app */}
    <MotionConfig reducedMotion="user">
      <AuthGate />
    </MotionConfig>
  </React.StrictMode>
);

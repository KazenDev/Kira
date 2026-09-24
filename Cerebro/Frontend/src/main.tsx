import React from 'react';
import ReactDOM from 'react-dom/client';
import { MotionConfig } from 'motion/react';
import AuthGate from './AuthGate';
import './estilos.css';

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

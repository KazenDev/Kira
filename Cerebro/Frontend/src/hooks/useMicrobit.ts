import { useEffect, useRef, useState } from 'react';
import { obtenerStatus, type EstadoMicrobit } from '../lib/api';
import { estadoMicrobit, textoMicrobit } from '../lib/ui-utils';
import type { EstadoPuenteBle } from '../lib/ble-protocolo';

const STATUS_POLL_MS = 3000;

/** Estado real del micro:bit (polling) y del puente BLE del navegador. */
export function useMicrobit() {
  const [status, setStatus] = useState<EstadoMicrobit | null>(null);
  const [bleEstado, setBleEstado] = useState<EstadoPuenteBle>('desconectado');
  const conectadoRef = useRef(false);

  useEffect(() => {
    let vivo = true;
    const tick = async () => {
      try {
        const s = await obtenerStatus();
        if (!vivo) return;
        setStatus(s);
        // Para decidir la ruta de voz sin recrear callbacks cada 3 s.
        conectadoRef.current = s.microbit.conectado;
      } catch {
        // Backend caído: se mantiene el último estado.
      }
    };
    tick();
    const id = setInterval(tick, STATUS_POLL_MS);
    return () => {
      vivo = false;
      clearInterval(id);
    };
  }, []);

  const mb = status?.microbit;
  // 'relay' = "placa por BLE": el enlace lo lleva el celular. Es un estado
  // BUENO (punto ámbar con latido), no una falla.
  const mbEstado = estadoMicrobit(mb);
  const mbTexto = textoMicrobit(mb);
  const mbTitulo = mb?.ultimo_ack ? `Último ACK: ${mb.ultimo_ack}` : '';
  const bleOcupado = bleEstado === 'conectando' || bleEstado === 'reconectando';
  const bleEtiqueta =
    bleEstado === 'conectado'
      ? 'Desconectar micro:bit'
      : bleEstado === 'rechazado'
        ? 'Reintentar conexión BLE'
        : bleOcupado
          ? 'Conectando micro:bit…'
          : 'Conectar micro:bit';

  return {
    status,
    bleEstado,
    setBleEstado,
    conectadoRef,
    patronReal: mb?.leds ?? null,
    mb,
    mbEstado,
    mbTexto,
    mbTitulo,
    bleOcupado,
    bleEtiqueta,
  };
}

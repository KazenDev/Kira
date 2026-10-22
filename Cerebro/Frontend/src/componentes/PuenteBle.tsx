/**
 * PuenteBle.tsx - EL CABLE INVISIBLE (modo feria) 🔌📡
 *
 * Sin PC en el stand, el navegador ES el cable entre el backend y la placa.
 * Toda la lógica vive en `usePuenteBle`; este componente solo pinta el botón
 * (o el icono compacto) y dispara conectar/desconectar.
 *
 * El controlador persistente de App se monta con `oculto`: mantiene el enlace
 * durante toda la sesión aunque el botón visible se desmonte.
 */
import { Bluetooth, BluetoothConnected, RefreshCw } from 'lucide-react';
import { usePuenteBle } from '../hooks/usePuenteBle';
import type { EstadoPuenteBle } from '../lib/ble-protocolo';

export type { EstadoPuenteBle };

interface Props {
  compacto?: boolean; // sólo icono
  oculto?: boolean; // controlador persistente sin botón propio
  onEstado?: (estado: EstadoPuenteBle) => void;
}

export default function PuenteBle({ compacto = false, oculto = false, onEstado }: Props) {
  const { estado, conectar, desconectar } = usePuenteBle({ onEstado });

  const caras: Record<EstadoPuenteBle, { icono: typeof Bluetooth; texto: string; clase: string }> = {
    desconectado: { icono: Bluetooth, texto: 'conectar placa', clase: '' },
    conectando: { icono: RefreshCw, texto: 'conectando...', clase: '' },
    conectado: { icono: BluetoothConnected, texto: 'placa por BLE', clase: 'conectado' },
    reconectando: { icono: RefreshCw, texto: 'reconectando...', clase: 'reconectando' },
    rechazado: { icono: Bluetooth, texto: 'puente ocupado', clase: 'rechazado' },
  };
  const cara = caras[estado];
  const Icono = cara.icono;

  if (oculto) return null;

  // MODO COMPACTO: solo el icono, con el estado en el color + tooltip
  // (vive en el header del chat, sin ocupar la zona de mensajes)
  if (compacto) {
    return (
      <button
        className={`icon-btn puente-icono ${cara.clase}`}
        onClick={estado === 'conectado' ? desconectar : conectar}
        disabled={estado === 'conectando' || estado === 'reconectando'}
        title={
          estado === 'conectado'
            ? 'Placa conectada por Bluetooth. Toca para desconectar.'
            : estado === 'rechazado'
              ? 'Otro dispositivo tiene el puente. Toca para intentar de nuevo.'
              : estado === 'reconectando' || estado === 'conectando'
                ? 'Conectando la placa…'
                : 'Conectar el micro:bit por Bluetooth'
        }
        aria-label="Puente Bluetooth con el micro:bit"
      >
        {estado === 'reconectando' || estado === 'conectando' ? (
          <Icono size={17} className="girando" />
        ) : (
          <Icono size={17} />
        )}
      </button>
    );
  }

  return (
    <button
      className={`puente-ble ${cara.clase}`}
      onClick={estado === 'conectado' ? desconectar : conectar}
      disabled={estado === 'conectando' || estado === 'reconectando'}
      title={
        estado === 'conectado'
          ? 'La placa obedece por Bluetooth. Toca para desconectar.'
          : estado === 'rechazado'
            ? 'Otro dispositivo ya es el puente. Toca para intentar de nuevo.'
            : 'Conecta el micro:bit por Bluetooth: la IA lo controla a través de este teléfono.'
      }
    >
      {estado === 'reconectando' || estado === 'conectando' ? (
        <Icono size={15} className="girando" />
      ) : (
        <Icono size={15} />
      )}
      <span>{cara.texto}</span>
    </button>
  );
}

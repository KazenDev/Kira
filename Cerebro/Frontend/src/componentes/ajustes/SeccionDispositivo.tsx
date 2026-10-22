import { Bluetooth } from 'lucide-react';
import type { EstadoMicrobit } from '../../lib/api';
import type { EstadoPuenteBle } from '../../lib/ble-protocolo';

type Props = {
  estado: EstadoMicrobit | null;
  bleEstado: EstadoPuenteBle;
  onAlternarBle: () => void;
};

export function SeccionDispositivo({ estado, bleEstado, onAlternarBle }: Props) {
  const microbit = estado?.microbit;
  const bleConectado = bleEstado === 'conectado' || microbit?.ble_relay === true;
  const bleOcupado = bleEstado === 'conectando' || bleEstado === 'reconectando';

  return (
    <section className="ajustes-seccion">
      <div className="ajustes-titulo">
        <Bluetooth size={16} strokeWidth={2.2} aria-hidden="true" />
        <h3>Dispositivo</h3>
      </div>
      <div className="ajustes-estado-card">
        <div>
          <strong>micro:bit</strong>
          <small>
            {microbit?.respondiendo
              ? `En línea${microbit.puerto ? ` · ${microbit.puerto}` : ''}`
              : microbit?.conectado
                ? `Conectada por USB${microbit.puerto ? ` · ${microbit.puerto}` : ''}`
                : bleConectado
                  ? 'Conectada por el puente Bluetooth del navegador'
                  : 'Sin conexión (ni USB ni puente Bluetooth)'}
          </small>
        </div>
        <span
          className={'estado-punto ' + (microbit?.respondiendo || bleConectado ? 'ok' : 'off')}
          aria-label={microbit?.respondiendo || bleConectado ? 'conectado' : 'desconectado'}
        />
      </div>
      <button type="button" className="ajustes-boton" onClick={onAlternarBle} disabled={bleOcupado}>
        <Bluetooth size={15} aria-hidden="true" />
        {bleConectado
          ? 'Desconectar puente Bluetooth'
          : bleOcupado
            ? 'Conectando…'
            : 'Conectar micro:bit por Bluetooth'}
      </button>
      <p className="ajustes-nota ajustes-nota-chica">
        El puente Bluetooth necesita un navegador compatible y, normalmente, HTTPS o localhost.
      </p>
    </section>
  );
}

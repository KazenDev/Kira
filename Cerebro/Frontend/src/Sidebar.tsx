// Sidebar: historial de chats a la izquierda, con buscador.
import { useEffect, useMemo, useRef, useState } from 'react';
import { Home, PanelLeft, PanelLeftOpen, Plus, Search, X, RefreshCw, Settings } from 'lucide-react';
import { PersonajeInfo } from './api';
import Avatar from './Avatar';
import Logo from './Logo';
import { ChatSesion, limpiarMarkdown, horaLista, extraerEmoji, tituloSinEmoji } from './tipos';
import { resaltarPartes } from './ui-utils';

interface ItemChat {
  pj: PersonajeInfo;
  sesion: ChatSesion;
}

interface Props {
  personajes: PersonajeInfo[];
  chats: Record<string, ChatSesion[]>;
  elegido: PersonajeInfo | null;
  chatActivo: string | null;
  mbEstado: string;
  mbTexto: string;
  mbTitulo: string;
  onAbrirChat: (pj: PersonajeInfo, sesionId: string) => void;
  onBorrarChat: (pjId: string, sesionId: string) => void;
  onNuevoChat: (pj: PersonajeInfo) => void;
  onSincronizar: () => void;
  onAbrirAjustes: () => void;
  onVolver: () => void;
  onCerrar: () => void;
  colapsada: boolean;
  onToggleColapsada: () => void;
}

export default function Sidebar({
  personajes,
  chats,
  elegido,
  chatActivo,
  mbEstado,
  mbTexto,
  mbTitulo,
  onAbrirChat,
  onBorrarChat,
  onNuevoChat,
  onSincronizar,
  onAbrirAjustes,
  onVolver,
  onCerrar,
  colapsada,
  onToggleColapsada,
}: Props) {
  const [busqueda, setBusqueda] = useState('');
  const [saliendo, setSaliendo] = useState<string | null>(null); // id del chat en animacion de salida
  const [sincronizando, setSincronizando] = useState(false); // el icono gira mientras sincroniza
  const [sel, setSel] = useState<number | null>(null); // selección con ↑↓ en el buscador
  const listaRef = useRef<HTMLDivElement | null>(null);

  // todos los chats (ambos personajes) ordenados por actividad
  const todosChats: ItemChat[] = useMemo(() => {
    const lista: ItemChat[] = [];
    personajes.forEach((pj) =>
      (chats[pj.id] ?? []).forEach((sesion) => lista.push({ pj, sesion }))
    );
    return lista.sort(
      (a, b) => new Date(b.sesion.fecha).getTime() - new Date(a.sesion.fecha).getTime()
    );
  }, [personajes, chats]);

  // filtro por texto
  const visibles = useMemo(() => {
    const q = busqueda.trim().toLowerCase();
    return todosChats.filter(({ pj, sesion }) => {
      if (!q) return true;
      const ultimo = sesion.mensajes[sesion.mensajes.length - 1];
      const texto = `${pj.nombre} ${sesion.titulo} ${ultimo ? limpiarMarkdown(ultimo.contenido) : ''}`.toLowerCase();
      return texto.includes(q);
    });
  }, [todosChats, busqueda]);

  // cambiar la lista (búsqueda/borrado) reinicia la selección del teclado
  useEffect(() => {
    setSel(null);
  }, [busqueda]);

  // …y el resaltado nunca se va del viewport
  useEffect(() => {
    if (sel === null) return;
    const nodo = listaRef.current?.children[sel] as HTMLElement | undefined;
    nodo?.scrollIntoView({ block: 'nearest' });
  }, [sel]);

  // ↑↓ recorren los resultados, Enter abre, Escape limpia (patrón Cmd+K)
  const alTeclear = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (!visibles.length) return;
      setSel((s) => Math.min((s ?? -1) + 1, visibles.length - 1));
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (!visibles.length) return;
      setSel((s) => Math.max((s ?? visibles.length) - 1, 0));
    } else if (e.key === 'Enter') {
      const item = visibles[sel ?? -1];
      if (item) {
        onAbrirChat(item.pj, item.sesion.id);
        setSel(null);
      }
    } else if (e.key === 'Escape') {
      if (busqueda) setBusqueda('');
      setSel(null);
    }
  };

  // resalta las coincidencias de la búsqueda dentro del texto del item
  const pintar = (texto: string) =>
    resaltarPartes(texto, busqueda).map((p, i) => (p.coincide ? <mark key={i}>{p.t}</mark> : p.t));

  // borrar con ANIMACION: primero se marca como 'saliendo' (la animacion CSS
  // colapsa el item), y despues de 300ms se llama al borrado real.
  // Se permite borrar OTRO chat mientras uno ya se esta animando (solo se
  // bloquea si se intenta borrar el MISMO dos veces seguidas).
  const pedirBorrado = (pjId: string, sesionId: string) => {
    // Una sola eliminación animándose a la vez: dos borrados simultáneos
    // podrían calcular la misma lista y uno "resucitar" al otro.
    if (saliendo !== null) return;
    setSaliendo(sesionId);
    setTimeout(() => {
      setSaliendo((s) => (s === sesionId ? null : s));
      onBorrarChat(pjId, sesionId);
    }, 300);
  };

  return (
    <>
      <header className="sidebar-header">
        <button
          className={'btn-plegar sidebar-plegar-desktop' + (colapsada ? ' plegado' : '')}
          onClick={onToggleColapsada}
          title={colapsada ? 'Expandir panel' : 'Plegar panel'}
          aria-label={colapsada ? 'Expandir panel de conversaciones' : 'Plegar panel de conversaciones'}
        >
          {colapsada ? (
            <PanelLeftOpen size={16} strokeWidth={2.2} aria-hidden="true" />
          ) : (
            <PanelLeft size={16} strokeWidth={2.2} aria-hidden="true" />
          )}
        </button>
        <button
          className="sidebar-accion sidebar-cerrar-movil"
          onClick={onCerrar}
          title="Cerrar panel"
          aria-label="Cerrar panel de conversaciones"
        >
          <X size={17} strokeWidth={2.2} aria-hidden="true" />
        </button>
        {!colapsada && (
          <div className="sidebar-logo">
            <Logo size={48} />
            <div>
              <h1>Kira</h1>
              <p>Vive en una micro:bit</p>
            </div>
          </div>
        )}
        <button
          className="sidebar-accion btn-inicio"
          onClick={onVolver}
          title="Volver al inicio"
          aria-label="Volver al inicio de Kira"
        >
          <Home size={17} strokeWidth={2.1} aria-hidden="true" />
        </button>
        <div className="sidebar-nuevo">
          {/* Kira es la única IA: el "+" abre una conversación nueva. */}
          <button
            className="btn-nuevo"
            onClick={() => {
              const pj = personajes[0];
              if (pj) onNuevoChat(pj);
            }}
            disabled={!personajes.length}
            title="Nueva conversación"
            aria-label="Nueva conversación con Kira"
          >
            <Plus size={20} strokeWidth={2.4} aria-hidden="true" />
          </button>
        </div>
      </header>

      {/* buscador (oculto cuando el panel está plegado) */}
      {!colapsada && (
      <div className="sidebar-filtros">
        <div className="buscador">
          <Search size={15} strokeWidth={2.2} className="buscador-ico" aria-hidden="true" />
          <input
            value={busqueda}
            onChange={(e) => setBusqueda(e.target.value)}
            onKeyDown={alTeclear}
            onBlur={() => setSel(null)}
            placeholder="Buscar conversación..."
            maxLength={60}
          />
          {busqueda && (
            <button className="buscador-x" onClick={() => setBusqueda('')} title="Limpiar">
              <X size={14} strokeWidth={2.4} aria-hidden="true" />
            </button>
          )}
        </div>
      </div>
      )}

      <div className={'lista-chats' + (colapsada ? ' oculta' : '')} ref={listaRef}>
        {visibles.length === 0 && (
          <p className="lista-vacia">
            {busqueda
              ? 'No hay conversaciones que coincidan.'
              : (
                <>Todavía no hay conversaciones.<br />Empezá una charla con Kira.</>
              )}
          </p>
        )}
        {visibles.map(({ pj, sesion }, idx) => {
          const activo = elegido?.id === pj.id && chatActivo === sesion.id;
          const ultimo = sesion.mensajes[sesion.mensajes.length - 1];
          // emoji del titulo en el circulito (estatico, sin la ballena)
          const emoji = extraerEmoji(sesion.titulo);
          const nombre = tituloSinEmoji(sesion.titulo);
          return (
            <div
              key={sesion.id}
              className={
                'item-chat' +
                (activo ? ' activo' : '') +
                (saliendo === sesion.id ? ' saliendo' : '') +
                (sel === idx ? ' sel' : '')
              }
              style={
                {
                  '--color': pj.color,
                  // stagger de entrada en cascada, con tope de 8: con muchos
                  // chats los ultimos no esperan mas de ~360ms invisibles
                  '--i': Math.min(idx, 8),
                } as React.CSSProperties
              }
            >
              <button
                type="button"
                className="item-chat-open"
                onClick={() => onAbrirChat(pj, sesion.id)}
                title={colapsada ? `${pj.nombre} — ${nombre}` : undefined}
                aria-label={`Abrir conversación: ${nombre}`}
                aria-current={activo ? 'page' : undefined}
              >
                <div className="item-avatar" style={{ background: pj.color }}>
                  {emoji ? (
                    <span className="item-emoji" aria-hidden="true">{emoji}</span>
                  ) : (
                    <Avatar color="#fff" size={26} />
                  )}
                </div>
                <div className="item-cuerpo">
                  <div className="item-fila">
                    <span className="item-nombre">{pintar(nombre)}</span>
                    <span className="item-hora">{horaLista(sesion.fecha)}</span>
                  </div>
                  <div className="item-fila">
                    <span className="item-preview">
                      {ultimo
                        ? pintar(
                            (ultimo.rol === 'usuario' ? 'Vos: ' : '') +
                              limpiarMarkdown(ultimo.contenido).slice(0, 38)
                          )
                        : 'Sin mensajes'}
                    </span>
                  </div>
                </div>
              </button>
              <button
                type="button"
                className="item-x"
                onClick={() => pedirBorrado(pj.id, sesion.id)}
                title="Borrar este chat"
                aria-label={`Borrar conversación: ${nombre}`}
              >
                <X size={14} strokeWidth={2.4} aria-hidden="true" />
              </button>
            </div>
          );
        })}
      </div>

      <footer className={'sidebar-footer' + (colapsada ? ' compacto' : '')}>
        <div className={`mb-status ${mbEstado}`} title={mbTitulo}>
          <span className="mb-dot" />
          <span className="mb-status-texto">{mbTexto}</span>
        </div>
        <div className="sidebar-footer-botones">
          <button
            className="btn-ajustes"
            onClick={onAbrirAjustes}
            title="Ajustes"
            aria-label="Abrir ajustes"
          >
            <Settings size={15} strokeWidth={2.2} aria-hidden="true" />
          </button>
          <button
            className={'icon-btn chico' + (sincronizando ? ' girando' : '')}
            onClick={async () => {
              setSincronizando(true);
              await onSincronizar();
              setTimeout(() => setSincronizando(false), 400);
            }}
            title="Re-sincronizar el micro:bit"
            aria-label="Re-sincronizar el micro:bit"
          >
            <RefreshCw size={15} strokeWidth={2.2} aria-hidden="true" />
          </button>
        </div>
      </footer>
    </>
  );
}

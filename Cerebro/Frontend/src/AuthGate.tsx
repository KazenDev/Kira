import { useEffect, useState } from 'react';
import type { FormEvent } from 'react';
import { Database, Eye, EyeOff, LoaderCircle, LogIn, ShieldCheck, UserPlus } from 'lucide-react';
import {
  cerrarSesion,
  iniciarSesion,
  obtenerSesion,
  registrarUsuario,
  type RespuestaAuth,
  type UsuarioKira,
} from './api';
import App from './App';

type Modo = 'login' | 'registro';

export default function AuthGate() {
  const [usuario, setUsuario] = useState<UsuarioKira | null>(null);
  const [cargando, setCargando] = useState(true);
  const [modo, setModo] = useState<Modo>('login');
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [confirmacion, setConfirmacion] = useState('');
  const [error, setError] = useState('');
  const [enviando, setEnviando] = useState(false);
  // Ver la contraseña. El motivo real: tipear 12 caracteres en un campo de
  // puntos y no poder verificar es la forma más fácil de registrar una cuenta
  // con un error de tipeo, y después uno no se acuerda cuál era. El botón
  // existe para eso, no por magia.
  const [verClave, setVerClave] = useState(false);

  useEffect(() => {
    let vigente = true;
    obtenerSesion()
      .then((respuesta) => {
        if (vigente) setUsuario(respuesta.user);
      })
      .catch((e) => {
        if (vigente) setError(e instanceof Error ? e.message : String(e));
      })
      .finally(() => {
        if (vigente) setCargando(false);
      });

    const sesionExpirada = () => {
      setUsuario(null);
      setCargando(false);
      setError('Tu sesión terminó. Volvé a entrar para seguir hablando con Kira.');
    };
    window.addEventListener('kira:sesion-expirada', sesionExpirada);
    return () => {
      vigente = false;
      window.removeEventListener('kira:sesion-expirada', sesionExpirada);
    };
  }, []);

  const entrar = async (event: FormEvent) => {
    event.preventDefault();
    if (enviando) return;
    setError('');
    if (modo === 'registro' && password !== confirmacion) {
      setError('Las contraseñas no coinciden.');
      return;
    }
    // Tapar la clave en cuanto se envía. Si la persona la dejó a la vista para
    // verificar, no tiene sentido dejarla descubierta mientras espera: el
    // servidor está respondiendo y cualquiera que mire la pantalla la lee.
    setVerClave(false);
    setEnviando(true);
    try {
      const respuesta: RespuestaAuth = modo === 'login'
        ? await iniciarSesion(username, password)
        : await registrarUsuario(username, password);
      setUsuario(respuesta.user);
      setPassword('');
      setConfirmacion('');
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setEnviando(false);
    }
  };

  const salir = async () => {
    setEnviando(true);
    try {
      await cerrarSesion();
      setUsuario(null);
      setError('');
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setEnviando(false);
    }
  };

  if (cargando) {
    return (
      <div className="auth-shell" role="status" aria-label="Abriendo Kira">
        <div className="auth-card auth-cargando">
          <LoaderCircle className="girando" size={28} aria-hidden="true" />
          <strong>Despertando a Kira…</strong>
        </div>
      </div>
    );
  }

  if (!usuario) {
    return (
      <main className="auth-shell">
        <section className="auth-card" aria-labelledby="auth-title">
          <div className="auth-brand">
            <div className="auth-logo"><Database size={22} aria-hidden="true" /></div>
            <div>
              <h1 id="auth-title">Kira</h1>
              <p>Una memoria distinta para cada persona.</p>
            </div>
          </div>

          <div className="auth-modes" role="tablist" aria-label="Acceso a Kira">
            <button
              type="button"
              role="tab"
              aria-selected={modo === 'login'}
              className={modo === 'login' ? 'activo' : ''}
              onClick={() => { setModo('login'); setError(''); }}
            >
              <LogIn size={15} aria-hidden="true" /> Entrar
            </button>
            <button
              type="button"
              role="tab"
              aria-selected={modo === 'registro'}
              className={modo === 'registro' ? 'activo' : ''}
              onClick={() => { setModo('registro'); setError(''); }}
            >
              <UserPlus size={15} aria-hidden="true" /> Crear cuenta
            </button>
          </div>

          <form className="auth-form" onSubmit={entrar}>
            <label htmlFor="auth-username">Usuario</label>
            <input
              id="auth-username"
              autoComplete="username"
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              minLength={3}
              maxLength={32}
              required
              autoFocus
            />

            <label htmlFor="auth-password">Contraseña</label>
            {/* El input va envuelto porque el botón del ojo tiene que SOBRELOS
                el campo, no empujar el layout. Por eso el input pasa de
                `width:100%` directo a `flex:1` dentro de la fila. */}
            <div className="auth-clave">
              <input
                id="auth-password"
                type={verClave ? 'text' : 'password'}
                autoComplete={modo === 'login' ? 'current-password' : 'new-password'}
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                minLength={8}
                maxLength={128}
                required
              />
              <button
                type="button"
                className="auth-ver"
                onClick={() => setVerClave(!verClave)}
                aria-label={verClave ? 'Ocultar contraseña' : 'Ver contraseña'}
                aria-pressed={verClave}
                title={verClave ? 'Ocultar' : 'Ver'}
              >
                {verClave ? <EyeOff size={17} aria-hidden="true" /> : <Eye size={17} aria-hidden="true" />}
              </button>
            </div>

            {modo === 'registro' && (
              <>
                <label htmlFor="auth-confirm">Repetir contraseña</label>
                {/* Comparte el mismo toggle: son la misma clave, y tener dos
                    botones independientes invita a que uno este abierto y el
                    otro cerrado, que es peor que ninguno. */}
                <div className="auth-clave">
                  <input
                    id="auth-confirm"
                    type={verClave ? 'text' : 'password'}
                    autoComplete="new-password"
                    value={confirmacion}
                    onChange={(e) => setConfirmacion(e.target.value)}
                    minLength={8}
                    maxLength={128}
                    required
                  />
                  <button
                    type="button"
                    className="auth-ver"
                    onClick={() => setVerClave(!verClave)}
                    aria-label={verClave ? 'Ocultar contraseña' : 'Ver contraseña'}
                    aria-pressed={verClave}
                    title={verClave ? 'Ocultar' : 'Ver'}
                  >
                    {verClave ? <EyeOff size={17} aria-hidden="true" /> : <Eye size={17} aria-hidden="true" />}
                  </button>
                </div>
              </>
            )}

            {error && <p className="auth-error" role="alert">{error}</p>}
            <button className="auth-submit" type="submit" disabled={enviando}>
              {enviando ? <LoaderCircle className="girando" size={17} aria-hidden="true" /> : modo === 'login' ? <LogIn size={17} aria-hidden="true" /> : <UserPlus size={17} aria-hidden="true" />}
              {enviando ? 'Abriendo…' : modo === 'login' ? 'Entrar a Kira' : 'Crear mi Kira'}
            </button>
          </form>

          <p className="auth-note">
            <ShieldCheck size={15} aria-hidden="true" />
            Tus recuerdos, diario, conversaciones e índice RAG quedan separados por cuenta.
            El micro:bit y los servicios de IA/TTS son compartidos por la instalación.
          </p>
          {modo === 'registro' && (
            <p className="auth-note auth-note-import">
              La primera cuenta recibe una copia de la memoria actual de Kira; el original queda como respaldo.
            </p>
          )}
        </section>
      </main>
    );
  }

  return <App usuario={usuario} onCerrarSesion={salir} />;
}

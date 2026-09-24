import { useEffect, useState } from 'react';
import type { FormEvent } from 'react';
import { Database, LoaderCircle, LogIn, ShieldCheck, UserPlus } from 'lucide-react';
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
            <input
              id="auth-password"
              type="password"
              autoComplete={modo === 'login' ? 'current-password' : 'new-password'}
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              minLength={8}
              maxLength={128}
              required
            />

            {modo === 'registro' && (
              <>
                <label htmlFor="auth-confirm">Repetir contraseña</label>
                <input
                  id="auth-confirm"
                  type="password"
                  autoComplete="new-password"
                  value={confirmacion}
                  onChange={(e) => setConfirmacion(e.target.value)}
                  minLength={8}
                  maxLength={128}
                  required
                />
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

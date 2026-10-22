// Núcleo HTTP: cookies de sesión, CSRF, timeout y aviso de sesión expirada.

const CSRF_COOKIE = 'kira_csrf';
const CSRF_HEADER = 'X-Kira-CSRF';
const METODOS_SEGUROS = new Set(['GET', 'HEAD', 'OPTIONS', 'TRACE']);

function csrfCookie(): string {
  const item = document.cookie.split('; ').find((parte) => parte.startsWith(`${CSRF_COOKIE}=`));
  return item ? decodeURIComponent(item.slice(CSRF_COOKIE.length + 1)) : '';
}

/** Fetch común: cookies de sesión + token CSRF para mutaciones. */
export function fetchKira(input: RequestInfo | URL, init: RequestInit = {}): Promise<Response> {
  const headers = new Headers(init.headers);
  const metodo = (init.method ?? 'GET').toUpperCase();
  if (!METODOS_SEGUROS.has(metodo)) {
    const token = csrfCookie();
    if (token) headers.set(CSRF_HEADER, token);
  }
  return fetch(input, {
    ...init,
    headers,
    credentials: init.credentials ?? 'same-origin',
  }).then((response) => {
    const url =
      typeof input === 'string' ? input : input instanceof URL ? input.pathname : input.url;
    if (response.status === 401 && !url.startsWith('/api/auth/')) {
      window.dispatchEvent(new Event('kira:sesion-expirada'));
    }
    return response;
  });
}

/** POST que no debe romper la charla si falla (avisos a la placa, etc.). */
export function postSilencioso(url: string, init?: RequestInit): Promise<Response | null> {
  return fetchKira(url, { method: 'POST', ...init }).catch(() => null);
}

/** fetch CON TOPE DE TIEMPO. Sin esto, si la red se cuelga a mitad de una
 * request (el celu cambia de WiFi a datos, la pantalla se apaga, nginx
 * parpadea), el `await` no resuelve NUNCA y el bucle del puente se muere en
 * silencio sin reportar nada: los logs del VPS mostraban justo eso — los
 * latidos se cortaban de golpe y 61s despues el server liberaba el puente.
 * OJO: NO se usa en el stream del chat (ese es largo a propósito). */
export async function conTope(
  url: string,
  ms: number,
  opciones: RequestInit = {}
): Promise<Response> {
  const ctrl = new AbortController();
  const reloj = setTimeout(() => ctrl.abort(), ms);
  try {
    return await fetchKira(url, { ...opciones, signal: ctrl.signal });
  } finally {
    clearTimeout(reloj);
  }
}

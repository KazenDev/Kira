// Token de dueño del puente: uno por PESTAÑA (dos tabs = dos tokens, uno gana).
import { useState } from 'react';
import { uuid } from '../../lib/ui-utils';

export function useTokenPuente(): string {
  const [token] = useState(() => {
    let t = sessionStorage.getItem('puente-token');
    if (!t) {
      t = uuid();
      sessionStorage.setItem('puente-token', t);
    }
    return t;
  });
  return token;
}

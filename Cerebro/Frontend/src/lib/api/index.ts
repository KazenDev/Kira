// Fachada del cliente HTTP. La app importa desde acá (`./lib/api`) y cada
// dominio vive en su archivo: auth, personajes, memoria, chat, ble, eventos.
export * from './tipos';
export * from './http';
export * from './auth';
export * from './personajes';
export * from './memoria';
export * from './chat';
export * from './ble';
export * from './eventos';
export * from './proveedores';

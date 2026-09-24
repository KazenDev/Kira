# Estilos de Kira

La entrada de la aplicación es `index.css`.

- `tokens.css` contiene el tema final y las variables compartidas.
- `00-legacy.css` a `06-markdown-graphite.css` son capas de compatibilidad que conservan reglas base históricas.
- `07-producto-real.css` es un wrapper de la capa final, que está dividida en `07-product-*.css` por responsabilidad.
- `08-ajustes.css` y `09-auth.css` contienen sus respectivos módulos.

El orden de los imports es intencional: durante la siguiente fase se pueden consolidar las capas históricas, pero no deben reordenarse sin revisar la cascada.

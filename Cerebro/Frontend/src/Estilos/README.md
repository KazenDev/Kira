# Estilos de Kira

La entrada de la aplicación es `index.css`.

- `tokens.css` contiene el tema final y las variables compartidas.
- `00-legacy.css` conserva la base estructural original.
- `compat-core.css` y `compat-graphite.css` preservan, en orden, las reglas de V3/V4/V5 que todavía aportan a la cascada.
- `Historico/` contiene las capas V3, V4 y V5 originales como referencia; no se importan en la aplicación.
- `07-producto-real.css` es un wrapper de la capa final, que está dividida en `07-product-*.css` por responsabilidad.
- `08-ajustes.css` y `09-auth.css` contienen sus respectivos módulos.

El orden de los imports es intencional: las capas de compatibilidad preservan el orden original para que la consolidación no cambie el aspecto. Las media queries permanecen junto a sus capas hasta poder reorganizarlas por componente sin alterar la cascada móvil.

# Remacdent FX: Lazo y Hueco

Dos efectos para Ableton en Mac (VST3 y Audio Unit), con un solo instalador.

## Lazo: compresor de bus
Compresor estéreo al estilo de los compresores de bus de consola clásicos. Sirve para "pegar" una mezcla, un bus de batería o el master.

| Control | Qué hace |
|---|---|
| Umbral | A partir de qué nivel empieza a comprimir (−40 a 0 dB) |
| Ratio | Cuánto comprime, de 1:1 a 20:1, continuo |
| Ataque | 0.1, 0.3, 1, 3, 10 o 30 ms. Los ataques lentos dejan pasar el golpe del bombo y la caja |
| Liberación | 0.1 a 1.2 s, o **Auto**, que se adapta a la música: suelta rápido tras los golpes y lento en lo sostenido |
| Ganancia | Recupera el volumen perdido (0 a +20 dB) |
| Mezcla | Compresión en paralelo |
| Filtro SC | Evita que los graves disparen la compresión (apagado a 2 kHz) |
| Rango | Máximo de compresión permitido |
| CLIP | Recortador suave a la salida, a doble resolución; nunca pasa de 0 dB |
| SC EXTERNO | Usa otra pista (sidechain) para controlar la compresión |

También tiene un medidor de aguja de reducción, medidores de entrada y salida, y 7 presets.

## Hueco: espacio espectral por sidechain
Ponlo en la pista que debe hacerse a un lado (bajo, pad, guitarras) y mándale por sidechain la pista importante (voz, bombo). Hueco baja **solo las frecuencias que esa pista está usando**, en 32 bandas, en vez de bajar todo el volumen. No tiene latencia.

| Control | Qué hace |
|---|---|
| Cantidad | Qué tanto se abre el hueco |
| Desde / Hasta | Zona de frecuencias donde actúa. Ejemplo: 200 Hz a 5 kHz para una voz |
| Ataque / Liberación | Qué tan rápido entra y suelta |
| Mezcla | Parte de la señal sin tocar |
| Modo | **Estéreo**: igual en ambos lados. **Izq/Der**: cada lado por separado. **Medio/Lados**: el hueco solo en el centro, o solo en los lados |

La pantalla muestra en azul el espectro del sidechain y en rojo el hueco que está abriendo.

## Cómo conectar el sidechain en Ableton
1. Pon Hueco (o Lazo) en la pista que se debe hacer a un lado.
2. En la barra del plugin, haz clic en el **triángulo o flecha** para desplegar las opciones y activa **Sidechain**.
3. En el menú que aparece, elige la pista que manda: la voz, el bombo, etc.
4. En Lazo, además, activa el botón **SC EXTERNO**.

Para el sidechain, usa la versión **VST3**.

## Compilar
GitHub lo compila solo cada vez que subes cambios (pestaña **Actions → Artifacts**) y genera `Remacdent-FX-Instalador-x.y.z.pkg`. Si cambias el código, sube la versión en `CMakeLists.txt` para que Ableton note el cambio.

JUCE 8 es gratis para uso personal (licencia AGPLv3 o "Starter").

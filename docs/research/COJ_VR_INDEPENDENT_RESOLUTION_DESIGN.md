# Resolución VR independiente — especificación técnica

Estado: **diseño aprobado para avance autónomo; escritor bloqueado por Q0**.
El diagnóstico de bindings está implementado y probado en host; el renderizado
con dimensiones independientes sigue pendiente de cualificación nativa y visor.
Referencia funcional: código de `main` en `3dba9f8` y aceptación acotada registrada
en [VALIDATION](../VALIDATION.md). Alcance acordado el 16 de octubre de 2026.

Este documento define la intervención y sus puertas de entrada. No afirma que
ya exista un punto nativo seguro para redimensionar toda la escena. Los hechos
de ingeniería inversa pertenecen a
[la investigación de resolución](COJ_VR_RENDER_RESOLUTION.md); las direcciones
de este documento son RVA del motor exacto identificado allí y en
[el censo de recursos](COJ_D3D9_RESOURCE_CENSUS.md). Una dirección de configuración
de vídeo no se convierte por ello en un escritor de resolución por ojo.

## 1. Objetivo y decisión

Renderizar geometría, profundidad y efectos de cada ojo en una familia de
recursos propia, con una extensión fija elegida antes del arranque. El tamaño de
los píxeles del escritorio no debe decidir el de los ojos. La aplicación conserva
sus dimensiones de ventana/backbuffer, coordenadas de UI y fuentes de teatro.
La salida nativa del escritorio puede recibir un espejo reducido; ninguna imagen
de ojo puede obtener su detalle mediante ampliación de la imagen de escritorio.

Se mantiene D3D9Ex → D3D11 → OpenVR, las dos llamadas al wrapper completo,
ópticas asimétricas, procedencia por ejecución, restauración transaccional y
las rutas actuales de interacción. No se modifica la pre-release publicada,
sus archivos/etiquetas ni el instalador público. No hay OpenXR, D3D10, resolución
dinámica, selector de escala, menú nuevo ni refactorización global.

Alternativas consideradas:

| Enfoque | Resultado y decisión |
| --- | --- |
| Contexto nativo de escena VR cualificado y familia D3D9 propia | **Elegido**, condicionado al mapa del apartado 3. Permite coherencia entre buffers, dimensiones internas y efectos, con restauración por ojo. |
| Sustituir llamadas D3D por coincidencia de ancho/alto o interceptar todo el dispositivo | Rechazado como escritor: puede afectar sombras, UI, reflejos o caches y no corrige tamaños internos. Los hooks se pueden usar para observación acotada. |
| Aumentar backbuffer, cambiar Video.scr o ampliar las texturas D3D11 | No satisface el alcance: cambia la resolución nativa del escritorio o solo interpola muestras existentes. No será el prototipo. |

**Límite de viabilidad:** si el motor solo ofrece una reinicialización global
que también altera la presentación/UI, no se habilita la resolución independiente.
Se documenta ese resultado y se conserva la base funcional; no se oculta el
problema con un blit de mayor tamaño.

## 2. Base comprobada y puntos exactos de intervención

Las rutas de código de la tabla son relativas a `src/`. «Propuesto» identifica
una modificación futura; no describe una API ya implementada.

| Frontera | Evidencia actual | Intervención propuesta |
| --- | --- | --- |
| `games/call_of_juarez/camera_probe.cpp`: `HookRenderViewImpl` | Produce ojo izquierdo, captura, restaura cámara y ejecuta el derecho. | Admitir un contexto de resolución antes del primer wrapper y un ámbito de recursos por ojo; no volver a despachar input, física o gameplay para generar el espejo. |
| Mismo archivo: `RenderCameraOnlyStereo` | Cine inmersivo y ajustes usan el wrapper por ojo; el cine no adquiere cuerpo/gameplay. | Misma familia VR y misma restauración para estas dos rutas, manteniendo sus propietarios distintos. El replay natural de recuperación queda fuera del ámbito VR. |
| Mismo archivo: `ReplayFullRenderViewForStereoEye` | Wrapper `0x30FB0`; guarda, limpia y restaura el guard de `view+0xD7`. | Conservarlo íntegro; no usar core `0x30E00` ni introducir más actualizaciones de scripts. Un ámbito VR fallido impide capturar/publicar ese ojo. |
| Mismo archivo: `ConfigureStereoEyeOverride`, `HookSetFov`, `BuildCameraProbeFrustum` | Posición/orientación y frustum proceden del seguimiento y EyeFov. | Mantener ópticas, unidades y near/far. Validar la proyección efectiva que usa cada pase después de cualquier cambio de target. |
| Motor: selección de recursos dentro del wrapper/core | **No cualificada todavía**: no existe inventario completo de slots/caches/efectos. | Cualificar el contexto que selecciona la familia de escena y sus tamaños. El adaptador de juego guarda/restaura únicamente campos exactos demostrados; el backend mantiene los recursos COM. No inventar RVA, offsets o firmas de este escritor. |
| Motor: configuración `0x1D81B0`, aplicación `0x1D8A00`, notificación `0x6830` | Setter de dimensiones propuestas, reinicialización global y cache de dimensiones nativas. | Puntos de investigación y exclusión: no invocarlos para cada ojo ni sobrescribir globalmente las dimensiones del escritorio. |
| Motor: `0x224EC0`, `0x225690`, `0x224F90` | El análisis estático retenido de `0x1D8A00` muestra llamadas en la rama de reconstrucción de postproceso y fallos de creación. Sus firmas, grafo y ámbito no están cualificados. | Investigar quién libera, configura y crea sus dependencias. No llamar a estas funciones como supuesto constructor offscreen seguro. |
| `backends/d3d9/device_vtable_hook.cpp`: `HookSetRenderTarget`, `HookSetDepthStencilSurface`, `HookClear`, `HookSetTexture` | Los callbacks de traza actuales registran nombre/entrada/salida, no un grafo completo de superficies y productores. | Añadir solo la observación de argumentos/identidad/callsite que falte; incluir viewport, scissor, creación y lecturas. No convertir la traza actual en prueba de propiedad de recursos. |
| `backends/d3d9/d3d9_stereo_capture.cpp`: `EnsureResources`, `CaptureEyeSurface`, `EndFrame`, `TryCollectReady` | Captura superficie explícita, anillo de tres pares, evento productor y lease al consumidor. | Usar la superficie final cualificada del ojo, aunque el wrapper haya restaurado otro RT0; copia completa del mismo tamaño. Preparación de recursos separada de su uso/publicación para detectar asignaciones fallidas antes de mutar la escena. |
| `games/call_of_juarez/native_stereo_proxy.cpp`: `BeginStereoFrame`, callbacks de captura/envío/invalidation, `BeforeDeviceReset`, `AfterDeviceReset` | Coordina propietario, generación, cadencia y captura; Reset correcto incrementa generación. | Coordinar admisión fija, epoch de resolución y retiro; el intento de Reset también invalida autoridad experimental aunque falle y no avance la generación nativa. |
| `backends/openvr/d3d9_shared_texture_bridge.cpp`: `CopyFrame`, `Poll`, `Shutdown` | Verifica layout/dispositivo/contexto, abre handles compartidos y copia a ojos D3D11. Retiene leases inciertos. | Conservar igualdad de descriptores; registrar extensión/epoch y rechazar pares incompletos. Nunca usar resize para hacer coincidir layouts. |
| `backends/openvr/stereo_presenter.cpp`: `EnsureTextures`, `UploadFrame`, `SubmitEyes` | Dos texturas de salida, según el frame; teatro tiene su propia fórmula geométrica. | Preparar por separado un par VR y un par de teatro de recuperación; conmutar solo pares completos y retirados. No reutilizar automáticamente el tamaño VR para teatro. |
| `backends/openvr/hud_text_overlay.cpp`: `HudTextCompositor::Draw`; retícula en `UploadFrame` | Paneles se proyectan con las dimensiones del destino; algunas marcas usan radios fijos en píxeles. | Comprobar tamaño angular y legibilidad de todos los elementos; corregir solo dependencias de extensión demostradas. No capturar/reproducir de nuevo el árbol nativo de sprites. |

No hace falta cambiar ABI de input, calibración, JNI de armas, ammo ni persistencia
de preferencias para este prototipo. Las adiciones propuestas se limitan a un
descriptor de extensión/layout, un ámbito D3D9 de escena, datos de cualificación
del adaptador CoJ y su conexión con los callbacks existentes. Los nombres/ficheros
nuevos se concretan en el plan local de ejecución del diseño aprobado.

## 3. Inventario que autoriza la familia de escena

Actualmente se ha observado RT0 A8R8G8B8 a 2560×1440, viewport completo y
`source_is_backbuffer=true`. Una observación de profundidad al final de ojo
registra formato 77 (`D24X8`) y dimensiones coincidentes, pero Z está desactivado
en ese instante. **Eso no identifica el buffer usado durante la geometría ni
prueba el contenido de profundidad.** El censo de MANAGED tampoco identifica
todos los targets de postproceso.

| Familia | Qué cambia en el diseño | Qué debe demostrarse antes de escribir |
| --- | --- | --- |
| Backbuffer/swap chains y UI nativa | Dimensiones y presentación siguen siendo las del juego. | Identidad de los targets de escritorio y sus consumidores de sprite/cursor; nunca admitidos como recursos VR por su tamaño. |
| Color de escena y salida final del ojo | Recurso(s) D3D9 DEFAULT a la extensión VR; salida final independiente del backbuffer. | Formato, slots/MRT activos, productor, consumidor, clears y ubicación del resolve/último efecto. Determinar si un color se puede reutilizar entre ojos. |
| Depth/stencil de escena | Superficie compatible con el color y extensión VR; inicialmente sin MSAA. | Uso durante geometría, stencil, clears y lectura posterior. Formato 77 al final no se generaliza a toda la familia. Cualquier lectura de profundidad obliga a cualificar también su vista/textura nativa. |
| Efectos dependientes de pantalla | Réplicas a escala exacta demostrada del tamaño VR. | Inventario por efecto activo: targets completos/reducidos, formatos, redondeos, historia temporal, lecturas cruzadas, quads, constantes texel y cache de tamaños. No asumir que todos los efectos existen o usan el mismo tamaño. |
| Sombras, reflejos y targets de materiales | Por defecto, recursos nativos sin cambios. | Demostrar independencia de pantalla. Si una reflexión o efecto usa resolución de escena, pasa al inventario anterior; no se omite por su nombre. |
| Captura/transporte | Tres pares de color compartidos a resolución de ojo, sin depth transportado. | Layout exacto, generación y retiro seguro. Las superficies nivel 0 y OpenSharedResource no cuentan como nuevas copias de píxeles. |
| Composición VR y teatro | Par D3D11 VR propio y par de teatro calculado con fuente/ópticas actuales. | Dispositivo/contexto, readiness, coste del par adicional, no envío de ojos de distinto propietario/tamaño. Texturas de overlays se contabilizan aparte. |

La cualificación Q0 debe entregar un registro local por recurso y una conclusión
duradera: clase de recurso, callsite RVA, clave de propietario nativo, dispositivo/
generación, formato, extent, niveles/MSAA, relación texture/surface, slot de bind,
primera/última escritura, lectores, clear y regla de destrucción/recreación.
Debe correlacionarse con frame, ojo, ruta gameplay/settings/movie y epoch de modo.
Identidades de puntero se limitan a esa generación; no se usan como firmas de build.

La captura acotada incluye creación, bind y uso en los dos wrappers, desde antes
del primer clear hasta la última lectura/escritura de efectos. Debe distinguir
los binds del propio mod para no inferir que sus texturas son recursos del motor.
Se revisan shaders/constantes y vértices de pantalla solo en los pases identificados;
no se conserva una traza exhaustiva durante toda la campaña. Repetir en una escena
con efectos y en una transición UI/cine evita aceptar como completo un frame vacío.

**Q0 no pasa** con un target desconocido, lectura no clasificada, estado de
historia compartida entre ojos o cache de dimensiones global que no pueda aislarse.
Resolver Q0 requiere observación en el juego iniciada/cerrada por el operador;
este trabajo de especificación no ha producido esa observación nueva.

## 4. Separación y primer prototipo

Tres valores distintos: `desktop_extent`, `eye_extent` y `theater_output_extent`.
El tercero se calcula con `ComputeFlatTheaterTextureExtent` desde la fuente de
escritorio y las ópticas. No se fuerza a coincidir con ninguno de los otros dos.

La solicitud experimental se fija en el candidato local antes de arrancar y se
liga al build/run manifest. No se relee ni aplica en caliente. La recomendación
OpenVR se registra como dato de comparación, sin adoptarse automáticamente.
Un dato inválido o una combinación no cualificada deja el prototipo desactivado.
Se conserva el esquema de preferencias y el panel actuales.

Secuencia de tamaños, cada uno en una ejecución nueva:

| Paso | Extensión fija por ojo | Finalidad |
| --- | --- | --- |
| A | 2560×1440, ruta vigente | Control A del A/B; no prueba independencia. |
| B0 | 2560×1440, familia VR separada | Aislar los efectos de cambiar el propietario de los recursos antes de aumentar carga. No se presenta como mejora de nitidez. |
| B1, primer aumento | **2560×1920** | Mantener muestras horizontales y aumentar las verticales un 33,3%; cambiar aspecto de forma controlada. Se exige detalle vertical nuevo y geometría angular estable. |
| B2, condicionado | 2560×2560 | Ensayo de aspecto cuadrado después de aceptar B1 y su presupuesto. No es requisito para dar B1 por válido. |
| C, condicionado | 2804×2860 | Evaluar la recomendación observada, con presupuesto nuevo y A/B propio. Nunca se convierte en el valor automático del prototipo. |

El primer escritor solo admite D3D9Ex compartido, formatos/MRT/efectos demostrados
y FSAA 0. Si aparece clásico, MSAA u otro layout, se conserva la ruta vigente sin
redimensionarla. No se elimina un efecto ni se cambian ajustes de vídeo para que
un frame sea admitido silenciosamente. Nuevos contextos que fallen la cualificación
usan el fallback descrito abajo, con razón visible en diagnóstico.

### Escritorio, sprites y espejo

En gameplay VR el resultado final del segundo ojo, ya copiado a su slot privado,
se puede reducir al backbuffer nativo para mantener un espejo sin un tercer
render de mundo. La política es ajuste proporcional con bandas si cambia el
aspecto, no estiramiento de la óptica. Es un cambio explícito del contenido del
espejo; ventana/backbuffer, UI y coordenadas conservan sus dimensiones.

El blit debe ocurrir después de restaurar el contexto VR y **antes** del pase
natural de sprites del escritorio. No debe borrar UI ya dibujada. Si Q0 no
demuestra ese orden, este espejo bloquea B0; no se resuelve reejecutando scripts
ni suponiendo que un tercer wrapper es inocuo. Si se necesita una pasada natural
adicional como alternativa, se revisa el diseño y se mide su coste/efectos antes.
Las lecturas de escena y posibles callbacks del wrapper son parte de Q0.

Intro, menú, pausa, carga, muerte y cine en teatro siguen dibujando/capturando
Present nativo a la resolución de escritorio. Entrar en teatro nunca copia un ojo
VR como sustituto de su fuente. Cine inmersivo y ajustes VR conservan la ruta
camera-only, ahora con la familia independiente si está cualificada. Subtítulos
inmersivos y captura gráfica completa del HUD siguen fuera de alcance.

## 5. Aspecto, ópticas y efectos

2560×1440 tiene aspecto 1,77778; 2804×2860, 0,98042. No son el mismo rectángulo
con una escala uniforme: ancho ×1,09531 y alto ×1,98611. B1 tiene aspecto 1,33333.
El cambio debe aumentar densidad de muestras sobre el mismo frustum angular,
no extender/recortar el mundo para encajarlo en el aspecto del escritorio.

Se conservan por ojo `EyeFov` y `eye_to_head`, y el builder existente:
`left/right = near*tan(angle_left/right)`,
`bottom/top = near*tan(angle_down/up)`. No se deriva un FOV horizontal de W/H,
no se centra una proyección asimétrica y no se interpreta un ángulo en radianes
como tangente raw de OpenVR. Se mantiene la convención near/far y profundidad
del motor, la base exacta derecha/arriba/frente y la conversión CoJ cm/XR m.

Los campos recomendados de EyeView no se sobrescriben para simular ópticas nuevas;
el layout efectivo es un valor aparte. Focus conserva su modificación nativa
actual del frustum de mundo; HUD y teatro siguen usando las ópticas físicas.
La pose explícita enviada al compositor debe ser la capturada para ese par.

Por cada pase cualificado se comprueba:

- Color/depth/MRT, viewport y scissor cubren las regiones de trabajo esperadas;
  no queda una ventana 2560×1440 dentro de un ojo de mayor tamaño.
- Proyección y viewport efectivos en geometría, no solo matrices al final del
  wrapper, donde el motor puede haber restaurado estado de pantalla.
- Frustum de culling y proyección de dibujo compatibles, incluidos bordes de
  cada ojo. Partículas, transparencias, agua y muzzle FX no pueden quedar con
  clips/tamaños del escritorio.
- Cada shader de pantalla usa dimensiones/inversas de su **input/output real**;
  targets a mitad/cuarto resuelven redondeo explícito. Bloom/blur/DOF, si existen
  en la escena, conservan el criterio nativo de radio (píxeles/UV/angular) según
  su contrato, no una regla global improvisada.
- Quads/XYZRHW y muestreo mantienen la convención de medio texel de D3D9. No se
  añade otro desplazamiento si el pase ya lo aplica. Véase
  [mapeo texel/píxel de Microsoft](https://learn.microsoft.com/en-us/windows/win32/direct3d9/directly-mapping-texels-to-pixels).
- Postproceso temporal, si se encuentra, tiene historia por ojo o queda fuera de
  admisión hasta demostrar su aislamiento; no se mezcla historia izquierda/derecha.
- HUD, cartucho, rueda, compás, subtítulos y punteros mantienen su tamaño angular
  y alineación. La retícula actual tiene radios en píxeles: se mide su cambio
  angular y solo se adapta su dibujo si hace falta, sin tocar el rayo de disparo.

Host: proyectar puntos conocidos centrales/laterales/verticales a NDC y a ambos
layouts, comprobar frusta asimétricos, near/far, culling simulado, profundidad,
scissor y medio texel con imágenes leídas de GPU. La coordenada UV de un punto
debe conservarse con error de reproyección ≤0,5 píxel del layout menor. Los
puntos no centrados deben conservar la disparidad correspondiente a su distancia.
Visor: escala, convergencia, bordes, movimiento de cabeza, Focus, transparencia,
efectos y legibilidad. Un patrón GPU correcto no acepta el motor ni el visor.

## 6. Recursos, propiedad y transacción

Se propone una familia VR por dispositivo/generación/extent/layout, preparada
antes de admitir el primer par. No hay asignaciones por ojo/frame en régimen
estable. Los recursos nativos se retienen intactos; no se reemplazan globalmente
ni se destruyen para financiar la familia VR.

La preparación tiene dos confirmaciones: el hilo dueño D3D9 construye/comprueba
la familia y el anillo; el presenter prepara el par VR y el par de teatro en su
hilo D3D11. Un intercambio acotado de descriptores/tokens comunica disponibilidad
por dispositivo/generación/layout. La inicialización de sharing usa contenido
privado y una prueba de finalización, sin enviar ese contenido al visor. Mientras
falte una confirmación se conserva la presentación base; el hilo del juego no
espera activamente a una asignación o query del presenter. Confirmación atrasada,
timeout o generación cambiada cancelan la preparación, sin conceder retiro de
GPU por el hecho de haber transcurrido tiempo. La limpieza se ejecuta en el hilo
dueño y respeta las pruebas de finalización de cualquier inicialización encolada.

El adaptador CoJ identifica slots/contexto de escena, las dependencias de tamaño
y el momento seguro para seleccionar/restaurar la familia. D3D9 posee las
superficies/texturas, sus identidades y el estado del dispositivo. D3D11 posee
las salidas y la copia. Solo descriptores por valor, estado y tokens de identidad
cruzan las capas neutrales. Una referencia texture/surface de nivel 0 es un alias
de la misma asignación, no una nueva superficie de píxeles.

Si Q0 demuestra que no hay dependencia de historia entre ojos, se reutiliza
secuencialmente una familia de escena: el copy del ojo izquierdo precede a los
dibujos del derecho en el mismo dispositivo D3D9. El anillo de seis texturas
privadas evita que esa reutilización pise la imagen en transporte. Si hay historia
por ojo, su coste separado se incorpora al inventario antes de B0.

Orden de un ojo:

1. Comprobar propietario/cámara/dispositivo/generación, recursos listos y espacio
   en el anillo. Guardar el estado exacto que el ámbito pueda modificar y retener
   sus referencias; no entrar con restauración anterior pendiente.
2. Seleccionar el contexto VR cualificado. Aplicar recursos, caches/tamaños y
   viewport/scissor de cada pase mediante el adaptador exacto. Mantener su cache
   nativa de estado coherente con los binds reales.
3. Ejecutar el wrapper completo con óptica/pose existentes. No reejecutar gameplay.
4. Capturar mediante `CaptureEyeSurface` la salida final verificada W×H, con copia
   igual a igual. Registrar el descriptor real y el vínculo con ese ojo/pose.
5. Restaurar campos nativos y estado gráfico en orden compatible; verificarlo.
   Restaurar RTs y depth antes del viewport/scissor guardados. Las restricciones
   de formato, MSAA y dimensiones deben cumplirse también durante el rollback;
   véanse [SetRenderTarget](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setrendertarget)
   y [SetDepthStencilSurface](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setdepthstencilsurface).
6. Solo tras dos ojos completos y todas las restauraciones correctas, emitir el
   evento productor. D3D11 verifica layout/epoch, copia y compone el par antes
   de admitir su presentación. La aceptación lógica no equivale a Submit atómico:
   si falla el segundo Submit se invalida la autoridad del par, como en la ruta actual.

La lista guardada se determina por escrituras observadas: RT0..MRT activos,
depth, viewport, scissor y enable, shaders/declaración/FVF, streams/index buffer,
texturas/samplers, render states, constantes y transforms que toque el ámbito o
el espejo, más slots/caches nativos cualificados. Un state block puede ayudar,
pero no sustituye RT/depth, referencias, caches del motor ni la verificación.
No se presupone que «restaurar matrices» deshaga lo dibujado en una superficie.

El ámbito no captura un fallo nativo de acceso para seguir ejecutando un motor
posiblemente corrupto. El reporter SEH existente conserva la propagación de la
excepción. Las pruebas de rollback con excepciones usan fallos recuperables
controlados; no prometen recuperar una corrupción o un fallo fatal del juego.

La identidad del par incluye build/run, owner epoch, eye-layout epoch,
device/generation, frame y pose sequence. El epoch de layout invalida callbacks
atrasados incluso si coincide dispositivo/generación. Los leases existentes
siguen siendo la autoridad de retiro; no se introduce un flag que los salte.

## 7. Memoria y rendimiento

Sean `P=W*H`, `D=desktop_width*desktop_height` y `T=theater_output_width*height`.
Las cifras siguientes son **payload lógico sin padding**, en MiB (2²⁰ bytes).
No equivalen a VRAM residente, commit ni VA del proceso. La familia nativa
original permanece residente según sus reglas y no está incluida en la tabla.

| Ojo | Píxeles vs A | Un color de 4 B/px | Captura, 6 colores | Salida VR, 2 colores | Transporte+salida | + escena mínima color y depth de 4 B/px |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 2560×1440 | 1,000 | 14,06 | 84,38 | 28,13 | 112,50 | 140,63 |
| 2560×1920 | 1,333 | 18,75 | 112,50 | 37,50 | 150,00 | 187,50 |
| 2560×2560 | 1,778 | 25,00 | 150,00 | 50,00 | 200,00 | 250,00 |
| 2804×2860 | 2,175 | 30,59 | 183,55 | 61,18 | 244,73 | 305,92 |

«Escena mínima» es un ejemplo para presupuestar, no una afirmación de que el
motor solo necesita dos superficies. Modelo completo:

`M_vr = 32P + sum(bytes_por_texel_i * extent_i * muestras_i * copias_i por nivel)`.

La suma incluye cada color/MRT/depth/intermedio/historia privada, con formato y
reglas de tamaño reales. Un ejemplo de un color de 4 B/px a mitad y otro a cuarto
añade 5,86 MiB en B1 y 9,56 MiB en C; no demuestra ese número de efectos del motor.
HDR, MSAA, mips y doble buffering alterarían ese modelo; MSAA no se admite en P1.

Además:

- Teatro reservado: `12D` SYSTEMMEM (42,19 MiB con 2560×1440), más `8T` del par
  D3D11 de recuperación. `T` debe calcularse y medirse, no sustituirse por D.
  Reservar ese par separado añade memoria respecto al presenter actual que
  alterna un único par entre modos; se debe medir el incremento.
- Abrir las seis texturas compartidas en D3D11 no duplica su payload. El copy
  a dos salidas propias sí. Referencias COM, vistas, queries y metadatos no son
  gratuitos, pero no se cuentan como otras seis imágenes completas.
- Flat MSAA podría requerir un resolve temporal de `4D`; FSAA 0 evita ese caso
  en el candidato. No hay readback continuo de ojos en P1. Capturas de QA pueden
  añadir staging y CPU; se presupuestan y se separan de la medición de rendimiento.
- Mantener clásica a tamaño VR añadiría al menos `24P` SYSTEMMEM y su transporte
  de CPU; está excluido de P1. Un fallo de sharing no debe activar ese coste
  silenciosamente. La ruta experimental debe fallar antes de esa asignación y
  pasar al teatro reservado; el fallback clásico vigente queda para ruta base.
- El pico incluye recursos nativos retenidos, familia nueva, slots anteriores
  pendientes, cache D3D11 abierto, salida vieja aún en uso por compositor/GPU,
  par de teatro, overlays y temporales. No se contabiliza una liberación hasta
  demostrar su retiro. No mantener dos anillos completos por conveniencia.

En C, solo `32P` aumenta 132,23 MiB respecto a A. El incremento total es mayor
por escena/efectos/teatro separado/solapamientos y no puede fijarse antes de Q0.
La asignación inicial y el paso de modo pueden alcanzar el máximo, no el frame
estable. No se estima una capacidad segura a partir de la VRAM nominal de la GPU.

### Admisión de memoria

P1 exige proceso x86 LAA verificado y conserva la corrección de identidad de
32 bits. LAA aumenta capacidad de direcciones, no hace seguros todos los usos
nativos/JVM ni evita fragmentación. No se traduce `GetAvailableTextureMem` en
«memoria libre del proceso». Se miden por separado VA libre total y mayor bloque
libre (mapa completo), private commit/working set, commit disponible del sistema
y memoria GPU dedicada/compartida mediante una fuente identificada.

Política experimental inicial, conservadora y revisable mediante diseño, no un
default público: antes de preparar familia/transporte, exigir al menos
`512 MiB + presupuesto_incremental_pendiente` de VA libre total y un bloque
libre mayor o igual a `max(128 MiB, 2*mayor_reserva_contigua_estimada)`.
Los tamaños estimados proceden de Q0 e incluyen temporales conocidos; si no se
puede acotar la reserva de driver o completar el mapa, se declara presupuesto
desconocido y no se habilita B1. Durante el ensayo, VA libre por debajo de
512 MiB o mayor bloque por debajo de 128 MiB desactiva el experimento y conserva
fallback. Estas cifras son márgenes de admisión, no garantía de asignación.

Toda asignación todavía debe comprobar HRESULT/objeto/handle. Una observación
pasiva de memoria no reserva capacidad contra una carga de nivel posterior.
Se repite el muestreo en carga/transición y una vez por segundo fuera del render
crítico; una invalidación de presupuesto se comunica al hilo dueño, sin liberar
recursos desde el observador. Si GPU/system budget no se puede medir con
fiabilidad, no se escala a C basándose solo en VA o en ausencia de error.

No se predice tiempo proporcional a P. CPU de motor, dos wrappers, geometría,
overdraw, shaders, cachés, ancho de banda, copias, efectos, esperas y compositor
tienen costes diferentes; un salto de residencia puede producir colas no lineales.
La copia del espejo y conservar el teatro tienen costes que deben aislarse con B0.

## 8. Fallos, retiro y fallback

Estados propuestos: `disabled → qualifying → preparing → ready → in_eye → ready`;
errores pasan a `retiring → baseline_theater` o `quarantined`. Son autoridad de
resolución, no nuevos estados de gameplay. Un fallo experimental queda enclavado
durante esa ejecución: no hay bucle de asignar/reintentar ni descenso secreto de
resolución. Reiniciar una sesión nueva es necesario para volver a intentarlo.

| Suceso | Obligación |
| --- | --- |
| Caps/layout/budget inválido | No mutar estado nativo; registrar rechazo y continuar ruta base. Verificar formatos, límites D3D9/D3D11 y compatibilidad de depth, no solo W/H. |
| Asignación parcial de escena/anillo/salida | Recursos nuevos privados se publican solo cuando están completos y comprobados. Liberar los que nunca se usaron; originales y teatro permanecen utilizables. |
| Anillo saturado o captura cancelada | No esperar ni sobrescribir leases. Reutilizar únicamente la reserva sin fence/publicación permitida por la corrección actual. Repetición solo dentro de freshness vigente y mismo propietario. |
| Fallo de copia/sharing | Rechazar ambos ojos; no activar readback clásico experimental. Mantener leases inciertos, restaurar escena y seleccionar nueva captura natural para teatro. |
| Cambio de owner, pausa, película, muerte o tracking | Cancelar autoridad inmediatamente según las barreras actuales. Una transición intencionada a teatro no es fallo: conserva familia preparada si sigue válida, sin autoridad para publicar frames anteriores. |
| Reset/ResetEx intentado, exitoso o fallido | Parar admisión experimental e invalidar epoch antes de reenviar. No alterar dimensiones originales ni fabricar éxito. Retirar recursos según sus reglas; tras el resultado, no reutilizar campos/superficies de escena previos sin nueva cualificación. Para P1 se permanece en ruta base el resto de la sesión. |
| Device/context replacement o device removed | Nada de reabrir recursos viejos sobre otro dispositivo. Mantener cuarentena si no existe prueba de retiro; volver a escena natural solo cuando el dispositivo nativo sea funcional. |
| Restauración incompleta | Invalidar par, retener referencias y registro de recuperación y bloquear más escrituras. No «continuar» sobre una mezcla de familia VR/nativa. Si no se puede restaurar por dispositivo perdido, no se promete teatro operativo: se conserva salida nativa cuando sea posible y se requiere cierre normal/reinicio. |
| Quit | Restaurar en el hilo dueño antes de destruir juego; usar el límite actual pre-DestroyGame y drenaje acotado. No marcar como retirado un lease abandonado ni borrar pruebas del error. |

No existe una garantía de imagen funcional si el dispositivo nativo ha dejado
de funcionar. El contrato alcanzable es rollback verificado cuando la API lo
permita, no publicar/modificar estado parcial y conservar una vía original para
la siguiente ejecución. Un requisito de recuperación automática de todo device
loss necesitaría otro diseño; no se fingirá que ya lo ofrece el prototipo.

La primera preparación reserva el teatro y valida su primer contenido antes de
habilitar VR independiente. El fallback ordinario usa un **Present nativo nuevo**,
no un ojo VR retenido ni un par de otro modo. Puede conservarse el último par
válido durante interrupción transitoria solo bajo el propietario actual y la
ventana existente de 250 ms; no se amplía ni renueva por admitir otro frame.
Pausa, muerte, pérdida de propietario y restauración fallida invalidan al instante.

Los objetos por liberar permanecen en su cola de retiro hasta prueba del evento
productor/consumidor correspondiente. Un timeout nunca concede reutilización.
Las salidas D3D11 que ya se hayan enviado también necesitan un límite de uso
seguro del compositor, además de los eventos de copy: Q0/P0 deben cualificarlo
con el SDK OpenVR fijado. Una referencia COM o un solo frame de espera no lo prueban.

Los hooks de Reset actuales no constituyen por sí solos este contrato: antes de
admitir P1 se prueban los nuevos recursos dentro de Reset/ResetEx y sus fallos.
No se bloquea el hilo de render esperando colas indefinidamente. Si la API/límite
nativo no permite retirar una dependencia a tiempo, esa combinación no se admite
como recuperable; el candidato no pasa la puerta de seguridad.

## 9. Medición reproducible y aceptación

Cada ejecución usa `prepare`/`finish`, run ID nuevo, hashes/build/source/flags y
guardado/escena identificados localmente. SteamVR y juego se arrancan/cierran
manualmente. Los datos crudos, capturas y comparativas van a `work/` y evidencia
ignorada; aquí y en VALIDATION solo se versionan contratos y conclusiones.

Comparar A, B0 y B1 con el mismo guardado, cámara/pose de referencia registrada,
GPU/driver, Hz del visor, opciones de juego, reloj/estado térmico y preferencia
cinemática. Desactivar grabación en la comparación principal. No cambiar SteamVR
supersampling ni añadir sharpening. Hacer al menos tres ejecuciones de cada ruta,
alternando A/B para reducir sesgo térmico. Cada muestra: 60 s de calentamiento,
180 s de escena estable y recorrido repetible de 120 s con giros/movimiento;
separar tiempos de carga y transiciones. Una sesión sostenida de 15 min en B1
incluye los cambios de propietario, sin repetir pruebas funcionales ajenas.

### Qué se registra

| Dominio | Medición y prueba de verdad |
| --- | --- |
| Extensión real | Descriptor de salida de escena, depth durante geometría, viewport/scissor de draws y todos los intermedios; ring compartido, source/destination D3D11 y textura enviada. Debe existir cadena de identidad por ojo/pose/epoch, no solo GetDesc al final. |
| Detalle nuevo | Fixture host con geometría subpíxel y patrón de alta frecuencia, y escena nativa con bordes/texto/material distante en pose comparable. Comparar imágenes nativas antes de composición y ambos ojos; no ampliar B desde A. La evidencia de draws mayores más detalle nuevo distingue rasterizado de interpolación. |
| Ópticas | FOV/frustum/near/far y matrices del pase; landmarks normalizados en ambos tamaños y disparidad, centro y periferia. Pose y contenido tienen que ser comparables; movimiento del operador no es prueba cuantitativa de distorsión. |
| CPU | QPC alrededor de cada wrapper, selección/restauración, copia encolada, espejo, composición y Submit; p50/p95/p99 y máximos por bloque. Un tiempo de enqueue no se etiqueta como tiempo GPU. |
| GPU | Queries D3D9 timestamp/frequency/disjoint por pase/efecto y eventos de copia; D3D11 timestamp-disjoint para copy/composición. Leer diferido y sin espera; query inválida/disjoint produce dato no disponible, no cero. Si faltan queries, usar captura/traza GPU externa identificada; los tiempos CPU actuales no sustituyen ese dato. |
| Cadencia | Tiempo entre pares nuevos, nuevos/repetidos/drops, edad de frame p50/p95/p99/max, esperas productor/consumidor, Hz configurado y logrado, reprojection/dropped frames OpenVR con sus límites de observación. |
| Memoria | VA total libre/mayor hueco y completitud de mapa, commit privado/sistema, working set, GPU dedicada/compartida, contadores de recursos y picos de preparación/transición/retiro. Diferenciar payload estimado de residencia medida. |
| Recuperación | Fault ID y etapa, snapshot anterior/posterior, owner/layout epoch, nuevos pares tras recuperar, estado de leases y counters finales. Un archivo de evidencia completo no convierte en pass una verificación fallida. |

Instrumentación detallada y readback de nitidez se ejecutan en un perfil de QA
separado. Para tiempos finales se usan contadores/queries acotados, sin dumps
ni captura de píxeles en el bucle. Medir A con/sin instrumentación; si perturba
p95 más del 2%, no usar esa muestra para concluir rendimiento. La telemetría
actual de `summarize_native_stereo_run.ps1` se reutiliza y amplía donde falte
información; sus `gpu_copy_queue_ms` miden enqueue, no duración de rasterizado.

### Criterios y decisión

**Corrección obligatoria, sin tolerancia a excepciones:**

- Escritorio, UI/puntero y fuente/extent geométrico de teatro iguales al control;
  ambos ojos y todos los recursos dependientes a sus extents cualificados.
- Cadena completa de geometría → salida → captura → copia → envío en al menos
  diez pares correlacionados por escena/ruta y en las transiciones ejercitadas.
  Cero frames de ojos mezclados, escalado ascendente desde escritorio o región
  de resolución antigua escondida dentro de una textura mayor.
- B0 mantiene proyecciones/landmarks y apariencia de efectos del control;
  B1 mejora detalle vertical en regiones angulares equivalentes y no reduce
  muestras horizontales. No se exige una mejora horizontal que B1 no ofrece.
- Todos los fallos inyectados restauran estado verificable o rechazan más
  mutación en cuarentena; ninguna ruta publica un par parcial/obsoleto. Cero
  leases pendientes/abandonados al salir saludablemente; asignaciones/handles
  y counters concuerdan después del retiro.
- Menús, teatro, cine, HUD, tracking, manos, armas, interacciones y recarga manual
  conservan su comportamiento aceptado en los contextos realmente ejercitados.
  Si falta escena/arma, queda pendiente; no se promociona por inferencia.

**Presupuesto de rendimiento experimental:** sea `B=1000/Hz` ms. A y B1 se
comparan a los Hz actuales sin rebajarlos silenciosamente. Para llamar usable
a B1, p95 de producción de par y de ruta GPU crítica ≤0,9B, p99 del intervalo
entre pares nuevos ≤1,25B, y p99 de edad de frame no empeora más de un B respecto
a A. La fracción de reprojection/drops no puede aumentar más de un punto
porcentual ni mostrar cola creciente o un nuevo periodo sostenido de más de
2 s sin pares nuevos durante gameplay enfocado. Si A ya incumple, no se concede
aceptación por comparación relativa: se conserva resultado de viabilidad y se
revisa tamaño/presupuesto antes de pedir otro ensayo en visor.

Memoria: cumplir los márgenes del apartado 7, cero errores de asignación y
ninguna acumulación por transición. Tras calentamiento, diez ciclos de
juego/pausa/teatro vuelven al mismo inventario vivo; crecimiento monotónico de
VA/commit o contadores requiere explicación y bloquea aceptación. El delta de
memoria observado debe poder conciliarse con recursos y solapamientos; no se
atribuye automáticamente a fuga cualquier cache nativa que también crezca en A.

Visor: aceptación explícita de mejora útil, ausencia de deformación/doble imagen,
legibilidad y movimiento estable. Se rechaza el tamaño si no aporta nitidez
útil, empeora comodidad/efectos o rompe una función aceptada, aunque pase host.
Se puede aceptar el mecanismo B0 y descartar B1; eso no autoriza otra dimensión
sin su presupuesto y comparación. C se evalúa solo después de B1/B2 seguros.

## 10. Plan incremental y puertas de revisión

Esta secuencia conserva las puertas técnicas del diseño aprobado. La autorización
de avance autónomo permite implementar sus requisitos; no sustituye la evidencia
nativa ni la aceptación física necesarias para habilitar un escritor.

| Etapa | Trabajo limitado | Salida exigida / condición de parada |
| --- | --- | --- |
| Q0 — propiedad | Cualificar alloc/bind/use/release, tamaños internos, efectos, espejo/sprites y retiro OpenVR. Usar observadores existentes; proponer observación mínima adicional si falta. | Tabla exacta del apartado 3 completa con fuentes estáticas y evidencia de ejecución. Ningún escritor habilitado con un desconocido. Si no hay contexto aislable, revisar viabilidad. |
| P0 — host | Política fija, preparación privada, transacción D3D9, cadena de identidad, pares D3D11 por modo y rollback. | Pruebas GPU de color/depth/MRT, viewport/scissor, projection, segunda asignación fallida, wrong device, owner loss, pendientes y fallos de copy/Submit/ResetEx. Regresión roja antes de cada corrección. Cero cambio en juego hasta superar esto. |
| B0 — aislamiento | Familia separada con 2560×1440; sesión canónica. | Mismo escritorio/teatro, efectos/ópticas y baseline funcional. Descriptor distinto por propiedad, no solo nombre de recurso. Memoria y espejo dentro del presupuesto. |
| B1 — primer incremento | Nueva ejecución fija 2560×1920, con presupuesto completo y A/B. | Detalle vertical real, dos ojos coherentes, rendimiento/memoria/rollback según apartado 9 y aceptación del operador. Fallo implica volver a la base, sin modificar la publicación. |
| B2/C — extensión opcional | Cuadrado y recomendación observada en ejecuciones separadas. | Mismas puertas; nuevos datos de memoria, aspecto y tiempo. Ninguna adopción automática ni interfaz nueva. |

Pruebas host de recuperación mínimas: fallo en cada creación de color/depth/
intermedio/query/salida, layout de un ojo inválido, source restablecido antes de
captura, excepción durante el wrapper con limpieza permitida, restores fallidos,
owner cambiado entre ojos/copy/envío, frames viejos, dos consumers pendientes,
fallo de prueba de retiro, Reset y ResetEx correcto/fallido, dispositivos/contextos
reemplazados, bloqueo de variante clásica experimental y teardown con GPU pendiente.
La prueba no puede marcar retirado un recurso solo porque terminó el fixture.

Visor agrupado: entrar desde menú/carga, mover cabeza/manos, caminar/snap/Focus,
disparar, una recarga manual antes de vaciar, Hands y una interacción disponible;
pausa/dashboard y cine cine/inmersivo cuando estén a mano, vuelta a estéreo,
HUD/subtítulos nativos según soporte existente y salida normal. No se fuerza
una escena ausente ni se reabre aceptación de cámara/calibración ya estable.
Reset/device loss en host no aceptan su equivalente físico.

Estados permanecen separados: diseño `planned`; política/recursos `implemented`;
pruebas deterministas `host-tested`; datos del motor `live-tested`; nitidez,
comodidad y continuidad observadas `headset-validated`. Nada de ello implica
campaña completa o `supported`. VALIDATION conserva cada aceptación real.

## 11. Riesgos y veredicto de diseño

| Riesgo principal | Mitigación / salida |
| --- | --- |
| Caches globales o un target no clasificado | Q0 bloquea el escritor; no ampliar por heurística de dimensiones. |
| Aspecto nuevo con efecto/culling de escritorio | Verificación dentro de cada pase y B0/B1; rechazar familia incompleta. |
| Pico de VA x86 o asignación nativa posterior | Presupuesto incremental, márgenes, fallback reservado y pruebas de carga; LAA no elimina este riesgo. |
| Estado parcial o recursos todavía en GPU/compositor | Transacción verificada, epoch, leases y prueba de retiro; cuarentena sin reutilización por timeout. |
| Perder manos/recarga/puños durante transiciones | No tocar sus políticas; preservar identidad de 32 bits y comprobar baseline en B0/B1. |
| Confundir texturas mayores con render real | Evidencia de draws/intermedios y detalle nuevo anterior al compositor. |
| Sobrecoste no lineal | Medir p95/p99, memoria y reprojection; no justificar C con la relación de píxeles. |

El diseño aprobado permite preparar un prototipo acotado **después** de cerrar
Q0. Incluye B0 al tamaño actual, B1 a 2560×1920, el espejo reducido con bandas, el par de teatro
reservado, los presupuestos y el fallback con cuarentena. Ninguna autorización
elimina la condición de parada por recursos nativos desconocidos.

## 12. Requisito implementado: observación de bindings

`backends/d3d9/render_resource_probe.hpp/.cpp` contiene el observador de valores
y su colector limitado. Lee MRT, profundidad, 16 samplers de píxel y cuatro de
vértice cuando proceda, alias superficie/textura, viewport, scissor, Z y shaders.
View/projection del estado fijo se registran con validez separada: no demuestran
la proyección usada por un shader programable. Las referencias COM adquiridas
se liberan dentro de cada consulta; las identidades copiadas no son leases ni
pruebas de vida, residencia o asignaciones únicas.

`DeviceHookCallbacks.resource_probe_event` observa después de SetRenderTarget,
SetDepthStencilSurface, SetTexture, SetViewport, SetScissorRect, Clear, las cuatro
variantes de DrawPrimitive y StretchRect. Reenvía argumentos una vez y preserva
HRESULT y LastError. Reset invalida antes de llamar al original; ResetEx también
cuando su interfaz comparte la vtable observada. Este hook extendido solo
invalida el diagnóstico, sin ejecutar política de Reset clásico ni modificar
sus parámetros. Una vtable extendida distinta queda sin cualificar. Creación,
destrucción y cambios internos no interceptados aún necesitan su mapa.

`CameraStereoRuntimeCallbacks.observe_render_pass` delimita exclusivamente el
wrapper completo en gameplay, ajustes y cine inmersivo. Captura, transporte y
replay natural de recuperación quedan fuera. El proxy correlaciona thread,
frame, owner nativo, dispositivo y generación del diagnóstico. Los intentos de
Reset/ResetEx cambian su época aun si fallan; este contador no reemplaza la
generación de transporte. Cancelación, identidad perdida, eventos del mismo
dispositivo en otro hilo, lectura fallida, hooks perdidos/recuperados o
desbordamiento impiden declarar completo el scope. La continuidad de slots se
comprueba al abrir/cerrar y los cambios conocidos de propiedad avanzan una época;
esto no prueba ausencia de cambios internos invisibles entre observaciones.

Activación **solo al arrancar**: control `renderResourceProbeEnabled`, falso por
defecto. `vr_test.ps1 prepare -BodyIkAtStart -RenderResourceProbe` lo propaga al
control y a la procedencia de staging/run. Tracking/body/recenter lo preservan.
Para ejercitar la recarga manual, conservar además su opción `-ManualReload`.
No se añade interfaz al panel ni se modifica la pre-release publicada.

El presupuesto global es seis intentos de par y 4096 registros distintos por
ojo. Selecciona el primer frame elegible después del arranque, cambios de owner,
ruta o generación y después intervalos superiores a 900 frames. Agrupa registros
idénticos consecutivos conservando el conteo; saturación marca `truncated` y no
amplía el límite. Reserva un buffer de CPU por hilo admitido y lo reutiliza;
no crea superficies GPU. Las consultas/logs pueden alterar mucho el pacing:
estas sesiones **no cualifican rendimiento** y no sustituyen A/B sin diagnóstico.

`tools/analyze_coj_render_resources.py` exige un solo intervalo exacto run/build,
scopes ordenados y ojos con igual identidad; verifica esquema, conteos, reads,
draws, frames crecientes y truncamiento. Devuelve `observed` para bindings
completos o `incomplete` con motivos, siempre `resizingAdmitted=false` y
`headsetValidated=false`. Los marcadores correlacionan; no autentican el log ni
reemplazan el inventario canónico. Crear la salida fuera de la evidencia.

Las pruebas de host cubren D3D9 y D3D9Ex reales, píxeles/estado invariantes,
profundidad presente/ausente, alias, liberación antes de Reset, ResetEx,
coalescencia, límite, hilo/identidad inválidos, cancelación, pérdida y recuperación
de hooks, resultados originales, controles y rechazo del analizador. Esto no
cualifica el grafo del juego, retiro OpenVR, nitidez ni continuidad en visor.
Q0 y el escritor B0/B1 siguen pendientes.

La primera captura real del colector cubre gameplay sin fallos de lectura ni
truncamiento, con continuidad funcional confirmada por el operador en los
contextos probados. El analizador sigue incompleto: un pase nativo combina color
FourCC `NULL` con viewport/profundidad mayores que su descriptor de color.
[La investigación de resolución](COJ_VR_RENDER_RESOLUTION.md#native-target-family-finding-from-the-gameplay-census)
recoge esta frontera; clasificarla con los datos existentes antes de repetir
captura. Esto no acepta Q0, ópticas programables, nuevas dimensiones ni pacing.

La entrada/salida de llamadas nativas y la apertura/cierre del observador
comparten un monitor de llamadas en vuelo; su mutex nunca se retiene al ejecutar
el original. Así se rechaza también una llamada extranjera cuyo callback de
salida aún no llegó. La mutación de hooks y la publicación/lectura de su época
se sincronizan entre sí. La inspección comprueba el dispositivo vigente bajo
su mutex antes de desreferenciarlo; un cambio de dispositivo invalida la época.
El buffer añade a VA/commit de CPU `sizeof(D3D9RenderResourceRecord) * 4096`
bytes por hilo admitido, como máximo seis hilos por presupuesto, además de
scratch de serialización/log. El arranque registra los bytes del build; este
coste del diagnóstico se elimina del experimento A/B, no se carga a VRAM.

## Referencias de API y límites de evidencia

- [OpenVR: tamaño recomendado](https://github.com/ValveSoftware/openvr/wiki/IVRSystem::GetRecommendedRenderTargetSize): dato de densidad/óptica, no orden al motor de redimensionar su escena.
- [OpenVR: API fijada en cabecera oficial](https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h): contrastar submit, poses y límites de uso con la versión del SDK local antes de implementar retiro de salidas.
- [D3D11 CopyResource](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copyresource): una copia no introduce muestras geométricas; se conserva layout compatible.
- [Consultas D3D9](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dquerytype) y [consultas D3D11](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_query): separar timestamp/evento/duración CPU y descartar datos temporales inválidos.
- [Límites de memoria de Windows](https://learn.microsoft.com/en-us/windows/win32/memory/memory-limits-for-windows-releases): capacidad de direcciones no equivale a residencia de recursos gráficos.

La documentación oficial limita la API; no demuestra cómo usa ChromeEngine cada
recurso. El mapa exacto del motor debe provenir del binario fijado y de su
ejecución correlacionada. Esta especificación conserva esas incertidumbres como
puertas verificables, sin sustituirlas por direcciones o métricas inventadas.

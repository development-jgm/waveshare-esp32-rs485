# Plan del proyecto — Control RS485 Waveshare ESP32

Control y monitorización de máquinas de lavandería para **McClean La Cuesta**
(shopId 50), dentro del sistema Lavamax SmartKiosk.

---

## Topología actual

Un Particle Photon 2 hace de puente entre Particle Cloud y un bus RS485
(9600 8N1) con tres módulos Waveshare ESP32-S3-POE-ETH-8DI-8RO.
ID del Photon: `0a10aced202194944a05320c`.

| Dirección RS485 | Máquinas | machineId |
|---|---|---|
| 1 | Secadoras 6–9 | 99, 100, 101, 102 |
| 2 | Lavadora 1 | 94 |
| 3 | Lavadoras 2–5 | 95, 96, 97, 98 |

Cada máquina usa un canal de relé para la activación y dos entradas digitales
optoacopladas (enchufada / en marcha). El bus lleva 120 Ω de terminación en
ambos extremos. Firmware Waveshare **v1.1.0**. La tabla de máquinas está en
`firmware/easyCleanApp-RS485/src/EasyClean-RS485.cpp`.

**Convención de cableado.** `INPUT_PULLUP`; opto en ON = pin a nivel bajo = bit 0.
La salida de "en marcha" de las secadoras es un 4N25 dedicado cuyo opto está
**en OFF mientras la máquina funciona**, así que `isRunning` = bit a 1.

Conviene fijarse en la asimetría que esto provoca: con la máquina parada la
entrada está clavada a masa por el opto (baja impedancia, robusta), y con la
máquina en marcha queda sostenida únicamente por el pull-up (alta impedancia,
sensible a ruido). Por tanto un glitch solo puede fingir una **parada**, nunca
un arranque. Ese detalle es el que justifica que la confirmación de estado se
aplique en un solo sentido.

---

## Hecho

### Sistema base
- Protocolo RS485 direccionado de 9 bytes: `[DEVICE_ADDRESS] + [payload de 8 bytes]`.
  Cada módulo descarta los paquetes que no van dirigidos a él. El CRC-16 Modbus
  se calcula solo sobre los 8 bytes del payload, así que las tablas de comandos
  son independientes de la dirección.
- Comandos de relé explícitos ON (0xFF) / OFF (0x00) en lugar del toggle (0x55),
  que deja el relé pegado si se pierde el segundo comando.
- Tres módulos flasheados, direccionados y verificados sobre el bus compartido.
- Firmware del Photon 2 `EasyClean-RS485.cpp` con las funciones cloud
  `activateMachine`, `testMachineIsPowered`, `testMachineIsUnderUsage`,
  `testConnectionToShop`, `publishNetworkInfo`, `rescanBus` y la variable
  `modules`.
- Detección de pago en efectivo: una máquina que arranca sin activación desde la
  nube publica `SupabaseCashPayment/`.
- Evento `MachineActivated` publicado en cada pulso de relé, como traza de
  auditoría.

### El incidente del 31-07-2026 — causa raíz y corrección
El razonamiento completo está en [Lecciones aprendidas](#lecciones-aprendidas).
Resumen de los cambios:

| commit | cambio |
|---|---|
| `3bdadb0` | Serializar el tráfico RS485: una trama por ciclo de `loop()` |
| `658d88b` | Ordenar la tabla de máquinas por `machineId` |
| `f52120b` | Sondear primero el device activado en el ciclo posterior al pulso |
| `e2629d8` | Publicar `MachineActivated` en cada activación |
| `e2a544f` | **Corregir el desbordamiento del buffer de recepción RS485 del Waveshare** |
| `7386d35` | Consulta de versión de firmware y escaneo del bus |

---

## Qué hemos ganado

- **El origen de los registros falsos está corregido de raíz**, no tapado. Unos
  250 registros de pago espurios en 22 horas resultaron venir de una corrupción
  de memoria, no de los sensores, ni del cableado, ni de las máquinas.
- **Visibilidad remota de los módulos.** La variable `modules` del Photon informa
  de la versión de firmware de cada módulo y distingue uno sano de uno que sigue
  con firmware antiguo (`legacy`) o de dos módulos compartiendo dirección
  (`conflict`). Antes esto solo podía deducirse observando el comportamiento.
- **Un bus que aguanta crecer.** El tráfico está serializado y la recepción
  acotada, así que añadir un cuarto módulo ya no puede corromper a los demás.
- **Funciones cloud que no pueden bloquear el bus.** Todas levantan un flag o
  leen estado cacheado; ninguna toca `Serial1`.
- **Una base medida para los ajustes.** Los tiempos reales de rotación de las
  máquinas se obtuvieron de una semana de datos limpios, no de una estimación.

---

## Lecciones aprendidas

**Un bug latente puede seguir siendo inalcanzable hasta que el sistema escala.**
`RS485_Loop()` leía los bytes de `available()` directamente sobre `buf[20]` sin
acotar. Con un solo módulo, lo máximo que puede haber en el buffer es una
consulta de 9 bytes más una respuesta de 9: **18 bytes**, justo por debajo del
límite. Al añadir dos módulos pasaron a circular seis tramas (~54 bytes) por
ciclo de sondeo, y todos los módulos las ven porque el bus es compartido. El
enlazador coloca `lidarSerial` justo detrás de `buf`:

```
0x3fca1c24  buf          (20 bytes)
0x3fca1c38  lidarSerial  <-- se sobrescribía
```

de modo que el desbordamiento corrompía el propio objeto UART. El fallo estaba
ahí desde el principio; la ampliación solo lo hizo alcanzable.

**La correlación señaló al culpable equivocado hasta que tuvimos una ventana de
control.** El 94,7 % de los registros falsos se concentraban en las secadoras, lo
que las hacía parecer culpables. Sobre esa base se construyeron dos hipótesis
—que la inversión del tambor cortaba el sensor, y luego que se corrompían las
tramas RS485— y **ambas eran erróneas**. Lo que zanjó el asunto fue exportar la
semana *anterior* al cambio: 168 registros con exactamente **1** intervalo por
debajo de 120 s, frente a 300 registros con **217** después, y el inicio acotado
al 31-07-2026 a las 15:12 hora local (14:12 UTC; la tienda está en UTC+1, no
UTC+2). Mismas máquinas, mismo cableado, mismos
optos. Eso convirtió un juego de adivinanzas en una regresión con hora de inicio
conocida.

**Pedir los datos de "antes" cuanto antes.** Costó menos que cualquiera de las
hipótesis que refutó.

**La salida de estado de un fabricante no es un contactor de motor.** Leer el
esquema del 4N25 descartó la teoría de la inversión del tambor en un solo paso.
Conseguir la documentación del hardware antes de teorizar sobre su
comportamiento.

**Las funciones cloud de Particle tienen que ser no bloqueantes.** Se ejecutan en
el hilo de aplicación; un `delay()` dentro de una permite que
`Particle.process()` reentre en `loop()`, y ambos acaban escribiendo en el mismo
bus RS485. La regla: validar el argumento, levantar un flag y salir. Las lecturas
devuelven estado cacheado y no tocan el bus.

**Una trama RS485 por ciclo de loop.** El Waveshare consulta su UART desde una
tarea que corre cada 50 ms y solo interpreta paquetes de exactamente 9 bytes. Dos
tramas seguidas se fusionan en su buffer y se descartan enteras, que es lo que
hacía perder comandos `CMD_OFF` y dejaba un relé pegado.

**Un flasheo que informa de éxito no ha surtido efecto necesariamente.** El módulo
sigue ejecutando el firmware antiguo hasta que se le hace un power-cycle por USB;
`--after hard-reset` no basta. Esto costó tiempo de depuración en dos ocasiones,
y por eso existe ahora la consulta de versión.

**Nunca tomar una acción irreversible a partir de una sola muestra.** No había
filtrado en ningún punto de la cadena: `digitalRead` cada 20 ms sin antirrebote →
`DIN_Flag` → respuesta RS485 → decisión de una sola muestra en el Photon →
registro de pago en Supabase. Una única lectura mala bastaba para inventarse una
venta.

**Instrumentar antes de parchear.** Aplicar los filtros defensivos antes de
corregir el desbordamiento habría ocultado si la corrección de fondo funcionaba.

---

## Pruebas en curso

**1. Un ciclo completo de secado debe producir un registro por uso.** Es la prueba
de aceptación de `e2a544f`. Antes de la corrección, la máquina 100 generó 17
registros en 13 minutos.

```sql
select machine_id, count(*), min(created_at), max(created_at)
from machine_usages
where machine_id between 94 and 102
  and created_at > '2026-08-01 13:00:00+00'
group by machine_id
order by machine_id;
```

Un registro por uso cierra el caso. Una secadora con quince significa que queda
una segunda causa.

**2. Estabilidad del escaneo del bus en arranque en frío.** El primer escaneo tras
reflashear los módulos reportó la dirección 3 como `conflict`, luego `legacy`, y
después se estabilizó: 8 escaneos consecutivos limpios. Lo más probable es que el
módulo aún se estuviera estabilizando tras su power-cycle, pero la dirección 3
lleva cuatro máquinas colgadas, así que conviene revisarlo en el próximo arranque
en frío.

```bash
curl "https://api.particle.io/v1/devices/0a10aced202194944a05320c/modules?access_token=$TOKEN"
curl https://api.particle.io/v1/devices/.../rescanBus -d access_token=$TOKEN -d arg=""
```

**3. Pago en efectivo end-to-end** contra el flujo real de la aplicación.

---

## Pendiente

### Capas defensivas (diseñadas, deliberadamente sin aplicar)
Se han dejado en espera para no enmascarar el resultado de la prueba 1. Ambas son
cambios solo del Photon, por OTA.

- **Confirmación de parada.** Exigir que el sensor de marcha lleve 2 minutos
  caído antes de dar la máquina por parada. Se aplica únicamente a la transición
  a parada, ya que los glitches solo pueden fingir una parada, de modo que la
  detección de arranque no gana ni un segundo de latencia.
  `testMachineIsUnderUsage` pasaría a devolver el estado confirmado en lugar del
  bit crudo, lo que además evita que una secadora aparezca como libre a mitad de
  ciclo.
- **Bloqueo de 30 minutos** entre registros de pago en efectivo de la misma
  máquina. El valor sale de una semana de datos limpios: las rotaciones reales
  son de ≥32 min (una máquina no puede reutilizarse antes de que termine su
  ciclo) y los duplicados falsos están por debajo de 14 min, dejando una banda
  vacía en medio. Un bloqueo de 30 minutos descarta 3 de 159 registros, y esos
  tres son a su vez sospechosos de ser duplicados. **No** debe condicionar a
  `activateMachine`: un pago por app tiene que disparar el relé siempre.

### Front end
- Sustituir la comprobación única posterior a la activación por una ventana de
  reintentos de 20–30 s. El firmware refresca el estado cacheado en ~1,6 s tras
  el pulso del relé; la latencia real es el arranque físico de la máquina.

### Hardware
- Sustituir el MAX3485 por un módulo RS485 con aislamiento galvánico.

### Integración
- Conexión de Lavamax SmartKiosk al flujo real de la aplicación.

### Datos
- Decidir qué hacer con los ~250 registros falsos del 31-07 y 01-08 de 2026.

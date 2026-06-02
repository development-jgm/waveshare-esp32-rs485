# EasyClean-FW — Plan de mejoras

> Archivo de seguimiento para la refactorización incremental del firmware.
> Actualizar el estado de cada tarea conforme se implemente.

---

## Contexto del proyecto

Firmware Particle IoT que controla hasta 6 lavadoras/secadoras por dispositivo en hoteles y apartamentos.
Gestiona activación vía cloud functions y detección de pago en efectivo por GPIO.

- **Archivo principal:** `src/EasyClean-FW.cpp` (~1200 líneas, ~50% código comentado)
- **Plataformas soportadas:** Argon, Boron, Photon2, con opción EthernetFeatherWing
- **Backend:** Supabase (migración desde Azure en curso)
- **Tiendas desplegadas:** 30+ (ver `shops_history.txt`)

---

## Leyenda de estado

| Estado | Significado |
|--------|-------------|
| `[ ]`  | Pendiente |
| `[~]`  | En progreso |
| `[x]`  | Completado |

---

## CRÍTICO

### C1 — Credenciales WiFi hardcodeadas en código fuente
- **Archivo:** `src/wifi_credentials/setup-hotspots.h` (líneas 8–20)
- **Problema:** SSIDs y contraseñas reales en texto plano versionadas en git. Redes personales y de hoteles expuestas.
- **Solución:** Migrar a gestión de credenciales vía Particle cloud o provisioning por Listening Mode. Eliminar el archivo del repositorio (añadir a `.gitignore`).
- **Estado:** `[ ]`

---

### C2 — Acceso a array fuera de límites (crash / corrupción de memoria)
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 90–122)
- **Problema:** `getMachineIndexFromMachineId()` devuelve `-1` si no encuentra la máquina, pero **ningún caller valida ese retorno**. Esto provoca accesos como `activateMachineFromCloudFlag[-1] = true` → comportamiento indefinido y posible crash.
- **Solución:** Añadir validación del retorno en todos los callers antes de usarlo como índice de array:
  ```cpp
  int idx = getMachineIndexFromMachineId(id);
  if (idx < 0) { /* log error y return */ }
  ```
- **Estado:** `[ ]`

---

### C3 — Lógica invertida en `testMachineIsPowered()`
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 105–116)
- **Problema:** Los pines usan `INPUT_PULLUP` (HIGH = apagado, LOW = encendido), pero la función devuelve `1` cuando lee `LOW` y `0` cuando lee `HIGH`. La API reporta el estado contrario al real.
- **Solución:** Corregir la lógica de retorno o añadir un comentario explicativo si la inversión es intencional por hardware externo.
- **Estado:** `[ ]`

---

## ALTOS

### A1 — Configuración de tienda hardcodeada, requiere recompilación
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 52–64)
- **Problema:** `shopId`, `machineId[]`, `tariffId[]`, `usagePrice[]` están en el código fuente. Cambiar cualquier valor implica recompilar y reflashear. Con 30+ tiendas es un riesgo operacional alto.
- **Solución propuesta:** Obtener la configuración desde Supabase al arrancar (ya existe infraestructura). Usar el `deviceId` de Particle como clave para buscar la configuración correcta.
- **Nota:** `usagePrice[]` ya tiene un comentario `// ToDo: Get this from the cloud` (línea 56).
- **Estado:** `[ ]`

---

### A2 — Race condition: flags compartidas entre ISR y loop sin `volatile`
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 69–73)
- **Problema:** `checkAvailableMachinePinLow[]` y `checkAvailableMachinePinHigh[]` son escritas en las ISR y leídas en el loop principal sin `volatile` ni sincronización. El compilador puede optimizarlas incorrectamente.
- **Solución:** Declarar las flags compartidas con `volatile`:
  ```cpp
  volatile bool checkAvailableMachinePinLow[] = {false, false, false, false, false, false};
  volatile bool checkAvailableMachinePinHigh[] = {false, false, false, false, false, false};
  ```
- **Estado:** `[ ]`

---

### A3 — Loop hardcodeado a 6 máquinas en lugar de `maximumNumberOfMachinesForThisBoard`
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 467–519)
- **Problema:** `for (int i = 0; i < 6; i++)` ignora el valor de `maximumNumberOfMachinesForThisBoard`. En placas Muon (5 máquinas) se accede al índice 5 → acceso inválido.
- **Solución:** Reemplazar todos los `i < 6` del loop principal por `i < maximumNumberOfMachinesForThisBoard`.
- **Estado:** `[ ]`

---

### A4 — Sin validación de entrada en funciones cloud
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 104–124)
- **Problema:** `activateMachine(String machineId)` no valida que el string sea un entero válido. Si la entrada falla, `toInt()` devuelve `0` y se activa la máquina en índice 0 por error. La función siempre devuelve `1`, incluso ante errores.
- **Solución:** Validar la entrada y devolver `-1` en caso de error para que la API comunique el fallo al cliente.
- **Estado:** `[ ]`

---

## MEDIOS

### M1 — Duplicación masiva: 6 funciones ISR idénticas
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 125–195)
- **Problema:** `isWorkingMachine0Change()` ... `isWorkingMachine5Change()` son ~140 líneas de código copiado. Cualquier corrección debe hacerse en 6 sitios.
- **Solución:** Particle Device OS no permite lambdas con captura en `attachInterrupt`, pero sí funciones libres con índice global. Usar un array de funciones o refactorizar para usar una única función con estado externo.
- **Estado:** `[ ]`

---

### M2 — `delay()` bloqueante en el loop principal
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 463–520)
- **Problema:** `delay(500)` al inicio del loop + varios `delay(50)` y `delay(200)` durante la activación. La nube responde con latencia mínima de ~550ms y se pueden perder interrupciones.
- **Solución:** Sustituir por temporización basada en `millis()`. Implementar máquina de estados para la secuencia de activación.
- **Estado:** `[ ]`

---

### M3 — Sin logging activo en producción
- **Archivo:** `src/EasyClean-FW.cpp` (línea 50 y líneas 413–416)
- **Problema:** `SerialLogHandler` y todas las llamadas a `Log.*` están comentadas. Imposible diagnosticar fallos en campo.
- **Solución:** Activar `SerialLogHandler` con nivel `INFO` o `WARN`. Añadir logs en puntos clave: activación, detección de pago, errores de validación.
- **Estado:** `[ ]`

---

### M4 — Funciones con demasiadas responsabilidades
- **Archivo:** `src/EasyClean-FW.cpp` (~líneas 212–237)
- **Problema:** `checkAvailableMachinePinHighFunction()` hace debounce, verifica estado de activación, valida disponibilidad de pago y publica evento a Supabase. Difícil de testear y mantener.
- **Solución:** Separar en funciones con responsabilidad única: `debouncePin()`, `shouldReportPayment()`, `reportCashPayment()`.
- **Estado:** `[ ]`

---

### M5 — Números mágicos sin documentar
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 76–78, 200, 471)
- **Problema:** Valores como `45` pulsos, `50`ms, `200`ms, `10` iteraciones de debounce no tienen constantes con nombre ni explicación clara de su origen.
- **Nota:** La línea 78 ya tiene un comentario extenso con excepciones por tienda — esto confirma que deberían ser configurables.
- **Solución:** Definir constantes con nombre descriptivo. Idealmente, hacerlos configurables por tienda (ver A1).
  ```cpp
  const int DEBOUNCE_ITERATIONS      = 10;
  const int DEBOUNCE_DELAY_MS        = 10;
  const int DEFAULT_PULSE_WIDTH_MS   = 50;
  const int DEFAULT_PULSE_INTERVAL_MS = 200;
  const int DEFAULT_PULSES_PER_CYCLE = 45;
  ```
- **Estado:** `[ ]`

---

### M6 — Manejo incompleto del EthernetFeatherWing
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 438–455)
- **Problema:** El setup salta el índice 4 con `continue` para EthernetFeatherWing (pines D3, D4, D5 reservados para Ethernet), pero luego adjunta la interrupción para la máquina 4 igualmente. Lógica contradictoria.
- **Solución:** Hacer el `attachInterrupt` condicional al tipo de placa.
- **Estado:** `[ ]`

---

## BAJOS (Deuda técnica)

### B1 — 680 líneas de código comentado (versión RF)
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 523–1202)
- **Problema:** Más del 50% del archivo es código muerto de una versión RF nunca completada. Dificulta la navegación y el mantenimiento.
- **Solución:** Mover a una rama git separada (`feature/rf-version`) y eliminar del archivo principal.
- **Estado:** `[ ]`

---

### B2 — Código Azure obsoleto marcado para eliminar
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 75, 239–244, 431–434)
- **Problema:** Variables y suscripciones con comentarios `// DELETE THIS LINE` que siguen presentes. Confunde sobre qué está activo.
- **Solución:** Eliminar todo el bloque Azure (`azureToken`, `myHandlerForLogClientIn`, `Particle.subscribe` de Azure).
- **Estado:** `[ ]`

---

### B3 — Inconsistencia en tipos de parámetros de funciones cloud
- **Archivo:** `src/EasyClean-FW.cpp` (líneas 104–160)
- **Problema:** Algunas funciones toman `String machineId` y hacen `.toInt()` internamente, otras esperan directamente `int`. Inconsistente e ineficiente.
- **Solución:** Normalizar: las funciones registradas con `Particle.function` deben tomar `String` (requerido por la API), y convertir a `int` al inicio con validación (ver A4).
- **Estado:** `[ ]`

---

### B4 — Sin `.gitignore` para credenciales
- **Problema:** El archivo `src/wifi_credentials/setup-hotspots.h` con contraseñas reales está en el repositorio. No hay `.gitignore` que lo excluya.
- **Solución:** Añadir `.gitignore`, eliminar el archivo del historial git (`git rm --cached`), y usar un archivo `.example` como plantilla.
- **Nota:** Relacionado con C1.
- **Estado:** `[ ]`

---

## Progreso global

| Severidad | Total | Completadas |
|-----------|-------|-------------|
| CRÍTICO   | 3     | 0           |
| ALTO      | 4     | 0           |
| MEDIO     | 6     | 0           |
| BAJO      | 4     | 0           |
| **Total** | **17**| **0**       |

---

## Notas de implementación

- **Orden recomendado:** C1 → B4 (van juntas) → C2 → C3 → A2 → A3 → A4 → M3 → resto en paralelo
- Antes de tocar el loop principal (M2), habilitar logging (M3) para poder verificar el comportamiento
- La mejora A1 (config desde cloud) es la de mayor impacto operacional pero requiere cambios en backend; dejarla para cuando la arquitectura esté más estabilizada
- Al implementar M1 (refactor ISR), verificar en hardware real — las interrupciones son sensibles al timing

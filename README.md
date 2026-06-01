# Waveshare ESP32 RS485 — Herramienta de control

Herramienta de línea de comandos para controlar hasta 3 módulos **Waveshare ESP32-S3-POE-ETH-8DI-8RO** sobre un bus RS485 compartido. Soporta activación de relés por dispositivo con duración de pulso configurable y monitorización continua de entradas digitales.

## Hardware

| Componente | Detalle |
|-----------|--------|
| Módulo | Waveshare ESP32-S3-POE-ETH-8DI-8RO |
| Interfaz | RS485 (9600 baud 8N1) |
| Puente RS485 | Particle Photon 2 + breakout MAX3485 (Wi-Fi → RS485) |
| Relés | 8× (10A 250VAC) — contactos NO/COM/NC |
| Entradas | 8× entradas digitales optoacopladas (INPUT_PULLUP) |

## Cableado

### Particle Photon 2 + MAX3485 (puente RS485 de producción)

```
Pin Photon 2    Breakout MAX3485    Notas
────────────    ────────────────    ─────────────────────────────────
TX              RX                  ← RX del breakout = DI (entrada driver)
RX              TX                  ← TX del breakout = RO (salida receptor)
D2              EN                  HIGH=transmitir, LOW=recibir
3V3             VCC
GND             GND (lado VCC)
                A  ─────────────── Waveshare RS485 A
                B  ─────────────── Waveshare RS485 B
```

> **Etiquetado TX/RX del breakout MAX3485:** estos breakouts genéricos de AliExpress etiquetan los pines desde la perspectiva del **chip**, no del MCU. `RX` en el breakout es la entrada del driver (DI) — conectar al TX del MCU. `TX` es la salida del receptor (RO) — conectar al RX del MCU. Es lo contrario de la convención habitual de cruce UART.

> **Referencia GND:** RS485 es diferencial — A+B solos son suficientes cuando ambos extremos comparten un potencial de tierra cercano (mismo rack, misma fuente). Añadir un hilo GND entre el GND del lado A/B del MAX3485 y el terminal **PE** del bloque RS485 del Waveshare si la comunicación es inestable con fuentes separadas o cables largos. No usar DGND — el bus RS485 del Waveshare está aislado de la tierra digital de la placa.

> **Contención de bus:** si VCC del MAX3485 cae por debajo de 3V, el pin EN probablemente está flotando. Asegurarse de que EN está firmemente conectado a D2 — un EN flotante activa TX y RX simultáneamente y provoca un cortocircuito en el bus.

### Convención de entradas digitales (opto)

El módulo usa `INPUT_PULLUP`. Los optoacopladores llevan el pin a LOW cuando conducen:

| Estado opto | Pin DI | Valor en código |
|-------------|--------|-----------------|
| ON (conduciendo) | LOW | `False` |
| OFF | HIGH (pullup) | `True` |

El script decodifica dos señales por máquina:

```
plugged = not inputs[plugged_di]          # opto ON = enchufada a la corriente
running = plugged and inputs[running_di]  # normalmente cerrado: opto ON = libre
```

### Cableado de relés

Usar contactos **COM + NC** si la máquina espera una señal de ruptura (circuito normalmente cerrado, se abre al activar). Usar **COM + NO** para activación normalmente abierta. Probar con el comando `relay`.

---

## Firmware Photon 2

Flashear `firmware/photon2/` al Photon 2:

```bash
particle compile p2 firmware/photon2 --saveTo firmware/photon2/photon2.bin
particle flash --usb firmware/photon2/photon2.bin
```

### Funciones Particle Cloud

#### `relay` — activar un relé

Argumento: `"device,channel[,duration_ms]"`

| Campo | Valores | Descripción |
|-------|---------|-------------|
| `device` | 1–3 | Dirección del módulo Waveshare en el bus |
| `channel` | 1–8 | Canal del relé |
| `duration_ms` | entero (opcional) | Duración del pulso en ms — por defecto 100 |

Ejemplos:
```
"1,1"       → relay CH1 device 1, 100 ms
"1,3,500"   → relay CH3 device 1, 500 ms
"2,1,1000"  → relay CH1 device 2, 1000 ms
```

Retorna `0` en éxito, negativo en error.

#### `queryDI` — leer entradas digitales

Argumento: `"device"` (1–3)

Retorna un entero 0–255: bitmask de las 8 entradas digitales del módulo indicado.

| Bit | DI | Valor 1 | Valor 0 |
|-----|----|---------|---------|
| 0 | DI1 | Opto OFF (pin HIGH/pullup) | Opto ON (pin LOW) |
| … | … | … | … |
| 7 | DI8 | Opto OFF (pin HIGH/pullup) | Opto ON (pin LOW) |

Ejemplos de interpretación:
- `255` (`0b11111111`) → todas las DI en HIGH → ningún optoacoplador activo → ninguna máquina enchufada ni en uso
- `254` (`0b11111110`) → DI1=0 (opto activo), DI2–DI8=1 (flotantes) → máquina 1 enchufada, sensor running libre

Retorna `-1` si el dispositivo no responde en 200 ms.

---

## Instalación

```bash
git clone https://github.com/development-jgm/waveshare-esp32-rs485.git
cd waveshare-esp32-rs485

python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Permisos de puerto serie (Linux)

```bash
sudo usermod -aG dialout $USER
# Cerrar sesión y volver a entrar, o aplicar inmediatamente:
newgrp dialout
```

---

## Flasheo del firmware ESP32

El módulo debe ejecutar el firmware modificado incluido en `firmware/MAIN_ALL/` antes de que la herramienta funcione. Las modificaciones son:

- **`WS_RS485.cpp`** — implementa el protocolo de 9 bytes con dirección y añade el comando de consulta DI que responde con el estado de las 8 entradas digitales como bitmask
- **`WS_DIN.h`** — `Relay_Immediate_Default = 0` desactiva el auto-mirroring DI→relé (evita que los relés se activen al enchufar una máquina)

### Entrar en modo bootloader (obligatorio antes de cada flash)

El interfaz USB JTAG del ESP32-S3 no soporta reset automático a modo de descarga. Hay que poner el módulo en modo bootloader manualmente cada vez:

1. **Desconectar** el cable USB del módulo
2. **Mantener** pulsado el botón **BOOT**
3. **Conectar** el cable USB mientras se mantiene BOOT
4. **Soltar** BOOT

El módulo permanecerá en modo bootloader ROM (conexión USB estable) hasta que sea flasheado y reseteado.

### Pasos para flashear

```bash
source venv/bin/activate

# Flashear como device 1 (puerto autodetectado)
./flash.sh --address 1

# Flashear como device 2
./flash.sh --address 2

# Puerto explícito
./flash.sh --address 1 --port /dev/ttyACM0
```

Tras flashear, desconectar el cable USB y conectar el módulo al bus RS485.

---

## Uso

```bash
source venv/bin/activate
export PARTICLE_TOKEN=tu_token_aqui

# Estado de todas las entradas digitales, device 1
python3 esp32_control.py --photon TU_DEVICE_ID status

# Pulso en relé 1 del device 1 durante 100 ms (por defecto)
python3 esp32_control.py --photon TU_DEVICE_ID relay --channel 1

# Pulso en relé 1 del device 2 durante 500 ms
python3 esp32_control.py --photon TU_DEVICE_ID relay --device 2 --channel 1 --duration 500

# Monitorización continua de ambos dispositivos, refresco cada 500 ms
python3 esp32_control.py --photon TU_DEVICE_ID poll --device 1 2
```

### Opciones globales

| Opción | Por defecto | Descripción |
|--------|-------------|-------------|
| `--photon DEVICE_ID` | — | Transporte Particle Cloud (producción) |
| `--port PATH` | `/dev/ttyUSB0` | Transporte dongle USB (desarrollo) |
| `--baudrate N` | `9600` | Velocidad en baudios (solo dongle USB) |

### `relay` — activar un relé

```
python3 esp32_control.py relay --channel N [--duration MS] [--device N]
```

| Opción | Por defecto | Descripción |
|--------|-------------|-------------|
| `--channel N` | *(obligatorio)* | Canal del relé 1–8 |
| `--duration MS` | `100` | Duración del pulso en milisegundos |
| `--device N` | `1` | Dirección del dispositivo (1–3) |

### `status` — lectura puntual de DI

```
python3 esp32_control.py status [--device N [N ...]]
```

Muestra los valores brutos de DI y el estado decodificado de cada máquina.

### `poll` — monitorización continua

```
python3 esp32_control.py poll [--device N [N ...]] [--interval SEC]
```

Refresca el terminal en el sitio. Pulsar `Ctrl+C` para detener.

---

## Protocolo RS485

Los comandos son **9 bytes**: `[DEVICE_ADDRESS] + [payload de 8 bytes]`. Cada módulo descarta silenciosamente los paquetes que no van dirigidos a él, por lo que todos los dispositivos pueden compartir el mismo bus sin colisiones.

La respuesta al query DI también es de 9 bytes: `[DEVICE_ADDRESS, 0x06, 0x01, DI_BITMASK, 0x00, 0x00, 0x00, 0x00, 0x00]`.

---

## Cableado por máquina

Cada lavadora usa dos entradas DI y una salida de relé del módulo ESP32:

| Señal | DI | Opto ON significa | Cableado |
|-------|----|-------------------|----------|
| Enchufada | DI impar (1, 3, 5, 7) | Máquina conectada a la corriente | Sensor normalmente abierto a entrada opto |
| En uso | DI par (2, 4, 6, 8) | Máquina ocupada | Sensor normalmente cerrado a entrada opto — opto conduciendo = libre |
| Activar | DO (relé) | — | Relé COM+NO (o COM+NC) a señal de arranque de la máquina |

El sensor "en uso" normalmente cerrado significa: opto ON (conduciendo) → DI LOW → `inputs[i] = False` → máquina **disponible**. Cuando la máquina arranca, el contacto se abre → opto OFF → `inputs[i] = True` → máquina **en uso**.

### Mapa de cableado DI (por defecto)

Ajustar `MACHINE_DI` en `esp32_control.py` para que coincida con el cableado físico.

| Máquina | DI Enchufada | DI En uso | Canal relé |
|---------|-------------|-----------|------------|
| 1 | DI1 (índice 0) | DI2 (índice 1) | CH1 |
| 2 | DI3 (índice 2) | DI4 (índice 3) | CH2 |
| 3 | DI5 (índice 4) | DI6 (índice 5) | CH3 |
| 4 | DI7 (índice 6) | DI8 (índice 7) | CH4 |

---

## Bus RS485 multidispositivo

Hasta 3 módulos pueden compartir el mismo bus RS485. Cada uno debe flashearse con una dirección única usando `--address N`. Flashear cada módulo individualmente por USB y luego conectar todos al bus:

```bash
# Flashear cada módulo individualmente (con el procedimiento del botón BOOT cada vez)
./flash.sh --address 1
./flash.sh --address 2
./flash.sh --address 3

# Luego monitorizar todos via Particle Cloud
python3 esp32_control.py --photon TU_DEVICE_ID poll --device 1 2 3
```

---

## Dongle USB (solo desarrollo / debug)

Se puede usar un dongle USB-RS485 durante el desarrollo o para inspeccionar el tráfico del bus. Aparece como `/dev/ttyUSB0` en Linux.

```
Dongle A+ → Módulo RS485 A+
Dongle B- → Módulo RS485 B-
```

```bash
python3 esp32_control.py --port /dev/ttyUSB0 status
```

Encontrar el puerto del dongle:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
# o:
dmesg | tail -20
```

> **Nota FTDI:** los dongles FTDI FT232R envían URBs de control de flujo RTS/CTS por defecto, lo que provoca una desconexión USB (errno 5) tras la primera escritura RS485. El script desactiva RTS/CTS y DTR/DSR al abrir y limpia explícitamente ambas líneas para evitarlo.

# MAX3485 Breakout — Referencia técnica

## Qué es

El MAX3485 es un transceptor RS485 half-duplex de 3.3V. Convierte UART (TX/RX) en señal diferencial RS485 (A/B). Solo puede transmitir o recibir en un momento dado — el pin DE/RE controla la dirección.

---

## Pinout del chip (SOIC-8)

```
        ┌──────────┐
   RO ──┤1        8├── VCC
  /RE ──┤2        7├── B
   DE ──┤3        6├── A
   DI ──┤4        5├── GND
        └──────────┘
```

| Pin | Nombre | Función |
|-----|--------|---------|
| RO | Receiver Output | Salida al MCU RX — lo que llega del bus RS485 |
| /RE | Receiver Enable | Activo LOW — habilita la recepción |
| DE | Driver Enable | Activo HIGH — habilita la transmisión |
| DI | Driver Input | Entrada desde el MCU TX — lo que se envía al bus |
| A | Bus A | Línea diferencial positiva |
| B | Bus B | Línea diferencial negativa |
| VCC | Alimentación | 3.3V |
| GND | Tierra | Referencia del lado lógico |

---

## Breakout genérico de AliExpress

Estos módulos exponen los pines con etiquetas propias:

```
┌─────────────────┐
│  TX  RX  EN     │  ← lado MCU
│                 │
│  A   B   GND    │  ← lado bus RS485
│        VCC  GND │  ← alimentación
└─────────────────┘
```

| Etiqueta breakout | Pin del chip | Conectar a |
|-------------------|--------------|------------|
| `TX` | RO (Receiver Output) | MCU **RX** |
| `RX` | DI (Driver Input) | MCU **TX** |
| `EN` | DE + /RE juntos | Pin de control del MCU |
| `A` | A | RS485 A |
| `B` | B | RS485 B |
| `GND` (lado A/B) | GND bus | Referencia RS485 (ver nota) |
| `VCC` | VCC | 3.3V |
| `GND` (lado VCC) | GND lógico | GND del MCU |

### CRÍTICO — El etiquetado TX/RX está invertido respecto al MCU

Las etiquetas son desde la **perspectiva del chip**, no del MCU:
- `RX` del breakout = lo que el chip recibe del MCU = **DI** → conectar al **TX del MCU**
- `TX` del breakout = lo que el chip envía al MCU = **RO** → conectar al **RX del MCU**

Conectar TX→TX y RX→RX (sentido "lógico") hace que el bus no funcione.

---

## Pin EN — control de dirección

En estos breakouts, DE y /RE están unidos internamente y expuestos como un único pin `EN`:

| Nivel EN | DE | /RE | Estado |
|----------|----|-----|--------|
| HIGH | 1 | 1 | Transmitiendo (driver activo, receptor deshabilitado) |
| LOW | 0 | 0 | Recibiendo (driver apagado, receptor activo) |
| **FLOTANTE** | — | — | **Ambos activos → cortocircuito en el bus** |

**Un EN flotante es un fallo grave**: habilita driver y receptor simultáneamente, causa cortocircuito diferencial en A/B, y provoca caída de VCC (puede bajar de 3.3V a 2.5V o menos). Siempre conectar EN a un nivel definido.

---

## Referencia de tierra (GND del lado A/B)

El breakout tiene dos GND:
- **GND lado VCC** — tierra lógica del chip, conectar al GND del MCU
- **GND lado A/B** — referencia de la señal RS485, conectar al GND del bus RS485 en el extremo remoto

RS485 es diferencial, por lo que A+B solos son suficientes cuando ambos extremos tienen potencial de tierra similar. El GND de bus solo es necesario si la comunicación es inestable (fuentes separadas, cables largos).

> En el módulo Waveshare ESP32-S3-POE-ETH-8DI-8RO, el bus RS485 está **aislado galvánicamente** del resto de la placa. El pin de referencia RS485 se llama **PE** en el bloque de terminales (junto a A y B), no DGND.

---

## Temporización half-duplex

Al transmitir, el DE debe bajar **después** de que el último bit haya salido del transceiver, no solo después de que el buffer del MCU esté vacío:

```cpp
digitalWrite(EN, HIGH);          // activar driver
Serial.write(buf, len);
Serial.flush();                  // esperar a que el UART termine
delayMicroseconds(2000);         // guard: ~2 periodos de byte a 9600 baud
digitalWrite(EN, LOW);           // volver a recepción
```

Sin el guard, el driver se apaga antes de que el shift register del transceiver termine, cortando el último byte.

---

## Wiring completo — Particle Photon 2 + MAX3485 → RS485

```
Photon 2      MAX3485 breakout     Bus RS485
──────────    ────────────────     ─────────
TX       ───► RX (= DI)
RX       ◄─── TX (= RO)
D2       ───► EN
3V3      ───► VCC
GND      ───► GND (lado VCC)
                A ──────────────── RS485 A
                B ──────────────── RS485 B
              GND (lado A/B) ───── PE del módulo remoto (opcional)
```

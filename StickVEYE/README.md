# StickVEYE

StickVEYE is the current **StickV -> Cardputer Grove UART proof project** for the future "Groqputer eye" path.

The first milestone is intentionally simple:

- **StickV** runs a custom MaixPy script
- **Cardputer** runs a small host/test UI
- the two talk over **direct Grove UART**
- no ESP32-C3 relay is required for this first proof

## Current proof

The current proof has moved beyond the plain UART ping test:

- the **StickV** now runs a **face-detection test script**
- the **Cardputer** still acts as a small **test host**
- the StickV screen shows a demo-style live camera view with face boxes when a face model is available
- the UART link carries both manual commands and autonomous face events

This project is meant to prove:

1. Grove/UART wiring works reliably between StickV and Cardputer
2. StickV can run a simple face-detection flow and send compact events
3. a simple host can stand in for the future Groqbot-side logic before we merge anything into Groqputer

## Files

- `src/main.cpp` - Cardputer host/test firmware
- `stickv/main.py` - StickV MaixPy script, uploaded as `/flash/boot.py`
- `platformio.ini` - Cardputer PlatformIO project config

## Wiring

### Current verified direct-link wiring

- **StickV GPIO35 (TX) -> Cardputer G1**
- **StickV GPIO34 (RX) -> Cardputer G2**
- **GND -> GND**

### Power note

This direct setup has been **confirmed working with an unmodified 4-wire Grove cable**, but **only for Cardputer in the 5VIN configuration**.

Important constraints:

- this note is for **Cardputer only**
- the Cardputer side must be set for **5VIN**
- the user **must not** use **5VOUT** for this direct wiring path
- do **not** assume the same full 4-wire Grove power path is safe on other M5 devices

In other words:

- **confirmed safe here:** StickV + Cardputer + unmodified 4-wire Grove cable + Cardputer configured for **5VIN**
- **not confirmed safe here:** other M5 hosts, or Cardputer configured to drive **5VOUT**

## Flashing

### StickV

Flash MaixPy, then upload the StickV script as `/flash/boot.py`.

### Face model requirement

The current custom StickV script expects a **face-detection model** in one of these locations:

- flashed at **`0x300000`**
- or on SD as **`/sd/facedetect.kmodel`**
- or on SD as **`/sd/face.kmodel`**

If no model is available, the StickV script still boots and talks over UART, but it reports:

```text
MODEL:MISSING
```

The face-detection flow used here follows the common MaixPy KPU pattern also seen in public StickV/Maix examples:

- `KPU.load(0x300000)` or `KPU.load("/sd/...")`
- `kpu.init_yolo2(...)`
- `kpu.run_yolo2(...)`

Example MaixPy flash:

```bash
python3 -m kflash -e -p /dev/ttyUSB0 -b 1500000 /path/to/maixpy_v0.6.3_m5stickv.bin
```

Example script upload:

```bash
ampy --port /dev/ttyUSB0 --delay 1 put stickv/main.py /flash/boot.py
```

### Cardputer

From this directory:

```bash
pio run -t upload
```

## Testing

The Cardputer firmware is a **test host**, not the real Groqputer firmware. It is meant to simulate the bot side in a safer sandbox.

On the Cardputer:

- `P` sends `PING`
- `I` sends `STATUS`
- `S` sends `SNAP`
- `F` sends `FRAME` and requests one grayscale preview frame
- `V` toggles repeated `FRAME` polling for a simple camera-view mode
- `L` returns to the text/log view
- `E` sends `EVENT`
- `D` sends `DETECT:TOGGLE`
- `M` sends `MOTION:TOGGLE`
- `O` sends `COLOR:TOGGLE`
- `C` clears the log

While preview mode is active, text commands like `P`, `I`, `S`, `E`, and `D`
pause frame polling until their reply is received so the host does not mix a
binary frame read with text UART events.

Expected results:

- `P` -> `PONG`
- `I` -> `STATUS:READY:...:FACES:<n>` when the model is loaded
- `S` -> `SNAP:FACES:<n>:X:...:Y:...:W:...:H:...`
- `F` -> Cardputer receives `FRAME:RGB565:80:60:9600`, 9,600 bytes of color image data, then `FRAME:END`
- `E` -> returns the last UART event line
- `D` -> `DETECT:ON` or `DETECT:OFF`
- `M` -> `MOTION:ON` or `MOTION:OFF`
- `O` -> `COLOR:ON` or `COLOR:OFF`

The preview path is intentionally lightweight:

- StickV samples the current QVGA camera frame down to **80x60 color**
- Cardputer scales that to a larger on-screen preview
- the UART transfer is intentionally chunked and paced for reliability
- this is meant to be a **usable snapshot poll**, not true live video
- face detection runs by default; motion and color are opt-in so they can be
  tested separately if needed

Autonomous StickV events now include:

- `BOOT:FACE_READY:<model-source>`
- `FACE:DETECTED:<count>:X:...:Y:...:W:...:H:...`
- `FACE:TRACKING:<count>:X:...:Y:...:W:...:H:...`
- `FACE:NONE`
- `MOTION:DETECTED:<count>:X:...:Y:...:W:...:H:...`
- `MOTION:NONE`
- `COLOR:DETECTED:<name>:COUNT:<n>:X:...:Y:...:W:...:H:...`
- `COLOR:NONE`
- `MODEL:MISSING:...`

## Next step

Once this face-detection test is stable, the next milestone is a **hybrid MaixPy recognition script**:

- keep the current **demo-style StickV local screen**
- extend UART events toward `FACE:KNOWN:<name>` and `FACE:UNKNOWN`
- keep the tracker example on SD for later MaixPy v2 use
- keep Groqputer integration separate until that contract is stable

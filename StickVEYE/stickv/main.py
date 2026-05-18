import lcd
import sensor
import time
import image
import KPU as kpu
from machine import UART
from fpioa_manager import fm

UART_BAUD = 115200
UART_TX_PIN = 35
UART_RX_PIN = 34

MODEL_FLASH_ADDR = 0x300000
MODEL_SD_CANDIDATES = ["/sd/facedetect.kmodel", "/sd/face.kmodel"]
YOLO2_ANCHOR = (
    1.889, 2.5245, 2.9465, 3.94056, 3.99987,
    5.3658, 5.155437, 6.92275, 6.718375, 9.01025,
)

TRACKING_REPORT_MS = 2500
MOTION_REPORT_MS = 1800
COLOR_REPORT_MS = 1800
OVERLAY_REFRESH_MS = 160
MOTION_SAMPLE_STEP_X = 16
MOTION_SAMPLE_STEP_Y = 16
MOTION_DIFF_THRESHOLD = 20
MOTION_MIN_CHANGED = 8
COLOR_PIXELS_THRESHOLD = 120
COLOR_AREA_THRESHOLD = 120
PREVIEW_WIDTH = 80
PREVIEW_HEIGHT = 60
PREVIEW_SAMPLE_STEP_X = 4
PREVIEW_SAMPLE_STEP_Y = 4
PREVIEW_CHUNK_BYTES = 32
PREVIEW_CHUNK_DELAY_MS = 2
COLOR_TARGETS = (
    ("RED", (0, 80, 40, 80, 15, 80), (255, 0, 0)),
    ("GREEN", (0, 80, -120, -10, 0, 30), (0, 255, 0)),
    ("BLUE", (0, 80, 30, 100, -120, -60), (0, 0, 255)),
)

# Current direct-link test wiring: StickV GPIO35 -> Cardputer G1, GPIO34 -> Cardputer G2.
fm.register(UART_TX_PIN, fm.fpioa.UART2_TX, force=True)
fm.register(UART_RX_PIN, fm.fpioa.UART2_RX, force=True)
uart = UART(UART.UART2, UART_BAUD, 8, 0, 0, timeout=1000, read_buf_len=2048)

lcd.init()
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.set_vflip(1)
sensor.set_hmirror(1)
sensor.run(1)
sensor.skip_frames(30)

last_status_text = "Booting"
last_event_text = "BOOT"
last_overlay_ms = 0
last_report_ms = 0
last_face_count = 0
detection_enabled = True
motion_enabled = False
color_enabled = False
motion_prev_samples = None
last_motion_count = 0
last_motion_report_ms = 0
last_color_name = "NONE"
last_color_count = 0
last_color_report_ms = 0

task_fd = None
model_source = "NONE"
model_error = ""


def send_line(text):
    global last_event_text
    last_event_text = text
    uart.write(text)
    uart.write("\n")


def load_face_model():
    global task_fd
    global model_source
    global model_error

    try:
        task_fd = kpu.load(MODEL_FLASH_ADDR)
        model_source = "FLASH:0x300000"
    except Exception as flash_error:
        model_error = str(flash_error)
        for path in MODEL_SD_CANDIDATES:
            try:
                task_fd = kpu.load(path)
                model_source = "FILE:" + path
                model_error = ""
                break
            except Exception as file_error:
                model_error = str(file_error)

    if task_fd:
        kpu.init_yolo2(task_fd, 0.5, 0.3, 5, YOLO2_ANCHOR)
        return True
    return False


def draw_banner(img, top, text, color):
    img.draw_rectangle(0, top, img.width(), 22, color=(0, 0, 0), fill=True)
    img.draw_string(4, top + 4, text, color=color, scale=1)


def summarize_faces(faces):
    if not faces:
        return 0, None
    largest = None
    largest_area = -1
    for face in faces:
        area = face.w() * face.h()
        if area > largest_area:
            largest_area = area
            largest = face
    return len(faces), largest


def send_status(face_count):
    model_state = "READY" if task_fd else "MISSING"
    send_line(
        "STATUS:" + model_state + ":" + model_source
        + ":FACES:" + str(face_count)
        + ":MOTION:" + ("ON" if motion_enabled else "OFF")
        + ":" + str(last_motion_count)
        + ":COLOR:" + ("ON" if color_enabled else "OFF")
        + ":" + last_color_name
    )


def send_face_event(prefix, face_count, face):
    if face:
        send_line(
            prefix + ":" + str(face_count)
            + ":X:" + str(face.x())
            + ":Y:" + str(face.y())
            + ":W:" + str(face.w())
            + ":H:" + str(face.h())
        )
    else:
        send_line(prefix + ":" + str(face_count))


def pixel_luma(pixel):
    if isinstance(pixel, tuple) or isinstance(pixel, list):
        return ((pixel[0] * 30) + (pixel[1] * 59) + (pixel[2] * 11)) // 100
    return pixel & 0xFF


def analyze_motion(img):
    global motion_prev_samples

    samples = []
    changed = 0
    min_x = img.width()
    min_y = img.height()
    max_x = -1
    max_y = -1
    samples_per_row = (img.width() + MOTION_SAMPLE_STEP_X - 1) // MOTION_SAMPLE_STEP_X

    for y in range(0, img.height(), MOTION_SAMPLE_STEP_Y):
        src_y = y + (MOTION_SAMPLE_STEP_Y // 2)
        if src_y >= img.height():
            src_y = img.height() - 1
        for x in range(0, img.width(), MOTION_SAMPLE_STEP_X):
            src_x = x + (MOTION_SAMPLE_STEP_X // 2)
            if src_x >= img.width():
                src_x = img.width() - 1
            samples.append(pixel_luma(img.get_pixel(src_x, src_y)))

    if motion_prev_samples is None or len(motion_prev_samples) != len(samples):
        motion_prev_samples = samples
        return 0, None

    for i in range(len(samples)):
        if abs(samples[i] - motion_prev_samples[i]) >= MOTION_DIFF_THRESHOLD:
            changed += 1
            sample_y = (i // samples_per_row) * MOTION_SAMPLE_STEP_Y
            sample_x = (i % samples_per_row) * MOTION_SAMPLE_STEP_X
            if sample_x < min_x:
                min_x = sample_x
            if sample_y < min_y:
                min_y = sample_y
            if sample_x > max_x:
                max_x = sample_x
            if sample_y > max_y:
                max_y = sample_y

    motion_prev_samples = samples
    if changed < MOTION_MIN_CHANGED:
        return changed, None

    box_w = (max_x - min_x) + MOTION_SAMPLE_STEP_X
    box_h = (max_y - min_y) + MOTION_SAMPLE_STEP_Y
    return changed, (min_x, min_y, box_w, box_h)


def analyze_color(img):
    best_name = "NONE"
    best_count = 0
    best_blob = None
    best_color = (255, 255, 255)
    best_area = -1

    for name, threshold, draw_color in COLOR_TARGETS:
        blobs = img.find_blobs([threshold], pixels_threshold=COLOR_PIXELS_THRESHOLD,
                               area_threshold=COLOR_AREA_THRESHOLD, merge=True)
        if not blobs:
            continue
        largest = None
        largest_area = -1
        for blob in blobs:
            area = blob.w() * blob.h()
            if area > largest_area:
                largest_area = area
                largest = blob
        if largest and largest_area > best_area:
            best_name = name
            best_count = len(blobs)
            best_blob = largest
            best_color = draw_color
            best_area = largest_area

    return best_name, best_count, best_blob, best_color


def send_preview_frame(img):
    if uart is None:
        raise Exception("UART unavailable")

    frame = bytearray(PREVIEW_WIDTH * PREVIEW_HEIGHT * 2)
    index = 0
    max_x = img.width() - 1
    max_y = img.height() - 1

    for y in range(PREVIEW_HEIGHT):
        src_y = (y * PREVIEW_SAMPLE_STEP_Y) + (PREVIEW_SAMPLE_STEP_Y // 2)
        if src_y > max_y:
            src_y = max_y
        for x in range(PREVIEW_WIDTH):
            src_x = (x * PREVIEW_SAMPLE_STEP_X) + (PREVIEW_SAMPLE_STEP_X // 2)
            if src_x > max_x:
                src_x = max_x
            pixel = img.get_pixel(src_x, src_y)
            if isinstance(pixel, tuple) or isinstance(pixel, list):
                r = pixel[0] & 0xFF
                g = pixel[1] & 0xFF
                b = pixel[2] & 0xFF
                packed = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            else:
                packed = pixel & 0xFFFF
            frame[index] = (packed >> 8) & 0xFF
            frame[index + 1] = packed & 0xFF
            index += 2

    uart.write("FRAME:RGB565:80:60:9600\n")
    chunk = PREVIEW_CHUNK_BYTES
    for offset in range(0, len(frame), chunk):
        uart.write(frame[offset:offset + chunk])
        time.sleep_ms(PREVIEW_CHUNK_DELAY_MS)
    uart.write("FRAME:END\n")


def handle_command(command, face_count, face, img):
    global last_status_text
    global detection_enabled
    global motion_enabled
    global color_enabled
    global motion_prev_samples
    global last_motion_count
    global last_color_name
    global last_color_count
    if command == "PING":
        last_status_text = "PING->PONG"
        send_line("PONG")
        return
    if command == "STATUS":
        last_status_text = "STATUS"
        send_status(face_count)
        return
    if command == "SNAP":
        last_status_text = "SNAP"
        send_face_event("SNAP:FACES", face_count, face)
        return
    if command == "FRAME":
        last_status_text = "FRAME"
        try:
            send_preview_frame(img)
        except Exception as frame_error:
            send_line("ERR:FRAME:" + str(frame_error))
        return
    if command == "EVENT":
        last_status_text = "EVENT"
        send_line(last_event_text)
        return
    if command == "DETECT:TOGGLE":
        detection_enabled = not detection_enabled
        last_status_text = "DETECT ON" if detection_enabled else "DETECT OFF"
        send_line("DETECT:" + ("ON" if detection_enabled else "OFF"))
        return
    if command == "MOTION:TOGGLE":
        motion_enabled = not motion_enabled
        motion_prev_samples = None
        last_motion_count = 0
        last_status_text = "MOTION ON" if motion_enabled else "MOTION OFF"
        send_line("MOTION:" + ("ON" if motion_enabled else "OFF"))
        return
    if command == "COLOR:TOGGLE":
        color_enabled = not color_enabled
        last_color_name = "NONE"
        last_color_count = 0
        last_status_text = "COLOR ON" if color_enabled else "COLOR OFF"
        send_line("COLOR:" + ("ON" if color_enabled else "OFF"))
        return
    if command == "HELP":
        last_status_text = "HELP"
        send_line("CMDS:PING,STATUS,SNAP,FRAME,EVENT,DETECT:TOGGLE,MOTION:TOGGLE,COLOR:TOGGLE")
        return
    last_status_text = "UNKNOWN"
    send_line("ERR:UNKNOWN:" + command)


model_loaded = load_face_model()
if model_loaded:
    last_status_text = "FACE READY"
    send_line("BOOT:FACE_READY:" + model_source)
else:
    last_status_text = "MODEL MISSING"
    send_line("MODEL:MISSING:need face model at 0x300000 or /sd/facedetect.kmodel")

while True:
    img = sensor.snapshot()
    now = time.ticks_ms()
    faces = None

    if task_fd and detection_enabled:
        try:
            faces = kpu.run_yolo2(task_fd, img)
        except Exception as run_error:
            last_status_text = "KPU ERR"
            send_line("ERR:KPU:" + str(run_error))
            faces = None

    face_count, largest_face = summarize_faces(faces)
    motion_count = 0
    motion_box = None
    color_name, color_count, color_blob, color_draw = "NONE", 0, None, (255, 255, 255)

    if motion_enabled:
        motion_count, motion_box = analyze_motion(img)
    if color_enabled:
        color_name, color_count, color_blob, color_draw = analyze_color(img)

    if faces:
        for face in faces:
            img.draw_rectangle(face.rect(), color=(0, 255, 0), thickness=2)
        img.draw_string(4, 26, "Faces: " + str(face_count), color=(0, 255, 0), scale=1)
    elif detection_enabled and task_fd:
        img.draw_string(4, 26, "Faces: 0", color=(255, 220, 0), scale=1)
    elif not detection_enabled:
        img.draw_string(4, 26, "Detect paused", color=(255, 220, 0), scale=1)
    if motion_box:
        img.draw_rectangle(motion_box, color=(255, 255, 0), thickness=2)
    if color_blob:
        img.draw_rectangle(color_blob.rect(), color=color_draw, thickness=2)

    if task_fd and detection_enabled:
        if face_count > 0 and last_face_count == 0:
            send_face_event("FACE:DETECTED", face_count, largest_face)
            last_report_ms = now
        elif face_count > 0 and time.ticks_diff(now, last_report_ms) >= TRACKING_REPORT_MS:
            send_face_event("FACE:TRACKING", face_count, largest_face)
            last_report_ms = now
        elif face_count == 0 and last_face_count > 0:
            send_line("FACE:NONE")
            last_report_ms = now
    last_face_count = face_count

    if motion_enabled:
        if motion_count > 0:
            if last_motion_count == 0 or time.ticks_diff(now, last_motion_report_ms) >= MOTION_REPORT_MS:
                if motion_box:
                    send_line(
                        "MOTION:DETECTED:" + str(motion_count)
                        + ":X:" + str(motion_box[0])
                        + ":Y:" + str(motion_box[1])
                        + ":W:" + str(motion_box[2])
                        + ":H:" + str(motion_box[3])
                    )
                else:
                    send_line("MOTION:DETECTED:" + str(motion_count))
                last_motion_report_ms = now
        elif last_motion_count > 0:
            send_line("MOTION:NONE")
            last_motion_report_ms = now
        last_motion_count = motion_count

    if color_enabled:
        if color_name != "NONE":
            if color_name != last_color_name or time.ticks_diff(now, last_color_report_ms) >= COLOR_REPORT_MS:
                if color_blob:
                    send_line(
                        "COLOR:DETECTED:" + color_name + ":COUNT:" + str(color_count)
                        + ":X:" + str(color_blob.x())
                        + ":Y:" + str(color_blob.y())
                        + ":W:" + str(color_blob.w())
                        + ":H:" + str(color_blob.h())
                    )
                else:
                    send_line("COLOR:DETECTED:" + color_name + ":COUNT:" + str(color_count))
                last_color_report_ms = now
            last_color_name = color_name
            last_color_count = color_count
        elif last_color_name != "NONE":
            send_line("COLOR:NONE")
            last_color_report_ms = now
            last_color_name = "NONE"
            last_color_count = 0

    if time.ticks_diff(now, last_overlay_ms) >= OVERLAY_REFRESH_MS:
        if task_fd:
            draw_banner(img, 0, last_status_text + " " + model_source, (255, 255, 255))
        else:
            draw_banner(img, 0, "MODEL MISSING", (255, 0, 0))
            draw_banner(img, 22, "Need 0x300000 or /sd/facedetect.kmodel", (255, 220, 0))
        lcd.display(img)
        last_overlay_ms = now

    data = uart.readline()
    if data:
        try:
            command = data.decode().strip()
        except Exception:
            command = str(data).strip()
        if command:
            handle_command(command, face_count, largest_face, img)

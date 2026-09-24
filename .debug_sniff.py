import serial, time
ser = serial.Serial("/dev/ttyACM0", 115200, timeout=0.2)
t0 = time.time()
with open("/home/zkazen/Escritorio/Kira/.debug_sniff.log", "w") as log:
    while time.time() - t0 < 200:
        d = ser.read(512)
        if d:
            log.write(d.decode(errors="replace"))
            log.flush()

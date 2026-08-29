import serial

# Open the serial port (replace 'COM3' with the actual port, e.g., '/dev/ttyACM0' on Linux)
ser = serial.Serial(
    port='/dev/tty.usbmodem48EA564731361',
    baudrate=50000000,
    timeout=1  # seconds; adjust as needed
)

print("Waiting for data...")
f = open ("samples.u12","wb+")
while True:
    # Read up to 64 bytes (USB FS max packet size)
    data = ser.read(30*2*10000)  # non-blocking read due to timeout

    if data:
        print(f"Received {len(data)}")
        f.write(data)
        f.flush()
        # Display each byte as hex
        #print("Hex values:", [f"{b:02X}" for b in data])

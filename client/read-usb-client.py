import serial
import socket
import struct

def conv2u16(raw):
    v0,=struct.unpack("<H",raw)
    v = float(v0)/0x1000
    x = int(v * 0xffff)-0x7fff
    return struct.pack("<h", x)

def conv_cs16_cf32(raw):
    v0,=struct.unpack("<h",raw)
    x= float(v0)/0x7fff
    return struct.pack("<f", x)


def conv2cf32(raw):
    v0,=struct.unpack("<H",raw)
    v = float(v0)/0x1000
    v = v - 0.5
    return struct.pack("<f", v)


def convert_all(data):
    ls = []
    n = len(data)//2
    for i in range(n):
        v=conv2cf32(data[2*i:2*i+2])
        ls.append(v)
    return b''.join(ls)

# Open the serial port (replace 'COM3' with the actual port, e.g., '/dev/ttyACM0' on Linux)
ser = serial.Serial(
    port='/dev/tty.usbmodem48F2825031361',
    baudrate=50000000,
    timeout=1  # seconds; adjust as needed
)

s = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
#s = socket.socket()
#s.connect(("127.0.0.1",2000))

print("Waiting for data...")
f = open ("samples.cf32","wb+")
while True:
    # Read up to 64 bytes (USB FS max packet size)
    data = ser.read(30*10*8) #30*2*10000)  # non-blocking read due to timeout

    if data:
        #print(f"Received {len(data)}")
        iq = convert_all(data)
        f.write(iq)
        f.flush()
        s.sendto(iq, ("127.0.0.1",2000))
        #s.sendall(iq)
        # Display each byte as hex
        #print("Hex values:", [f"{b:02X}" for b in data])

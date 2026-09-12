from iqlib import DeviceClient, auto_detect_port

import math

def calc_inc(f_out, f_clk):
    return int((1 << 32) * f_out / f_clk)

class SinTxNoLUT:
    BITS = 12  # DAC resolution (12-bit: 0..4095)
    
    def __init__(self, f_out=1_000, f_clk=32_000):
        self.PHASE_INC = calc_inc(f_out, f_clk)
        self.phase = 0
        
        # Scaling parameters for 12-bit unsigned DAC
        self.max_val = (1 << self.BITS) - 1  # 4095
        self.mid_val = self.max_val / 2.0    # 2047.5

    def next(self):
        # 1. Update 32-bit phase accumulator
        self.phase = (self.phase + self.PHASE_INC) & 0xFFFFFFFF
        
        # 2. Convert 32-bit phase to radians: [0, 2^32) -> [0, 2*pi)
        rad_sin = (self.phase / (1 << 32)) * 2.0 * math.pi
        
        # 3. Add pi/2 (90 degrees) for the Cosine component
        rad_cos = rad_sin + (math.pi / 2.0)
        
        # 4. Compute sine and cosine using math.sin()
        sin_val = round(self.mid_val + self.mid_val * math.sin(rad_sin))
        cos_val = round(self.mid_val + self.mid_val * math.sin(rad_cos))
        
        return sin_val, cos_val

    def get_iq(self, n_samples):
        iq_data = bytearray()
        for _ in range(n_samples):
            s, c = self.next()
            iq_data.extend(s.to_bytes(2, 'little'))
            iq_data.extend(c.to_bytes(2, 'little'))
        return iq_data
    

class SinTx:
    BITS = 12
    
    def __init__(self, f_out=1_000, f_clk=32_000, N = 32):
        self.N = N
        self.PHASE_INC = calc_inc(f_out, f_clk)
        self.phase = 0
        self.sin_lut = []
        
        max_val = (1 << self.BITS) - 1  # 4095
        mid_val = max_val / 2.0         # 2047.5
        
        for i in range(self.N):
            v = round(mid_val + mid_val * math.sin(2 * math.pi * i / self.N))
            self.sin_lut.append(v)

    def next(self):
        self.phase = (self.phase + self.PHASE_INC) & 0xFFFFFFFF        
        index = (self.phase >> 27) & (self.N - 1)
        cos_index = (index + (self.N // 4)) % self.N        
        return self.sin_lut[index], self.sin_lut[cos_index]

    def get_iq(self, n_samples):
        iq_data = bytearray()
        for _ in range(n_samples):
            s, c = self.next()
            iq_data.extend(s.to_bytes(2, 'little'))
            iq_data.extend(c.to_bytes(2, 'little'))
        return iq_data


def main():
    #test()

    dds = SinTxNoLUT(f_out=1, f_clk=32)
    port = auto_detect_port()
    print(f"Using port {port}")
    client = DeviceClient(port=port, baudrate=50000000, timeout=3.0)

    try:
        resp = client.ping()
        print(f"Ping response: {resp.decode()}")
        iq_data = b""
        request_size2, overflow2, tx_usb_overflow2, rx_usb_overflow2 = 0, 0, 0, 0
        while True:
            SB = 512
            
            request_size, overflow, tx_usb_overflow, rx_usb_overflow = client.req_iq(payload=iq_data)
            print(f"Send IQ response: {request_size}, {overflow}, {tx_usb_overflow}, {rx_usb_overflow}")
            if request_size>=SB:
                n = request_size//SB
                # k = request_size%SB
                for i in range(n-1):
                    iq_data = dds.get_iq(SB//4)
                    client.send_iq(payload=iq_data)

                iq_data = dds.get_iq(SB//4)
            else:
                iq_data = b""

            # if overflow2 != overflow or tx_usb_overflow2 != tx_usb_overflow or rx_usb_overflow2 != rx_usb_overflow:
            #     print(f"Send IQ response: {request_size}, {overflow}, {tx_usb_overflow}, {rx_usb_overflow}")
            #     overflow2, tx_usb_overflow2, rx_usb_overflow2 = overflow, tx_usb_overflow, rx_usb_overflow

            # request_size, overflow, tx_usb_overflow, rx_usb_overflow = client.get_iq_stream_tx_info()
            # print(f"Send IQ response: {request_size}, {overflow}, {tx_usb_overflow}, {rx_usb_overflow}")            
            # SB = 512
            # n = request_size//SB
            # # k = request_size%SB
            # for i in range(n):
            #     iq_data = dds.get_iq(SB//4)
            #     client.send_iq(payload=iq_data)


            # client.send_iq(payload=b"\x00" * 512)
            # client.send_iq(payload=b"\x00" * 512)
            # client.send_iq(payload=b"\x00" * 512)
            # client.send_iq(payload=b"\x00" * 512)
            # client.send_iq(payload=b"\x00" * 512)

    finally:
        client.close()

import matplotlib.pyplot as plt

if __name__ == "__main2__":
    # --- Parameters: edit these as needed ---
    f_out = 16
    f_clk = 32
    n_samples = 16
 
    gen = SinTxNoLUT(f_out=f_out, f_clk=f_clk)
 
    sins, coss = [], []
    for _ in range(n_samples):
        s, c = gen.next()
        sins.append(s)
        coss.append(c)
 
    # --- Plot ---
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.plot(sins, marker='o', label='I (sin)')
    ax.plot(coss, marker='s', label='Q (cos)')
    ax.set_title(f"12-bit DAC I/Q Output (f_out={f_out} Hz, f_clk={f_clk} Hz)")
    ax.set_xlabel("Sample index")
    ax.set_ylabel("DAC code (0-4095)")
    ax.set_ylim(-100, 4195)
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
 
    fig.savefig("sintx_plot.png", dpi=150)
    plt.show()
 
    print("I values:", sins)
    print("Q values:", coss)

if __name__ == "__main__":
    main()
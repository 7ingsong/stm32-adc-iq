from iqlib import DeviceClient, auto_detect_port

def main():
    port = auto_detect_port()
    print(f"Using port {port}")
    client = DeviceClient(port=port, baudrate=50000000, timeout=3.0)

    try:
        resp = client.ping()
        print(f"Ping response: {resp.decode()}")
        f = open ("samples.cf32","wb+")
        while True:
            data = client.serial.read(2048*10)
            client.push(data)
            while client.get_size() >= (0x108*2):
                iq = client.process()
                f.write(iq)
                f.flush()

                # print(f"iq length={len(iq)}")
                client.sock_udp.sendto(iq, ("127.0.0.1",2000))

    finally:
        client.close()


if __name__ == "__main__":
    main()
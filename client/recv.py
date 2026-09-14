from iqlib import DeviceClient, auto_detect_port
import socket
import time

def main():
    port = auto_detect_port()
    print(f"Using port {port}")
    client = DeviceClient(port=port, baudrate=50000000, timeout=3.0)

    sock = socket.socket()
    sock.connect(("127.0.0.1", 2000))
    try:
        resp = client.ping()
        print(f"Ping response: {resp.decode()}")
        client.start_rx()
        f = open ("samples.cf32","wb+")
        deadline = time.time()+60
        while (deadline-time.time())>0:
            data = client.get_rx_iq_samples()
            iq = client.convert_all(data)
            f.write(iq)
            f.flush()
            sock.sendall(iq)

        client.stop_rx()

    finally:
        client.close()


if __name__ == "__main__":
    main()
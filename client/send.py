from iqlib import DeviceClient, auto_detect_port

def main():
    port = auto_detect_port()
    print(f"Using port {port}")
    client = DeviceClient(port=port, baudrate=50000000, timeout=3.0)

    try:
        resp = client.ping()
        print(f"Ping response: {resp.decode()}")

        request_size, overflow, tx_usb_overflow, rx_usb_overflow = client.send_iq(b"\x01"*128)
        print(f"Send IQ response: {request_size}, {overflow}, {tx_usb_overflow}, {rx_usb_overflow}")
    finally:
        client.close()


if __name__ == "__main__":
    main()
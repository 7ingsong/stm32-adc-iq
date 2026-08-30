import argparse
import pathlib
import sys
import time

import serial
from serial.tools import list_ports


FRAME_MAGIC = b"\xA5\x5A"
FRAME_HEADER_SIZE = 8
FRAME_MAX_PAYLOAD = 55

CMD_PING = 0x01
CMD_IQ_STREAM = 0x30

RESP_ACK = 0x80
RESP_ERR = 0x81
RESP_IQ_STREAM = 0xB0
RESP_IQ_DATA = 0xB1

ERR_NAMES = {
    1: "ERR_BAD_MAGIC",
    2: "ERR_BAD_LENGTH",
    3: "ERR_BAD_CRC",
    4: "ERR_BAD_COMMAND",
    5: "ERR_BAD_PAYLOAD",
    6: "ERR_FIFO_OVERFLOW",
    7: "ERR_EEPROM",
    8: "ERR_FPGA",
    9: "ERR_BAD_IMAGE",
}


class ProtocolError(RuntimeError):
    pass


def score_port(port_info):
    name = (port_info.device or "").lower()
    description = (port_info.description or "").lower()
    manufacturer = (port_info.manufacturer or "").lower()
    product = (port_info.product or "").lower()
    interface = (port_info.interface or "").lower()

    score = 0
    if "usbmodem" in name or "acm" in name:
        score += 5
    if "usbserial" in name or "ttyusb" in name:
        score += 3
    if "cdc" in description or "virtual com" in description:
        score += 5
    if "usb" in description:
        score += 2
    if "stm" in manufacturer or "stmicro" in manufacturer:
        score += 3
    if "stm" in product or "cdc" in product:
        score += 2
    if "cdc" in interface:
        score += 2

    return score


def auto_detect_port():
    ports = list(list_ports.comports())
    scored = []

    for port_info in ports:
        port_score = score_port(port_info)
        if port_score > 0:
            scored.append((port_score, port_info.device))

    scored.sort(key=lambda item: (-item[0], item[1]))
    if not scored:
        raise SystemExit("could not auto-detect a USB CDC port; pass --port explicitly")

    if len(scored) == 1 or scored[0][0] > scored[1][0]:
        return scored[0][1]

    raise SystemExit("multiple possible USB CDC ports found; pass --port explicitly")


def checksum16(init, data):
    total = init & 0xFFFF
    for value in data:
        total = (total + value) & 0xFFFF
    return total


def frame_checksum(cmd, seq, payload):
    length = len(payload)
    prefix = bytes((cmd, seq, length & 0xFF, (length >> 8) & 0xFF))
    total = checksum16(0, prefix)
    return checksum16(total, payload)


def build_frame(cmd, seq, payload=b""):
    if len(payload) > FRAME_MAX_PAYLOAD:
        raise ValueError(f"payload is too large: {len(payload)}")

    crc = frame_checksum(cmd, seq, payload)
    return bytes(
        (
            FRAME_MAGIC[0],
            FRAME_MAGIC[1],
            cmd,
            seq,
            len(payload) & 0xFF,
            (len(payload) >> 8) & 0xFF,
            crc & 0xFF,
            (crc >> 8) & 0xFF,
        )
    ) + payload


class DeviceClient:
    def __init__(self, port, baudrate, timeout):
        self.serial = serial.Serial(port=port, baudrate=baudrate, timeout=timeout)
        self.seq = 1

    def close(self):
        self.serial.close()

    def next_seq(self):
        value = self.seq
        self.seq = (self.seq + 1) & 0xFF
        return value

    def read_exact(self, size):
        data = bytearray()
        while len(data) < size:
            chunk = self.serial.read(size - len(data))
            if not chunk:
                raise TimeoutError(f"timed out while waiting for {size} bytes")
            data.extend(chunk)
        return bytes(data)

    def read_frame(self):
        while True:
            first = self.serial.read(1)
            if not first:
                raise TimeoutError("timed out while waiting for frame start")
            if first != FRAME_MAGIC[:1]:
                continue

            second = self.serial.read(1)
            if not second:
                raise TimeoutError("timed out while waiting for frame magic")
            if second != FRAME_MAGIC[1:2]:
                continue

            header = self.read_exact(FRAME_HEADER_SIZE - 2)
            cmd = header[0]
            seq = header[1]
            length = header[2] | (header[3] << 8)
            crc = header[4] | (header[5] << 8)

            if length > FRAME_MAX_PAYLOAD:
                raise ProtocolError(f"invalid payload length {length}")

            payload = self.read_exact(length)
            calc_crc = frame_checksum(cmd, seq, payload)
            if calc_crc != crc:
                raise ProtocolError(
                    f"CRC mismatch: got 0x{crc:04X}, expected 0x{calc_crc:04X}"
                )

            return {"cmd": cmd, "seq": seq, "payload": payload}

    def req_command(self, cmd, cmd_resp=RESP_ACK, payload=bytes()):
        seq = self.next_seq()
        self.serial.reset_input_buffer()
        self.serial.write(build_frame(cmd, seq, payload))

        while True:
            response = self.read_frame()

            if response["cmd"] == RESP_IQ_DATA:
                continue

            if response["seq"] != seq:
                continue

            break

        if response["cmd"] == RESP_ERR:
            payload = response["payload"]
            if len(payload) >= 2:
                err_code = payload[0]
                err_detail = payload[1]
                err_name = ERR_NAMES.get(err_code, f"UNKNOWN_ERROR_{err_code}")
                raise ProtocolError(
                    f"device returned {err_name} (code={err_code}, detail={err_detail})"
                )
            raise ProtocolError("device returned malformed error frame")

        if response["cmd"] != cmd_resp:
            raise ProtocolError(f"unexpected response 0x{response['cmd']:02X}")

        return response["payload"]

    def ping(self):
        return self.req_command(CMD_PING, cmd_resp=RESP_ACK)

    def set_iq_stream_mode(self, mode):
        return self.req_command(CMD_IQ_STREAM, cmd_resp=RESP_IQ_STREAM, payload=bytes((mode,)))

    def set_iq_stream(self, enabled):
        return self.set_iq_stream_mode(1 if enabled else 0)

    def get_iq_stream_status(self):
        return self.set_iq_stream_mode(2)


def main():
    port = auto_detect_port()
    print(f"Using port {port}")
    client = DeviceClient(port=port, baudrate=50000000, timeout=3.0)

    try:
        resp = client.ping()
        print(f"Ping response: {resp.decode()}")
        
    finally:
        client.close()


if __name__ == "__main__":
    main()
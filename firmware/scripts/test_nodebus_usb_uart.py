import unittest
import serial
import time
import threading

SERIAL_PORT = "/dev/ttyUSB0"  # Adjust to your platform Target Port
BAUD_RATE = 115200

# Constants matching MCU header parameters
NODEBUS_SOF = b"\xDE\xAD\xBE\xEF"
NODE_ID_TARGET = b"\x01"
VERSION_V1 = b"\x01"
VERSION_V2 = b"\x02"

NODEBUS_PAYLOAD_MAX_SIZE = 256  # Adjust if your buffer size differs

def crc16_ccitt(data: bytes) -> int:
    """Computes pure CRC-16-CCITT sequentially over data bytes."""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def encode_varint(value: int) -> bytes:
    """Encodes an integer into a Base-128 Varint."""
    if value == 0:
        return b"\x00"
    varint_bytes = bytearray()
    while value > 0x7F:
        varint_bytes.append((value & 0x7F) | 0x80)
        value >>= 7
    varint_bytes.append(value & 0x7F)
    return bytes(varint_bytes)


class TestNodebusParser(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """Open serial port connection once for the entire test suite."""
        cls.ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.5)
        time.sleep(2.0)  # Safe bootloader timeout delay
        print(f"\n[INIT] Connected to {SERIAL_PORT} for validation matrix.")

    @classmethod
    def tearDownClass(cls):
        """Ensure clean close of hardware resource on termination."""
        if cls.ser.is_open:
            cls.ser.close()
            print("\n[TERM] Closed serial communication profile.")

    def setUp(self):
        """Flush the MCU's hardware RX/TX queues before running each test window."""
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        time.sleep(0.1)

    def send_and_listen(self, packet_bytes: bytes, listen_time: float = 0.5) -> list:
        """Sends raw bytes, prints out raw transmission data, and logs MCU responses."""
        print(f"\n[TX] Writing bytes: {packet_bytes.hex().upper()}")
        self.ser.write(packet_bytes)
        time.sleep(listen_time)
        
        lines = []
        while self.ser.in_waiting > 0:
            line = self.ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"  [MCU LOG]: {line}")
                lines.append(line)
        return lines

    def test_01_valid_standard_packet(self):
        """Verifies standard payload serialization parses and executes correctly."""
        print("\n--- Running Test 1: Valid Standard Packet ---")
        payload = b"Hello\x00"
        varint_size = encode_varint(len(payload))
        
        protected = NODE_ID_TARGET + VERSION_V1 + varint_size + payload
        crc_bytes = crc16_ccitt(protected).to_bytes(2, byteorder='big')
        packet = NODEBUS_SOF + protected + crc_bytes

        logs = self.send_and_listen(packet)
        
        self.assertTrue(any("SOF matched" in l for l in logs), "Parser failed to lock onto frame sequence.")
        self.assertFalse(any("CRC mismatch" in l for l in logs), "Valid CRC calculation flag rejected.")

    def test_02_empty_payload_packet(self):
        """Validates that a 0-byte payload varint cleanly passes straight to CRC resolution."""
        print("\n--- Running Test 2: Empty Payload Packet ---")
        payload = b""
        varint_size = encode_varint(len(payload))
        
        protected = NODE_ID_TARGET + VERSION_V1 + varint_size + payload
        crc_bytes = crc16_ccitt(protected).to_bytes(2, byteorder='big')
        packet = NODEBUS_SOF + protected + crc_bytes

        logs = self.send_and_listen(packet)
        self.assertTrue(any("SOF matched" in l for l in logs))
        self.assertFalse(any("CRC mismatch" in l for l in logs))

    def test_03_random_noise_preceding_sof(self):
        """Validates state recovery when stream starts with garbage bytes before valid SOF."""
        print("\n--- Running Test 3: Random Noise Before SOF ---")
        payload = b"ValidData\x00"
        varint_size = encode_varint(len(payload))
        
        protected = NODE_ID_TARGET + VERSION_V1 + varint_size + payload
        crc_bytes = crc16_ccitt(protected).to_bytes(2, byteorder='big')
        valid_packet = NODEBUS_SOF + protected + crc_bytes
        
        noise = b"\xFF\x00\xAA\x55\xDE\xAD\xDE\xAD" 
        dirty_stream = noise + valid_packet

        logs = self.send_and_listen(dirty_stream)
        self.assertTrue(any("SOF matched" in l for l in logs), "Parser failed to recover frame locking after line noise injection.")
        self.assertFalse(any("CRC mismatch" in l for l in logs))

    def test_04_intentional_bad_crc(self):
        """Ensures frame integrity traps execution if data footprint modified or corrupt."""
        print("\n--- Running Test 4: Invalid CRC Injection ---")
        payload = b"SecurePayload\x00"
        varint_size = encode_varint(len(payload))
        
        protected = NODE_ID_TARGET + VERSION_V1 + varint_size + payload
        bad_crc_bytes = b"\x42\x42"  
        packet = NODEBUS_SOF + protected + bad_crc_bytes

        logs = self.send_and_listen(packet)
        self.assertTrue(any("CRC mismatch" in l for l in logs), "Parser failed to safely reject corrupted execution array.")

    def test_05_oversized_payload_boundary(self):
        """Validates log warnings trip gracefully when payload declaration violates buffer architecture."""
        print("\n--- Running Test 5: Oversized Payload ---")
        oversized_len = NODEBUS_PAYLOAD_MAX_SIZE + 10
        payload = b"A" * oversized_len
        varint_size = encode_varint(len(payload))
        
        protected = NODE_ID_TARGET + VERSION_V1 + varint_size + payload
        crc_bytes = crc16_ccitt(protected).to_bytes(2, byteorder='big')
        packet = NODEBUS_SOF + protected + crc_bytes

        logs = self.send_and_listen(packet)
        self.assertTrue(any("Oversized" in l or "oversized" in l for l in logs), "No defensive warn triggered when processing overflow length array.")

    def test_06_unsupported_version_rejection(self):
        """Asserts that packets with higher version profiles are discarded cleanly."""
        print("\n--- Running Test 6: Invalid Protocol Version ---")
        payload = b"FutureFormat\x00"
        varint_size = encode_varint(len(payload))
        
        protected = NODE_ID_TARGET + VERSION_V2 + varint_size + payload
        crc_bytes = crc16_ccitt(protected).to_bytes(2, byteorder='big')
        packet = NODEBUS_SOF + protected + crc_bytes

        logs = self.send_and_listen(packet)
        self.assertTrue(any("Ignoring" in l or "version" in l for l in logs), "Parser did not safely exit stream execution upon illegal version read.")


if __name__ == "__main__":
    unittest.main()

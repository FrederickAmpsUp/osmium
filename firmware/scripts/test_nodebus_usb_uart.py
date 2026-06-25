import serial
import time
import threading

# Configure to match your MCU's serial settings
SERIAL_PORT = "/dev/ttyUSB0"  # Change to '/dev/ttyUSB0' or similar on Linux/Mac
BAUD_RATE = 115200

def crc16_ccitt(data: bytes) -> int:
    """Computes the CRC-16-CCITT (Polynomial: 0x1021, Init: 0xFFFF) over data bytes."""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def serial_reader(ser):
    """Continuously reads and prints lines from the MCU in the background."""
    while ser.is_open:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f"[MCU]: {line}")
        except (serial.SerialException, OSError) as e:
            if getattr(e, 'errno', None) == 9 or "closed" in str(e).lower():
                break
            print(f"\nReader error: {e}")
            break

try:
    # Open the serial port
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    time.sleep(2)  # Wait for MCU to reset after opening port
    print(f"Connected to {SERIAL_PORT} at {BAUD_RATE} baud.")

    # Start the background reader thread
    reader_thread = threading.Thread(target=serial_reader, args=(ser,), daemon=True)
    reader_thread.start()

    # Shared framing constants
    sof = b"\xDE\xAD\xBE\xEF"
    node_id = b"\x01"
    null_terminator = b"\x00"

    # --- Test 1: Standard Valid Packet with Valid CRC ---
    # Payload: "Hello" (5) + \x00 (1) = 6 bytes total. size_byte = 6 - 1 = 5
    size_byte_1 = b"\x05"  
    payload_1 = b"Hello" + null_terminator
    
    # CRITICAL: The MCU computes the CRC starting from the header (ID + Size) through the payload
    protected_data_1 = node_id + size_byte_1 + payload_1
    crc_1 = crc16_ccitt(protected_data_1)
    crc_bytes_1 = crc_1.to_bytes(2, byteorder='big') # Match big-endian sequential read
    
    valid_packet = sof + protected_data_1 + crc_bytes_1

    print("\nSending Test 1: Valid Packet with Matching CRC...")
    print(f"Bytes sent: {valid_packet.hex().upper()}")
    ser.write(valid_packet)
    time.sleep(2)

    # --- Test 2: Overlapping Noise Packet with Valid CRC ---
    # Testing that your 'else if' fallback works AND the subsequent CRC checks out
    size_byte_2 = b"\x02"  # "Hi" (2) + \x00 (1) = 3 bytes total. size_byte = 3 - 1 = 2
    payload_2 = b"Hi" + null_terminator
    
    protected_data_2 = node_id + size_byte_2 + payload_2
    crc_2 = crc16_ccitt(protected_data_2)
    crc_bytes_2 = crc_2.to_bytes(2, byteorder='big')
    
    junk_overlap = b"\xDE\xAD\xDE\xAD\xBE\xEF" # Double DEAD sequence
    overlap_packet = junk_overlap + protected_data_2 + crc_bytes_2

    print("\nSending Test 2: Overlapping SOF Packet with Matching CRC...")
    print(f"Bytes sent: {overlap_packet.hex().upper()}")
    ser.write(overlap_packet)
    time.sleep(2)

    # --- Test 3: Corrupted Packet to Verify Checksum Rejection ---
    # We will send the exact same packet as Test 1, but deliberately break the CRC
    bad_crc_bytes = b"\x42\x42" 
    corrupted_packet = sof + protected_data_1 + bad_crc_bytes

    print("\nSending Test 3: Intentional Bad CRC Packet (Should trigger log.warn)...")
    print(f"Bytes sent: {corrupted_packet.hex().upper()}")
    ser.write(corrupted_packet)
    time.sleep(2)

    ser.close()
    print("\nTesting complete.")

except serial.SerialException as e:
    print(f"Error opening or writing to serial port: {e}")

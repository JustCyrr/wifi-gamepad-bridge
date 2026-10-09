import socket
import struct
import time
import math

SERVER_IP = "127.0.0.1"
SERVER_PORT = 52150
MAGIC = 0x57
PACKET_TYPE_INPUT = 0x01
SESSION_TOKEN = 0x8F4A

# Gamepad buttons
BTN_A = 0x1000
BTN_B = 0x2000
BTN_X = 0x4000
BTN_Y = 0x8000

def run_test(duration_sec=10, target_hz=120):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print(f"[*] Starting simulated gamepad sender to {SERVER_IP}:{SERVER_PORT}")
    print(f"[*] Rate: {target_hz} Hz | Duration: {duration_sec}s")
    
    interval = 1.0 / target_hz
    seq = 0
    start_time = time.time()
    
    while time.time() - start_time < duration_sec:
        t = time.time() - start_time
        seq += 1
        
        # Animate thumbsticks in circle
        lx = int(math.cos(t * 3.0) * 30000)
        ly = int(math.sin(t * 3.0) * 30000)
        rx = int(math.cos(-t * 2.0) * 20000)
        ry = int(math.sin(-t * 2.0) * 20000)
        
        # Animate triggers
        lt = int((math.sin(t * 2.0) + 1.0) * 127)
        rt = int((math.cos(t * 2.0) + 1.0) * 127)
        
        # Toggle buttons periodically
        buttons = 0
        if int(t) % 4 == 0:
            buttons |= BTN_A
        elif int(t) % 4 == 1:
            buttons |= BTN_B
        elif int(t) % 4 == 2:
            buttons |= BTN_X
        else:
            buttons |= BTN_Y
            
        timestamp_us = int(time.time() * 1_000_000)
        
        # 28 bytes binary format: <BBH I Q H B B h h h h
        packet = struct.pack(
            "<BBHIQHBBhhhh",
            MAGIC,
            PACKET_TYPE_INPUT,
            SESSION_TOKEN,
            seq,
            timestamp_us,
            buttons,
            lt,
            rt,
            lx,
            ly,
            rx,
            ry
        )
        
        sock.sendto(packet, (SERVER_IP, SERVER_PORT))
        time.sleep(interval)
        
    print(f"[+] Sent {seq} simulated input packets successfully!")

if __name__ == "__main__":
    run_test(duration_sec=15, target_hz=120)

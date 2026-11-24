# tcp_server.py  —— pip install 不需要，标准库即可
import socket, struct, time, math

HOST, PORT = "127.0.0.1", 9000

def build_frame(items):
    data = b""
    for sid, val, unit in items:
        data += bytes([sid]) + struct.pack("<f", val) + bytes([unit])
    body = bytes([len(data), 0x01]) + data
    checksum = sum(body) & 0xFF
    return b"\xAA\x55" + body + bytes([checksum])

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind((HOST, PORT))
srv.listen(1)
print(f"TCP server listening on {HOST}:{PORT}")

conn, addr = srv.accept()
print("client connected:", addr)

phase = 0.0
tick = 0
try:
    while True:
        temp = 25 + 5 * math.sin(phase)
        volt = 12 + 0.8 * math.sin(phase * 0.7 + 1.2)
        curr = 2 + 0.5 * math.sin(phase * 0.5 + 2.0)
        frame = build_frame([(1, temp, 1), (2, volt, 2), (3, curr, 3)])

        # 随机切片，制造半包；偶尔连发两帧，制造粘包
        import random
        stream = frame * (2 if tick % 10 == 0 else 1)
        i = 0
        while i < len(stream):
            n = random.randint(1, 7)
            conn.sendall(stream[i:i+n])
            i += n
            time.sleep(0.005)

        tick += 1
        phase += 0.15
        time.sleep(0.2)
except (BrokenPipeError, KeyboardInterrupt):
    print("server exit")
finally:
    conn.close()
    srv.close()
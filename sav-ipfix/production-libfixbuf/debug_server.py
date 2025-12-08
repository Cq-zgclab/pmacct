#!/usr/bin/env python3
"""
Simple TCP server to dump IPFIX bytes
"""
import socket
import sys

def hexdump(data):
    """Print hexdump of binary data"""
    for i in range(0, len(data), 16):
        hex_str = ' '.join(f'{b:02x}' for b in data[i:i+16])
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data[i:i+16])
        print(f'{i:04x}  {hex_str:<48}  {ascii_str}')

def main():
    host = '127.0.0.1'
    port = 4739
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((host, port))
        server.listen(1)
        print(f'Listening on {host}:{port}...')
        
        conn, addr = server.accept()
        with conn:
            print(f'Connected by {addr}')
            data = conn.recv(65536)
            print(f'\nReceived {len(data)} bytes:\n')
            hexdump(data)
            
            # Parse IPFIX header
            if len(data) >= 16:
                version = int.from_bytes(data[0:2], 'big')
                length = int.from_bytes(data[2:4], 'big')
                export_time = int.from_bytes(data[4:8], 'big')
                seq_num = int.from_bytes(data[8:12], 'big')
                obs_domain = int.from_bytes(data[12:16], 'big')
                
                print(f'\nIPFIX Message Header:')
                print(f'  Version: {version}')
                print(f'  Length: {length}')
                print(f'  Export Time: {export_time}')
                print(f'  Sequence: {seq_num}')
                print(f'  Observation Domain: {obs_domain}')

if __name__ == '__main__':
    main()

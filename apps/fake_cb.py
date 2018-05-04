from socket import *
import os, sys, time

s = socket(AF_UNIX, SOCK_STREAM)
try:
    os.unlink('/tmp/CLIPBOARD_SOCKET')
except OSError:
    pass
s.bind('/tmp/CLIPBOARD_SOCKET')
s.listen()
while True:
    c = s.accept()[0]

    d = int.from_bytes(c.recv(1), byteorder='little')
    print(f'\ncmd = {d}')
    region = int.from_bytes(c.recv(1), byteorder='little')
    print(f'region = {region}')
    size = int.from_bytes(c.recv(4), byteorder='little')
    print(f'size = {size}')
    if size > 0:
        data = c.recv(size + 1)
        print('data = {}'.format(data.decode('utf-8')))

    #d = int.from_bytes(c.recv(1), byteorder='little')
    #print(f'\ncmd = {d}')
    print(c.recv(1))
    #d = int.from_bytes(c.recv(1), byteorder='little')
    #print(f'region = {d}')
    print(c.recv(1))
    #d = int.from_bytes(c.recv(4), byteorder='little')
    #print(f'size = {d}')
    print(c.recv(4))
    print(c.recv(10 + 1))
    #if d > 0:
    #    d = c.recv(d)
    #    print('data = {}'.format(d.decode('utf-8')))

    r = b'\x02' + region.to_bytes(1, byteorder='little') + size.to_bytes(4, byteorder='little') + data
    c.send(r)
    time.sleep(2)

from socket import *
import os, sys, time

s = socket(AF_UNIX, SOCK_STREAM)
try:
    os.unlink('/tmp/cb_sock')
except OSError:
    pass
s.bind('/tmp/cb_sock')
s.listen()
while True:
    c = s.accept()[0]
    d = int.from_bytes(c.recv(1), byteorder='little')
    print(f'cmd = {d}')

    d = int.from_bytes(c.recv(1), byteorder='little')
    print(f'region = {d}')

    d = int.from_bytes(c.recv(4), byteorder='little')
    print(f'size = {d}')

    if d > 0:
        d = c.recv(d)
        print('data = {}'.format(d.decode('utf-8')))

    r = b'\x02\x08\x07\x00\x00\x00\x68\x68\x65\x6c\x6c\x6f\x00'
    c.send(r)
    time.sleep(2)

import socket

HOST = "127.0.0.1"
PORT = 8080

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    print("Connected!")

    while True:
        s.sendall(bytes(input("Send command to server> "), "utf-8"))
        data = s.recv(1024)

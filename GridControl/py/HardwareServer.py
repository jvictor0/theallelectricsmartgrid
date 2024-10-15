import socket
import sys
import Socket
import Event
import time

if __name__ == "__main__":
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_address = ((sys.argv[1]), (int(sys.argv[2])))
    sock.bind(server_address)
    sock.listen(1)

    while True:
        connection, client_address = sock.accept()
        try:
            socket = Socket.Socket(connection)
            socket.SetNonBlocking()
            while True:
                events = Event.GetEvents(socket)
                for event in events:
                    print(event)

                time.sleep(0.1)
        finally:
            connection.close()

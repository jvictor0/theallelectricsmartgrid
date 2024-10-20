import socket
import sys
import Socket
import Event
import time
import Grid

g_client_sockets = []

def PressCallback(x, y, edge):
    if edge == Grid.NeoTrellis.EDGE_RISING:
        velocity = 127
    else:
        velocity = 0
        
    for client_socket in g_client_sockets:
        Event.SendEvents(client_socket, [Event.Event(Event.EVENT_GRID_TOUCH, Event.PosToIndex((x, y)), [velocity])])

        print("semding press %d %d"%(x,y))

if __name__ == "__main__":
    Grid.InitGrid(PressCallback)
    
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
            g_client_sockets.append(socket)
            while True:
                events = Event.GetEvents(socket)
                for event in events:
                    pos = event.GetPos()
                    Grid.GridColor(pos[0], pos[1], event.GetColor())
                    print("received grid color")

                if len(events) > 0:
                    Grid.ShowGrid()

                Grid.SyncGrid()
                time.sleep(0.02)
        finally:
            connection.close()
            g_client_sockets.remove(socket)

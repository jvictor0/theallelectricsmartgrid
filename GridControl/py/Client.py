import Socket
import Event
import Grid
import select
import time
import socket
import os

class ClientGridCellState:
    def __init__(self, x, y):
        self.x = x
        self.y = y
        self.active_when_selected = True
        self.inactive_when_deselected = False
        self.color = None

class ClientGridState:
    def __init__(self, width = 20, height = 8):
        self.width = width
        self.height = height
        self.cells = []
        for x in range(width):
            self.cells.append([])
            for y in range(height):
                self.cells[x].append(ClientGridCellState(x, y))

    def Get(self, x, y):
        return self.cells[x][y]

    def SetColor(self, x, y, color):
        self.cells[x][y].color = color

class GridCellState:
    def __init__(self, x, y):
        self.x = x
        self.y = y
        self.color = None
        self.client_id = None

    def SelectClient(self, client_id, mgr):
        if self.client_id is not None:
            old_client_grid_cell_state = mgr.clients[self.client_id].grid_state.Get(self.x, self.y)
            if old_client_grid_cell_state.inactive_when_deselected:
                self.color = None
                self.client_id = None
                Grid.GridColor(self.x, self.y, self.color)
                
        client_grid_cell_state = mgr.clients[client_id].grid_state.Get(self.x, self.y)
        if client_grid_cell_state.active_when_selected:
            self.color = client_grid_cell_state.color
            self.client_id = client_id
            Grid.GridColor(self.x, self.y, self.color)


class GridState:
    def __init__(self, width = 20, height = 8):
        self.width = width
        self.height = height
        self.cells = []
        self.selected = None
        for x in range(width):
            self.cells.append([])
            for y in range(height):
                self.cells[x].append(GridCellState(x, y))

    def Get(self, x, y):
        return self.cells[x][y]

    def Select(self, client_id, mgr):
        for x in range(self.width):
            for y in range(self.height):
                self.cells[x][y].SelectClient(client_id, mgr)

    def SetColor(self, x, y, color):
        self.cells[x][y].color = color

    def RemoveClient(self, client_id):
        for x in range(self.width):
            for y in range(self.height):
                if self.cells[x][y].client_id == client_id:
                    self.cells[x][y].color = None
                    self.cells[x][y].client_id = None
        
class Client:
    def __init__(self, connection):
        self.socket = Socket.Socket(connection)
        self.grid_state = ClientGridState()
        self.ClearEvents()
        self.Handshake()

    def ClearEvents(self):
        self.events_to_send = {
            Event.EVENT_GRID_COLOR : [],
            Event.EVENT_GRID_TOUCH : []}
        
    def Handshake(self):
        buffer = bytearray(1)
        self.socket.Read(buffer, 1, 0)
        self.client_id = buffer[0]

    def SendEvents(self):
        for event_type, events in self.events_to_send.items():
            Event.SendEvents(self.socket, events)

        self.ClearEvents()
            
    def AddEvent(self, event):
        self.events_to_send[event.typ].append(event)

    def GetEvents(self):
        return Event.GetEvents(self.socket)

    def Close(self):
        self.socket.Close()

class ClientManager:
    def __init__(self):
        self.clients = {}
        self.fd_to_client = {}
        self.grid_state = GridState()
        self.poll = select.poll()
        self.OpenPort()

    def AddClient(self, connection):
        client = Client(connection)
        print("    Client id %d" % client.client_id)

        if client.client_id in self.clients:
            self.RemoveClient(client.client_id)
        
        self.clients[client.client_id] = client
        self.poll.register(client.socket.fd, select.POLLIN)
        self.fd_to_client[client.socket.fd.fileno()] = client.client_id

        if len(self.clients) == 1:
            self.grid_state.Select(client.client_id, self)

    def RemoveClient(self, client_id):
        client = self.clients[client_id]
        self.poll.unregister(client.socket.fd)
        client.Close
        del self.fd_to_client[client.socket.fd.fileno()]
        del self.clients[client_id]
        self.grid_state.RemoveClient(client_id)

    def ProcessColorEvent(self, client_id, event):
        print("ProcessColorEvent %s" % event)
        self.clients[client_id].grid_state.SetColor(event.GetX(), event.GetY(), event.GetColor())
        if self.grid_state.Get(event.GetX(), event.GetY()).client_id == client_id:
            print("    Setting color")
            self.grid_state.SetColor(event.GetX(), event.GetY(), event.GetColor())
            Grid.GridColor(event.GetX(), event.GetY(), event.GetColor())

    def ProcessTouchEvent(self, event):
        if event.GetX() == 19 and event.GetY() == 7:
            os.system("sudo shutdown now")
            
        client_id = self.grid_state.Get(event.GetX(), event.GetY()).client_id
        if client_id is not None:
            self.clients[client_id].AddEvent(event)

    def SendEvents(self):
        for client in self.clients.values():
            client.SendEvents()

    def GetClientEvents(self):
        result = []
        for fd, event in self.poll.poll(0):
            client_id = self.fd_to_client[fd]
            print("Client %d has event" % client_id)
            events = self.clients[client_id].GetEvents()
            if len(events) == 0:
                if not self.clients[client_id].socket.IsStillConnected():
                    print("Client %d disconnected" % client_id)
                    self.RemoveClient(client_id)
                    
            for event in events:
                result.append((client_id, event))

        did_something = True
        while did_something:
            did_something = False
            for client_id, client in self.clients.items():
                if client.socket.HasData():
                    print("Client %d has event" % client_id)
                    did_something = True
                    for event in self.clients[client_id].GetEvents():
                        result.append((client_id, event))

        return result

    def GetAndProcessClientEvents(self):
        for client_id, event in self.GetClientEvents():
            if event.typ == Event.EVENT_GRID_COLOR:
                self.ProcessColorEvent(client_id, event)
                    
    def GetAndProcessGridEvents(self):
        for event in Grid.GetEvents():
            if event.typ == Event.EVENT_GRID_TOUCH:
                self.ProcessTouchEvent(event)

    def GetAndProcessEvents(self):
        self.GetAndProcessClientEvents()
        self.GetAndProcessGridEvents()

    def OpenPort(self):
        self.bind_port = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.bind_port.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.bind_port.bind(("127.0.0.1", 7040))
        self.bind_port.listen(1)

    def AcceptConnection(self):
        readable, writable, errored = select.select([self.bind_port], [], [], 0)
        for s in readable:
            if s is self.bind_port:
                connection, addr = self.bind_port.accept()
                print("Accepted connection")
                self.AddClient(connection)

    def DoLoop(self):
        while True:
            self.AcceptConnection()
            self.GetAndProcessEvents()
            self.SendEvents()
            Grid.ShowGrid()
            time.sleep(0.02)

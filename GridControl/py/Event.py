import struct

EVENT_GRID_TOUCH = 0
EVENT_GRID_COLOR = 1

def IndexToPos(index):
    return (index % 20, index // 20)

def PosToIndex(pos):
    return pos[0] + pos[1] * 20

class Event:
    def __init__(self, typ, index, value):
        self.typ = typ
        self.index = index
        self.value = value

    @staticmethod
    def NumValues(typ):
        if typ == EVENT_GRID_TOUCH:
            return 1
        elif typ == EVENT_GRID_COLOR:
            return 3
        else:
            assert False, typ

    def Serialize(self, buffer, offset):
        buffer[offset] = self.index
        for i in range(self.NumValues(self.typ)):
            buffer[offset + 1] = self.value[i]

    @staticmethod
    def Deserialize(buffer, offset, typ):
        index = struct.unpack("B", buffer[offset:offset + 1])[0]
        values = []
        for i in range(Event.NumValues(typ)):
            values.append(struct.unpack("B", buffer[offset + 1 + i:offset + 1 + i + 1])[0])
                  
        return Event(typ, index, values)

    def GetPos(self):
        return IndexToPos(self.index)

    def SetPos(self, pos):
        self.index = PosToIndex(pos)

    def SetVelocity(self, velocity):
        self.value[0] = velocity

    def GetColor(self):
        return tuple(self.value[0:3])

    def __str__(self):
        if self.typ == EVENT_GRID_TOUCH:
            return "Touch: %d, %d" % self.GetPos()
        elif self.typ == EVENT_GRID_COLOR:
            return "Color: %d, %d, %d" % self.GetColor()
        else:
            assert False
                  
def GetEvents(socket):
    events = []
    buffer = bytearray(4096)
    written = socket.Read(buffer, 1, 0, False)
    if written == 0:
        return events

    typ = struct.unpack("B", buffer[0:1])[0]
    socket.Read(buffer, 1, 0)
    num_events = struct.unpack("B", buffer[0:1])[0]
    to_read = num_events * (1 + Event.NumValues(typ))
    socket.Read(buffer, to_read, 0)
    for i in range(num_events):
        offset = i * (1 + Event.NumValues(typ))
        events.append(Event.Deserialize(buffer, offset, typ))
        print(events[-1])

    return events

def SendEvents(socket, events):
    if len(events) == 0:
        return
    
    buffer = bytearray(2 + len(events) * (1 + Event.NumValues(events[0].typ)))
    buffer[0] = events[0].typ
    buffer[1] = len(events)
    for i, event in enumerate(events):
        event.Serialize(buffer, 2 + i * (1 + Event.NumValues(event.typ)))

    socket.Write(buffer)

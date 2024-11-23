import time
import board
import datetime
from adafruit_neotrellis.neotrellis import NeoTrellis
from adafruit_neotrellis.multitrellis import MultiTrellis
import GridId
import Event

i2c_bus = board.I2C() 
trelli = [[NeoTrellis(i2c_bus, False, addr=a, auto_write=False) for a in GridId.addrs_row_one],
          [NeoTrellis(i2c_bus, False, addr=a, auto_write=False) for a in GridId.addrs_row_two]]

trellis = MultiTrellis(trelli)

trellis.brightness = 1.0

grid_events = []
needs_show = False

def FormatTimestamp():
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")

def GridColor(x, y, color):
    global needs_show
    if color is None:
        color = (0, 0, 0)
        
    trellis.color(x, y, color)
    needs_show = True

def PressCallback(x, y, edge):
    global grid_events
    if edge == NeoTrellis.EDGE_RISING:
        velocity = 127
    else:
        velocity = 0
        
    grid_events.append(Event.Event(Event.EVENT_GRID_TOUCH, Event.PosToIndex((x, y)), [velocity]))
    print(FormatTimestamp(), "press %d %d %d" % (x, y, velocity))
    
def InitGrid():
    print("InitGrid")
    for y in range(8):
        for x in range(20):
            trellis.activate_key(x, y, NeoTrellis.EDGE_RISING)
            trellis.activate_key(x, y, NeoTrellis.EDGE_FALLING)
            trellis.set_callback(x, y, PressCallback)
            trellis.color(x, y, (0,0,0))

    print("InitGrid done")

def SyncGrid():
    trellis.sync()

def ShowGrid():
    global needs_show
    if needs_show:
        needs_show = False
        trellis.show()

def GetEvents():
    global grid_events
    grid_events = []
    SyncGrid()
    return grid_events
    

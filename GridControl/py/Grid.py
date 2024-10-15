import time
import board
from adafruit_neotrellis.neotrellis import NeoTrellis
from adafruit_neotrellis.multitrellis import MultiTrellis
import GridId

i2c_bus = board.I2C() 
trelli = [[NeoTrellis(i2c_bus, False, addr=a, auto_write=False) for a in GridId.addrs_row_one],
          [NeoTrellis(i2c_bus, False, addr=a, auto_write=False) for a in GridId.addrs_row_two]]

trellis = MultiTrellis(trelli)

trellis.brightness = 1.0

def GridColor(x, y, color):
    trellis.color(x, y, color)

def InitGrid(fn):
    print("InitGrid")
    for y in range(8):
        for x in range(20):
            trellis.activate_key(x, y, NeoTrellis.EDGE_RISING)
            trellis.activate_key(x, y, NeoTrellis.EDGE_FALLING)
            trellis.set_callback(x, y, fn)
            trellis.color(x, y, (0,0,0))

    print("InitGrid done")

def SyncGrid():
    trellis.sync()

def ShowGrid():
    trellis.show()

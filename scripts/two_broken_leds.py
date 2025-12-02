import mido
import time
import itertools

# ============================================================
# CONFIG
# ============================================================

TARGET_NAME = "WRLD.BLDR"   # substring match for your device name
CC_OUT_NUM  = 98
CC_OUT_CH   = 3        # channel 4 in MIDI-land is 3 (zero-based)
CC_IN_CH    = 2        # channel 3 inbound is 2 here

# ============================================================
# Locate ports
# ============================================================

out_port_name = None
in_port_name = None

for name in mido.get_output_names():
    print(f"Found output port: {name}")
    if TARGET_NAME.lower() in name.lower():
        out_port_name = name
        break

for name in mido.get_input_names():
    print(f"Found input port: {name}")
    if TARGET_NAME.lower() in name.lower():
        in_port_name = name
        break

if not out_port_name:
    raise RuntimeError(f"No MIDI OUT matches '{TARGET_NAME}'")

if not in_port_name:
    raise RuntimeError(f"No MIDI IN matches '{TARGET_NAME}'")

print(f"Opening OUT: {out_port_name}")
print(f"Opening IN : {in_port_name}")

outport = mido.open_output(out_port_name)
inport = mido.open_input(in_port_name)

# ============================================================
# Main loop
# ============================================================

values = itertools.cycle([0, 127])

time_delta = 0.25
num_leds = 4

try:
    while True:
        # Send alternating CC values
        v = next(values)
        for i in range(num_leds):
            msg = mido.Message('control_change',
                               channel=CC_OUT_CH,
                               control=CC_OUT_NUM + i,
                               value=v)
            outport.send(msg)

        # Read incoming messages for a moment
        t_end = time.time() + time_delta
        while time.time() < t_end:
            time.sleep(0.001)

except KeyboardInterrupt:
    print("\nExiting...")

finally:
    outport.close()
    inport.close()

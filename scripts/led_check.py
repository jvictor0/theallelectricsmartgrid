import mido
import time
import itertools

# ============================================================
# CONFIG
# ============================================================

TARGET_NAME = "WRLD.BLDR"   # substring match for your device name
CC_OUT_NUM  = 0
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

def MkMessage(num, val):
    if num < 64:
        return mido.Message('control_change',
                            channel=CC_OUT_CH,
                            control=num if num > 0 else 64,
                            value=val)
    else:
        return mido.Message('control_change',
                            channel=CC_OUT_CH + 1,
                            control=num - 64 if num > 64 else num,
                            value=val)

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
print("Sending alternating CC, printing any CC from chan 3...")

time_delta = 1.0
num_leds = 1

try:
    while True:
        # Send alternating CC values
        v = next(values)
        for i in range(num_leds):
            msg = MkMessage(i, v)
            outport.send(msg)

        # Read incoming messages for a moment
        t_end = time.time() + time_delta
        while time.time() < t_end:
            for msg in inport.iter_pending():
                if msg.type == 'control_change' and msg.channel == CC_IN_CH and msg.control == 1:
                    # set time_delta exponentially from 1 Hz to 200 Hz
                    # time_delta = 1.0 / freq, freq in [1, 200]
                    # We'll use msg.value in [0,127] to scale exponentially
                    min_freq = 1.0
                    max_freq = 200.0
                    # Map msg.value [0,127] to alpha [0,1]
                    alpha = msg.value / 127.0
                    freq = min_freq * ((max_freq / min_freq) ** alpha)
                    time_delta = 1.0 / freq
                    print("messages per second:", num_leds / time_delta, "time_delta:", time_delta, "leds:", num_leds)
                elif msg.type == 'control_change' and msg.channel == CC_IN_CH and msg.control == 2:
                    for i in range(128):
                        msg_clear = MkMessage(i, 0)
                        outport.send(msg_clear)
                    num_leds = msg.value + 1
                    print("Messages per second:", num_leds / time_delta, "time_delta:", time_delta, "leds:", num_leds)
            time.sleep(0.001)

except KeyboardInterrupt:
    print("\nExiting...")

finally:
    outport.close()
    inport.close()

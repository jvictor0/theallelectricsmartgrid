addrs_row_one = [0x2e + (1 << (4 - i)) for i in range(4)] + [0x2e]
addrs_row_two = [a + 1 for a in addrs_row_one]
addrs = [addrs_row_one, addrs_row_two]

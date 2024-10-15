

class Socket:
    def __init__(self, fd = None):
        self.fd = fd
        self.buffer = bytearray(4096)
        self.buffer_head = 0
        self.buffer_tail = 0

    def SetNonBlocking(self):
        self.fd.setblocking(False)
        
    def Read(self, buffer, size, offset, blocking = True):
        written = 0
        while size > 0:
            self.FillBuffer()
            if self.buffer_head == self.buffer_tail:
                if not blocking:
                    return written
                
                continue
            
            n = min(size, self.buffer_tail - self.buffer_head)
            buffer[offset:offset + n] = self.buffer[self.buffer_head:self.buffer_head + n]
            self.buffer_head += n
            offset += n
            size -= n
            written += n

        return written
        
    def FillBuffer(self):
        if self.buffer_head == self.buffer_tail:
            self.buffer_head = 0
            self.buffer_tail = self.fd.recv_into(self.buffer)

    def Write(self, buffer):
        self.fd.sendall(buffer)

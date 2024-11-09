import socket

class Socket:
    def __init__(self, fd = None):
        self.fd = fd
        self.buffer = bytearray(4096)
        self.buffer_head = 0
        self.buffer_tail = 0
        self.SetNonBlocking()

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
            try:
                self.buffer_tail = self.fd.recv_into(self.buffer)
            except BlockingIOError:
                pass

    def HasData(self):
        return self.buffer_head != self.buffer_tail
            
    def Write(self, buffer):
        self.fd.sendall(buffer)

    def Close(self):
        self.fd.close()

    def IsStillConnected(self):
        try:
            data = self.fd.recv(1, socket.MSG_PEEK)
            if len(data) == 0:
                return False

            return True
        except BlockingIOError:
            return True
        except ConnectionResetError:
            return False
        except Exception as e:
            print("IsStillConnected: %s" % e)
            return False
        
    def __del__(self):
        self.Close()

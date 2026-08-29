import struct 
import sys


class Conv12bits2u16:

    def __init__(self, fileName):
        self.fd = open(fileName,"rb")
        self.fd_out = open(fileName+".cs16","wb+")

    def conv2u16(self,raw):
        v0,=struct.unpack("<H",raw)
        v = float(v0)/0x1000
        x = int(v * 0xffff)-0x7fff
        return struct.pack("<h", x)

    def conv2u16be(self,raw):
        v0,=struct.unpack(">H",raw)
        v = float(v0)/0x1000
        x = int(v * 0xffff)-0x7fff
        return struct.pack("<h", x)

    def run(self):
        while True:
            raw = self.fd.read(2)
            if len(raw)<2:
                break
            x = self.conv2u16(raw)
            self.fd_out.write(x)


if __name__=="__main__":
    fileName="samples.u12"    
    c=Conv12bits2u16(fileName)
    c.run()
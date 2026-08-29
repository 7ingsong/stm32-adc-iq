import struct 
import sys


class Conv12bits2u16:

    def __init__(self, fileName):
        self.fd = open(fileName,"rb")
        self.fd_out = open(fileName+".cs16","wb+")
        self.fd_out1 = open(fileName+".1.s16","wb+")
        self.fd_out2 = open(fileName+".2.s16","wb+")
        

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
        i=0
        while True:
            raw = self.fd.read(1)
            if len(raw)!=1:
                break
            magic,=struct.unpack("<B",raw)
            i+=1
            if magic != 0xAA:
                print("error:", i)
                continue
            block = self.fd.read(60)
            if len(block)!=60:
                break

            i+=60

            raw = self.fd.read(1)
            if len(raw)!=1:
                break

            magic,=struct.unpack("<B",raw)
            i+=1
            if magic != 0x55:
                print("error:", i)
                continue
            j =0 
            while len(block)>0:
                raw = block[:2]
                if len(raw)<2:
                    break
                block=block[2:]
                x = self.conv2u16(raw)
                self.fd_out.write(x)
                if j%2==0:
                    self.fd_out1.write(x)
                else:
                    self.fd_out2.write(x)
                j+=1


if __name__=="__main__":
    fileName="samples.u12"    
    c=Conv12bits2u16(fileName)
    c.run()
import usb.core
import usb.util
import struct
import time
from time import sleep
from dataclasses import dataclass, field
from struct import pack, unpack

@dataclass
class SystemData:
    addrs: list[int] # The list of i2c addresses to try out.
    engage_prog: bool # Whether or not to engage the programming pin when programming
    data: bytes  # The actual data
    initseq: int # ID of the init sequence
    HEADER_LENGTH = 2 * 2 + 2 + 0x10

    def serialize(self):
        raw = self.data
        checksum = sum(raw) & 0xFFFE
        if self.engage_prog: checksum |= 1
        i2caddrs = self.addrs[:]
        while len(i2caddrs) < 0x10: i2caddrs.append(i2caddrs[-1])
        assert len(i2caddrs) == 0x10
        output = pack("<HHBB", len(self.data), checksum, self.initseq, 0) + bytes(i2caddrs) + raw
        return output

    @classmethod
    def parse(klass, data):
        (ln, checksum, initseq, _) = unpack("<HHBB", data[:6])
        assert ln == len(data) - klass.HEADER_LENGTH
        assert checksum == sum(contents := bytes(data[22:])) & 0xFFFE
        return klass(list(data[6:22]), not not (checksum & 1), contents, initseq)


class RedBox:
    def __init__(self):
        self.device = usb.core.find(idVendor=0xF00F, idProduct=0x0002)
        if self.device is None: raise ValueError("RedBox board not found!")
        # self.device.reset()
        self._reset()

    def write_system_data(self, sys_data: SystemData):
        raw = sys_data.serialize()
        self.device.write(4, raw)
        self._validate()
    def _reset(self):
        self.device.ctrl_transfer(0b0010_0000, 2, 1, 0, b'')
    def _validate(self):
        self.device.ctrl_transfer(0b0010_0000, 3, 1, 0, b'')
        self.assert_no_error()

    def program(self):
        self.device.ctrl_transfer(0b0010_0000, 1, 1, 0, b'')
        self.assert_no_error()
    def write_ram(self):
        self.device.ctrl_transfer(0b0010_0000, 5, 1, 0, b'')
        self.assert_no_error()

    def read_otp(self):
        self.device.ctrl_transfer(0b0010_0000, 4, 1, 0, b'')
        self.assert_no_error()
        return self.read_system_data()

    def read_ram(self):
        self.device.ctrl_transfer(0b0010_0000, 6, 1, 0, b'')
        self.assert_no_error()
        return self.read_system_data()

    def read_system_data(self):
        length = int.from_bytes(self.device.ctrl_transfer(0b1010_0000, 0xFC, 0, 0, 4, timeout=1000), 'little')
        self.device.ctrl_transfer(0b1010_0000, 0xFD, 0, 0, 1, timeout=1000)
        data = self.device.read(0x83, length, timeout=1000)
        return SystemData.parse(data)



    def read_error(self):
        return int.from_bytes(self.device.ctrl_transfer(0b1010_0000, 0xFE, 0, 0, 4, timeout=1000), 'little')
    def assert_no_error(self):

        while (err := self.read_error()) == 0xFFFFFFFF:
            sleep(0.1)

        if err != 0:
            print(f"Error!! {err}")
            assert False


def pack_nvm_format_to_binary(raw_txt: str) -> bytes:
    data = raw_txt.split("\n")
    data = [x.strip() for x in data if x.strip()][1:]
    prep = lambda a: (int(a[0]), int(a[2]))
    entries = [prep(a.split('\t')) for a in data]
    bytesize = (max(entries, key=lambda e: e[0])[0] + 7) // 8
    final_data = [0] * bytesize
    for n, v in entries:
        if v:
            final_data[n // 8] |= 1 << (n & 7)
        else:
            final_data[n // 8] &= ~(1 << (n & 7))
    return bytes(final_data)

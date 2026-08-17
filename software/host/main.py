from redbox import RedBox, SystemData, pack_nvm_format_to_binary
from argparse import ArgumentParser
from dataclasses import dataclass, field
import tomllib
import os

@dataclass
class ChipWriteData:
    engagepctl: bool = False
    padstart: bytes = b''
    padend: bytes = b''
    # [[bitreg, value], [bitreg2, value2]]
    forceadjust: list[list[int]] = field(default_factory=list)

    def prepare_image(self, image: bytes, ignore_forceadjust: bool):
        if not ignore_forceadjust:
            image = list(image)
            for (register_bit_no, value) in self.forceadjust:
                base, bit_off = divmod(register_bit_no, 8)
                if value:
                    image[base] |= (1 << bit_off)
                else:
                    image[base] &= ~(1 << bit_off)
            image = bytes(image)
        return self.padstart + image + self.padend

@dataclass
class ChipData:
    chip: str # Name
    i2caddrs: list[int] # The list of i2c addresses to try out.
    initseq: int # ID of the init sequence
    programsize: int
    verification_mask: bytes

    write_ram_data: ChipWriteData = field(default_factory=ChipWriteData) # Config for writing RAM
    write_otp_data: ChipWriteData = field(default_factory=ChipWriteData) # Config for writing OTP

    def build_system_data_for_otp(self, content: bytes, ignore_forceadjust: bool):
        return SystemData(self.i2caddrs, self.write_otp_data.engagepctl, self.write_otp_data.prepare_image(content, ignore_forceadjust), self.initseq)

    def build_system_data_for_ram(self, content: bytes, ignore_forceadjust: bool):
        return SystemData(self.i2caddrs, self.write_ram_data.engagepctl, self.write_ram_data.prepare_image(content, ignore_forceadjust), self.initseq)

    def mask_data_for_verification(self, data: bytes) -> bytes:
        return bytes(a & b for a, b in zip(data, self.verification_mask))

def read_image(name: str) -> bytes:
    with open(name, 'rb') as e:
        raw = e.read()
    if name.lower().endswith('.txt'):
        raw = pack_nvm_format_to_binary(raw.decode('ascii'))
    return raw

def parse_chip_write_data(from_file: dict[str, any]) -> ChipWriteData:
    out = ChipWriteData()
    out.engagepctl = from_file.get('engagepctl', False)
    out.padstart = bytes(from_file.get('padstart', []))
    out.padend = bytes(from_file.get('padend', []))
    out.forceadjust = from_file.get('forceadjust', [])
    return out

def form_system_data_from_library(chip_name: str) -> ChipData:
    path = chip_name if chip_name.endswith('.toml') else os.path.join(os.path.split(__file__)[0], 'library', chip_name + '.toml')
    with open(path, 'rb') as fd:
        content = tomllib.load(fd)
    chip_data = ChipData(content['meta']['chip'], content['meta']['i2caddrs'], content['meta']['initseq'], content['meta']['programsize'], bytes.fromhex(content['meta']['verification_mask']))
    if 'write_ram' in content:
        chip_data.write_ram_data = parse_chip_write_data(content['write_ram'])
    if 'write_otp' in content:
        chip_data.write_otp_data = parse_chip_write_data(content['write_otp'])
    if len(chip_data.verification_mask) != chip_data.programsize:
        raise BaseException("Invalid library file: Verification mask length does not match program data length")
    return chip_data


def write_ram(args):
    rb = RedBox()
    contents = read_image(args.file)
    chip_data = form_system_data_from_library(args.chip)
    if len(contents) != chip_data.programsize:
        print("Program size doesn't match the definition in library file!'")
        return;
    rb.write_system_data(chip_data.build_system_data_for_ram(contents, args.ignore_forceadjust))
    rb.write_ram()
    print("OK")

def write_otp(args):
    if not args.no_confirm:
        if input("Please type 'YES' to continue. This action will write the one-time-programmable memory. This action CANNOT BE UNDONE. ") != 'YES':
            print("Aborted.")
            return
    rb = RedBox()
    contents = read_image(args.file)
    chip_data = form_system_data_from_library(args.chip)
    if len(contents) != chip_data.programsize:
        print("Program size doesn't match the definition in library file!'")
        return
    wr_system_data = chip_data.build_system_data_for_otp(contents, args.ignore_forceadjust)
    rb.write_system_data(wr_system_data)
    rb.program()
    print("PROGRAM OK")
    if not args.no_verify:
        _, data = universal_read(args, 'read_otp', rb)
        write_data = wr_system_data.data
        if l := len(chip_data.write_otp_data.padstart):
            write_data = write_data[l:]
        if l := len(chip_data.write_otp_data.padend):
            write_data = write_data[:-l]
        write_data = chip_data.mask_data_for_verification(write_data)
        read_back_data = chip_data.mask_data_for_verification(data.data)
        if read_back_data != write_data:
            print("Read / Verify MISMATCH!")
            print("OTP:")
            hexdump(read_back_data)
            print()
            print("Expected:")
            hexdump(write_data)
        else:
            print("VERIFY OK")


def universal_read(args, reading_func, rb: RedBox = None) -> (ChipData, SystemData):
    if not rb:
        rb = RedBox()
    chip_data = form_system_data_from_library(args.chip)
    rb.write_system_data(chip_data.build_system_data_for_ram(b'\0' * chip_data.programsize, False))
    getattr(rb, reading_func)()
    new_system_data = rb.read_system_data()
    return chip_data, new_system_data


def read_mem(args, reading_func='read_otp'):
    _, data = universal_read(args, reading_func)
    with open(args.destination, 'wb') as e:
        e.write(data.data)
    print("OK")

def hexdump(data: bytes):
    for i in range(0, len(data), 16):
        row = data[i:i+16]
        row_hex_str = ' '.join(f'{e:02x}' for e in row)

        print(f"{i:04x}\t{row_hex_str}{' ' * (47 - len(row_hex_str))}\t")


def dump_mem(args, reading_func='read_otp', memory_name='OTP'):
    chip, data = universal_read(args, reading_func)
    print("Read data from GreenPAK:")
    print(f"Chip: {chip.chip}")
    print(f"Program size: {chip.programsize}")
    print(f"I2C Address: {data.addrs[0]}")
    print()
    print(f"{memory_name} contents:")
    hexdump(data.data)

def read_ram(args):
    read_mem(args, 'read_ram')
def dump_ram(args):
    dump_mem(args, 'read_ram', 'RAM')

def main():
    args = ArgumentParser(
        prog='RedBox',
        description='An open-source programmer for the Renesas / Silego GreenPAK mixed-domain chips',
    )
    args.add_argument('-c', '--chip', help='The name of the chip to use', required=True)

    subparsers = args.add_subparsers(dest='action', required=True)

    parser_write = subparsers.add_parser('write-ram', help='Write the program to GreenPAK\' RAM')
    parser_write.add_argument('file', help='The file to write')
    parser_write.add_argument('--ignore-forceadjust', help="Ignore chip-specific directives to set certain registers to default values", action='store_true')
    parser_write.set_defaults(func=write_ram)

    parser_write_otp = subparsers.add_parser('write-otp', help='Write the program to GreenPAK\' OTP (ONE TIME PROGRAMMABLE memory)')
    parser_write_otp.add_argument('file', help='The file to write')
    parser_write_otp.add_argument('-y', '--no-confirm', help="Skip the confirmation prompt", action='store_true')
    parser_write_otp.add_argument('--ignore-forceadjust', help="Ignore chip-specific directives to set certain registers to default values", action='store_true')
    parser_write_otp.add_argument('--no-verify', help="Do not verify the chip contents after writing", action='store_true')
    parser_write_otp.set_defaults(func=write_otp)

    parser_read_ram = subparsers.add_parser('read-ram', help='Read the GreenPAK\'s RAM')
    parser_read_ram.add_argument('destination', help='The destination')
    parser_read_ram.set_defaults(func=read_ram)

    parser_dump_ram = subparsers.add_parser('dump-ram', help='Dump the GreenPAK\'s RAM as HEX to the terminal')
    parser_dump_ram.set_defaults(func=dump_ram)

    parser_read_otp = subparsers.add_parser('read-otp', help='Read the GreenPAK\'s OTP')
    parser_read_otp.add_argument('destination', help='The destination')
    parser_read_otp.set_defaults(func=read_mem)

    parser_dump_otp = subparsers.add_parser('dump-otp', help='Dump the GreenPAK\'s OTP as HEX to the terminal')
    parser_dump_otp.set_defaults(func=dump_mem)

    parsed = args.parse_args()
    parsed.func(parsed)

if __name__ == "__main__":
    main()

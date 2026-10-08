"""Reject unsafe relocation/bounds inputs before embedding an ARM image."""
import importlib.util
from pathlib import Path
import struct
import unittest
ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location("zzbuild",ROOT/"scripts/build_zz9000_debug.py")
build=importlib.util.module_from_spec(spec);spec.loader.exec_module(build)

def fixture():
    data=bytearray(256)
    data[:6]=b"\x7fELF\x01\x01"
    struct.pack_into("<HHIIIIIHHHHHH",data,16,3,40,1,0,52,84,0,52,32,1,40,1,0)
    struct.pack_into("<8I",data,52,1,128,0,0,16,32,7,4)
    struct.pack_into("<10I",data,84,0,9,0,0,160,8,0,0,4,8)
    struct.pack_into("<I",data,128,12)
    struct.pack_into("<II",data,160,0,23)
    return data

class PayloadTests(unittest.TestCase):
    def test_relative_pointer_and_zero_initialized_bss(self):
        image,rel,entry=build.unpack_elf(fixture())
        self.assertEqual((len(image),rel,entry),(32,[0],0))
        self.assertEqual(image[16:],bytes(16))

    def test_rejects_non_arm_or_big_endian(self):
        for off,val in ((5,2),(18,3)):
            data=fixture();data[off]=val
            with self.assertRaises(ValueError):build.unpack_elf(data)

    def test_rejects_out_of_allocation_image_or_relocation(self):
        for off,val in ((52+8,0x30000000),(52+20,0x8000),(160,32),(160,1),(128,32)):
            data=fixture();struct.pack_into("<I",data,off,val)
            with self.assertRaises(ValueError):build.unpack_elf(data)

    def test_rejects_symbol_lookup_and_instruction_relocations(self):
        for info in (2,28,0x117):
            data=fixture();struct.pack_into("<I",data,164,info)
            with self.assertRaises(ValueError):build.unpack_elf(data)

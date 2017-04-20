#!/usr/bin/python
#
# CTR FIRM/NCCH IDA Loader by blasty
# ==================================
# requires construct and idapython.

import sys
import struct
from idaapi import Choose
import construct
from construct import *

def u8(name, len = 1):
	if len == 1:
		return ULInt8(name)
	else:
		return Bytes(name, len)

def u16(name):
	return ULInt16(name)

def u32(name):
	return ULInt32(name)

def u64(name):
	return ULInt64(name)

class media_unit_adapter(Adapter):
	def _encode(self, obj, context):
		return struct.pack("<L", obj / 0x200)
	def _decode(self, obj, context):
		return struct.unpack("<L", obj)[0] * 0x200


def media_unit(name):
	return media_unit_adapter(u8(name, 4))

def fw_header(name):
	return Struct(name,
		u8("magic", 4),
		u32("unk"),
		u32("arm11_entry"),
		u32("arm9_entry"),
		u8("reserved", 0x30),
		Repeater(4, 4, fw_section_hdr("section_headers"))
	)

def fw_section_hdr(name):
	return Struct(name,
		u32("offset"),
		u32("loadadr"),
		u32("size"),
		u32("core"),
		u8("hash", 0x20)
	)

def ncch_hdr(name):
	return Struct(name,
		u8("sig", 0x100),
		u8("magic", 4),
		media_unit("content_size"),
		u64("partition_id"),
		u16("maker_code"),
		u16("version"),
		u32("reserved"),
		u64("program_id"),
		u8("tmp_flag"),
		u8("reserved", 0x2f),
		u8("product_code", 0x10),
		u8("exhdr_hash", 0x20),
		u32("exhdr_size"),
		u32("reserved2"),
		u8("flags", 8),
		media_unit("plain_offset"),
		media_unit("plain_size"),
		u8("reserved3", 8),
		media_unit("exefs_offset"),
		media_unit("exefs_size"),
		media_unit("exefs_hash_size"),
		u8("reserved4", 4),
		media_unit("romfs_offset"),
		media_unit("romfs_size"),
		media_unit("romfs_hash_size"),
		u8("reserved5", 4),
		u8("exefs_sb_hash", 0x20),
		u8("romfs_sb_hash", 0x20)
	)

def ncch_exhdr_systeminfo_flags(name):
	return Struct(name,
		u8("reserved", 5),
		u8("flag"),
		u8("remasterversion", 2)
	)

def ncch_exhdr_systeminfo(name):
	return Struct(name,
		u32("savedata_size"),
		u32("reserved"),
		u64("jump_id"),
		u8("reserved2", 0x30)
	)

def ncch_exhdr_codesegment_info(name):
	return Struct(name,
		u32("address"),
		u32("num_maxpages"),
		u32("code_size")
	)

def ncch_exhdr_codeset(name):
	return Struct(name,
		Bytes("name", 8),
		ncch_exhdr_systeminfo_flags("flags"),
		ncch_exhdr_codesegment_info("text"),
		u32("stack_size"),
		ncch_exhdr_codesegment_info("ro"),
		u8("reserved", 4),
		ncch_exhdr_codesegment_info("data"),
		u32("bss_size")
	)

def ncch_exhdr_dependencylist(name):
	return Struct(name,
		Repeater(8, 8, Bytes("program_id", 0x30))
	)

def ncch_exhdr_storageinfo(name):
	return Struct(name,
		u64("ext_savedata_id"),
		u64("system_savedata_id"),
		u64("reserved"),
		u8("access_info", 7),
		u8("other_attrib", 1)
	)

def ncch_exhdr_arm11_system_caps(name):
	return Struct(name,
		u64("program_id"),
		u8("flags", 8),
		Repeater(2, 2, u8("reslimit_desc", 0x10)),
		ncch_exhdr_storageinfo("storageinfo"),
		Repeater(8, 8, u8("service_access_control", 0x10)),
		u8("reserved", 0x1f),
		u8("reslimit_cat", 1)
	)

def ncch_exhdr_arm11_kernel_caps(name):
	return Struct(name,
		Repeater(4, 4, u8("descriptors", 28)),
		u8("reserved", 0x10)
	)

def ncch_exhdr_arm9_access_control(name):
	return Struct(name,
		u8("descriptors", 15),
		u8("descversion", 1)
	)

def ncch_exhdr_accessdesc(name):
	return Struct(name,
		u8("sig", 0x100),
		u8("pubkey_mod", 0x100),
		ncch_exhdr_arm11_system_caps("arm11_system_caps"),
		ncch_exhdr_arm11_kernel_caps("arm11_kernel_caps"),
		ncch_exhdr_arm9_access_control("arm9_access_control")
	)

def ncch_exhdr(name):
	return Struct("ncch_exhdr",
		ncch_exhdr_codeset("codeset_info"),
		ncch_exhdr_dependencylist("dependencies"),
		ncch_exhdr_systeminfo("systeminfo"),
		ncch_exhdr_arm11_system_caps("arm11_system_caps"),
		ncch_exhdr_arm11_kernel_caps("arm11_kernel_caps"),
		ncch_exhdr_arm9_access_control("arm9_access_control"),
		ncch_exhdr_accessdesc("accessdesc")
	)

def exefs_file_header(name):
	return Struct(name,
		Bytes("filename", 8),
		u32("offset"),
		u32("size")
	)

def exefs_header(name):
	return Struct(name,
		Repeater(8, 8, exefs_file_header("file_headers")),
		Bytes("reserved", 0x80),
		Repeater(8, 8, Bytes("file_hashes", 0x20))
	)

def get_type(f):
	f.seek(0x00)
	if f.read(4) == "FIRM":
		return "FIRM"

	f.seek(0x100)
	if f.read(4) == "NCCH":
		return "NCCH"

	return 0

def accept_file(f, n):
	if n == 0:
		return get_type(f)

	return 0
 
def load_firm_file(f, offs):
	f.seek(offs)
	f_hdr = fw_header("f_hdr")
	hdr = f_hdr.parse(f.read(256))

	f.seek(offs)
	b = f.read(f.size() - offs)

	for section in hdr.section_headers:
		if section.size == 0:
			continue

		add_segm(0, section.loadadr, section.loadadr + section.size, ".section0"+str(section.core), "CODE")
		mem2base(b[section.offset:], section.loadadr, section.loadadr + section.size)

	return 1

def load_ncch_file(f, offset):
	n_hdr = ncch_hdr("ncch_hdr")
	f.seek(offset)
	b = f.read(f.size()-offset)
	print "a"
	hdr = n_hdr.parse(b[0:0x200])
	print "b"
	print hdr
	n=(0x200+hdr['exhdr_size'])
	exhdr = ncch_exhdr("exhdr").parse(b[0x200:])

	o = hdr.exefs_offset
	e = o + hdr.exefs_size
	exefs = b[o:e]

	exefs_hdr = exefs_header("ehdr").parse(exefs)
	print "NAME: %s" % exhdr.codeset_info.name

	text = exhdr.codeset_info.text
	print "  |_ .text -> addr:%08x nump:%08x size:%08x" % (text.address, text.num_maxpages, text.code_size)
	add_segm(0, text.address, text.address + text.code_size, ".text", "CODE")
	offs = 0x200
	mem2base(exefs[offs:], text.address, text.address + text.code_size)

	ro = exhdr.codeset_info.ro
	print "  |_ .ro   -> addr:%08x nump:%08x size:%08x" % (ro.address, ro.num_maxpages, ro.code_size)
	add_segm(0, ro.address, ro.address + ro.code_size, ".ro", "DATA")
	offs = 0x200 + text.code_size
	mem2base(exefs[offs:], ro.address, ro.address + ro.code_size)

	data = exhdr.codeset_info.data
	print "  |_ .data -> addr:%08x nump:%08x size:%08x" % (data.address, data.num_maxpages, data.code_size)
	add_segm(0, data.address, data.address + data.code_size, ".data", "DATA")
	offs = 0x200 + text.code_size + ro.code_size
	mem2base(exefs[offs:], data.address, data.address + data.code_size)

	return 1

def load_file(f, neflags, format):
	idaapi.set_processor_type("ARM", SETPROC_ALL|SETPROC_FATAL)
	tp = get_type(f)

	if tp == "NCCH":
		return load_ncch_file(f, 0)
	elif tp == "FIRM":
		return load_firm_file(f, 0)

	return 0

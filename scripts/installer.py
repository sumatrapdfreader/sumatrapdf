import os.path
import struct
import zlib

INSTALLER_HEADER_FILE      = "kifi"
INSTALLER_HEADER_FILE_ZLIB = "kifz"
INSTALLER_HEADER_END       = "kien"

def write_no_size(fo, data):
  log("Writing %d bytes at %d '%s'" % (len(data), fo.tell(), data))
  fo.write(data)
  
def write_with_size(fo, data, name=None):
  if name:
    log("Writing %d bytes at %d (data of name %s)" % (len(data), fo.tell(), name))
  else:
    log("Writing %d bytes at %d (data)" % (len(data), fo.tell()))
  fo.write(data)
  tmp = struct.pack("<I", len(data))
  log("Writing %d bytes at %d (data size)" % (len(tmp), fo.tell()))
  fo.write(tmp)

def append_file(fo, path, name_in_installer):
  fi = open(path, "rb")
  data = fi.read()
  fi.close()
  assert len(data) == os.path.getsize(path)
  write_with_size(fo, data, name_in_installer)
  write_with_size(fo, name_in_installer)
  write_no_size(fo, INSTALLER_HEADER_FILE)

def append_file_zlib(fo, path, name_in_installer):
  fi = open(path, "rb")
  data = fi.read()
  fi.close()
  assert len(data) == os.path.getsize(path)
  data2 = zlib.compress(data, 9)
  assert len(data2) < os.path.getsize(path)
  write_with_size(fo, data2, name_in_installer)
  write_with_size(fo, name_in_installer)
  write_no_size(fo, INSTALLER_HEADER_FILE_ZLIB)
  
  """
  data3 = bz2.compress(data, 9)  
  print("")
  print("uncompressed: %d" % len(data))
  print("zlib        : %d" % len(data2))
  print("bz2         : %d" % len(data3))
  print("")
  """

def mark_end(fo):
  write_no_size(fo, INSTALLER_HEADER_END)

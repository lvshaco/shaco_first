#_*_coding:utf-8_*_

##
# @file tolua_opt.py
# @brief    excel数据转化为tbl数据
# @author lvxiaojun
# @version 
# @Copyright shengjoy.com
# @date 2012-12-19

import struct
import os 
from exparser import *
from exconvertor import *

__all__ = []

_optname = u"tbl"

def _serialize_to_file(table, field_map, outfile):
    """
    序列化所有数据
    """
    items = table.items
    op = file(outfile, "wb")
    op.write(struct.pack("I", len(items)))
    rowlen = 0
    for i in range(field_map_size(field_map)):
        flen = field_map_get_flen(field_map, i)
        ftype = field_map_get_ftype(field_map, i)
        if ftype == "uarray":
            flen = 2 + flen * 4
        rowlen += flen
    op.write(struct.pack("I", rowlen))
    for row in range(len(items)): 
        item = items[row]
        for i in range(len(item)):
            val = item[i]
            fname = field_map_get_fname(field_map, i)
            fvname = field_map_get_fvname(field_map, i)
            flen = field_map_get_flen(field_map, i)
            ftype = field_map_get_ftype(field_map, i)
           
            if (ftype == "uint32" or ftype == "int32") and flen == 4:
                op.write(struct.pack("i", val and int(val) or 0))
            elif (ftype == "uint64" or ftype == "int64") and flen == 8:
                op.write(struct.pack("Q", val and long(val) or 0))
            elif ftype == "string":
                op.write(struct.pack("%ds" % (flen-1), str(val)))
                op.write(struct.pack("c", '\0'))
            elif ftype == "uarray":
                if val:
                    subv = map(lambda x: int(x), val.split(","))
                    nsub = len(subv)
                else:
                    nsub = 0
                op.write(struct.pack("H", nsub))
                for vi in range(flen):
                    op.write(struct.pack("I", subv[vi] if vi < nsub else 0))
            else:
                log.write("\n[error : unknow field type, \
                name#%s, vname#%s, type#%s, len#%s val#%s]\n"%
                (fname, fvname, ftype, flen, val))
                exit(1)
    op.close() 

def _convert(infile, sheetdesc, out_dir):
    """
    执行转化
    """
    fields    = sheetdesc["fields"]
    field_map = ep_filter_fields(fields, _optname)
    if len(field_map) == 0: return CONV_NO_FIELDS

    sheetname = sheetdesc["name"]
    outfile   = sheetdesc["outfile"]
    outfile   = os.path.join(out_dir, "%s.%s"%(outfile, _optname))
    sheet     = ep_open(infile, sheetname)
    table     = ep_parse(sheet, field_map)
    _serialize_to_file(table, field_map, outfile)
    log.write(outfile)
    return CONV_OK

#安装本引擎
ec_install_opt(_optname, _convert)

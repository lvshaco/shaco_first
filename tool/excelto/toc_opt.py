#_*_coding:utf-8_*_

##
# @file toc_opt.py
# @brief    excel数据转化为c数据
# @author lvxiaojun
# @version 
# @Copyright shengjoy.com
# @date 2012-12-19

import struct
import os 
from exparser import *
from exconvertor import *

__all__ = []

_optname = u"c"

def _serialize_to_file(cname, sheetname, field_map, outfile):
    """
    序列化所有数据
    """
    op = file(outfile, "wb")
    op.write("// %s\n"%sheetname)
    op.write("struct %s_tplt {\n"%cname)
    for i in range(field_map_size(field_map)):
        fname = field_map_get_fname(field_map, i)
        fvname = field_map_get_fvname(field_map, i)
        flen   = field_map_get_flen(field_map, i)
        ftype  = field_map_get_ftype(field_map, i)
        if (ftype == "uint32" or ftype == "int32") and flen == 4:
            fstr = "%s_t %s"%(ftype, fvname);
        elif (ftype == "uint64" or ftype == "int64") and flen == 8:
            fstr = "%s_t %s"%(ftype, fvname);
        elif ftype == "string":
            fstr = "char %s[%d]"%(fvname, flen);
        else:
            log.write("\n[error : unknow field type, \
            name#%s, vname#%s, type#%s, len#%s ]\n"%
            (fname, fvname, ftype, flen))
            exit(1)
        op.write("    %s; %s %s\n"%(fstr, "//".rjust(25-len(fstr)), fname))
    op.write("\n};");
    op.close() 

def _convert(infile, sheetdesc, out_dir):
    """
    执行转化
    """
    fields    = sheetdesc["fields"]
    field_map = ep_filter_fields(fields, _optname)
    if len(field_map) == 0: return CONV_NO_FIELDS

    sheetname = sheetdesc["name"]
    cname     = sheetdesc["outfile"]
    outfile   = sheetdesc["outfile"]
    outfile   = os.path.join(out_dir, "%s.%s"%(outfile, _optname))
    _serialize_to_file(cname, sheetname, field_map, outfile)
    log.write(outfile)
    return CONV_OK

#安装本引擎
ec_install_opt(_optname, _convert)

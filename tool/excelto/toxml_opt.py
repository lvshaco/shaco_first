#_*_coding:utf-8_*_

##
# @file tolua_opt.py
# @brief    excel数据转化为xml数据
# @author lvxiaojun
# @version 
# @Copyright shengjoy.com
# @date 2012-12-19

import os
from parser import *
from convertor import *

__all__ = []

_optname = u"xml"

def _seri_table(name, table, field_map):
    r = []
    for item in table.items:
        v = _seri_item(item, field_map)
        r.append(v)
    root = "<root>\n    %s\n</root>" % "\n    ".join(r)
    return '<?xml version="1.0" encoding="utf-8"?>\n%s' % root

def _seri_item(item, field_map):
    """
    序列化单行数据
    """
    r = []
    for i in range(len(item)):
        fvname = field_map_get_fvname(field_map, i)
        ftype = field_map_get_ftype(field_map, i)
        v = _seri_value(item[i], ftype)
        r.append('%s="%s"' % (fvname, v))
    return "<item %s />" % " ".join(r)

def _seri_value(v, ftype):
    """
    序列化值
    """
    if ftype == "string":
        v = v.replace("\n", "")
        return unicode(v)
    elif ftype == "float":
        return unicode(float(v) if v else 0.0)
    elif ftype == "QWORD":
        return unicode(long(v)  if v else 0)
    else:
        return unicode(int(v)   if v else 0)

def _dump_to_file(outfile, s):
    """
    输出到文件
    """
    op = file(outfile, "wb")
    op.write(s)
    op.close()

def _convert(infile, sheetdesc, out_dir):
    """
    执行转化
    """
    fields = sheetdesc["fields"]
    field_map = ep_filter_fields(fields, _optname)
    if len(field_map) == 0: return CONV_NO_FIELDS
  
    sheetname = sheetdesc["name"]
    outfile   = sheetdesc["outfile"]
    outfile = os.path.join(out_dir, "%s.%s"%(outfile, _optname))
    sheet = ep_open(infile, sheetname)
    table = ep_parse(sheet, field_map)
    s = _seri_table(sheetname.upper(), table, field_map)
    _dump_to_file(outfile, s)
    log.write(outfile)
    return CONV_OK

#安装本引擎
ec_install_opt(_optname, _convert)

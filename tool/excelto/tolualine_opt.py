#_*_coding:utf-8_*_

##
# @file tolualine_opt.py
# @brief    excel数据转化为lua数据
# @author lvxiaojun
# @version 
# @Copyright shengjoy.com
# @date 2012-12-19

import os
from parser import *
from convertor import *

__all__ = []

_optname = u"lualine"

def _serialize_field(v):
    """
    序列化字段
    """
    return '"%s"'%unicode(v)

def _serialize_table(name, table):
    """
    序列化所有数据
    """
    items = table.items
    r = []
    for row in range(1, len(items)):
        item = items[row]
        line = []
        for col in range(1, len(item)):
            line.append(_serialize_field(item[col]))
        r.append("{%s}"%",".join(line))
    return "%s={\n%s\n}" % (name, ",\n".join(r))

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
    sheetname = sheetdesc["name"]
    outfile   = sheetdesc["outfile"]
    outfile = os.path.join(out_dir, "_%s.lua"%(outfile))
    sheet = ep_open(infile, sheetname)
    table = ep_parse_raw(sheet)
    s = _serialize_table(sheetname.upper(), table)
    _dump_to_file(outfile, s)
    log.write(outfile)
    return CONV_OK

#安装本引擎
ec_install_opt(_optname, _convert)

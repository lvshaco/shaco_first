#_*_coding:utf-8_*_

##
# @file tolua_opt.py
# @brief    excel数据转化为lua数据
# @author lvxiaojun
# @version 
# @Copyright shengjoy.com
# @date 2012-12-19

import os
from exparser import *
from exconvertor import *

__all__ = []

_optname = u"lua"

class seri_opt:
    def seri(self, name, table, field_map):
        """
        序列化所有数据
        """
        if table.haskey():
            return self._seri_by_key(name, table, field_map)
        else:
            return self._seri_normal(name, table, field_map)
         
    def _seri_normal(self, name, table, field_map):
        items = table.items
        r = []
        for i in range(len(items)):
            v = self._seri_item(items[i], field_map, -1)
            r.append(v)
        return "%s={\n%s\n}" % (name, ",\n".join(r))

    def _seri_by_key(self, name, table, field_map):
        data = table.restruct()
        keytype = field_map_get_ftype(field_map, table.keycol)
        r = []
        for key in sorted(data.keys()):
            group = data[key]
            g = []
            for item in group:
                v = self._seri_item(item, field_map, table.keycol)
                g.append(v)
            r.append("%s={\n%s\n}" % (self._seri_key(key, keytype), ",\n".join(g)))
        return "%s={\n%s\n}" % (name, ",\n".join(r))

    def _seri_item(self, item, field_map, keycol):
        """
        序列化单行数据
        """
        r = []
        for i in range(len(item)):
            if i == keycol: continue
            fvname = field_map_get_fvname(field_map, i)
            ftype = field_map_get_ftype(field_map, i)
            v = self._seri_value(item[i], ftype)
            f = self._seri_field(fvname, v)
            r.append(f)
        return "{%s}" % ",".join(r)

    def _seri_field(self, fvname, v):
        return "%s=%s"%(fvname, v)

    def _seri_key(self, v, ftype):
        return "[%s]" % self._seri_value(v, ftype)

    def _seri_value(self, v, ftype):
        """
        序列化值
        """
        if ftype == "string":
            v = v.replace("\n", "")
            return '"%s"'%unicode(v)
        elif ftype == "float":
            return unicode(float(v) if v else 0.0)
        elif ftype == "QWORD":
            return unicode(long(v)  if v else 0)
        else:
            return unicode(int(v)   if v else 0)

_optname2 = u"lua2"

class seri_opt2(seri_opt):
    def _seri_field(self, fvname, v):
        return v

def _dump_to_file(outfile, s):
    """
    输出到文件
    """
    op = file(outfile, "wb")
    op.write(s)
    op.close()

def _convert(infile, sheetdesc, out_dir, optname, seri):
    """
    执行转化
    """
    fields = sheetdesc["fields"]
    field_map = ep_filter_fields(fields, optname)
    if len(field_map) == 0: return CONV_NO_FIELDS
  
    sheetname = sheetdesc["name"]
    outfile   = sheetdesc["outfile"]
    if outfile[-4:] != "data":
        outfile = outfile + "data"

    outfile = os.path.join(out_dir, "_%s.%s"%(outfile, _optname))
    sheet = ep_open(infile, sheetname)
    table = ep_parse(sheet, field_map)
    s = seri.seri(sheetname.upper(), table, field_map)
    _dump_to_file(outfile, s)
    log.write(outfile)
    return CONV_OK

def _convert1(infile, sheetdesc, out_dir):
    return _convert(infile, sheetdesc, out_dir, _optname, seri_opt())

def _convert2(infile, sheetdesc, out_dir):
    return _convert(infile, sheetdesc, out_dir, _optname2, seri_opt2())


#安装本引擎
ec_install_opt(_optname, _convert1)
ec_install_opt(_optname2, _convert2)

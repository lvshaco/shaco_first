#_*_ coding:utf-8 _*_

##
# @file convert_excel.py
# @brief    excel转化脚本
# @author lvxiaojun
# @version 
# @Copyright shengjoy.com
# @date 2012-12-19

import os
import sys

def _import(idir):
    path = os.path.split(os.path.realpath(__file__))[0]
    path = os.path.join(path, idir)
    for item in os.listdir(path):
        f = os.path.join(path, item)
        if os.path.isfile(f):
            module, ext = os.path.splitext(os.path.basename(f))
            if ext == ".py": 
                dirname = os.path.dirname(f)
                insert_path = False
                if not dirname in sys.path:
                    sys.path.insert(0, dirname)
                    insert_path = True
                __import__(module)
                if insert_path:
                    del sys.path[0]


def _get_outdirs(s):
    out_dirs = {}
    paths = s.split(":")
    for one in paths:
        type2path = one.split("=")
        types = type2path[0].split(".")
        for t in types:
            out_dirs[t] = type2path[1]
    print out_dirs
    return out_dirs

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print "usage : %s config in_dir out_dir\
                (lua.lua2.lualine=*:tbl=*:xml=*:c=*)" % sys.argv[0]
        sys.exit(1)

    _import("excelto")
    config = sys.argv[1]
    in_dir = sys.argv[2]
    out_dirs = _get_outdirs(sys.argv[3])
    convertor = sys.modules["convertor"]
    convertor.ec_convert(config, in_dir, out_dirs) 

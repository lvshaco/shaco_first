#_*_coding:utf-8_*_

##
# @file exconvertor.py
# @brief    excel数据转化器
# @author lvxiaojun
# @version 
# @Copyright shengjoy.com
# @date 2012-12-19

import os
from exparser import *

(CONV_OK, CONV_NO_FIELDS) = range(2)

class convert_opt:
    """
    转化引擎类
    """
    def __init__(self, name, fn):
        self.name = name
        self.fn = fn

"""
实现新的转化目标格式，只需要新建一个py模块，实现opt方法，后
调用ec_install_opt安装即可生效，不必再外部显示调用，即插即用。
"""
_convert_opts = []
def ec_install_opt(name, opt):
    """ 
    安装转化引擎
    """
    _convert_opts.append(convert_opt(name, opt))


def ec_convert(cfgfile, in_dir, out_dirs):
    """ 
    执行转化 
    """
    excels = ep_parse_config(cfgfile)
    for opt in _convert_opts:
        for excelname, sheets in excels.items():
            for sheetname, sheet in sheets.items():
                if opt.name in sheet["to"]:
                    infile  = os.path.join(in_dir, excelname)
                    if not out_dirs.has_key(opt.name):
                        log.write("[!!!error] not set the out directory to#%s\n"%opt.name)
                        return
                    out_dir = out_dirs[opt.name]
                    if not os.path.exists(out_dir):
                        os.mkdir(out_dir)
                    log.write("[excel to %s] %s (%s) ->  "%
                            (opt.name, infile, sheetname))
                    r = opt.fn(infile, sheet, out_dir)
                    if r == CONV_OK:
                        log.write("[ok]\n")
                    elif r == CONV_NO_FIELDS:
                        log.write("[warning: not fields need to convert]\n")
                    else:
                        log_write("[unknown error]\n")
                        
                

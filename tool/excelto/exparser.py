#_*_coding:utf-8_*_

##
# @file exparser.py
# @brief    excel数据解析器
# @author lvxiaojun
# @version 
# @Copyright shengjoy.com
# @date 2012-12-19

import time
import sys
import xlrd
import re
from xml.dom import minidom

__all__ = [ "log",
            "ep_parse_config", 
            "ep_filter_fields", 
            "ep_openexcel",
            "ep_opensheet",
            "ep_open", 
            "ep_parse", 
            "ep_parse_raw", 
            "ep_table",
            "field_map_get_fname", 
            "field_map_get_fvname", 
            "field_map_get_ftype", 
            "field_map_get_flen",
            "field_map_size"]

reload(sys)
sys.setdefaultencoding("utf-8")
log = sys.stdout

def field_map_get_fname(field_map, index):
    return field_map[index]["name"] 
def field_map_get_fvname(field_map, index):
    return field_map[index]["vname"]
def field_map_get_ftype(field_map, index):
    return field_map[index]["type"]
def field_map_get_flen(field_map, index):
    return field_map[index]["len"]
def field_map_size(field_map):
    return len(field_map)

def _open_minidom(cfgfile):
    """
    打开xml
    """
    fp = open(cfgfile, "r")
    s = fp.read()
    fp.close()
    try:
        charset = re.compile(".*\s*encoding=\"([^\"]+)\".*", re.M).match(s).group(1)
    except:
        charset = "utf-8"
    if charset.upper() != "UTF-8":
        s = re.sub(charset, "UTF-8", s)
        s = s.decode(charset).encode("UTF-8")
    try:
        dom = minidom.parseString(s)
    except Exception, e:
        log.write("[error : %s]\n"%str(e))
        exit(1)
    return dom

def ep_parse_config(cfgfile):
    """
    解析xml配置文件
    """
    def _get_attri(field, attr, force):
        if force or field.attributes.has_key(attr):
            return field.attributes[attr].value
        return u""

    dom = _open_minidom(cfgfile)
    root = dom.getElementsByTagName("table")
    excels = {}
    for t_node in root:
        excelname = _get_attri(t_node, "name", True)
        s_nodes = t_node.getElementsByTagName("SHEET")
        sheets = {}
    	for s_node in s_nodes:
            to = _get_attri(s_node, "to", False)
    	    outfile = _get_attri(s_node, "filename", True)
            pos = outfile.find(".")
            if pos != -1: outfile = outfile[:pos]
    	    sheetname = _get_attri(s_node, "name", True)
            f_nodes = s_node.getElementsByTagName("FIELD")
            
            fields = []
    	    for f_node in f_nodes:
                field = {
                    "name":_get_attri(f_node, "name", True),
                    "type":_get_attri(f_node, "type", True),
                    "len" :int(_get_attri(f_node, "len", True)),
                    "vname":_get_attri(f_node, "vname", False),
                    "to":_get_attri(f_node, "to", False).split(","),
                    "key":_get_attri(f_node, "key", False)}
                #为了兼容当前配置，此字段为空，表示需要转化为tbl格式
                if not field["to"][0]:
                    field["to"] = ["tbl"]
                #导出tbl强制导出c
                if "tbl" in field["to"] and \
                (not "c" in field["to"]):
                    field["to"].append("c")

                fields.append(field)
            sheet = {"name":sheetname, 
                     "outfile":outfile,
                     "to": to.split(","),
                     "fields":fields }
            #为了兼容当前配置，此字段为空，表示需要转化为tbl格式
            if not sheet["to"][0]:
                sheet["to"] = ["tbl"]
            #导出tbl强制导出c
            if "tbl" in sheet["to"] and \
            (not "c" in sheet["to"]):
                sheet["to"].append("c")
            sheets[sheetname] = sheet
        excels[excelname] = sheets
    return excels

def ep_filter_fields(fields, type):
    """
    根据目标类型过滤字段
    """ 
    field_map = []
    for field in fields:
        if type in field["to"]:
            field_map.append(field)
    return field_map

def ep_openexcel(excelname):
    return xlrd.open_workbook(excelname)

def ep_opensheet(excel, sheetname):
    try:
        sheet = excel.sheet_by_name(sheetname)
    except Exception, e:
        log.write("[error : %s]\n"%str(e))
        exit(1)
    return sheet

def ep_open(excelname, sheetname):
    """
    打开excel
    """
    excel = ep_openexcel(excelname)
    return ep_opensheet(excel, sheetname)

def _get_fields_col(sheet, field_map):
    """
    获取字段对应的列索引
    """
    col_count = sheet.ncols
    fields_col = [-1 for i in range(len(field_map))]
    for i in range(len(field_map)):
        fname = field_map_get_fname(field_map, i)
        for col in range(col_count):
            if sheet.cell_value(0, col) == fname:
                if fields_col[i] != -1:
                    log.write("\n[error : repeat field %s in col (%d vs %d)]\n" % 
                            (fname, fields_col[i], col))
                    exit(1)
                fields_col[i] = col
    lacks = 0
    for i in range(len(fields_col)):
        if fields_col[i] == -1:
            log.write("\n  can not found field#%s"%
                    field_map_get_fname(field_map, i))
            lacks = lacks+1
    if lacks > 0:
        log.write("\n[error : %d fields lack]\n" % lacks)
        exit(1)

    return fields_col

def _get_key_col(field_map):
    """
    获取key所在列索引
    """
    for i in range(len(field_map)):
        key = field_map[i]["key"]
        if key == "unique":
            return i, True
        elif key == "key":
            return i, False
    return -1, False

class ep_table:
    """
    ep_parse解析结果
    """
    def __init__(self, keycol, unique, items):
        self.keycol = keycol
        self.unique = unique
        self.items  = items

    def haskey(self):
        return self.keycol >= 0

    def restruct(self):
        if not self.haskey():
            return self.items
        r = {}
        for item in self.items:
            key = item[self.keycol]
            if not r.has_key(key):
                r[key] = []
            r[key].append(item)
        return r

def ep_parse(sheet, field_map):
    """
    生成数据行列表
    """
    keycol, unique = _get_key_col(field_map)
    fields_col = _get_fields_col(sheet, field_map)
    items = []
    for row in range(1, sheet.nrows):
        item = []
        for i in range(len(fields_col)):
            fname = field_map_get_fname(field_map, i)
            col = fields_col[i]
            v = sheet.cell_value(row, col)
            item.append(v)
        items.append(item)
    return ep_table(keycol, unique, items)

def ep_parse_raw(sheet):
    """
    生成所有数据行列表
    """
    items = []
    for row in range(0, sheet.nrows):
        item = []
        for col in range(0, sheet.ncols):
            v = sheet.cell_value(row, col)
            item.append(v)
        items.append(item)
    return ep_table(-1, False, items)


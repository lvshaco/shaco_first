#_*_coding:utf-8_*_

##
# @file tolua_opt.py
# @brief    
# @author lvxiaojun
# @version 
# @Copyright shengjoy.com
# @date 2012-12-19

import struct
import os
from exparser import *
from exconvertor import *

__all__ = []

_optname = u"map"

TYPEID_MAX = 10 # 0~9
CTYPE_ITEM = 0
CTYPE_CELL = 1

def parse_blocksheet(infile, map_config, outfile):
    #log.write("\nmap_config:")
    #print(map_config)
    # id, width, height, block, colortex, spectex
    mapid   = int(map_config["id"])
    width   = int(map_config["width"])
    height  = int(map_config["height"])
    block   = int(map_config["block"])
    colortex = map(lambda x: int(x), map_config["colortex"].split(","))
    spectex = map(lambda x: int(x), map_config["spectex"].split(","))
    
    sheetname = "%s%d"%(u"地块", block)
    log.write("\n  map%d parse (%s) -> "%(mapid, sheetname))

    if height <= 0 or width <=0 or height > 99*100:
        log.write("height <= 0 or widht <= 0\n")
        exit(1)
    
    depth = (height+99)/100
    if depth != len(colortex):
        log.write("colortex count != depth\n")
        exit(1)
    
    outdir = os.path.dirname(outfile)
    outfile = os.path.join(outdir, "map%d.map"%mapid)
    op = file(outfile, "wb")
 
    sheet   = ep_open(infile, sheetname)
    table   = ep_parse_raw(sheet)

    items = table.items
    row   = len(items)-3
    col   = len(items[0])-4
    if row < height:
        log.write("height too big\n")
        exit(1)

    if col < width:
        log.write("width too big\n")
        exit(1)

    # map struct ->
    # height
    # width
    # typeids [ntype1,ntype2,...,ntypeheight, [1,2,3][1,2,3], ...]
    # cell_tag list
    """
    struct cell_tag {
        uint16_t isassign:1; // or rand typed need rand
        uint16_t dummy:15;
        uint8_t  cellrate;   // if 0, then cellid is done
        uint8_t  itemrate;   // if 0, then itemid is done
        uint32_t cellid;
        uint32_t itemid;
    }
    """
    op.write(struct.pack("H", width))
    op.write(struct.pack("H", height))

    typeids = map(lambda x: map(lambda y: int(y), x.split(",")), items[1][2].split("|"))
    #print(typeids)
    if depth > len(typeids):
        log.write("typeids count no enough\n")
        exit(1)

    curoff = 0
    for i in range(depth):
        num = len(typeids[i])
        if num == 0 or num >= TYPEID_MAX:
            log.write("num must 0~%d"%TYPEID_MAX)
            exit(1)
        op.write(struct.pack("B", curoff))
        op.write(struct.pack("B", num))
        curoff += num

    for i in range(depth):
        for typeid in typeids[i]:
            op.write(struct.pack("B", typeid))

    for h in range(height):
        item = items[h+3]
        ch = h+1
        for w in range(width):
            cw = w+1
            cell = item[w+4]
            if isinstance(cell, basestring):
                cell_config = map(lambda x: int(x), cell.split(","))
               
                lcell = len(cell_config)
                if lcell <= 0:
                    log.write("empty cell(%d,%d)\n"%(cw,ch))
                    exit(1)

                isassign = (int(cell_config[0]) != 0) and 1 or 0
            else:
                isassign = 0
            if isassign:
                ctype = int(cell_config[1])
                if ctype == CTYPE_ITEM:
                    typeid   = 0
                    cellrate = 0
                    itemid   = int(cell_config[2]) if lcell > 2 else 0
                    itemrate = int(cell_config[3]) if lcell > 3 else 0
                elif ctype == CTYPE_CELL:
                    typeid   = int(cell_config[2]) if lcell > 2 else 0
                    cellrate = int(cell_config[3]) if lcell > 3 else 0
                    itemid   = int(cell_config[4]) if lcell > 4 else 0
                    itemrate = int(cell_config[5]) if lcell > 5 else 0
                else:
                    log.write("unkown ctype, in (%d,%d)\n"%(cw,ch))
                    exit(1)
            else:
                ctype    = CTYPE_CELL
                typeid   = 0
                cellrate = 0
                itemid   = 0
                itemrate = 0

            if ctype == CTYPE_CELL:
                texid = colortex[h/100]
                cellid = 1000 + typeid*100 + texid
            else:
                cellid = 0
            if cellid == 0 and itemid == 0:
                log.write("null cell(%d, %d)\n"%(cw,ch))
                exit(1)
            if cellrate < 0 or cellrate > 99:
                log.write("cellrate must 0~99, in (%d,%d)\n"%(cw,ch))
                exit(1)
            if itemrate <0 or itemrate > 99:
                log.write("itemrate must 0~99, in (%d,%d)\n"%(cw,ch))
                exit(1)
            if cellid > 0 and cellrate == 0:
                cellrate = 100
            if itemid > 0 and itemrate == 0:
                itemrate = 100
            flag = isassign
            op.write(struct.pack("H", flag))
            op.write(struct.pack("B", cellrate))
            op.write(struct.pack("B", itemrate))
            op.write(struct.pack("I", cellid))
            op.write(struct.pack("I", itemid))
            #log.write("isassign %d, ctype %d, cellrate %d, itemrate %d, cellid %d, \
                    #itemid %d, in(%d,%d)\n"%
                    #(isassign, ctype, cellrate, itemrate, cellid, itemid, cw,ch))
    op.close()
    log.write(outfile)

def parse_mapsheet(infile, table, field_map, outfile):
    """
    序列化所有数据
    """
    
    map_config = {}

    items = table.items
    for row in range(len(items)): 
        item = items[row]
        for i in range(len(item)):
            val = item[i]
            fvname = field_map_get_fvname(field_map, i)
            map_config[fvname] = val
        parse_blocksheet(infile, map_config, outfile)

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
    parse_mapsheet(infile, table, field_map, outfile)
    return CONV_OK

#安装本引擎
ec_install_opt(_optname, _convert)

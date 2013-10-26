#_*_ coding:utf-8 _*_

import os
import sys

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print "usage : %s path outfile" % sys.argv[0]
        sys.exit(1)

    path = sys.argv[1];
    outfile = sys.argv[2];
    name, _ = os.path.splitext(os.path.basename(outfile))
    out = open(outfile, "w");
    out.write("#ifndef __%s_h__\n"%name.lower())
    out.write("#define __%s_h__\n"%name.lower())
    files = os.listdir(path);
    print "[concat file]"
    for fname in files:
        fname = os.path.join(path, fname)
        print "+%s"%fname
        f = open(fname, "r");
        s = f.read()
        out.write("\n")
        out.write(s)
        out.write("\n")
        f.close();
    out.write("\n")
    out.write("#endif")
    out.close()
    print "=%s"%outfile

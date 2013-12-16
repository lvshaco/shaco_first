require "config_base"
def_node("game", 0)

cmdctl_handler="cmdctlgame"
tplt_handler="tpltgame"

sc_service=sc_service..",cmdctlgame,tpltgame,game"

require "config_base"
def_node("world", 0)

sc_service=sc_service..",cmdctlworld,tpltworld,world,gamematch,playerdb,rolelogic,ringlogic,awardlogic,attribute"

cmdctl_handler="cmdctlworld"
tplt_handler="tpltworld"

world_gmax=10 
world_cmax_pergate=10000 
world_hmax_pergate=11000 

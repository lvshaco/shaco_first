ngx.header.content_type = "text/plain"

local redis = require "resty.redis"
local cjson = require "cjson"
local strfmt = string.format
local ngxvar = ngx.var
local connpool_size = 100
local conn_alive = 10000
local shm_expire = 6--todo
--local rankt = ngxvar.rank_type
local rankt = ngxvar['arg_t']
local typevalid = {dashi=true, xinshou=true,chuji=true,zhongji=true,gaoji=true,daren=true,prices=true}
if not typevalid[rankt] then
    --ngx.say("invalid rank type:", rankt)
    return
end

local function _query_scorelist()
    local red  = redis:new()
    local ok, err = red:connect(ngxvar.rank_redis_ip, ngxvar.rank_redis_port);
    if not ok then
        ngx.log(ngx.ERR, err);
        return
    end 
    local rs, err = red:zrevrange(strfmt("rank:%s", rankt), "0", "100", "withscores")
    if not rs then
        ngx.log(ngx.ERR, err)
        return
    end
    local ok, err = red:set_keepalive(conn_alive, connpool_size)
    if not ok then
        ngx.log(ngx.ERR, err) 
    end
    return rs
end

local function _query_ranklist(scorelist)
    local red  = redis:new()
    local ok, err = red:connect(ngxvar.user_redis_ip, ngxvar.user_redis_port);
    if not ok then
        ngx.log(ngx.ERR, err);
        return
    end
    local clist = {}
    local idx=1
    local uid
    red:init_pipeline()
    for i=1, #scorelist, 2 do 
        uid = scorelist[i]
        clist[idx] = {id=uid, score=scorelist[i+1], name=nil, role=nil}
        idx = idx+1
        red:hmget(strfmt("user:%u", uid), "name", "role")
    end
    local rs, err = red:commit_pipeline()
    if not rs then 
        ngx.log(ngx.ERR, err);
        return
    end
    local ok, err = red:set_keepalive(conn_alive, connpool_size)
    if not ok then
        ngx.log(ngx.ERR, err) 
    end
    for i, one in ipairs(rs) do
        if one[1] then
            clist[i].name = one[1]
            clist[i].role = one[2]
        end
    end
    return clist 
end

local rankshm = ngx.shared.rank
local ranklist = rankshm:get(rankt)
if not ranklist then
    local scorelist = _query_scorelist()
    if not scorelist then
        return
    end
    --ngx.say("scorelist:", cjson.encode(scorelist))
    ranklist = _query_ranklist(scorelist)
    if not ranklist then
        return
    end
    ranklist = cjson.encode(ranklist)
    rankshm:set(rankt, ranklist, shm_expire)
    --ngx.say("ranklist:", ranklist)
end
ngx.say(cjson.encode(ranklist))

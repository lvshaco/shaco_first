--
--https://github.com/agentzh/lua-resty-redis
--
ngx.header.content_type = "text/plain"

local redis = require "resty.redis"
local cjson = require "cjson"
local strfmt = string.format
local floor = math.floor
local ngxvar = ngx.var
local CONNPOOL_SIZE = 100
local CONN_ALIVE = 10000
local shm_expire = 6--todo
--local RANKT = ngxvar.rank_type
local RANKT = ngxvar['arg_t']
local typevalid = {dashi=true, xinshou=true,chuji=true,zhongji=true,gaoji=true,daren=true,prices=true}
if not typevalid[RANKT] then
    --ngx.say("invalid rank type:", RANKT)
    return
end

local RANK_REDIS_IP="192.168.1.145"
local RANK_REDIS_PORT=6391

local USER_REDIS = {
{ "192.168.1.145", 6400 },
{ "192.168.1.145", 6401 },
{ "192.168.1.145", 6402 },
{ "192.168.1.145", 6403 },
{ "192.168.1.145", 6404 },
{ "192.168.1.145", 6405 },
{ "192.168.1.145", 6406 },
{ "192.168.1.145", 6407 },
{ "192.168.1.145", 6408 },
{ "192.168.1.145", 6409 },
{ "192.168.1.145", 6410 },
{ "192.168.1.145", 6411 },
{ "192.168.1.145", 6412 },
{ "192.168.1.145", 6413 },
{ "192.168.1.145", 6414 },
{ "192.168.1.145", 6415 },
{ "192.168.1.145", 6416 },
{ "192.168.1.145", 6417 },
{ "192.168.1.145", 6418 },
{ "192.168.1.145", 6419 },
{ "192.168.1.145", 6420 },
{ "192.168.1.145", 6421 },
{ "192.168.1.145", 6422 },
{ "192.168.1.145", 6423 },
{ "192.168.1.145", 6424 },
{ "192.168.1.145", 6425 },
{ "192.168.1.145", 6426 },
{ "192.168.1.145", 6427 },
{ "192.168.1.145", 6428 },
{ "192.168.1.145", 6429 },
{ "192.168.1.145", 6430 },
{ "192.168.1.145", 6431 }, 
};

local NUSER_REDIS = #USER_REDIS

local function login(ip, port, auth)
    local red = redis:new()
    local ok, err = red:connect(ip, port)
    if not ok then
        ngx.log(ngx.ERR, strfmt("redis %s:%u connect fail %s", ip, port, err));
        return
    end
    
    local ok, err = red:auth(auth)
    if not ok then
        ngx.log(ngx.ERR, "redis %s:%u auth fail %s", ip, port, err)
        return
    end
    return red
end

local function rest(red)
    local ok, err = red:set_keepalive(CONN_ALIVE, CONNPOOL_SIZE)
    if not ok then
        ngx.log(ngx.ERR, err) 
    end
end

local function query_scorelist()
    local red = login(RANK_REDIS_IP, RANK_REDIS_PORT, ngxvar.rank_redis_auth);
    if red then
        local rs, err = red:zrevrange(strfmt("rank:%s", RANKT), "0", "100", "withscores")
        if rs then
            rest(red);
            return rs;
        end
        ngx.log(ngx.ERR, strfmt("redis scorelist %s err %s", RANKT, err))
    end
end

local function query_ranklist(sl) 
    local auth = ngxvar.user_redis_auth
    local clist = {}
    local idx=1
    for i=1, #sl, 2 do 
        local uid = sl[i]
        local addr = USER_REDIS[(uid % NUSER_REDIS)+1]
        local red = login(addr[1], addr[2], auth)
        clist[idx] = {id=uid, score=floor(sl[i+1]/10000000000), name="", role=0}
        if red then
            local user, err = red:hmget(strfmt("user:%u", uid), "name", "role")
            if user then
                clist[idx].name = user[1]
                clist[idx].role = user[2]
            end
            rest(red);
        end
        idx = idx+1 
    end
    return clist
end

local rankshm = ngx.shared.rank
local result = rankshm:get(RANKT)
if not result then
    local sl = query_scorelist()
    if sl then
        local rl = query_ranklist(sl)
        if rl then
            result = strfmt('{"ranklist":%s}', cjson.encode(rl))
            rankshm:set(RANKT, result, shm_expire)
            ngx.say(result)
        end
    end
else
    ngx.say(result)
end

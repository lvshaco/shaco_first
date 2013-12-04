require "pathconfig"

local sys = require("liblsys")
require("signal")
require("socket")

local do_shell_path = ".."
local strfmt = string.format

local function do_shell_command(client, command)
    local f = io.popen(command, "r")
    local result = f:read("*l")
    while result do
        result = sys.iconv(result, "gbk", "utf-8")
        client:send(result .. "\n")
        result = f:read("*l")
    end
    print (result)

    f:close()
end

----------------------------------------------------------------------------------
-- command list
----------------------------------------------------------------------------------
local function update_res(client, args)
    do_shell_command(client, strfmt('cd %s && 2>&1 make res', do_shell_path))
end

local function reload_res(client, args)
    do_shell_command(client, strfmt(
    'cd %s && 2>&1 ./shaco-cli --cmd "all all reloadres"', do_shell_path))
end

local command_map = {
    update = update_res,
    reload = reload_res,
}

----------------------------------------------------------------------------------
-- run_server
----------------------------------------------------------------------------------
local function run_server(host, port)
    local server, client
    local command
    local result = "\n"
    local err
    local exec
    server = assert(socket.bind(host, port))
    print("listen in#" .. host .. ":" .. port)
    while 1 do
        client = server:accept()
        if client then
            print("new client#", client:getpeername())
            command = client:receive();
            if command then
                exec = command_map[command]
                print("execute command#" .. command)
                if exec then
                    result = exec(client, command)
                    err = "ok"
                else
                    err = "no command"
                end
                print("execute command#" .. command .. "#" .. err)
                client:send(command .. " execute complete, please check the result.\n")
            end
            print("client#", client:getpeername(), "close")
            client:shutdown()
            client:close()
        end
    end
end

local arg = { ... }
if #arg < 2 then
    print("usage: commandd.lua ip port")
    return -1;
end

local ip = arg[1]
local port = arg[2]
if #arg >= 3 and arg[3] == "-d" then
    sys.daemon(1, 1)
end
sys.setenv("PYTHONIOENCODING", "utf-8")

signal.signal("CHLD", function() end)    
signal.signal("INT", "cdefault")

run_server(ip, port)

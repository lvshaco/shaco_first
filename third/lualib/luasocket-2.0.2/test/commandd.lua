
local function do_shell_command(client, command)
    local f = io.popen(command, "r")
    local result = f:read("*l")
    while result do
        client:send(result .. "\n")
        result = f:read("*l")
    end
    f:close()
end

local function update_data(client, args)
    do_shell_command(client, "cd ~/pandora-server/trunk && make res")
    return "update_data\n"
end

local function restart_server(client, args)
    do_shell_command(client, "cd ~/pandora-server/trunk && ./eagle restart super")
    return "restart_server\n"
end

local command_map = {
    update = update_data,
    restart = restart_server,
}

local function run_server(host, port)
    local server, client
    local command
    local result = "\n"
    local err = "ok"
    local exec
    server = assert(socket.bind(host, port))
    print("listen in#" .. host .. ":" .. port)
    while 1 do
        client = assert(server:accept())
        print("new client#" .. client:getpeername())
        command = client:receive();
        if command then
            exec = command_map[command]
            print("execute command#" .. command)
            if exec then
                result = exec(client, command)
            else
                err = "no command"
            end
            print("execute command#" .. command .. "#" .. err)
            assert(client:send(result))
        end
        print("client#", client:getpeername(), "close")
        client:close()
    end
end

print(os.execute("export LD_LIBRARY_PATH=/home/lvxiaojun/lua_lib/lua"))
package.path = package.path .. ";/home/lvxiaojun/lua_lib/?/?.lua"
package.cpath = package.cpath .. ";/home/lvxiaojun/lua_lib/?/?.so"
print(package.path)
print(package.cpath)

require("signal")
signal.signal("CHLD", function() end)    
signal.signal("INT", "cdefault")

require("socket")
run_server("localhost", 8383)

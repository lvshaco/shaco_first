
local function run_client(host, port, command)
    require("socket")
    print("connecting to " .. host .. ":" .. port)
    local c = assert(socket.connect(host, port))
    assert(c:send(command .. "\n"))
    local result = 1
    while result do
        result = c:receive()
        print(result)
    end
end

print(os.execute("export LD_LIBRARY_PATH=/home/lvxiaojun/lua_lib/lua"))
package.path = package.path .. ";/home/lvxiaojun/lua_lib/?/?.lua"
package.cpath = package.cpath .. ";/home/lvxiaojun/lua_lib/?/?.so"
print(package.path)
print(package.cpath)

require("signal")
signal.signal("INT", "cdefault")

local command = io.read()
run_client("localhost", 8383, command)

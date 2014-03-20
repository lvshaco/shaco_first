require "pathconfig"

local function run_client(host, port, command)
    require("socket")
    print("connecting to " .. host .. ":" .. port)
    local c = assert(socket.connect(host, port))
    assert(c:send(command .. "\n"))
    local result = c:receive()
    while result do
        print(result)
        result = c:receive()
    end
end

require("signal")
signal.signal("INT", "cdefault")

run_client("localhost", 8383, "update")

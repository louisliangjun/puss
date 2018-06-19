-- sock.lua

local puss_socket = puss.require('puss_socket')

local function utable_init(sock)
	local utable = sock:utable()
	utable.squeue = {}
	utable.rqueue = {}
	utable.soffset = nil
	utable.recv_cur_sz = nil
	utable.recv_cur = nil
end

__exports.listen = function(ip, port)
	local listen_sock = puss_socket.socket_new()
	listen_sock:create()
	listen_sock:set_nonblock(true)
	listen_sock:bind(ip, port)
	listen_sock:listen()
	return listen_sock
end

__exports.accept = function(listen_sock)
	local sock, addr = listen_sock:accept()
	if not sock then return end
	utable_init(sock)
	sock:set_nonblock(true)
	return sock, addr
end

__exports.connect = function(ip, port)
	local sock = puss_socket.socket_new()
	sock:create()
	utable_init(sock)
	sock:set_nonblock(true)
	sock:connect(ip, port)
	return sock
end

__exports.send = function(sock, ...)
	if not (sock and sock:valid()) then return end
	local utable = sock:utable()
	local squeue = utable.squeue
	local pkt = puss.pickle_pack(...)
	local hdr = string.pack('<I4', #pkt)
	-- print(string.format('send: %u %q', #pkt, pkt))
	table.insert(squeue, hdr)
	table.insert(squeue, pkt)
end

local function do_send(sock, utable)
	local squeue = utable.squeue
	local pkt = squeue[1]
	if not pkt then return false end
	local soffset = utable.soffset or 0
	-- print(string.format('sock send: %u %u %q', soffset, #pkt, pkt))
	local res, err = sock:send(pkt, soffset)
	if res > 0 then
		soffset = soffset + res
		if soffset >= #pkt then
			utable.soffset = 0
			table.remove(squeue, 1)
			return true
		end
		utable.soffset = soffset
	end
	return false
end

local function do_recv(sock, utable)
	local recv_cur_sz = utable.recv_cur_sz
	if not recv_cur_sz then
		local pkt, err = sock:recv(4, true)
		if not pkt then return false end
		recv_cur_sz = string.unpack('<I4', pkt)
		utable.recv_cur_sz = recv_cur_sz
	end

	local recv_cur = utable.recv_cur or ''
	local need_sz = recv_cur_sz - #recv_cur
	if need_sz < 0 then
		sock:close()
		return false
	end
	if need_sz == 0 then
		utable.recv_cur_sz = nil
		utable.recv_cur = nil
		return true
	end
	local pkt, err = sock:recv(need_sz)
	if not pkt then return false end
	-- print(string.format('sock recv: %u %u %q', need_sz, #pkt, pkt))
	recv_cur = (recv_cur=='') and pkt or (recv_cur .. pkt)
	if #recv_cur < recv_cur_sz then
		utable.recv_cur = recv_cur
		return false
	end
	utable.recv_cur_sz = nil
	utable.recv_cur = nil
	table.insert(utable.rqueue, recv_cur)
	return true
end

__exports.update = function(sock, dispatch)
	if not (sock and sock:valid()) then return end
	local utable = sock:utable()
	while sock:valid() do
		if not do_send(sock, utable) then break end
	end
	while sock:valid() do
		if not do_recv(sock, utable) then break end
	end

	local rqueue = utable.rqueue
	local pkt = table.remove(rqueue, 1)
	while pkt do
		-- print(string.format('recv: %u %q', #pkt, pkt))
		dispatch(puss.pickle_unpack(pkt))
		pkt = table.remove(rqueue, 1)
	end
end


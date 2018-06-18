-- sock.lua

local puss_socket = puss.require('puss_socket')

__exports.send_message = function(sock, ...)
	local squeue = sock:get_data('squeue')
	if not squeue then
		squeue = {}
		sock:set_data('squeue', squeue)
	end
	local pkt = puss.pickle_pack(...)
	local hdr = string.pack('<I4', #pkt)
	table.insert(squeue, hdr)
	table.insert(squeue, pkt)
end

__exports.recv_message = function(sock, ...)
	local rqueue = sock:get_data('rqueue')
	if not rqueue then return end
	local pkt = table.remove(rqueue, 1)
	return pkt
end

local function do_send(sock)
	if not sock:valid() then return end
	local squeue = sock:get_data('squeue')
	if not squeue then return end
	local pkt = squeue[1]
	if not pkt then return end
	local soffset = sock:get_data('soffset') or 0
	local res, err = sock:send(pkt, soffset)
	if res > 0 then
		soffset = soffset + res
		if soffset >= #pkt then
			soffset = 0
			table.remove(squeue, 1)
		end
		sock:set_data('soffset', soffset)
	end
end

local function do_recv(sock)
	if not sock:valid() then return end
	local recv_cur_sz = sock:get_data('recv_cur_sz')
	if recv_cur_sz then return recv_cur_sz end
	local res, msg = sock:peek(4)
	if res < 0 then
		if msg==11 then return end	-- linux
		if msg==10035 then return end	-- WSAEWOULDBLOCK(10035)	-- TODO : now win32 test
		-- WSAECONNRESET(10054)
		sock:close()
		return
	end
	if res==0 then
		sock:close()
		return
	end
	if res < 4 then
		return
	end
	recv_cur_sz = string.unpack('<I4', msg)
	sock:get_data('recv_cur_sz', recv_cur_sz)

	local recv_cur = sock:get_data('recv_cur') or ''
	local need_sz = recv_cur_sz - #recv_cur
	if need_sz < 0 then
		sock:close()
		return
	end
	if need_sz > 0 then
		local res, msg = sock:recv(need_sz)
		if res < 0 then
			if msg==11 then return end	-- linux
			if msg==10035 then return end	-- WSAEWOULDBLOCK(10035)	-- TODO : now win32 test
			-- WSAECONNRESET(10054)
			print('recv error:', res, msg)
			sock:close()
			return
		end
		if res==0 then
			sock:close()
			return
		end
		recv_cur = recv_cur .. msg
		if #recv_cur < recv_cur_sz then
			sock:set_data('recv_cur', recv_cur)
			return
		end
	end
	local rqueue = sock:get_data('rqueue')
	if not rqueue then
		rqueue = {}
		sock:set_data('rqueue', rqueue)
	end
	sock:set_data('recv_cur_sz', nil)
	sock:set_data('recv_cur', nil)
	table.insert(rqueue, recv_cur)
end

__exports.update = function(sock)
	do_send(sock)
	do_recv(sock)
end


local strfmt = string.format

local fs = puss.import('core.diskfs')
local parse = puss.load_plugin('puss_luaparser').parse

local function trace_demo(fname)
	local callbacks = {}
	local depth = 0
	local spaces = '                                                                                                               '
	local _print = print
	local print = function(...) _print(spaces:sub(1,depth), ...) end

	local function on_trace(chunk, ast_type, ts, te, ...)
		-- print(ast_type, chunk:token(ts))
		local cb = callbacks[ast_type]
		if not cb then error('unknown ast_type: ' .. tostring(ast_type)) end
		local token, value, spos, epos, sline, eline, slpos, elpos = chunk:token(ts)
		_print(strfmt('%s<%s line="%d-%d" col="%d-%d" token="%s"> %s </%s>', spaces:sub(1,depth), ast_type, sline, eline, spos-slpos, epos-elpos, token, value, ast_type))
		cb(chunk, ast_type, ts, te, ...)
	end

	local function trace(name, iter_or_node)
		if not iter_or_node then return end
		local old = depth
		_print(strfmt('%s<%s>', spaces:sub(1,old+1), name))
		depth = depth + 2
		iter_or_node(on_trace)
		depth = old
		_print(strfmt('%s</%s>', spaces:sub(1,old+1), name))
	end

	callbacks.error = function(chunk, ast_type, ts, te, msg)
		print('<msg>', msg, '</msg>')
	end
	callbacks.emptystat = function(chunk, ast_type, ts, te)
	end
	callbacks.caluse = function(chunk, ast_type, ts, te, cond, body)
		trace('cond', cond)
		trace('body', body)
	end
	callbacks.ifstat = function(chunk, ast_type, ts, te, caluses)
		trace('caluses', caluses)
	end
	callbacks.whilestat = function(chunk, ast_type, ts, te, cond, body)
		trace('cond', cond)
		trace('body', body)
	end
	callbacks.dostat = function(chunk, ast_type, ts, te, body)
		trace('body', body)
	end
	callbacks.fornum = function(chunk, ast_type, ts, te, var, from, to, step, body)
		trace('var', var)
		trace('from', from)
		trace('to', to)
		trace('step', step)
		trace('body', body)
	end
	callbacks.forlist = function(chunk, ast_type, ts, te, vars, iters, body)
		trace('vars', vars)
		trace('iters', iters)
		trace('body', body)
	end
	callbacks.repeatstat = function(chunk, ast_type, ts, te, body, cond)
		trace('body', body)
		trace('cond', cond)
	end
	callbacks.localfunc = function(chunk, ast_type, ts, te, name, ismethod, params, vtypes, body)
		trace('name', name)
		trace('params', params)
		trace('vtypes', vtypes)
		trace('body', body)
	end
	callbacks.localstat = function(chunk, ast_type, ts, te, vars, values)
		trace('vars', vars)
		trace('values', values)
	end
	callbacks.labelstat = function(chunk, ast_type, ts, te)
	end
	callbacks.retstat = function(chunk, ast_type, ts, te, values)
		trace('values', values)
	end
	callbacks.breakstat = function(chunk, ast_type, ts, te)
	end
	callbacks.gotostat = function(chunk, ast_type, ts, te)
	end
	callbacks.exprstat = function(chunk, ast_type, ts, te, expr)
		trace('expr', expr)
	end
	callbacks.assign = function(chunk, ast_type, ts, te, vars, values)
		trace('vars', vars)
		trace('values', values)
	end
	callbacks.constructor = function(chunk, ast_type, ts, te, fields)
		trace('fields', fields)
	end
	callbacks.vnil = function(chunk, ast_type, ts, te, val)
	end
	callbacks.vbool = function(chunk, ast_type, ts, te, val)
	end
	callbacks.vint = function(chunk, ast_type, ts, te, val)
	end
	callbacks.vflt = function(chunk, ast_type, ts, te, val)
	end
	callbacks.vstr = function(chunk, ast_type, ts, te, val)
	end
	callbacks.vname = function(chunk, ast_type, ts, te, parent)
		trace('parent', parent)
	end
	callbacks.vararg = function(chunk, ast_type, ts, te)
	end
	callbacks.unary = function(chunk, ast_type, ts, te, expr)
		trace('expr', expr)
	end
	callbacks.bin = function(chunk, ast_type, ts, te, op, l, r)
		print('<op>', op, '</op>')
		trace(l, 'left', l)
		trace(r, 'right', r)
	end
	callbacks.var = function(chunk, ast_type, ts, te)
	end
	callbacks.vtype = function(chunk, ast_type, ts, te)
	end
	callbacks.call = function(chunk, ast_type, ts, te, ismethod, name, params)
		if ismethod then print('<ismethod>', ismethod, '</ismethod>') end
		trace('name', name)
		trace('params', params)
	end
	callbacks.func = function(chunk, ast_type, ts, te, name, ismethod, params, vtypes, body)
		print('<ismethod>', ismethod, '</ismethod>')
		trace('name', name)
		trace('params', params)
		trace('vtypes', vtypes)
		trace('body', body)
	end
	callbacks.field = function(chunk, ast_type, ts, te, k, v)
		trace('key', k)
		trace('val', v)
	end

	local script = fs.load(fname)
	local chunk = parse('app.lua', script)
	-- chunk:trace()
	chunk:iter(on_trace)
end

function __main__()
	puss.trace_pcall(trace_demo, puss._path..'/core/app.lua')
	puss.sleep(5000000)
end
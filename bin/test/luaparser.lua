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

	local function trace(node, name)
		if not node then return end
		local old = depth
		_print(strfmt('%s<%s>', spaces:sub(1,old+1), name))
		depth = depth + 2
		on_trace(node())
		depth = old
		_print(strfmt('%s</%s>', spaces:sub(1,old+1), name))
	end

	local function iter_trace(name, iter, count)
		local old = depth
		_print(strfmt('%s<%s count=%s>', spaces:sub(1,old+1), name, count))
		depth = depth + 2
		iter(on_trace)
		depth = old
		_print(strfmt('%s</%s>', spaces:sub(1,old+1), name))
	end

	callbacks.error = function(chunk, ast_type, ts, te, msg)
		print(msg)
	end
	callbacks.emptystat = function(chunk, ast_type, ts, te)
	end
	callbacks.caluse = function(chunk, ast_type, ts, te, cond, body, nstats)
		trace(cond, 'cond')
		iter_trace('body', body, nstats)
	end
	callbacks.ifstat = function(chunk, ast_type, ts, te, caluses, ncaluses)
		iter_trace('caluses', caluses, ncaluses)
	end
	callbacks.whilestat = function(chunk, ast_type, ts, te, cond, body, nstats)
		trace(cond, 'cond')
		iter_trace('body', body, nstats)
	end
	callbacks.dostat = function(chunk, ast_type, ts, te, body, nstats)
		iter_trace('body', body, nstats)
	end
	callbacks.fornum = function(chunk, ast_type, ts, te, var, from, to, step, body, nstats)
		trace(var, 'var')
		trace(from, 'from')
		trace(to, 'to')
		trace(step, 'step')
		iter_trace('body', body, nstats)
	end
	callbacks.forlist = function(chunk, ast_type, ts, te, vars, nvars, iters, niters, body, nstats)
		iter_trace('vars', vars, nvars)
		iter_trace('iters', iters, niters)
		iter_trace('body', body, nstats)
	end
	callbacks.repeatstat = function(chunk, ast_type, ts, te, body, nstats, cond)
		iter_trace('body', body, nstats)
		trace(cond, 'cond')
	end
	callbacks.localfunc = function(chunk, ast_type, ts, te, name, ismethod, params, nparams, vtypes, nvtypes, body, nstats)
		trace(name, 'name')
		iter_trace('params', params, nparams)
		iter_trace('vtypes', vtypes, nvtypes)
		iter_trace('body', body, nstats)
	end
	callbacks.localstat = function(chunk, ast_type, ts, te, vars, nvars, values, nvalues)
		iter_trace('vars', vars, nvars)
		iter_trace('values', values, nvalues)
	end
	callbacks.labelstat = function(chunk, ast_type, ts, te)
	end
	callbacks.retstat = function(chunk, ast_type, ts, te, values, nvalues)
		iter_trace('values', values, nvalues)
	end
	callbacks.breakstat = function(chunk, ast_type, ts, te)
	end
	callbacks.gotostat = function(chunk, ast_type, ts, te)
	end
	callbacks.exprstat = function(chunk, ast_type, ts, te, expr)
		trace(expr, 'expr')
	end
	callbacks.assign = function(chunk, ast_type, ts, te, vars, nvars, values, nvalues)
		iter_trace('vars', vars, nvars)
		iter_trace('values', values, nvalues)
	end
	callbacks.constructor = function(chunk, ast_type, ts, te, fields, nfields)
		iter_trace('fields', fields, nfields)
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
		trace(parent, 'parent')
	end
	callbacks.vararg = function(chunk, ast_type, ts, te)
	end
	callbacks.unary = function(chunk, ast_type, ts, te, expr)
		trace(expr, 'expr')
	end
	callbacks.bin = function(chunk, ast_type, ts, te, l, r)
		trace(l, 'left')
		trace(r, 'right')
	end
	callbacks.var = function(chunk, ast_type, ts, te)
	end
	callbacks.vtype = function(chunk, ast_type, ts, te)
	end
	callbacks.call = function(chunk, ast_type, ts, te, name, params, nparams)
		trace(name, 'name')
		iter_trace('params', params, nparams)
	end
	callbacks.func = function(chunk, ast_type, ts, te, name, ismethod, params, nparams, vtypes, nvtypes, body, nstats)
		trace(name, 'name')
		iter_trace('params', params, nparams)
		iter_trace('vtypes', vtypes, nvtypes)
		iter_trace('body', body, nstats)
	end
	callbacks.field = function(chunk, ast_type, ts, te, k, v)
		trace(k, 'key')
		trace(v, 'val')
	end

	local script = fs.load(fname)
	local chunk = parse('app.lua', script)
	-- chunk:trace()
	iter_trace('chunk', chunk:iter())
end

function __main__()
	puss.trace_pcall(trace_demo, puss._path..'/core/app.lua')
	puss.sleep(5000000)
end
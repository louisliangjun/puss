local fs = puss.import('core.diskfs')
local parse = puss.load_plugin('puss_luaparser').parse

local callbacks =
	{ error = function(chunk, ast_type, ts, te, msg)
		print(ast_type, ts, te, msg)
	  end
	, emptystat = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, caluse = function(chunk, ast_type, ts, te, cond, body, nstats)
		print(ast_type, ts, te, cond, body)
	  end
	, ifstat = function(chunk, ast_type, ts, te, caluses, ncaluses)
		print(ast_type, ts, te, ncaluses)
	  end
	, whilestat = function(chunk, ast_type, ts, te, cond, body, nstats)
		print(ast_type, ts, te, nstats)
	  end
	, dostat = function(chunk, ast_type, ts, te, body, nstats)
		print(ast_type, ts, te, nstats)
	  end
	, fornum = function(chunk, ast_type, ts, te, var, from, to, step, body, nstats)
		print(ast_type, ts, te, nstats)
	  end
	, forlist = function(chunk, ast_type, ts, te, vars, nvars, iters, niters, body, nstats)
		print(ast_type, ts, te, nvars, niters, nstats)
	  end
	, repeatstat = function(chunk, ast_type, ts, te, body, nstats, cond)
		print(ast_type, ts, te, nstats)
	  end
	, localfunc = function(chunk, ast_type, ts, te, name, ismethod, params, nparams, vtypes, nvtypes, body, nstats)
		print(ast_type, ts, te, ismethod, nparams, nvtypes, nstats)
	  end
	, localstat = function(chunk, ast_type, ts, te, vars, nvars, values, nvalues)
		print(ast_type, ts, te, nvars, nvalues)
	  end
	, labelstat = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, retstat = function(chunk, ast_type, ts, te, values, nvalues)
		print(ast_type, ts, te, nvalues)
	  end
	, breakstat = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, gotostat = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, exprstat = function(chunk, ast_type, ts, te, expr)
		print(ast_type, ts, te)
	  end
	, assign = function(chunk, ast_type, ts, te, vars, nvars, values, nvalues)
		print(ast_type, ts, te, nvars, nvalues)
	  end
	, constructor = function(chunk, ast_type, ts, te, fields, nfields)
		print(ast_type, ts, te)
	  end
	, vnil = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, vbool = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, vint = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, vflt = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, vstr = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, vname = function(chunk, ast_type, ts, te, parent)
		print(ast_type, ts, te)
	  end
	, vararg = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, unary = function(chunk, ast_type, ts, te, expr)
		print(ast_type, ts, te)
	  end
	, bin = function(chunk, ast_type, ts, te, l, r)
		print(ast_type, ts, te)
	  end
	, var = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, vtype = function(chunk, ast_type, ts, te)
		print(ast_type, ts, te)
	  end
	, call = function(chunk, ast_type, ts, te, name, params, nparams)
		print(ast_type, ts, te, nparams)
	  end
	, func = function(chunk, ast_type, ts, te, name, ismethod, params, nparams, vtypes, nvtypes, body, nstats)
		print(ast_type, ts, te, ismethod, nparams, nvtypes, nstats)
	  end
	, field = function(chunk, ast_type, ts, te, k, v)
		print(ast_type, ts, te)
	  end
	}

local function on_iter(chunk, ast_type, ts, te, ...)
	-- print(ast_type, chunk:token(ts))
	local cb = callbacks[ast_type]
	if not cb then error('unknown ast_type: ' .. tostring(ast_type)) end
	cb(chunk, ast_type, ts, te, ...)
end

local function test()
	local script = fs.load(puss._path..'/core/app.lua')
	local chunk = parse('app.lua', script)
	-- chunk:trace()
	local iter, n = chunk:iter()
	--print(iter, n)
	iter(on_iter)
end

function __main__()
	print(pcall(test))
	puss.sleep(5000000)
end
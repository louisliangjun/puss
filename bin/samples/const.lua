puss.dofile(puss._path .. '/samples/utils.lua')

function __main__()
	local consts = puss._consts
	consts.TEST_CONST = 111
	table.trace(consts, 'puss._consts')

	print('NOTICE: current file already compiled, so no enums replace, TEST_CONST:', tostring(TEST_CONST))
	print('NOTICE: if current file want use enums, use puss._consts.TEST_CONST:', consts.TEST_CONST)
	load("print('NOTICE: new compiled file/string support, TEST_CONST:', TEST_CONST)", 'string')()
	puss.dofile(puss._path .. '/samples/const.lua')
end

print('* test_use_consts TEST_CONST:', TEST_CONST)


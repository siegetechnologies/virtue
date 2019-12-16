#!/usr/bin/python3



import funcs_vm
import funcs_bake



#better way to do this...


funcs = {
	"start": funcs_vm.vm_start_func,
	"list": funcs_vm.vm_list_func,
	"get": funcs_vm.vm_get_func,
	"stop": funcs_vm.vm_stop_func,
	"bake": {
		"start": funcs_bake.bake_start_func,
		"list": funcs_bake.bake_list_func,
		"get": funcs_bake.bake_get_func,
		"stop": funcs_bake.bake_stop_func,
	}
}

def parse(name, args):
	a = name
	f = funcs
	while isinstance(f, dict) and a and a[0] in f:
		f = f[a[0]]
		a = a[1:]

	if callable(f):
		return f(args)
	else:
		print("error " + str(name))

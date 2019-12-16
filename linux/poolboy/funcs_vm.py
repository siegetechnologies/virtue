import pprint

import vmpool

from traceback import print_exc

from threading import Thread

def vm_start_func(args):

	virtueID = args.get('virtueID')
	ramsize = args.get('ramsize')
	cpucount = args.get('cpucount')
	imageurl = args.get('imageurl')
	toport = args.get('toport')
	nettest = args.get('nettest')

	info = args.get('info')
	vconf = args.get('vconf')


	missing = []

	if not virtueID: missing.append("virtueID")
	if not imageurl: missing.append("imageurl")
	if not vconf: missing.append("vconf")
	if not info: missing.append("info")
	if not toport: missing.append("toport")
	if not nettest: missing.append("nettest")

	if missing:
		return {"success":False, "missing":missing}

	try:
		vm = vmpool.createandadd(virtueID, imageurl, info, vconf, toport, nettest, ramsize=ramsize, cpucount=cpucount)
		t = Thread(target = vm.start)
		t.start()
	except:
		print_exc()
		return {"success":False}
#	multiprocessing.active_children()

	return {"success":True}


def vm_list_func(args):
	selector = args.get('selector')
	return vmpool.list(selector)

def vm_get_func(args):
	try:
		return vm_list_func(args)[0]
	except:
		return None


def vm_stop_func(args):
	virtueID = args.get('virtueID')
	if not virtueID:
		return {"success":False, "missing":"virtueID"}
	vmpool.stopid(virtueID)
	return {"success":True}

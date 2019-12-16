import pprint

import bakepool

from traceback import print_exc

from threading import Thread

'''
bake_start_func
params:
bakeID: string      | a unique id to use for the baked image
ramsize: string     | Just make it "4G", uses qemu, not kube conventions
cpucount: string    | qemu will be run with -smp <cpucount>. This is a string, not an int
srcurl: stirng      | url of the base image
dsturl: string      | destination image
vconf: string (opt) | if given as a string, the config drive will contain a single file, "user-data" with that string as the contents
vconf: dict (opt)   | if given as a dict, the config drive, will contain a file for each dict entry, where the filename is the key, and the file contents are the value
'''
def bake_start_func(args):

	bakeID = args.get('bakeID')
	ramsize = args.get('ramsize')
	cpucount = args.get('cpucount')
	srcurl = args.get('srcurl')
	dsturl = args.get('dsturl')
	vconf = args.get('vconf', "VIRTUEID=BAKE")
	print( "Vconf is {}".format( vconf ) )

	info = args.get('info')


	missing = []

	if not bakeID: missing.append("bakeID")
	if not srcurl: missing.append("srcurl")
	if not dsturl: missing.append("dsturl")
	if not info: missing.append("info")

	if missing:
		return {"success":False, "missing":missing}

	try:
		bak = bakepool.createandadd(bakeID, srcurl, dsturl, info, ramsize=ramsize, cpucount=cpucount,vconf=vconf)
		t = Thread(target = bak.start)
		t.start()
	except:
		print_exc()
		return {"success":False}
#	multiprocessing.active_children()

	return {"success":True}


def bake_list_func(args):
	selector = args.get('selector')
	return bakepool.list(selector)

def bake_get_func(args):
	try:
		return bake_list_func(args)[0]
	except:
		return None


def bake_stop_func(args):
	bakeID = args.get('bakeID')
	if not bakeID:
		return {"success":False, "missing":"bakeID"}
	bakepool.stopid(bakeID)
	return {"success":True}

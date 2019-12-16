#!/usr/bin/python3
import urllib.request, urllib.parse
import json
import pprint

import strutil

import hashlib
import base64

#todo automate?

node = '10.3.76.115'
url = 'https://%s:5443' % node

import ssl
gcontext = ssl.SSLContext(ssl.PROTOCOL_TLSv1) #todo better

def api(path, args):
	data = json.dumps(args).encode()
	res = urllib.request.urlopen(url + path, data, context=gcontext, timeout=10)
	return res.read().decode('utf-8')


def poolbake(bakeID, srcurl, dsturl, info, vconf, ramsize='', cpucount=''):
	if cpucount and not isinstance(cpucount, int):
		cpucount = int(cpucount)
	if not cpucount or not isinstance(cpucount, int):
		print("cpucount not set?. Must be 1>=cpucount>=32. Setting to 1 (min)")
		cpucount = 1
	if cpucount < 1:
		print("cpucount " + str(cpucount) + " is too low. Must be 1>=cpucount>=32. Setting to 1 (min)")
		cpucount = 1
	if cpucount > 32:
		print("cpucount " + str(cpucount) + " is too low. Must be 1>=cpucount>=32. Setting to 32 (max)")
		cpucount = 32
	if not ramsize:
		print("warning.... ramsize not set. Defaulting to 1G")
		ramsize = "1G"
	#tear off the kubernetes 'i' at the end
	ramsize = ramsize.strip()
	if ramsize[-1] == 'i':
		ramsize = ramsize[:-1]
	req = {}
	#edit the stuffs
	req['bakeID'] = bakeID
	req["ramsize"] = ramsize
	req["cpucount"] = str(cpucount)	#todo make nicer
#	req["srcurl"] = "https://s3.amazonaws.com/siege-virtuevms/" + repoID + ".qcow2" #todo not do this
#	req["dsturl"] = "https://s3.amazonaws.com/siege-virtuevms/" + repoID + ".qcow2_baked" #todo not do this

	req["srcurl"] = srcurl
	req["dsturl"] = dsturl
	req['info'] = info
	req['vconf'] = vconf

	return json.loads(api('/bake/start', req))

def pooldeploy(virtueID, imageurl, info, vconf, toport, nettest, ramsize='', cpucount=''):

#	print("Creating deploy for user " + usernm)

	# todo move this logic to poolboy?
	if cpucount and not isinstance(cpucount, int):
		cpucount = int(cpucount)
	if not cpucount or not isinstance(cpucount, int):
		print("cpucount not set?. Must be 1>=cpucount>=32. Setting to 1 (min)")
		cpucount = 1
	if cpucount < 1:
		print("cpucount " + str(cpucount) + " is too low. Must be 1>=cpucount>=32. Setting to 1 (min)")
		cpucount = 1
	if cpucount > 32:
		print("cpucount " + str(cpucount) + " is too low. Must be 1>=cpucount>=32. Setting to 32 (max)")
		cpucount = 32

	if not ramsize:
		print("warning.... ramsize not set. Defaulting to 1G")
		ramsize = "1G"

	if not toport:
		return {"Error":"toport not set"}


	#tear off the kubernetes 'i' at the end
	ramsize = ramsize.strip()
	if ramsize[-1] == 'i':
		ramsize = ramsize[:-1]

	req = {}
	#edit the stuffs
	req['virtueID'] = virtueID
	req["ramsize"] = ramsize
	req["cpucount"] = str(cpucount)	#todo make nicer
#	req["imageurl"] = "https://s3.amazonaws.com/siege-virtuevms/" + repoID + ".qcow2_baked" #todo not do this
	req["imageurl"] = imageurl
	req['info'] = info
	req['vconf'] = vconf
	req['toport'] = toport
	req['nettest'] = nettest

	return json.loads(api('/start', req))

def poollist(usernm='', rolenm='', virtuenm=''):
	req = {}
	if virtuenm: req['virtueID'] = virtuenm
	if usernm: req["userID"] = usernm
	if rolenm: req["roleID"] = rolenm

	return json.loads(api('/list', {"selector": req}))

def bakelist(bakeid=''):
	req = {}
	if bakeid:
		req['bakeID'] = bakeid
	return json.loads(api('/bake/list', {"selector": req}))


def bakedestroy(bakeid):
	return json.loads(api('/bake/stop', {"bakeID": bakeid}))


def pooldestroy(virtueID):
	return json.loads(api('/stop', {"virtueID": virtueID}))



def basic_tests():
	print("starting poolb basic tests")
	pl = poollist()
	assert( isinstance(pl, list) )
	print("completed poolb basic tests")


basic_tests()

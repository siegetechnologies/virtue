#!/usr/bin/python3
import urllib.request, urllib.parse
import pprint
import json
import sys
import time

from pathlib import Path
#for ignoring invalid certs
#https://stackoverflow.com/questions/27835619/urllib-and-ssl-certificate-verify-failed-error
import ssl
gcontext = ssl.SSLContext(ssl.PROTOCOL_TLSv1)  # Only for gangstars



bakeID = "baketest"
imageurl = "http://localhost:8000/win2016-winprov.qcow2"
url = 'http://localhost:5443'




def api(path, args):
	data = json.dumps(args).encode()
	res = urllib.request.urlopen(url + path, data, context=gcontext)
	return res.read().decode('utf-8')

#https://stackoverflow.com/questions/287871/print-in-terminal-with-colors
class bc:
	HEADER = '\033[95m'
	OKBLUE = '\033[94m'
	OKGREEN = '\033[92m'
	WARNING = '\033[93m'
	FAIL = '\033[91m'
	ENDC = '\033[0m'
	BOLD = '\033[1m'
	UNDERLINE = '\033[4m'

def pc(stran, color):
	print(color + stran + bc.ENDC)


with open(str(Path.home()) + '/.ssh/id_rsa.pub') as f:
	sshkey = f.read()


vconf = open('bake.ps1').read()


pc('start bakeID', bc.HEADER)
with open(str(Path.home()) + '/.ssh/id_rsa.pub') as f:
	start = json.loads(api('/bake/start', {"bakeID":bakeID, "srcurl":imageurl, "dsturl":imageurl + "_baked", 'info':{'bakeID':bakeID, 'no':True}, 'vconf':{"/test.ps1":vconf}, "cpucount":"4","ramsize":"4G"}))
pprint.pprint(start)

pc('listing virtue', bc.HEADER)
list = json.loads(api('/bake/list', {'selector':{}}))
pprint.pprint(list)

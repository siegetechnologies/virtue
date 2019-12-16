#!/usr/bin/python3
import urllib.request, urllib.parse
import pprint
import json
import sys
from time import sleep

from pathlib import Path
#for ignoring invalid certs
#https://stackoverflow.com/questions/27835619/urllib-and-ssl-certificate-verify-failed-error
import ssl
gcontext = ssl.SSLContext(ssl.PROTOCOL_TLSv1)  # Only for gangstars


try:
	bakeid = sys.argv[1]
except:
	print( "Usage: {} bakeID <url>".format(sys.argv[0]))
	sys.exit(0)

try:
	url = sys.argv[2]
except:
	url = 'https://localhost:5443'



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



pc('stop bakeid', bc.HEADER)
stop = json.loads(api('/bake/stop', {"bakeID":bakeid}))
pprint.pprint(stop)

sleep(1)

pc('listing bake', bc.HEADER)
list = json.loads(api('/bake/list', {}))
pprint.pprint(list)


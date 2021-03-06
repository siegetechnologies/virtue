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


try:
	virtueid = sys.argv[1]
	imageurl = sys.argv[2]
except:
	print( "Usage: {} virtueID imageurl <url>".format(sys.argv[0]))
	sys.exit(0)

try:
	url = sys.argv[3]
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


with open(str(Path.home()) + '/.ssh/id_rsa.pub') as f:
	sshkey = f.read()

vconf = ''
vconf += 'VIRTUEID=%s\n' % virtueid
vconf += 'VIRTUE_MAVENCONF=%s\n' % ""
vconf += 'VIRTUE_AUTH_SSH=%s\n' % sshkey
vconf += 'YEETMAN=Easteregg\n'



pc('start virtueid', bc.HEADER)
with open(str(Path.home()) + '/.ssh/id_rsa.pub') as f:
	start = json.loads(api('/start', {"virtueID":virtueid, "imageurl":imageurl, 'toport':22, 'nettest':'ssh', 'info':{'virteuid':virtueid, 'no':True}, 'vconf':vconf}))
pprint.pprint(start)

pc('listing virtue', bc.HEADER)
list = json.loads(api('/list', {'selector':{}}))
pprint.pprint(list)

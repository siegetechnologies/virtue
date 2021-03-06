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
	username = sys.argv[1]
except:
	print( "Usage: {} <username> [url]".format(sys.argv[0]))
	sys.exit(0)


try:
	url = sys.argv[2]
except:
	url = 'https://localhost:4443'



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



pc('login as admin', bc.HEADER)
login = json.loads(api('/user/login', {"username":"admin", "password":"changeMe!"}))
pprint.pprint(login)
admintok = login['userToken']

pc('create an awg', bc.HEADER)
cret = json.loads(api('/user/create', {"userToken":admintok, "username":username, "password":"virtuerestdefaultpw"}))
pprint.pprint(cret)

pc('login as awg', bc.HEADER)
login = json.loads(api('/user/login', {"username":username, "password":"virtuerestdefaultpw"}))
pprint.pprint(login)
awgtok = login['userToken']


#pc('awg adds an ssh key', bc.HEADER)
#with open(str(Path.home()) + '/.ssh/id_rsa.pub') as f:
#	keyboy = json.loads(api('/user/key', {"userToken":awgtok, "sshkey":f.read()}))
#	pprint.pprint(keyboy)



pc('create an app for xterm', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"xterm", "type":"linux", "launchCmd":"xterm -e bash", "install":"xterm"}))
pprint.pprint(goodrole)

pc('create a role for xterm', bc.HEADER)
goodrole = json.loads(api('/role/create', {"userToken":admintok, "roleID":"xterm", "applicationIDs":["xterm"]}))
pprint.pprint(goodrole)

pc('ok, admin will add rolo', bc.HEADER)
badv = json.loads(api('/user/role/authorize', {"userToken":admintok, "username":username, "roleID":"xterm"}))
pprint.pprint(badv)



pc('lets wait until the role is ready', bc.HEADER)


while True:
	badv = json.loads(api('/role/get', {"userToken":admintok, "roleID":"xterm"}))
	pprint.pprint(badv)
	if 'status' in badv and badv['status'] != 'creating' and badv['status'] != 'uploading' and badv['status'] != 'bundling':
		break
	time.sleep(1)

pc('launch it', bc.HEADER)


with open(str(Path.home()) + '/.ssh/id_rsa.pub') as f:
	badv = json.loads(api('/virtue/create', {"userToken":admintok, "userID":username, "roleID":"xterm", "sshkey":f.read()}))
	pprint.pprint(badv)


'''
'''

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
	print( "Usage: demoforuser.py <username>" )
	sys.exit(0)

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

pc('create a user', bc.HEADER)
cret = json.loads(api('/user/create', {"userToken":admintok, "username":username, "password":"virtuerestdefaultpw"}))
pprint.pprint(cret)

pc('create an app for firefox', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"firefox", "type":"windows", "icon":"https://ffp4g1ylyit3jdyti1hqcvtb-wpengine.netdna-ssl.com/firefox/files/2017/12/firefox-logo-300x310.png", "launchCmd":"C:\\Program Files\\Mozilla Firefox\\firefox.exe","install":"firefox"}))
pprint.pprint(goodrole)

pc('create a role for windows', bc.HEADER)
goodrole = json.loads(api('/role/create', {"userToken":admintok, "roleID":"winff", "applicationIDs":["firefox"]}))
pprint.pprint(goodrole)

pc('ok, admin will add rolo', bc.HEADER)
badv = json.loads(api('/user/role/authorize', {"userToken":admintok, "username":username, "roleID":"winff"}))
pprint.pprint(badv)

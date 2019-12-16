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

pc('create an app for powerpoint', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"powerpoint", "type":"windows", "icon":"http://png-5.findicons.com/files/icons/2795/office_2013_hd/2000/powerpoint.png", "launchCmd":"C:\\Program Files\\Microsoft Office\\root\\office16\\POWERPNT.exe","install":"office365proplus"}))
pprint.pprint(goodrole)

pc('create an app for word', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"word", "type":"windows", "icon":"http://icons.iconarchive.com/icons/dakirby309/simply-styled/256/Microsoft-Word-2013-icon.png", "launchCmd":"C:\\Program Files\\Microsoft Office\\root\\office16\\WINWORD.exe"}))
pprint.pprint(goodrole)

pc('create a role for windows', bc.HEADER)
goodrole = json.loads(api('/role/create', {"userToken":admintok, "roleID":"winoffice", "applicationIDs":["word", "powerpoint"]}))
pprint.pprint(goodrole)

pc('ok, admin will add rolo', bc.HEADER)
badv = json.loads(api('/user/role/authorize', {"userToken":admintok, "username":username, "roleID":"winoffice"}))
pprint.pprint(badv)

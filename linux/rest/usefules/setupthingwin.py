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

pc('create a awgaines', bc.HEADER)
cret = json.loads(api('/user/create', {"userToken":admintok, "username":username, "password":"virtuerestdefaultpw"}))
pprint.pprint(cret)

pc('create an app for calc', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"calc", "type":"windows", "icon":"https://ih1.redbubble.net/image.572650548.7607/flat,1000x1000,075,f.u1.jpg", "launchCmd":"C:\\Windows\\System32\\calc.exe"}))
pprint.pprint(goodrole)
pc('create an app for powershell', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"powershell", "type":"windows", "icon":"http://blog.appliedis.com/wp-content/uploads/2013/02/logo-powershell.png","launchCmd":"C:\\Windows\\System32\\WindowsPowerShell/v1.0/powershell.exe"}))
pprint.pprint(goodrole)

pc('create an app for chrome', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"chrome", "type":"windows", "icon":"https://logos-download.com/wp-content/uploads/2016/05/Google_Chrome_logo_small.png","launchCmd":"C:\\Program Files (x86)\\Google\\Application\\chrome.exe", "install":"googlechrome"}))
pprint.pprint(goodrole)

pc('create a role for windsaz', bc.HEADER)
goodrole = json.loads(api('/role/create', {"userToken":admintok, "roleID":"wintest", "applicationIDs":["calc", "powershell", "chrome"],"mounts":"user","ramsize":"4096Mi","cpucount":2}))
pprint.pprint(goodrole)

pc('ok, admin will add rolo', bc.HEADER)
badv = json.loads(api('/user/role/authorize', {"userToken":admintok, "username":username, "roleID":"wintest"}))
pprint.pprint(badv)


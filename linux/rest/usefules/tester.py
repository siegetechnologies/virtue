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
	url = sys.argv[1]
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

def pc(stran, color = bc.HEADER):
	print(color + stran + bc.ENDC)
	sys.stdout.flush()


pc('login as admin')
login = json.loads(api('/user/login', {"username":"admin", "password":"changeMe!"}))
pprint.pprint(login)
admintok = login['userToken']

pc('Admin delete jimmbo')
jdl = json.loads(api('/user/remove', {"userToken":admintok, "username":"jimbo"}))
pprint.pprint(jdl)

pc('create a jimbo')
jcr = json.loads(api('/user/create', {"userToken":admintok, "username":"jimbo", "password":"jjnewtron"}))
pprint.pprint(jcr)

pc('login as jim')
login = json.loads(api('/user/login', {"username":"jimbo", "password":"jjnewtron"}))
pprint.pprint(login)
jimtok = login['userToken']

pc('jim change his password')
pw = json.loads(api('/user/password', {"userToken":jimtok, "password":"Goddardfetchgood"}))
pprint.pprint(pw)


pc('login as jim... again')
login = json.loads(api('/user/login', {"username":"jimbo", "password":"Goddardfetchgood"}))
pprint.pprint(login)
jimtok = login['userToken']

pc('Admin change jims password for him')
pw = json.loads(api('/user/password', {"userToken":admintok, "userID":"jimbo", "password":"newjimbo"}))
pprint.pprint(pw)

pc('login as jim... again again')
login = json.loads(api('/user/login', {"username":"jimbo", "password":"newjimbo"}))
pprint.pprint(login)
jimtok = login['userToken']




pc('admin is going to delete all virtues')
virts = json.loads(api('/virtue/list', {"userToken":admintok }))
pprint.pprint(virts)

for k,v in virts.items():
	delts = json.loads(api('/virtue/destroy', {"userToken":admintok, "virtueID":k}))
	pprint.pprint(delts)

pc('admin is going to delete all roles')
roles = json.loads(api('/role/list', {"userToken":admintok }))
pprint.pprint(roles)

for v in roles:
	delts = json.loads(api('/role/remove', {"userToken":admintok, "roleID":v['roleID']}))
	pprint.pprint(delts)

pc('admin is going to delete all apps')
apps = json.loads(api('/application/list', {"userToken":admintok }))
pprint.pprint(apps)

for v in apps:
	delts = json.loads(api('/application/remove', {"userToken":admintok, "applicationID":v['appID']}))
	pprint.pprint(delts)


pc('jim try to create a role')

badrole = json.loads(api('/role/create', {"userToken":jimtok, "roleID":"vidya", "applicationIDs":["supertuxkart"]}))
pprint.pprint(badrole)


pc('now admin will create apps')
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"supertuxkart","install":"supertuxkart", "type":"linux"}))
pprint.pprint(goodrole)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"nexuiz", "type":"linux", "install":"nexuiz"}))
pprint.pprint(goodrole)

pc('now admin will create role, since jampe is a lowly user')
goodrole = json.loads(api('/role/create', {"userToken":admintok, "roleID":"vidya", "applicationIDs":["supertuxkart"]}))
pprint.pprint(goodrole)

pc('admin is going to list applications')
apps = json.loads(api('/application/list', {"userToken":admintok }))
pprint.pprint(apps)


pc('jimmy try to start a virtue (No ssh key, no role auth, thing probably isnt done creating yet)')
badv = json.loads(api('/virtue/create', {"userToken":jimtok, "roleID":"vidya"}))
pprint.pprint(badv)

pc('jimmy adds an ssh key')
with open(str(Path.home()) + '/.ssh/id_rsa.pub') as f:
	keyboy = json.loads(api('/user/key', {"userToken":jimtok, "sshkey":f.read()}))
	pprint.pprint(keyboy)


pc('jimmy try to start a virtue (no role auth, thing probably isnt done creating yet)')
badv = json.loads(api('/virtue/create', {"userToken":jimtok, "roleID":"vidya"}))
pprint.pprint(badv)


pc('ok, admin will add rolo')
badv = json.loads(api('/user/role/authorize', {"userToken":admintok, "username":"jimbo", "roleID":"vidya"}))
pprint.pprint(badv)


pc('jimmy try to start a virtue (thing probably isnt done creating yet)')
badv = json.loads(api('/virtue/create', {"userToken":jimtok, "roleID":"vidya"}))
pprint.pprint(badv)


pc('lets wait until the role is ready')


while True:
	badv = json.loads(api('/role/get', {"userToken":jimtok, "roleID":"vidya"}))
	pprint.pprint(badv)
	if 'status' in badv and badv['status'] != "creating":
		break
	time.sleep(1)

pc('alright lets create the virtue this time!')

pc('this one should finally work')
badv = json.loads(api('/virtue/create', {"userToken":jimtok, "roleID":"vidya"}))
pprint.pprint(badv)





#ok, that worked...
pc('admin, create a new role!')
#now admin will
goodrole = json.loads(api('/role/create', {"userToken":admintok, "roleID":"gams", "applicationIDs":["nexuiz"]}))
pprint.pprint(goodrole)

pc('ok, admin will authorize rolo')
badv = json.loads(api('/user/role/authorize', {"userToken":admintok, "username":"jimbo", "roleID":"gams"}))
pprint.pprint(badv)


pc('wait until role is created (loop)')

while True:
	badv = json.loads(api('/role/get', {"userToken":jimtok, "roleID":"gams"}))
	pprint.pprint(badv)
	if 'status' in badv and badv['status'] != "creating":
		break
	time.sleep(1)


#and now jambi will make some new virts
pc('jimmy try to start a virtue vidya')
badv = json.loads(api('/virtue/create', {"userToken":jimtok, "roleID":"vidya"}))
pprint.pprint(badv)


#and now jambi will make some new virts
pc('jimmy try to start a virtue gams')
badv = json.loads(api('/virtue/create', {"userToken":jimtok, "roleID":"gams"}))
pprint.pprint(badv)


#lets have jimbo delete all the ones that are for vidya
pc('jimbo is going to delete his virtues for vidya')
virts = json.loads(api('/virtue/list', {"userToken":jimtok, "roleID":"vidya" }))
pprint.pprint(virts)

for k,v in virts.items():
	delts = json.loads(api('/virtue/destroy', {"userToken":jimtok, "virtueID":k}))
	pprint.pprint(delts)




pc('jimbo lists his virtues, should only be gam')
virts = json.loads(api('/virtue/list', {"userToken":jimtok }))
pprint.pprint(virts)


pc('Admin delete all virtues')
virts = json.loads(api('/virtue/list', {"userToken":admintok, "mine":False }))
pprint.pprint(virts)
delts = json.loads(api('/virtue/destroyall', {"userToken":admintok, "mine":False}))
pprint.pprint(delts)
virts = json.loads(api('/virtue/list', {"userToken":admintok, "mine":False }))
pprint.pprint(virts)

#for k,v in virts.items():
#	delts = json.loads(api('/virtue/destroy', {"userToken":admintok, "virtueID":k}))
#	pprint.pprint(delts)



pc('Admin will attempt to delete application in use by a role')
appdel = json.loads(api('/application/remove', {"userToken":admintok, "applicationID":"supertuxkart"}))
pprint.pprint(appdel)

pc('Admin will delete the role')
roledel = json.loads(api('/role/remove', {"userToken":admintok, "roleID":"vidya"}))
pprint.pprint(roledel)

roles = json.loads(api('/role/list', {"userToken":admintok }))
pprint.pprint(roles)

pc('Admin will attempt to delete application (should work)')
appdel = json.loads(api('/application/remove', {"userToken":admintok, "applicationID":"supertuxkart"}))
pprint.pprint(appdel)

roles = json.loads(api('/application/list', {"userToken":admintok }))
pprint.pprint(roles)



pc('Admin delete jimmbo')
jdl = json.loads(api('/user/remove', {"userToken":admintok, "username":"jimbo"}))
pprint.pprint(jdl)

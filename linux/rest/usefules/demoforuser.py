#!/usr/bin/python3
import pprint
import json

from helper import api,bc,pc


pc('login as admin', bc.HEADER)
login = json.loads(api('/user/login', {"username":"admin", "password":"changeMe!"}))
pprint.pprint(login)
admintok = login['userToken']

pc('create a user', bc.HEADER)
cret = json.loads(api('/user/create', {"userToken":admintok, "username":username, "password":"virtuerestdefaultpw"}))
pprint.pprint(cret)

pc('create an app for firefox', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"firefox", "type":"linux", "install":"firefox"}))
pprint.pprint(goodrole)

pc('create an app for xterm', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"xterm", "type":"linux", "install":"xterm"}))
pprint.pprint(goodrole)


pc('create an app for calc', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"calc", "type":"windows", "icon":"https://ih1.redbubble.net/image.572650548.7607/flat,1000x1000,075,f.u1.jpg", "launchCmd":"C:\\Windows\\System32\\calc.exe"}))
pprint.pprint(goodrole)

pc('create an app for iexplorer', bc.HEADER)
goodrole = json.loads(api('/application/create', {"userToken":admintok, "applicationID":"ie", "type":"windows", "icon":"https://ih1.redbubble.net/image.572650548.7607/flat,1000x1000,075,f.u1.jpg", "launchCmd":"C:\\Program Files (x86)\\Internet Explorer\\iexplore.exe "}))
pprint.pprint(goodrole)

pc('create a role for firefox + xterm', bc.HEADER)
goodrole = json.loads(api('/role/create', {"userToken":admintok, "roleID":"ffoxxterm", "applicationIDs":["firefox","xterm"], "cpucount":2, "ramsize":"4096Mi"}))
pprint.pprint(goodrole)
pc('create a role for windows', bc.HEADER)
goodrole = json.loads(api('/role/create', {"userToken":admintok, "roleID":"iecalc", "applicationIDs":["calc", "ie"]}))
pprint.pprint(goodrole)

pc('ok, admin will add rolo', bc.HEADER)
badv = json.loads(api('/user/role/authorize', {"userToken":admintok, "username":username, "roleID":"ffoxxterm"}))
pprint.pprint(badv)
pc('ok, admin will add rolo', bc.HEADER)
badv = json.loads(api('/user/role/authorize', {"userToken":admintok, "username":username, "roleID":"iecalc"}))
pprint.pprint(badv)



pc('lets wait until the role is ready', bc.HEADER)


while True:
	badv = json.loads(api('/role/get', {"userToken":admintok, "roleID":"ffoxxterm"}))
	pprint.pprint(badv)
	if 'status' in badv and badv['status'] != 'creating' and badv['status'] != 'uploading' and badv['status'] != 'bundling':
		break
	time.sleep(1)


'''
'''

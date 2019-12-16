#!/usr/bin/python3
import pprint
import json

from helper import api,bc,pc

pc('login as admin')
login = json.loads(api('/user/login', {"username":"admin", "password":"changeMe!"}))
pprint.pprint(login)
admintok = login['userToken']

pc('Admin list all roles')
virts = json.loads(api('/role/list', {"userToken":admintok, "mine":False }))
pprint.pprint( virts )

pc('Admin list all virtues')
virts = json.loads(api('/virtue/list', {"userToken":admintok, "mine":False }))
pprint.pprint( virts )

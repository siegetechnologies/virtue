#!/usr/bin/python3
## backend that will eventually be replaced by sqlite?


import json

filenm = ""



#so we dont have race conditions with writing to the jaysonn
from multiprocessing import Lock

mutex = Lock()

#json internal
def json_grabby(filename):
	try:
		with open(filename) as f:
			return json.load(f)
	except:
		return {}

def json_putty(filename, db):
	try:
		with open(filename, 'w') as f:
			json.dump(db, f)
	except:
		pass


#just return them all
def backend_get_roles():
	with mutex:
		db = json_grabby(filenm)
	try:
		return db['Roles']
	except:
		return {}
	return {}
#gets the role, returns empty if doesnt exist
def backend_get_role(roleid):
	try:
		return backend_get_roles()[roleid]
	except:
		return {}
	return {}

#just retun them all
def backend_get_applications():
	with mutex:
		db = json_grabby(filenm)
	try:
		return db['Applications']
	except:
		return {}
	return {}

#gets the application, returns empty if doesnt exist
def backend_get_application(appid):
	try:
		return backend_get_applications()[appid]
	except:
		return {}
	return {}



#set role, if role doesnt exist, creates it
def backend_set_role(roleid, role):
	with mutex:
		db = json_grabby(filenm)
		if 'Roles' not in db:
			db['Roles'] = {}
		db['Roles'][roleid] = role
		json_putty(filenm, db)

def backend_update_role(roleid, role):
	with mutex:
		db = json_grabby(filenm)
		#error out
		if 'Roles' not in db or roleid not in db['Roles']:
			return False
		db['Roles'][roleid].update(role)
		json_putty(filenm, db)
	return True

#set application. If no exist, create it
def backend_set_application(appid, app):
	with mutex:
		db = json_grabby(filenm)
		if 'Applications' not in db:
			db['Applications'] = {}
		db['Applications'][appid] = app
		json_putty(filenm, db)

def backend_update_application(appid, app):
	with mutex:
		db = json_grabby(filenm)
		#error out
		if 'Applications' not in db or appid not in db['Applications']:
			return False
		db['Applications'][appid] = app
		json_putty(filenm, db)
	return True

#currently json only?
def backend_init(filename):
	global filenm
	filenm = filename

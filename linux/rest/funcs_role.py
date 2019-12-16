import json
import uuid
import publish
import auth
import strutil
import sqlite_backend
import pprint
import traceback

from multiprocessing import Lock

import funcs_application	#for role import

def role_get_func(args):
	try:
		usertoken = args['userToken']
		roleid = args['roleID']
		if not usertoken or not strutil.roleidwhite(roleid):
			raise
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}
	except:
		return {"error":'invalidOrMissingParameters: role get {"userToken": "USERTOKEN", "roleID": "ROLEID" }'}

	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	if( user is None ):
		return {'error':'invalid or expired userToken'}

	#grab role
	res = sqlite_backend.global_sqlite_db.role_get(roleid)

	return res

roleduplock = Lock()

'''
 /role/create
{ "usertoken":string
, "roleID":string
, "applicationIDs":list(string)
, "type": string "linux" or "windows" optional
, "sensors": list(string)
}

'''
def role_create_func(args):
	try:
		# required
		usertoken = args['userToken']
		roleID = args['roleID']
		applicationIDs = args['applicationIDs']

		# optional
		mounts = args.get( 'mounts', 'none' )
		ramsize = args.get( 'ramsize', '1024Mi' )
		cpucount = args.get( 'cpucount', 1 )
		sensors = args.get( 'sensors', [] )

	except KeyError as e:
		return {'error': "missing paramater {}".format( e.args[0] ) }

	if( not strutil.roleidwhite( roleID ) ):
		return {"error":"invalid role id {}".format( roleID ) }


	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	if (not user or 'isAdmin' not in user): return {"error":"inavlid or expired userToken"}

	if not user['isAdmin']:
		return {"error":"userNotAuthorized"}


	#run through apps, find type
	#todo can possibly make faster/better
	#todo can probably do this entirely in SQL constraints
	#todo do this entirely in SQL constraints

	type = None
	if type in args and args['type']:
		type = args['type']
	else:
		for a in applicationIDs:
			try:
				ap = sqlite_backend.global_sqlite_db.application_get(a)
			except:
				return {"error":"Application id " + a + " doesn't exist!"}
			try:
				if not type:
					type = ap['type']
				if ap['type'] != type:
					return {"error":"Application IDs with differing roles! " + type + " vs " + ap['type']}
			except:
				continue

	if not type:
		return {"error":"Could not determine application Type from applications. Or you didnt specify it"}

	if not isinstance(cpucount, int):
		cpucount = 1
	if cpucount < 1:
		return {"error":"cpucount " + str(cpucount) + " is too low. Must be 1>=cpucount>=32"}
	if cpucount > 32:
		return {"error":"cpucount " + str(cpucount) + " is too high. Must be 1>=cpucount>=32"}

	sqlres = sqlite_backend.global_sqlite_db.role_create(roleID, applicationIDs, mounts, type, ramsize, cpucount, sensors=sensors)
	if 'error' in sqlres:
		return sqlres
	res = sqlite_backend.global_sqlite_db.role_get(roleID)
	if type.lower() == 'linux':
		publish.queue_role_creation(res)
	if type.lower() == 'windows':	#temp #todo
		sqlite_backend.global_sqlite_db.role_set_status(roleID, 'ready')

	if not res:
		res = {"success":True}
	return res


def role_remove_func(args):
	try:
		usertoken = args['userToken']
		roleID = args['roleID']
	except KeyError as e:
		return {'error': "missing paramater {}".format( e.args[0] ) }
	if( not strutil.roleidwhite( roleID ) ):
		return {"error":"invalid role id {}".format( roleID ) }


	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	if (not user or 'isAdmin' not in user): return {"error":"inavlid or expired userToken"}

	if not user['isAdmin']:
		return {"error":"userNotAuthorized"}


	res = sqlite_backend.global_sqlite_db.role_remove(roleID)
	if not res:
		res = {"success":True}
	return res

def role_list_func(args):
	try:
		usertoken = args['userToken']
		if not usertoken:
			raise
	except:
		return {"error":'invalidOrMissingParameters: role list {"userToken": "USERTOKEN" }'}


	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	try:
		username = user['uname']
		if not username:
			raise
	except:
		return {"error":"invalid or expired userToken"}

	#verify that user is an admin
	try:
		if not user['isAdmin']:
			raise
	except:
		return {"error":"userNotAuthorized"}

	res = sqlite_backend.global_sqlite_db.role_list()
	return res


def role_import_func(args):
	try:
		usertoken = args['userToken']
		roles = args['roles']
	except KeyError as e:
		return {'error':'missing argument {}'.format( e.args[0] ) }

	user = auth.global_auth_module.check_token( usertoken )
	if( not user or 'isAdmin' not in user ):
		return {'error': 'invalid or expired user token {}'.format( usertoken ) }

	if( not user['isAdmin'] ):
		return {'error': 'insufficient permissions to import roles' }

	if isinstance(roles, dict):
		#convert to list
		a = []
		for k,v in roles.items():
			v['roleID'] = k
			a += v
		roles = a
	elif not isinstance(roles, list):
		return {'error': 'roles argument not in dict or list form' }
	assert( isinstance( roles, list ) )

	ir = []
	for v in roles:
		print( "Importing role {}".format( v['roleID'] ) )
		kar = None
		try:
			v['userToken'] = usertoken
			kres = None
			if 'applications' in v:
				v['applicationIDs'] = [a['applicationID'] for a in v['applications']] # this is a hack
				kres = funcs_application.application_import_func(v)
				print( "Importing applications -> {}".format( kres ) )
			kar = role_create_func(v)
			pprint.pprint( v )
			print( "creating role -> {}".format( kar ) )
			#if kres:
			#	kar['applicationimportresults'] =kres
		except Exception as e:
			traceback.print_exc()
			kar = {"error":"role import failed with exception {}".format( e )}
			pass
		kar['roleID'] = v['roleID']
		ir.append( kar )
		pprint.pprint( ir )
	res = {"roleimportresults":ir}
	return res

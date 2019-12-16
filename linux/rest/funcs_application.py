import sqlite_backend
import strutil
import auth
import traceback
#this whole thing is TODO

def application_create_func( args ):
	try:
		# Required args
		usertoken = args['userToken']
		applicationID = args['applicationID']
		type = args['type']
	except KeyError as e:
		return {'error':'missing argument {}'.format( e.args[0] ) }

	# optional args
	launchCmd = args.get( 'launchCmd', applicationID )
	install = args.get( 'install', "" )
	icon = args.get( 'icon', "" )
	desktop = args.get( 'desktop', applicationID)
	postInstall = args.get( 'postInstall', "" )

	if type.lower() not in ['linux', 'windows']:
		return {'error':'application type must be either "linux" or "windows"'}


	if( not strutil.applicationidwhite( applicationID ) ):
		print ('bad application name {}'.format( applicationID ))
		return {'error':'bad application name {}'.format( applicationID ) }

	#todo come up with a better whitelist
	if( not strutil.applicationidwhite( desktop ) ):
		print ('bad desktop name {}'.format( desktop ))
		return {'error':'bad desktop name {}'.format( desktop ) }

	if( not strutil.installwhite( install ) ):
		print ('bad install name {}'.format( install ))
		return {'error':'bad install candidate {}'.format( install ) }

	#todo whitelist postinstall?

	user = auth.global_auth_module.check_token( usertoken )
	if( not user or 'isAdmin' not in user ):
		return {'error': 'invalid or expired user token {}'.format( usertoken ) }

	if( not user['isAdmin'] ):
		return {'error': 'insufficient permissions to add application' }

	res = sqlite_backend.global_sqlite_db.application_create( applicationID,launchCmd,install,icon,desktop,postInstall, type)
	if not res:
		res = {"success": True}
	return res


def application_remove_func( args ):
	try:
		usertoken = args['userToken']
		if 'applicationID' in args:
			applicationID = args['applicationID']
		else:
			applicationID = args['appID']
	except KeyError as e:
		return {'error':'missing argument {}'.format( e.args[0] ) }

	if( not strutil.applicationidwhite( applicationID ) ):
		return {'error':'bad application name {}'.format( applicationID ) }

	user = auth.global_auth_module.check_token( usertoken )
	if( not user or 'isAdmin' not in user ):
		return {'error': 'invalid or expired user token {}'.format( usertoken ) }

	if( not user['isAdmin'] ):
		return {'error': 'insufficient permissions to add application' }

	res = sqlite_backend.global_sqlite_db.application_remove(applicationID)
	if( "error" not in res ):
		return {'success': True }

	badroles = []
	rl = sqlite_backend.global_sqlite_db.role_list()
	for r in rl:
		if 'applications' not in r or 'roleID' not in r or not r['roleID']:
			continue
		appnames = [ a['appID'] for a in r['applications']]
		if applicationID in appnames:
			badroles.append(r['roleID'])
	return {'error' : 'There are some roles that use the application', 'badroles' : badroles }



def application_get_func(args):
	try:
		usertoken = args['userToken']
		applicationID = args['applicationID']
	except KeyError as e:
		return {"error":'missing argument {}'.format( e.args[0] ) }
	if (not strutil.applicationidwhite(applicationID) ):
		return {'error':'applicationID {} is in an invalid format. Check for invalid chars?'.format( applicationID ) }
	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	if( not user or 'isAdmin' not in user ):
		return {'error': 'invalid or expired user token {}'.format( usertoken ) }

	if( not user['isAdmin'] ):
		return {'error': 'insufficient permissions to get applications' }
	#grab app

	res = sqlite_backend.global_sqlite_db.application_get(applicationID)

	return res

def application_list_func(args):
	try:
		usertoken = args['userToken']
	except KeyError as e:
		return {"error":'missing argument {}'.format( e.args[0] ) }
	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	if( not user or 'isAdmin' not in user ):
		return {'error': 'invalid or expired user token {}'.format( usertoken ) }

	if( not user['isAdmin'] ):
		return {'error': 'insufficient permissions to list applications' }

	res = sqlite_backend.global_sqlite_db.application_list()
	return res


def application_import_func(args):
	try:
		usertoken = args['userToken']
		applications = args['applications']
	except KeyError as e:
		return {'error':'missing argument {}'.format( e.args[0] ) }

	user = auth.global_auth_module.check_token( usertoken )
	if( not user or 'isAdmin' not in user ):
		return {'error': 'invalid or expired user token {}'.format( usertoken ) }

	if( not user['isAdmin'] ):
		return {'error': 'insufficient permissions to import applications' }

	if isinstance(applications, dict):
		#convert to list
		a = []
		for k,v in applications.items():
			v['applicationID'] = k
			a += v
		applications = a
	elif not isinstance(applications, list):
		return {'error': 'applications argument not in dict or list form' }

	ir = []
	for v in applications:
		try:
			v['userToken'] = usertoken
			ir.append( application_create_func(v) )
		except:
			pass
	res = {"applicationimportresults":ir}
	return res

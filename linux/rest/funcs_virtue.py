import auth
import strutil
import base64
import efsshim

import sqlite_backend
import json


import funcs_user #for user_role_list in virtue_create_all_func


from multiprocessing import Lock #used for mutexing the create check



import poolb


from traceback import print_exc

from pprint import pprint


'''
Ease of use function that lists virtues and then destroys every one in that list
Arguments are the same to virtue list (same stuff happens with specifying userID and being an admin)
So you can use filters like roleID.... same sort of stuff happens with list
Just passes through args, same with auth stuff. All of the auth checking happens by list and by destroy, so its completely safe
Pretty much just a small wrapper
'''

def virtue_destroy_all_func(args):
	listy = virtue_list_func(args)
	if 'error' in listy:
		return listy
	res = []
	for k in listy:	#keys are virtueids, so i dont need values
		args['virtueID'] = k['virtueID']
		res.append(virtue_destroy_func(args))
	return res




'''
similar as above, but lists roles and starts virtues
same stuff with args, auth, etc
masquarade (should) work
'''
def virtue_create_all_func(args):
	listy = funcs_user.user_role_list_func(args)
	pprint(listy)
	if 'error' in listy:
		return listy
	res = []
	if 'roles' not in listy:	#unknown error?
		return []
	for k in listy['roles']:	#roles use lists instead of dicts? (might change later)
		if 'roleID' not in k or not k['roleID']:
			continue
		args['roleID'] = k['roleID']
		res.append(virtue_create_func(args))
	return res

def virtue_list_poolb_func(args):
	try:
		usertoken = args['userToken']
		if not usertoken:
			raise
	except:
		return {"error":'invalidOrMissingParameters: virtue list {"userToken": "USERTOKEN" }'}
	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	try:
		username = user['uname']
		if not username:
			raise
	except:
		return {"error":"invalid or expired userToken"}
	try:
		isAdmin = user['isAdmin']
	except:
		isAdmin = False
	try:
		mine = args['mine']
	except:
		mine = True	#default to only show mine
	#if they are not an admin, they shouldnt see others
	if not isAdmin:
		mine = True


#grab selectors, if set
	selectuser = ''
	if 'userID' in args:
		selectuser = args['userID']
		if not strutil.useridwhite(selectuser):
			return {"error":"TODO bad userID"}

	selectrole = ''
	if 'roleID' in args:
		selectrole = args['roleID']
		if not strutil.roleidwhite(selectrole):
			return {"error":"TODO bad roleID"}



	selectvirtue = ''
	if 'virtueID' in args:
		selectvirtue = args['virtueID']
		if not strutil.virtueidwhite(selectvirtue):
			return {"error":"TODO bad virtueID"}


	if not selectuser and mine: selectuser = username #dont bother grabbing other users, aka optimization


	ressy = poolb.poollist(usernm=selectuser, rolenm=selectrole, virtuenm=selectvirtue)

	pprint(ressy)

	resp = []
	for j in ressy:
		try:
			rese = j['info']
			rese['port'] = j['port']
			rese['virtueID'] = j['virtueID']
			rese['state'] = j['state']
			rese['available']  = j['state'].lower() in ['ready', 'running']
			resp.append( rese )
		except:
			print_exc()
			pass
	return resp

#wrapper for both linux and windows lists
'''
By default, will list info for all virtues you are authorized to (user only lists their virtues, admin lists all virtues)
if you specify the arg userid, will list all virtues that you are authorized to that have that userid (useful only for admins)
if you specify roleid, will list all virtues that you are authorized to that have that roleid
if you speficy virtueid... etc
if you specify mine, will only return "your" virtues, even if you have access to others. Default mine = True
if you specify more than 1, it matches on more than 1 (roleid, userid, etc)
'''
def virtue_list_func(args):
	try:
		usertoken = args['userToken']
		if not usertoken:
			raise
	except:
		return {"error":'invalidOrMissingParameters: virtue list {"userToken": "USERTOKEN" }'}
	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	try:
		username = user['uname']
		if not username:
			raise
	except:
		return {"error":"invalid or expired userToken"}

	return virtue_list_poolb_func(args)
#	return virtue_list_linux_func(args)
	#win = virtue_list_windows_func(args)



def virtue_create_func(args):
	print( "/virtue/create" )
	print( args )
	try:
		usertoken = args['userToken']
		roleid = args['roleID']
		debug = args.get( 'debug', False )
		sshkey = args.get( 'sshkey', '')
		windows_key_hack = args.get( 'windows_key_hack', '' )

		if not strutil.roleidwhite(roleid) or not usertoken:
			raise
	except:
		return {"error":'invalidOrMissingParameters: virtue create {"userToken": "USERTOKEN", "roleID": "ROLEID" }'}

	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	if 'uname' in user and user['uname']:
		username = user['uname']
	else:
		return {"error":"invalid or exprired userToken"}

	#masqurade if an admin
	if 'userID' in args and args['userID']:
		if not user['isAdmin']:
			return {"error":"user not authenticated to launch virtues for other users"}
		try:
			if not strutil.useridwhite(args['userID']):
				raise
			user = auth.global_auth_module.user_get(usertoken, args['userID'])
			username = user['uname']
			if not username:
				raise
		except:
			return {"error":"invalid userID"}

	#verify that user has access to role
	userroles = sqlite_backend.global_sqlite_db.user_role_list(username)
	try:
		if all([j['roleID'] != roleid for j in userroles]):
			raise
	except:
		return {"error":"user " + username + " not authenticated to access roleID " + roleid}

	role = sqlite_backend.global_sqlite_db.role_get(roleid)
	if not role:
		return {"error":"invalid or nonexiztent roleID " + roleid}

	if role['status'] == "creating":
		#todo real error codeo
		return {"error":"role Not Ready"}
	if role['status'] != "ready":
		#todo real error codeo
		return {"error":"roleBorked"}

	if 'type' not in role:
		return {"error":"Broken role? no type " + roleid}

	#check if virtue already started for that virtue for that role

	ressy = poolb.poollist(usernm=username, rolenm=roleid)
	if ressy:
		return {"error":"Virtue with role " + roleid + " already started for user " + username}

	return virtue_create_poolb_func(role, user, sshkey)

def virtue_create_poolb_func(role, user, sshkey=''):
	roleid = role.get('roleID')
	username = user.get('uname')
	if not sshkey:
		sshkey = user.get('sshkey')
	ramsize = role.get('ramsize')
	cpucount = role.get('cpucount')
	virtueid = strutil.genvirtueid()
	applications = role.get('applications')
	mounts = role.get( 'mounts', "" )
	if not sshkey:
		return {"error":"no ssh key for this user. Either specify it or upload it"}



#set up info
	info = {
		'virtueID':virtueid,
		'userID':username,
		'roleID':roleid,
		'applications':applications,
		'resourceIDs':'todo',
		'transducerIDs':'todo',
		'type':'linux',
		'host':poolb.node, #BIG TODO
		'mounts':mounts
	}
#set up vconf

	#todo grab type proper


	toport = '22'
	nettest = "ssh"
	if info['type'] == 'windows':
		toport = '3389'
		nettest = "rdp"

	sensors = role['sensors']
	mavenconf = base64.b64encode('\n'.join( [ s['data'] for s in sensors if s['sensorType'] == 'mavenconf']).encode('utf-8')).decode('utf-8').strip()

	usak = ''
	psak = ''
	psan=''
	usan=''
	usapass=''
	psapass=''

	if 'user' in mounts:
		usan=strutil.saniefsuser(username)
		usak,usapass = efsshim.createshim(usan)
	if 'persistent' in mounts:
		psan=strutil.saniefsurd(username, roleid)
		psak,psapass = efsshim.createshim(psan)
	armors = [s for s in role['sensors'] if s['sensorType'] == 'apparmor']
	apparmor = 'runtime/default'
	if( len( armors ) > 0 ):
		if( len( armors ) > 1 ):
			print("Error, role has more than one apparmor profile, using first one")
		kube.loadApparmorOnNodes( armors[0]['data'] )
		apparmor = 'localhost/' + armors[0]['sensorID'] # really hope this matches the profile text
#		debug = True
	#todo change this to a try except so its clean


	vconf = ''
	vconf += 'VIRTUEID=%s\n' % virtueid
	vconf += 'VIRTUE_MAVENCONF=%s\n' % mavenconf
	vconf += 'VIRTUE_AUTH_SSH=%s\n' % sshkey
	vconf += 'YEETMAN=Easteregg\n'
	vconf += 'VIRTUE_USER_AUTH_NAME=%s\n' % usan
	vconf += 'VIRTUE_PERSISTENT_AUTH_NAME=%s\n' % psan
	vconf += 'VIRTUE_USER_AUTH_KEY=%s\n' % usak
	vconf += 'VIRTUE_PERSISTENT_AUTH_KEY=%s\n' % psak

	#todo have a definable baseimage, patchimage, etc
	imageurl = "https://s3.amazonaws.com/siege-virtuevms/" + roleid + ".qcow2_baked" #todo

	res = poolb.pooldeploy(virtueid, imageurl, info, vconf, toport, nettest, ramsize=ramsize, cpucount=cpucount)
	return {"name":virtueid}

def virtue_destroy_poolb_func(args):
	try:
		usertoken = args['userToken']
		virtueid = args['virtueID']
		if not usertoken or not strutil.virtueidwhite(virtueid):
			raise
	except:
		return {"error":'invalidOrMissingParameters: virtue destroy {"usertoken": "USERTOKEN", "virtueid": "VIRTUEID" }'}


	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	try:
		username = user['uname']
		if not username:
			raise
	except:
		return {"error":"invalid or expired userToken"}

	try:
		isAdmin = user['isAdmin']
	except:
		isAdmin = False


	#get the virtue, verify access
	try:
		res = poolb.poollist(virtuenm=virtueid)[0]
	except:
		return {"error":"invalid or nonexistent virtueID"}

	try:
		if not username == res['info']['userID'] and not isAdmin:
			raise
	except:
		return {"error":"userNotAuthorized"}

	poolb.pooldestroy(virtueid)

	return {"success":True}

def virtue_destroy_func(args):
	try:
		usertoken = args['userToken']
		virtueid = args['virtueID']
		if not usertoken or not strutil.virtueidwhite(virtueid):
			raise
	except:
		return {"error":'invalidOrMissingParameters: virtue destroy {"usertoken": "USERTOKEN", "virtueid": "VIRTUEID" }'}


	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	try:
		username = user['uname']
		if not username:
			raise
	except:
		return {"error":"invalid or expired userToken"}

	return virtue_destroy_poolb_func(args)


def virtue_cleanup_func(args):
	##will look through services, deployments, etc and find any issues. Then will remove anything related to that issue
	try:
		usertoken = args['userToken']
		if not usertoken:
			raise
	except:
		return {"error":'invalidOrMissingParameters: virtue cleanup {"usertoken": "USERTOKEN" }'}


	#verify userid
	user = auth.global_auth_module.check_token(usertoken)
	try:
		username = user['uname']
		if not username:
			raise
	except:
		return {"error":"invalid or expired userToken"}

	try:
		isAdmin = user['isAdmin']
	except:
		isAdmin = False


#grab selectors, if set
	selectuser = ''
	if 'userID' in args:
		selectuser = args['userID']
		if not strutil.useridwhite(selectuser):
			return {"error":"TODO bad userID"}

	selectrole = ''
	if 'roleID' in args:
		selectrole = args['roleID']
		if not strutil.roleidwhite(selectrole):
			return {"error":"TODO bad roleID"}



	selectvirtue = ''
	if 'virtueID' in args:
		selectvirtue = args['virtueID']
		if not strutil.virtueidwhite(selectvirtue):
			return {"error":"TODO bad virtueID"}



	#if not admin, only grab my own
	if not isAdmin:
		if selectuser and selectuser != username:
			#todo error better
			return {"error":"Setting a userID while not admin?"}
		selectuser = username


	#todo
	#a lot of todo



	for f in poolb.poollist(virtuenm=selectvirtue, usernm = selectuser, rolenm = selectrole):
		try:
			if f['state'].lower() not in ['initializing', 'booting', 'imagedl', 'configgen', 'ready' ]:
				raise
		except:
			try:
				#double check user
				if(f['info']['userID'] == selectuser):
					poolb.pooldestroy(f['virtueID'])
			except:
				print_exc()

		return {"success":True}

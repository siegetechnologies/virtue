import auth
import strutil
import pprint

#from aws_utils import grabWorkspaceIp
from aws_utils import usersWorkspaceIP

def user_login_func(args, ip=None):
	print( args )
	print("ip is " + str(ip))
	try:
		uname = args['username']
		passw = args['password']
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}
	try:
		if ip == '127.0.0.1' or ip == usersWorkspaceIp(uname):
			return auth.global_auth_module.user_login( uname, passw )
	except:
		pass
	return {"error": "Invalid IP for user"}
def user_password_func(args):
	try:
		usertoken = args['userToken']
		newpw = args['password']
		if not usertoken:
			raise
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}
	except:
		return {"error": "Missing argument TODO"}
#verify user token and grab info
	user = auth.global_auth_module.check_token(usertoken)
	try:
		username = user['uname']
		if not username:
			raise
	except:
		return {"error":"invalid or expired userToken"}

#masqurade if an admin
	if 'userID' in args and args['userID']:
		if not user['isAdmin']:
			print("NOOOOO")
			return {"error":"user not authorized to change other users passwords"}
		return auth.global_auth_module.user_change_pw_uname(args['userID'], newpw)
#otherwise go through usual system?
	return auth.global_auth_module.user_change_pw(usertoken, newpw)

def user_logout_func(args):
	try:
		tok = args['userToken']
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}
	return auth.global_auth_module.user_logout( tok )


#todo move these to funcs_role?
def user_role_list_func(args):
	try:
		tok = args['userToken']
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}
	return auth.global_auth_module.user_role_list( tok )

def user_role_authorize_func(args):
	try:
		tok = args['userToken']
		uname = args['username']
		roleID = args['roleID']
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}
	return auth.global_auth_module.user_add_role( tok, uname, roleID )

def user_role_unauthorize_func(args):
	try:
		tok = args['userToken']
		uname = args['username']
		roleID = args['roleID']
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}
	return auth.global_auth_module.user_remove_role( tok, uname, roleID )


def user_list_func(args):
	try:
		tok = args['userToken']
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}
	return auth.global_auth_module.user_list( tok )

def user_get_func(args):
	try:
		tok = args['userToken']
		uname = args['username']
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}
	return auth.global_auth_module.user_get( tok, uname )


def user_create_func(args):
#todo maybe add "rAnDoM" password generation support
	try:
		tok = args['userToken']
		nusername = args['username']
		npassw = args['password']
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}

	if not tok or not strutil.useridwhite(nusername):
		return {'error':'invalid username {}'.format( nusername ) }
	#todo passwordwhitelist

#isAdmin is not a required field, defaults to False
	try:
		nadmin = args['isAdmin']
	except:
		nadmin = False

	return auth.global_auth_module.user_create(tok, nusername, npassw, nadmin)


def user_remove_func(args):
	try:
		tok = args['userToken']
		nusername = args['username']
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}

	if not tok or not strutil.useridwhite(nusername):
		return {'error':'invalid username {}'.format( nusername ) }

	return auth.global_auth_module.user_remove(tok, nusername)





def user_key_func(args):

	try:
		usertoken = args['userToken']
#todo sanitize key?
		if not usertoken:
			raise
	except KeyError as k:
		return {"error": "missing argument: {}".format( k.args[0] )}
	except:
		return {"error": "Missing argument TODO"}


#grab key if there
	sshkey = None
	try:
		sshkey = args['sshkey']
	except:
		pass

#verify user token and grab info
	user = auth.global_auth_module.check_token(usertoken)
	try:
		username = user['uname']
		if not username:
			raise
	except:
		return {"error":"invalid or expired userToken"}


#todo might change this behavior a little...
	if sshkey:
		return auth.global_auth_module.user_update_ssh_key(usertoken, sshkey)

	try:
		sshkey = user['sshkey']
	except:
		pass
	return sshkey

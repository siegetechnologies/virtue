
import string
import hashlib
from hmac import compare_digest
import uuid
from os import urandom
import binascii
import datetime
from time import sleep
from sqlite_backend import global_sqlite_db,SQLiteBackend


def randombytes( length=32 ):
	assert( length > 0 )
	return urandom( length )

def valid_username( uname ):
	valid_chars = string.ascii_lowercase + string.ascii_uppercase + string.digits +  '_'
	return (all( c in valid_chars for c in uname )) and (len(uname) < 64)

'''
Module to manage authentication.
May be subject to frustration
Awaiting better documentation

Function return types are pretty inconsistant, working on fixing that
'''
class AuthModule:
	# https://docs.python.org/3.5/library/hashlib.html#key-derivation
	preferred_hash_algo = 'sha512'
	preferred_hash_reps = 100000

	'''
	By default, the module is created with 1 admin user.
	This should change when the DB is not destroyed every time the code restarts
	Maybe have a parameter to use existing DB?
	'''
	def __init__(self, storage, defaultAdminUser=None, defaultAdminPass=None ):
		self.storage = storage
		self.dummy_salt = randombytes( )
		assert( self.preferred_hash_algo in hashlib.algorithms_guaranteed )

		if( defaultAdminUser is None or defaultAdminPass is None ):
			return

		# Adding info for admin user
		salt = randombytes(  )
		passhash = self._do_passhash( defaultAdminPass, salt )
		# insert into table values (uname,salt,passhash
		self.storage.store_user( defaultAdminUser, salt, passhash, True )

	def user_exists( self, uname ):
		res =  self.storage.get_user_info( uname )
		return (res is not None)

	def _do_passhash( self, passw, salt ):
		return hashlib.pbkdf2_hmac(
			self.preferred_hash_algo,
			passw.encode('ascii'),
			salt,
			self.preferred_hash_reps)

	'''
	Admin method to add a new user
	'''
	def user_create( self, admintoken, uname, passw, isAdmin=False ):
		if( not self._validate_admin_token( admintoken ) ):
			return {'error':'invalid admin token {}'.format(admintoken)}
		if( self.user_exists( uname ) ):
			return {'error':'user {} already exists'.format(uname)}
		# TODO(?) password complexity requirements?
		if( not valid_username( uname ) ):
			return {'error':'username contains invalid characters'}

		salt = randombytes(  )
		passhash = self._do_passhash( passw, salt )
		# insert into table values (uname,salt,passhash
		self.storage.store_user( uname, salt, passhash, isAdmin )
		return {'success':True}

	'''
	admin method to remvove a new user
	'''

	def user_remove( self, admintoken, uname):
		if( not self._validate_admin_token( admintoken ) ):
			return {'error':'invalid admin token {}'.format(admintoken)}
		if not self.user_exists( uname ) :
			print("Warning, user " + uname + " doesnt exist, will attempt to delete anyway\nThis May Error!")

		self.storage.user_remove( uname)
		return {'success':True}

	def user_update_ssh_key( self, usertoken, sshkey ):
		uinfo = self.check_token( usertoken )
		if( uinfo is None ):
			return {'error':'invalid userToken'}
		self.storage.user_set_ssh_key( uinfo['uname'], sshkey )
		return {'success':True}

	def user_change_pw( self, usertoken, newpassword):
		uinfo = self.check_token( usertoken )
		if( uinfo is None ):
			return {'error':'invalid userToken'}
		# TODO(?) password complexity requirements?
		salt = randombytes(  )
		passhash = self._do_passhash( newpassword, salt )
		self.storage.user_set_password( uinfo['uname'], salt, passhash )
		#kill the previous auth keys
		self.user_logout_all(uinfo['uname'])
		return {'success':True}


	#only to be called by previously authed functions
	def user_change_pw_uname(self, username, newpassword):
		if not self.user_exists(username):
			return {'error':'specified user does not exist'}
		# TODO(?) password complexity requirements?
		salt = randombytes(  )
		passhash = self._do_passhash( newpassword, salt )
		self.storage.user_set_password( username, salt, passhash )
		#kill the previous auth keys
		self.user_logout_all(username)
		return {'success':True}

	'''
	perform login
	Salting and hashing is done in this module, backend data storage
	and retieval is done elswhere

	Assuming the uname and password are correct, a 48-byte base64 encoded token
	will be returned

	the token will expire in a time based on the duration parameter (default 8 hours)

	If login info is incorrect, returns None
	'''
	def user_login( self, uname, passw, duration=datetime.timedelta(hours=8) ):
		'''
		Get the salt for the user. If the salt isn't found, this user doesn't exist
		But we still want to compute the password, to prevent timing attacks,
		so we have a dummy salt we can use instead.
		'''
		info = self.storage.user_full_info( uname )
		if( info is None ):
			salt = self.dummy_salt
		else:
			salt = info['salt']

		passhash = self._do_passhash( passw, salt )
		#reduces the likelyhood of a timing attak?
		#we hash the password on server, so that means its impossible? to actually perform a timing attack here
		#we should probably be hasing the passwords clientside anyway (which would mean this would actually be a timing attack vector)
		#so i should fix it anyway
#		if( info is None or passhash != info['passhash'] ):


	#TODO not entirely 100% sure that hmac's compare_digest is the right thing for this, but it appears to work.
		if( info is None or not compare_digest(passhash, info['passhash']) ):
			return {'error':'login failed'}

		# can change this to uuid4 if you want
		tok = binascii.b2a_base64( randombytes(48) ).strip().decode( 'ascii' )
		expiration = (datetime.datetime.now()+duration).timestamp()
		self.storage.add_user_token( tok, expiration, uname )
		return {'userToken':tok}

	#only to be run by other authed functions
	def user_logout_all( self, uname ):
		self.storage.delete_user_token_by_name( uname )
		return {'success':True}

	def user_logout( self, token ):
		if( self.check_token( token ) is None ):
			return {'error':'invalid userToken {}'.format( token )}
		self.storage.delete_user_token( token )
		return {'success':True}


	'''
	returns user information associated with a login token, or None,
	if the token token cannot be found is has expired
	'''
	def check_token( self, tok ):
		res = self.storage.get_user_token( tok )
		if( res is None ):
			return None

		if( datetime.datetime.fromtimestamp( res['expiration'] ) < datetime.datetime.now() ):
			self.storage.delete_user_token( tok )
			return None

		return res


	def _validate_admin_token( self, admintoken ):
		res = self.check_token( admintoken )
		return res is not None and res['isAdmin']

	'''
	add role to user.
	Performs validation of admin token, and makes sure user really exists
	'''
	def user_add_role( self, admintoken, uname, roleid ):
		if( not self._validate_admin_token( admintoken ) ):
			return {'error':'invalid admin token {}'.format(admintoken)}

		if( not self.user_exists( uname ) ):
			return {'error':'user {} does not exist'.format( uname ) }

		# TODO check role for existance

		self.storage.add_user_role( uname, roleid )
		return {'success':True}

	def user_remove_role( self, admintoken, uname, roleid ):
		if( not self._validate_admin_token( admintoken ) ):
			return {'error':'invalid admin token {}'.format(admintoken)}

		if( not self.user_exists( uname ) ):
			return {'error':'user {} does not exist'.format( uname ) }

		# TODO check role for existance

		self.storage.remove_user_role( uname, roleid )
		return {'success':True}

	def user_role_list( self, tok ):
		tok_info = self.check_token( tok )
		if( tok_info is None ):
			return {'error':'invalid userToken {}'.format(tok)}

		return {'roles': self.storage.user_role_list( tok_info['uname'] )}

	def delete( self ):
		#self.storage.delete_db( )
		pass

	def user_list( self, admintoken ):
		if( not self._validate_admin_token( admintoken ) ):
			return {'error':'invalid admin token {}'.format(admintoken)}
		return {'users':self.storage.user_list( )}

	def user_get( self, admintoken, uname ):
		if( not self._validate_admin_token( admintoken ) ):
			return {'error':'invalid admin token {}'.format(admintoken)}
		res =  self.storage.get_user_info( uname )
		if( res is None ):
			return {'error': 'user {} not found'.format( uname ) }
		return res


def basic_tests():
	tmpstorage = SQLiteBackend( 'test db', True )
	a = AuthModule( tmpstorage, defaultAdminUser='root', defaultAdminPass='toor' )

	# user login
	admtok_resp = a.user_login( 'root', 'toor' )
	assert( 'userToken' in admtok_resp )
	admtok = admtok_resp['userToken']

	#user change password
	chpw_resp =  a.user_change_pw(admtok, 'changed' )
	assert( 'success' in chpw_resp )

	#verify that login has been changed
	assert( 'error' in a.user_list( admtok ) )

	#user login again
	admtok_resp =  a.user_login( 'root', 'changed' )
	assert( 'userToken' in admtok_resp )
	admtok = admtok_resp['userToken']

	#verify no user check
	chpw_resp =  a.user_change_pw_uname('noexist', 'changed2' )
	assert( 'error' in chpw_resp )

	#user change password a different way
	chpw_resp =  a.user_change_pw_uname('root', 'changed2' )
	assert( 'success' in chpw_resp )

	#verify that login has been changed
	assert( 'error' in a.user_list( admtok ) )

	#user login again
	admtok_resp =  a.user_login( 'root', 'changed2' )
	assert( 'userToken' in admtok_resp )
	admtok = admtok_resp['userToken']

	# user create
	assert( 'error' not in a.user_create( admtok, 'jesse', 'password1' ) )

	# user list
	ulist_resp = a.user_list( admtok )
	assert( 'users' in ulist_resp )
	assert( { d['uname'] for d in ulist_resp['users'] } == {'jesse','root'} )

	# more user login
	assert( 'error' in a.user_login( 'steve', 'password' ) )
	assert( 'error' in a.user_login( 'jesse', 'bad_pass' ) )
	tok_resp = a.user_login( 'jesse', 'password1' )
	assert( 'userToken' in tok_resp )
	ustok = tok_resp['userToken']


	#removed, since roles are no longer part of auth framework
	# role list
	#rolelist_resp = a.user_role_list( ustok )
	#assert( 'roles' in rolelist_resp )
	#assert( rolelist_resp['roles'] == [] )

	# authorize role
	#a.user_add_role( admtok, 'jesse', 'test role' )
	#rolelist_resp = a.user_role_list( ustok )
	#print( rolelist_resp )
	#assert( 'roles' in rolelist_resp )
	#assert( rolelist_resp['roles'] == ['test role'] )

	# logout
	a.user_logout( ustok )
	assert( a.check_token( ustok ) is None )

	tok_resp = a.user_login( 'jesse', 'password1' )
	assert( 'userToken' in tok_resp )
	ustok = tok_resp['userToken']

	# user remove
	assert ( "success" in a.user_remove(admtok, "jesse") )
	assert ( a.check_token( ustok ) is None )

	# user list
	ulist_resp = a.user_list( admtok )
	assert( 'users' in ulist_resp )
	assert( { d['uname'] for d in ulist_resp['users'] } == {'root'} )


	a.delete()
	print( "Basic user tests complete" )
	tmpstorage.delete_db()


global_auth_module = None
# run at load time
if( global_auth_module is None ):
	basic_tests()
	if( len( global_sqlite_db.user_list() ) > 0 ):
		global_auth_module = AuthModule( global_sqlite_db )
	else:
		global_auth_module = AuthModule( global_sqlite_db, defaultAdminUser='admin', defaultAdminPass='changeMe!' )

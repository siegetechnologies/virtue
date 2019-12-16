
import sqlite3
from os import remove
from os.path import exists
from multiprocessing import Lock
import traceback
import itertools

SUCCESS = {"success":True}

global_sqlite_db = None

class SQLiteBackend:

	def __init__( self, dbname='virtue_system', rebuild=True ):
		self.dbname = dbname
		self.conn = sqlite3.connect( self.dbname, check_same_thread=False )
		self.conn.row_factory = sqlite3.Row
		self.conn_mut = Lock()
		if( rebuild or (not self.db_exists()) ):
			self.db_create()
		assert( self.db_exists() )


	def db_create( self ):
		with self.conn_mut:
			c = self.conn.cursor()
			# Enforce Foreign key constraints
			c.execute( '''DROP TABLE IF EXISTS userRoles''' )
			c.execute( '''DROP TABLE IF EXISTS userTokens''' )
			c.execute( '''DROP TABLE IF EXISTS roleApps''' )
			c.execute( '''DROP TABLE IF EXISTS roleSensors''' )
			c.execute( '''DROP TABLE IF EXISTS applications''' )
			c.execute( '''DROP TABLE IF EXISTS users''' )
			c.execute( '''DROP TABLE IF EXISTS roles''' )
			c.execute( '''DROP TABLE IF EXISTS sensors''' )
			c.execute( '''PRAGMA foreign_keys = ON''')

			c.execute( '''CREATE TABLE users (
				uname text PRIMARY KEY,
				salt blob,
				passhash blob,
				isAdmin integer,
				sshkey TEXT)''')
			c.execute( '''CREATE TABLE userTokens (
				token text PRIMARY KEY,
				expiration numeric,
				uname text,
				FOREIGN KEY(uname) REFERENCES users(uname))''')
			c.execute( '''CREATE TABLE roles (
				roleID text PRIMARY KEY,
				status text,
				mounts text,
				ramsize text,
				cpucount integer,
				type text)''')
			c.execute( '''CREATE TABLE applications (
				appID text PRIMARY KEY,
				launchCmd text,
				install text,
				icon text,
				desktop text,
				postInstall text,
				type text)''')
			c.execute( '''CREATE TABLE sensors (
				sensorID text PRIMARY KEY,
				sensorType text,
				data text)''')
			c.execute( '''CREATE TABLE roleApps (
				roleID text,
				appID text,
				FOREIGN KEY(appID) REFERENCES applications(appID),
				FOREIGN KEY(roleID) REFERENCES roles(roleID),
				PRIMARY KEY (roleID, appID))''' )
			c.execute( '''CREATE TABLE userRoles (
				uname text,
				roleID text,
				FOREIGN KEY(roleID) REFERENCES roles(roleID),
				FOREIGN KEY(uname) REFERENCES users(uname),
				PRIMARY KEY(roleID,uname))''')
			c.execute( '''CREATE TABLE roleSensors (
				roleID text,
				sensorID text,
				FOREIGN KEY(sensorID) REFERENCES sensors(sensorID),
				FOREIGN KEY(roleID) REFERENCES roles(roleID),
				PRIMARY KEY (roleID, sensorID))''' )

			self.conn.commit()

	def db_exists( self ):
		if( self.dbname == ":memory:"):
			return True
		return exists( self.dbname ) # good enough

	def get_user_info( self, uname ):
		with self.conn_mut:
			c = self.conn.cursor()
			c.execute( '''SELECT uname,isAdmin,sshkey FROM users WHERE uname=? ''',(uname,) )
			res = c.fetchone()
			if( res is None ):
				return None
			ret = dict( res )
			ret['roles'] = self._roles_for_user( uname )
		return ret

	def user_full_info( self, uname ):
		with self.conn_mut:
			c = self.conn.cursor()
			c.execute( '''SELECT * FROM users WHERE uname=? ''',(uname,) )
			res = c.fetchone()
			if( res is None ):
				return None
			ret = dict( res )
			ret['roles'] = self._roles_for_user( uname )
		return dict( res )


	def delete_user_token( self, token ):
		with self.conn_mut:
			c = self.conn.cursor()
			c.execute( '''DELETE from userTokens WHERE token=?''', (token,) )
			self.conn.commit()

	'''
		same as above but by username
	'''
	def delete_user_token_by_name( self, uname ):
		with self.conn_mut:
			c = self.conn.cursor()
			c = self.conn.cursor()
			c.execute( '''DELETE from userTokens WHERE uname=?''', (uname,) )
			self.conn.commit()

	def tokens_list( self ):
		with self.conn_mut:
			cursor = self.conn.execute( ''' SELECT * from userTokens''' )
			return [dict(d) for d in cursor.fetchall() ]


	# expiration is a iso-8601 string
	# TODO exception checking. UNIQUE, EXISTANCE?
	def add_user_token( self, token, expiration, uname ):
		with self.conn_mut:
			try:
				c = self.conn.cursor()
				# DELETE existing tokens
				c.execute( '''DELETE FROM userTokens where uname = ?''', (uname, ) );
				c.execute( ''' INSERT into userTokens VALUES (?,?,?) ''', (token,expiration,uname) );
				self.conn.commit()
			except sqlite3.IntegrityError as e:
				if( e.args[0].startswith( 'UNIQUE' ) ):
					return {'error':'token already exists?'}
				elif( e.args[0].startswith( 'FOREIGN KEY' ) ):
					return {'error':'user {} does not exist'.format( uname )}
				else:
					raise
			except Exception as e:
				traceback.print_exc()
		return SUCCESS

	def get_user_token( self, token ):
		with self.conn_mut:
			c = self.conn.cursor()
			c.execute( '''SELECT token,expiration,userTokens.uname,isAdmin,sshKey FROM userTokens
				INNER JOIN users ON users.uname = userTokens.uname
				WHERE userTokens.token=? ''',(token,) )
			res = c.fetchone()
		if( res is None ):
			return None
		return dict( res )


	def store_user( self, uname, salt, passhash, isAdmin ):
		with self.conn_mut:
			try:
				c = self.conn.cursor()
				c.execute( '''INSERT into users values (?,?,?,?,NULL)''', (
					uname,
					sqlite3.Binary(salt),
					sqlite3.Binary(passhash),
					isAdmin) )
				self.conn.commit()
			except sqlite3.IntegrityError as e:
				if( e.args[0].startswith( 'UNIQUE' ) ):
					return {'error':'user already exists'}
				else:
					raise
			except Exception as e:
				traceback.print_exc()
				return {'error':e.format_exc()}

	def user_set_ssh_key( self, uname, keytext ):
		with self.conn_mut:
			try:
				c = self.conn.cursor()
				c.execute( '''UPDATE users SET sshkey=? WHERE uname=?''', (keytext, uname ) )
				self.conn.commit()
			except Exception as e:
				traceback.print_exc()
				return {'error':e.format_exc()}
		return SUCCESS

	def user_set_password( self, uname, salt, passhash ):
		with self.conn_mut:
			try:
				c = self.conn.cursor()
				c.execute( '''UPDATE users SET salt=?,passhash=? WHERE uname=?''', (sqlite3.Binary(salt), sqlite3.Binary(passhash), uname ) )
				self.conn.commit()
			except Exception as e:
				traceback.print_exc()
				return {'error':e.format_exc()}
		return SUCCESS

	def add_user_role( self, uname, roleId ):
		# this check should be done by AuthModule too
		with self.conn_mut:
			try:
				c = self.conn.cursor()
				c.execute( '''INSERT into userRoles VALUES (?,?)''',(uname,roleId) )
				self.conn.commit()
			except sqlite3.IntegrityError as e:
				if( e.args[0].startswith( 'UNIQUE' ) ):
					return {'error':'user already authorized for this role'}
				elif( e.args[0].startswith( 'FOREIGN KEY' ) ):
					return {'error':'user {} or role {} does not exist'.format(uname,roleId)}
				else:
					raise
			except Exception as e:
				traceback.print_exc()
				return {'error':e.format_exc()}
		return SUCCESS

	def remove_user_role( self, uname, roleId ):
		# this check should be done by AuthModule too
		with self.conn_mut:
			try:
				c = self.conn.cursor()
				c.execute( '''DELETE FROM userRoles WHERE roleID=? AND uname=?''',(roleId,uname) )
				self.conn.commit()
			except Exception as e:
				traceback.print_exc()
				return {'error':e.format_exc()}
		return SUCCESS

	def delete_db( self ):
		if( self.dbname != ':memory:' ):
			remove( self.dbname )

	def user_list( self ):
		with self.conn_mut:
			c = self.conn.cursor()
			c.execute( ''' SELECT uname,isAdmin,sshkey from users''' )
			res = c.fetchall()
			ret = [dict(r) for r in res]
			for user in ret:
				user['roles'] = self._roles_for_user( user['uname'] )
			return ret

	def role_get( self, roleID ):
		with self.conn_mut:
			c = self.conn.cursor()
			c.execute( '''SELECT * FROM roles
				WHERE roleID = ?''', (roleID,) )
			res = c.fetchone()
			if( res is None ):
				return None
			resd = dict( res )
			resd['applications'] = self._apps_for_role( res['roleID'] )
			resd['sensors'] = self._sensors_for_role( res['roleID'] )
		return resd

	# assumes locked mutex
	def _apps_for_role(self, roleID ):
		apps = self.conn.execute( '''
			SELECT applications.* FROM roleApps
			INNER JOIN applications
			on roleApps.appID = applications.appID
			WHERE roleApps.roleID = ?''', (roleID,))
		return [dict(r) for r in apps]

	def _sensors_for_role(self, roleID ):
		sensors = self.conn.execute( '''
			SELECT sensors.* FROM roleSensors
			INNER JOIN sensors
			on roleSensors.sensorID = sensors.sensorID
			WHERE roleSensors.roleID = ?''', (roleID,))
		return [dict(r) for r in sensors]

	# assumes locked mutex
	def _roles_for_user(self, uname ):
		c = self.conn.cursor()
		roles = c.execute( '''
			SELECT roles.* FROM userRoles
			INNER JOIN roles on roles.roleID = userRoles.roleID
			WHERE userRoles.uname = ?''', (uname,))
		return [dict(r) for r in roles.fetchall()]

	def user_role_list( self, uname ):
		with self.conn_mut:
			return self._roles_for_user( uname )

	'''
		removes all userRoles for a given user... essentially "stripping them of their rights"
	'''
	def user_role_clear_user(self, uname):
		with self.conn_mut:
			c = self.conn.cursor()
			c.execute( '''DELETE FROM userRoles
				WHERE uname=?''', (uname,) )
			self.conn.commit()
	'''
		removes all userRoles for a given role...
	'''
	def user_role_clear_role(self, roleid):
		with self.conn_mut:
			c = self.conn.cursor()
			c.execute( '''DELETE FROM userRoles
				WHERE roleID=?''', (roleID,) )
			self.conn.commit()
	'''
	completely removes a user from the database
	will also remove the corresponding tokens and userroles
	'''
	def user_remove(self, uname):
		with self.conn_mut:
			c = self.conn.cursor()
			#delete any tokens
			c.execute( '''DELETE from userTokens WHERE uname=?''', (uname,) )
			#delete any userRoles
			c.execute( '''DELETE FROM userRoles
				WHERE uname=?''', (uname,) )
			#finally delete user
			c.execute( '''DELETE FROM users
				WHERE uname=?''', (uname,) )
			self.conn.commit()

	'''
	remove an application from the database
	will (probably) error if any roles use the application
	'''
	def application_remove(self, appid):
		with self.conn_mut:
			try:
				c = self.conn.cursor()
				c.execute( '''DELETE FROM applications
					WHERE appID=?''', (appid,) )
				self.conn.commit()
			except sqlite3.IntegrityError as e:
				return {"error":"cannot remove application {}. It is in use by another role"}
		return SUCCESS

	'''
	completely removes a role from the database
	will also remove the userroles
	'''
	def role_remove(self, roleid):
		with self.conn_mut:
			c = self.conn.cursor()
			#delete any userRoles
			c.execute( '''DELETE FROM roleApps
				WHERE roleID=?''', (roleid,) )
			c.execute( '''DELETE FROM userRoles
				WHERE roleID=?''', (roleid,) )
			c.execute( '''DELETE FROM roleSensors
				WHERE roleID=?''', (roleid,) )
			#finally delete role
			c.execute( '''DELETE FROM roles
				WHERE roleID=?''', (roleid,) )
			self.conn.commit()

	'''
	'''
	def role_list( self ):
		with self.conn_mut:
			c = self.conn.cursor()
			c.execute( '''SELECT * FROM roles''' )
			roles = [dict(r) for r in c.fetchall()]
			for r in roles:
				r['applications'] = self._apps_for_role( r['roleID'] )
				r['sensors'] = self._sensors_for_role( r['roleID'] )
		return roles

	def sensor_create( self, sensorID, sensorType, data ):
		with self.conn_mut:
			try:
				self.conn.execute( ''' INSERT INTO sensors values (?,?,?) ''',(sensorID,sensorType,data) )
				self.conn.commit()
			# TODO
			except sqlite3.IntegrityError as e:
				if( e.args[0].startswith( 'UNIQUE' ) ):
					return {'error':'Sensor {} already exists'.format( sensorID )}
				raise
			except Exception as e:
				traceback.print_exc()
				return {'error':e.format_exc()}
		return SUCCESS

	def sensor_get( self, sensorID ):
		with self.conn_mut:
			row = self.conn.execute( ''' SELECT * FROM sensors WHERE sensorID=?''', (sensorID,) )
		ent = row.fetchone()
		if( ent ):
			return dict( ent )
		return None

	def sensor_list( self ):
		with self.conn_mut:
			cursor = self.conn.execute( ''' SELECT * FROM sensors''' )
		return [dict(r) for r in cursor.fetchall()]

	def application_create( self, appID, launchCmd, install, icon, desktop, postInstall, type):
		with self.conn_mut:
			try:
				self.conn.execute( ''' INSERT INTO applications values (?,?,?,?,?,?,?) ''',(appID,launchCmd,install,icon,desktop,postInstall,type) )
				self.conn.commit()
			# TODO
			except sqlite3.IntegrityError as e:
				if( e.args[0].startswith( 'UNIQUE' ) ):
					return {'error':'Application {} already exists'.format( appID )}
				raise
			except Exception as e:
				traceback.print_exc()
				return {'error':e.format_exc()}


	def application_get( self, appID ):
		with self.conn_mut:
			row = self.conn.execute( ''' SELECT * FROM applications WHERE appID=?''', (appID,) )
		ent = row.fetchone()
		if( ent ):
			return dict( ent )
		return None

	def application_list( self ):
		with self.conn_mut:
			cursor = self.conn.execute( ''' SELECT * FROM applications''' )
		return [dict(r) for r in cursor.fetchall()]

	# Sorry about the try-except stuff
	def role_create( self, roleID, apps, mounts, type, ramsize, cpucount, sensors=[]):
		with self.conn_mut:
			c = self.conn.cursor()
			try:
				c.execute( ''' INSERT INTO roles values (?,?,?,?,?,?) ''', (roleID, 'creating', mounts, ramsize, cpucount, type) )
				for a in apps :
					try:
						c.execute( ''' INSERT INTO roleApps VALUES (?,?)''', (roleID,a) )
					except sqlite3.IntegrityError as e:
						# happens when the app or role doesn't exist. But we know the role does
						if( e.args[0].startswith( 'FOREIGN KEY' ) ):
							c.execute( ''' DELETE FROM roles where roleID = ? ''', (roleID,) )
							self.conn.commit()
							return {'error':'app {} does not exist'.format(a)}
						# if roleApp already exists, don't delete the role
						if( e.args[0].startswith( 'UNIQUE' ) ):
							self.conn.commit()
							return {'error':'app {} already exists for this role'.format(a)}
						# dunno
						c.execute( ''' DELETE FROM roles where roleID = ? ''', (roleID,) )
						self.conn.commit()
						raise
					except Exception as e:
						traceback.print_exc()
						c.execute( ''' DELETE FROM roles where roleID = ? ''', (roleID,) )
						self.conn.commit()
						return {'error':traceback.format_exc()}
				for s in sensors:
					try:
						c.execute( ''' INSERT INTO roleSensors VALUES (?,?)''', (roleID,s) )
					except sqlite3.IntegrityError as e:
						traceback.print_exc()
						# Gotta delete roleApps, otherwise deleting roles fails Foreign key
						c.execute( ''' DELETE from roleApps WHERE roleID = ? ''', (roleID,) )
						c.execute( ''' DELETE FROM roles where roleID = ? ''', (roleID,) )
						self.conn.commit()
						if( e.args[0].startswith( 'FOREIGN KEY' ) ):
							return {'error':'sensor {} does not exist'.format(s)}
						raise
					except Exception as e:
						traceback.print_exc()
						c.execute( ''' DELETE from roleApps WHERE roleID = ? ''', (roleID,) )
						c.execute( ''' DELETE FROM roles where roleID = ? ''', (roleID,) )
						self.conn.commit()
						return {'error':traceback.format_exc()}

			except sqlite3.IntegrityError as e:
				if( e.args[0].startswith( 'UNIQUE' ) ):
					return {'error':'Role {} already exists'.format( roleID )}

			except Exception as e:
				traceback.print_exc()
				return {'error':traceback.format_exc()}
		return SUCCESS

	def role_set_status( self, roleID, statusText ):
		with self.conn_mut:
			c = self.conn.cursor()
			c.execute( '''UPDATE roles set status=? where roleID=?''', (statusText, roleID ) )
			self.conn.commit()

def sql_tests( ):
	s = SQLiteBackend( dbname='virtue_test' )
	print( "Starting SQL tests" )
	assert( s.application_list( ) == [] )
	s.application_create( 'test app 1', './run', 'install', 's3://icon.ico', 'test app one', '', "linux")
	s.application_create( 'test app 2', './run', 'install', 's3://icon.ico', 'test app two', '', "linux")
	s.application_create( 'test app 3', './run', 'install', 's3://icon.ico', 'test app three', '', "linux")
	assert( s.application_get( 'test app 1' ) == {'appID':'test app 1','launchCmd':'./run','install':'install','icon':'s3://icon.ico', 'desktop':'test app one', 'postInstall':'', "type":"linux"} )
	assert( {e['appID'] for e in s.application_list()} ==
		{ 'test app 1', 'test app 2','test app 3'} )

	assert( s.sensor_list( ) == [] )
	sensor1 = s.sensor_create( sensorID='testsensor1', sensorType='apparmor', data='junk' )
	assert( s.sensor_list( )[0] == {'sensorID':'testsensor1','sensorType':'apparmor','data':'junk', } )
	sensor2 = s.sensor_create( sensorID='testsensor2', sensorType='apparmor', data='junk' )
	assert( {i['sensorID'] for i in s.sensor_list( )} == {'testsensor1','testsensor2'} )
	sensor3 = s.sensor_create( sensorID='testsensor3', sensorType='apparmor', data='junk' )
	assert( s.sensor_get( 'testsensor1' ) == {'sensorID':'testsensor1','sensorType':'apparmor','data':'junk'} )


	rc = s.role_create(
		roleID='test role 1',
		apps=['test app 1', 'test app 2'],
		mounts="user, persistent",
		type="linux",
		ramsize="1024Mi",
		cpucount=1,
		sensors=['testsensor1','testsensor3'])
	assert( rc == SUCCESS )
	role =  s.role_get( 'test role 1' )
	assert( role )
	assert( len( role['applications'] ) == 2 )
	assert( len( role['sensors'] ) == 2 )

	assert( role['status'] == 'creating' )
	s.role_set_status( 'test role 1', 'ready' )
	role =  s.role_get( 'test role 1' )
	assert( role['status'] == 'ready' )
	assert( role['roleID'] == 'test role 1' )
	assert(role['mounts'] == 'user, persistent')
	assert(role['type'] == 'linux')
	assert(role['ramsize'] == "1024Mi")
	assert(role['cpucount'] == 1)
	assert( { d['appID'] for d in role['applications'] } == {'test app 1','test app 2' } )
	assert( { d['sensorID'] for d in role['sensors'] } == {'testsensor1','testsensor3'} )
	s.role_create( 'test role 2', ['test app 3'], "persistent", "linux", "1024Mi", 2)
	assert( len( s.role_list( ) ) == 2 )
	# role already exists
	assert( 'error' in s.role_create( 'test role 2', ['test app 3'] , "persistent", "linux", "1024Mi", 2) )
	# bad app
	assert( 'error' in s.role_create( 'new role', ['bad app', 'test app 1'] , "persistent", "linux", "1024Mi", 1) )
	assert( len( s.role_list( ) ) == 2 )

	from os import urandom
	s.store_user( "test user 1", salt=urandom(10), passhash=urandom(10), isAdmin=True )
	assert( len( s.user_list() ) == 1 )
	s.store_user( "test user 2", salt=urandom(10), passhash=urandom(10), isAdmin=False )
	ulist = s.user_list()
	assert( len( ulist ) == 2 )
	assert( {u['uname'] for u in ulist} == {'test user 1', 'test user 2'} )
	assert( s.get_user_info( "test user 1" )['sshkey'] is None )
	assert( s.get_user_info( "test user 1" )['isAdmin'] == 1 )
	assert( s.user_role_list( "test user 1" ) == [] )
	s.add_user_role( "test user 1", "test role 1" )
	roles = s.user_role_list( "test user 1" )
	assert( {d['roleID'] for d in roles} == {'test role 1'} )

	# duplicate
	assert( 'error' in s.add_user_role( "test user 1", "test role 1" ) )

	# nonexistant users/roles
	assert( 'error' in s.add_user_role( "bad user", "test role 1" ) )
	assert( 'error' in s.add_user_role( "test user 1", "bad role" ) )

	import datetime
	expiration = (datetime.datetime.now()+datetime.timedelta(hours=1)).timestamp()
	s.add_user_token( "test token", expiration, "test user 1" )
	assert( len( s.tokens_list( ) ) == 1 )
	# fails due to duplicated token
	s.add_user_token( "test token", expiration, "test user 2" )
	assert( len( s.tokens_list( ) ) == 1 )

	# this should fail due to nonexistant user
	s.add_user_token( "blah", expiration, "non user" )
	assert( len( s.tokens_list( ) ) == 1 )

	s.add_user_token( "user2 token", expiration, "test user 2" )
	assert( len( s.tokens_list( ) ) == 2 )

	# new token is added, old one is removed
	s.add_user_token( "updated token", expiration, "test user 1" )
	toklist = s.tokens_list()
	assert( len( toklist ) == 2 )
	assert( "test token" not in (tok['token'] for tok in toklist ) )
	assert( "updated token" in (tok['token'] for tok in toklist ) )
	s.add_user_token( "second user token", expiration, "test user 2" )
	assert( len( s.tokens_list() ) == 2 )
	newtok = s.get_user_token( 'second user token')
	assert( all( (newtok[k] == v for k,v in {'token': 'second user token', 'uname': 'test user 2', 'isAdmin': 0 }.items() ) ) )



	#todo user removal
	#todo role removal
	#todo application removal

	print( "sql tests passed" )




# run at load time
if( global_sqlite_db is None ):
	sql_tests()
	global_sqlite_db = SQLiteBackend( dbname='virtue_system', rebuild=True )

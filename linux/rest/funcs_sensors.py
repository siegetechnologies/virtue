from sqlite_backend import global_sqlite_db
import strutil
import auth

# Currently the only type is apparmor
sensor_types = ['apparmor', 'mavenconf']

'''
# sensors list
## args (json encoded)
  - usertoken: the token of the admin user (required)
  - type: optional type filter
## response
	[
		{
			'sensorID':<id>,
			'sensorType': 'apparmor',
			'data': <data>,
		},
		{...}
	]
  - OR -
  { 'error' : <text> } 

TODO: optional type filters
'''

def sensors_list( args ):
	try:
		usertoken = args['userToken']
		t = args.get( 'type', None )
	except KeyError as e:
		return {'error':'missing argument {}'.format( e.args[0] ) }
	user = auth.global_auth_module.check_token( usertoken )
	if( 'isAdmin' not in user or (not user['isAdmin']) ):
		return {'error': 'this user token does not have permission to list sensors' }
	if( t ):
		if( t not in sensor_types ):
			return {'error': 'invalid sensor type {}'.format( t ) }
	res = global_sqlite_db.sensor_list()
	if( t ):
		res = list( filter( (lambda s:s['sensorType']==t), res ) )
	return res
	# TODO

'''
# sensors add
## args (json encoded)
  - usertoken: the token of the admin user
  - sensorID: unique id of sensor
  - sensorType: type of sensor
  - data: type-dependant data field.
    + for apparmor, this is the profile data
## response
  { 'success': True }
  - OR -
  { 'error' : <text> }

TODO: optional type filters
'''
def sensors_create( args ):
	try:
		usertoken = args['userToken']
		t = args['sensorType']
		sensorid = args['sensorID']
		data = args['data']
	except KeyError as e:
		return {'error':'missing argument {}'.format( e.args[0] ) }
	pass
	user = auth.global_auth_module.check_token( usertoken )
	if( 'isAdmin' not in user or (not user['isAdmin']) ):
		return {'error': 'this user token does not have permission to create sensors' }
	if( t not in sensor_types ):
		return {'error': 'invalid sensor type: {}'.format( t ) }
	return global_sqlite_db.sensor_create( sensorid, t, data)


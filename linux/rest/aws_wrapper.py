import boto3
import itertools
import pprint
import efsshim
from time import sleep
from multiprocessing import Process,active_children
from ntpath import basename
from strutil import genvirtueid,saniefsuser
from traceback import print_exc
from time import time

# allows us to cache the results of some function calls
# particularly aws api stuff that never changes
from functools import lru_cache,partial

ec2 = boto3.client('ec2')
ssm = boto3.client('ssm')
ds = boto3.client('ds')
ps1_template_file = 'ps1_template.ps1'

'''
win_virtue_list
returns the instance id of every instance with a tag of WindowsVirtue
returns a dict in the form
{
	'InstanceId':string,
	'PrivateIpAddress':string,
	'VirtueType':'Windows',
	'VirtueId':string,
	'UserId':string,
	'RoleId':string,
}
caveat:
	only works when the value for WindowsVirtue tag is empty
	if WindowsVirtue tag is present, but VirtueId tag is not, this will error

'''
def win_virtue_list(user='', roleId='', virtueId=''):
	Filters=[
			 #{ 'Name':'tag:WindowsVirtue', 'Values':[''] },
			 { 'Name':'tag-key', 'Values':['VirtueId'] },
			 { 'Name':'instance-state-name',	'Values':['pending','running']},
		]
	if user:
		Filters.append({ 'Name':'tag:UserId', 'Values':[user] })
	if roleId:
		Filters.append({ 'Name':'tag:RoleId', 'Values':[roleId] })
	if virtueId:
		Filters.append( { 'Name':'tag:VirtueId', 'Values':[virtueId] })

	i = None
	# This is to get around a bug in botocore where errors sometimes bubble up to us
	while( True):
		try:
			i = ec2.describe_instances(Filters=Filters)
			break
		except:
			# Just try again
			continue

	if( 'Reservations' not in i ):
		return []
	res = i['Reservations']
	if( res == [] ):
		return []
#	pprint.pprint( res )
#	pprint.pprint( [r['Instances'] for r in res ] )

	return [_amzret_to_wininfo( instance ) for instance in
			itertools.chain.from_iterable( (r['Instances'] for r in res ) )]

'''
terminates a virtue
requires a virtueID, if you also want to verify that the user has access to it, supply a usernm
'''
def win_virtue_destroy(virtueid, usernm = ''):
	Filters=[
			 #{ 'Name':'tag:WindowsVirtue', 'Values':[''] },
			 { 'Name':'tag:VirtueId', 'Values':[virtueid] },
			 { 'Name':'instance-state-name',	'Values':['pending','running', 'shutting-down', 'stopping', 'stopped']},
		]
	if usernm:
		Filters.append({ 'Name':'tag:UserId', 'Values':[usernm] })
	#find all running virtues with that virtueid
	resp = []
	i = ec2.describe_instances(Filters = Filters)
	if( 'Reservations' not in i ):
		return []
	res = i['Reservations']
	if( res == [] ):
		return []
#aight, KILL EM
	for r in res:
		for j in r['Instances']:
			#one last check for virtueid
			vid = _extract_virtueId(j['Tags'])
			if vid != virtueid:
				continue
#todo test for username too? pretty redundant
			print("doin the ol deleted on " + j['InstanceId'])
			resp.append(boto3.resource('ec2').Instance(j['InstanceId']).terminate())
	return resp

def ebs_for_userrole( user, roleId ):
	ebs = ec2.describe_volumes(
		Filters=[
			{
				"Name":"tag:UserId","Values":[user]
			},
			{
				"Name":"tag:RoleId","Values":[roleId]
			},
		]
	)
	vols = ebs['Volumes']


	if( len( vols ) == 0 ):
		return None

	if( len( vols ) == 1 ):
		if( vols[0]['Attachments'] != [] ):
			raise Exception( "ebs volume {} is already attached to an instance".format( vols[0]['VolumeId'] ) )
		print( "Using existing persistance volume {}".format( vols[0]['VolumeId'] ) )
		return vols[0]['VolumeId']

	raise Exception( "Multiple ebs volumes exist for this userRole" )
	# should probably delete them all and just rebuild
	# TODO


'''
Maybe we'll do something here eventually
¯\_(ツ)_/¯
'''
def win_virtue_publish( user, apps, roleId ):
	return None
	'''
	Unreachable code that may be useful for generating ebs volumes
	for roles with persistance

	We actually do this in win_virtue_create lazily the first time the volume is needed
	'''
	ebs = ec2.create_volume(
		AvailabilityZone = az,
		Size = 30,
		TagSpecifications=[
			_dict_to_kvlist( {"user":user,"roleid":roleId} )
		],
	)


test_role = {
	"roleID":"notwincalc",
	"applications":[
		{
			"launchCmd":"C:\\Windows\\System32\\calc.exe"
		}
	],
	"mounts":"persistent"
}
'''
win_virtue_create
user is a string that is the username of someone in the virtue directory service
apps is a list of a list of executable paths
	e.g. ['C:\Windows\System32\calc.exe','C:\Program Files\....\ppt.exe']
VirtueId is a string value that should be significant to the system as a whole
'''
def win_virtue_create( user, roleDict ):
	print( "win_virtue_create!") 
	roleId = roleDict['roleID']
	repoId = roleDict['repo']
	apps = [a['launchCmd'] for a in roleDict['applications']]
	print( "got apps {}".format( apps ) )
	virtueId = genvirtueid()
	print( "trying to start virtueId {}".format( virtueId ) )

	active_children() # reap any zombie processes
	i = ec2.run_instances(
		ImageId=_get_win_ami( repoId ),
		MinCount=1,
		MaxCount=1,
		KeyName="yeet",
		# TODO this prevents us from stopping and restarting an instance
		# if we want to support that in the future for pause/resume
		# functionality this will have to be removed
		InstanceInitiatedShutdownBehavior='terminate',
		BlockDeviceMappings=[
			{
				"DeviceName":"/dev/sda1",
				"Ebs":{
					"DeleteOnTermination":False,
					"VolumeSize":30,
				},
			},
		],
		InstanceType='t2.large',
		#UserData = "<powershell>"+_ps1_template( user, apps)+"</powershell>",
		NetworkInterfaces=[ {
			# TODO turn this off before deploying
			'AssociatePublicIpAddress':False,
			'DeviceIndex': 0,
			'SubnetId':_get_winvirtue_subnet(),
			'Groups':[_get_rdp_security_group()],
			}],
		TagSpecifications=[ {
			'Tags':
				_dict_to_kvlist( {
					"Name":"Win_Virtue_"+virtueId,
					"VirtueId":virtueId,
					"UserId":user,
					"RoleId":roleId,
					"Status":"pending",
					"Available":"False",
				} ),
			'ResourceType':'instance',
		} ],
		# TODO find this dynamically
		IamInstanceProfile={'Name':'AWSSSMRole'},
		UserData=virtueId,
	)
	if( len( i['Instances'] ) != 1 ):
		raise Exception( "Instance did not start" )
	ret = _amzret_to_wininfo( i['Instances'][0] )
	ret['VirtueId'] = virtueId
	p = Process( target=_setup_windows_virtue_wrappy, args=(ret,user,roleDict) )
	p.start()
	return ret


def _setup_windows_virtue_wrappy(wininfo, user, roleDict):
	try:
		_setup_windows_virtue(wininfo, user, roleDict)
	except Exception as e:
		instance_id = wininfo['InstanceId']
		print( "Something went wrong with this windows virtue, killing it dead")
		print_exc( e )
		ec2.terminate_instances(InstanceIds=[instance_id])

def _setup_windows_virtue( wininfo, user, roleDict ):
	# TODO for persistent instances, find/create ebs store and ec2.attach_volume
	# otherwise don't bother
	t0 = time()
	instance_id = wininfo['InstanceId']
	assoc = None
	try:
		assoc = ssm.create_association(
			Name=_ssm_assoc_name(),
			InstanceId=instance_id,
			)
	except:
		print( "SSM assoc name not found, aborting" )
		# TODO kill instance
		raise
	print( assoc )

	print( "Starting windows virtue in instance {}".format( instance_id ) )
	_wait_instance_state( instance_id, 'running' )

	if( 'persistent' in roleDict['mounts'] ):
		print( "Trying to attach persistent volume" )
		ebs_id = ebs_for_userrole( user, roleDict['roleID'] )
		current_drive = _drive_for_instance( instance_id )
		if( ebs_id is None ):
			print( "No existing userRole drive, tagging this one" )
			ec2.create_tags(
				Resources=[current_drive],
				Tags = _dict_to_kvlist( {
					"RoleId":roleDict['roleID'],
					"UserId":user
				} )
			)
		else:
			print( "There is an existing drive, swapping now" )
			# stop instance
			print( "Stopping instance" )
			ec2.stop_instances( InstanceIds=[instance_id])
			_wait_instance_state( instance_id, 'stopped' )
			print( "Instance stopped" )
			# detach root volume
			print( "Detaching starter drive" )
			ec2.detach_volume( VolumeId=current_drive )

			sleep( 60 ) # TODO not this

			print( "drive detatched" ) #probably
			# attach ebs
			print( "attaching UserRole volume" )

			# this will fail if the instance still has a drive attached, which it shoudn't
			ec2.attach_volume( Device="/dev/sda1", VolumeId=ebs_id, InstanceId=instance_id )
			print( "Restarting instance" )
			ec2.start_instances( InstanceIds=[instance_id] )

			# delete the starter drive
			print( "Deleting starter volume" )
			ec2.delete_volume( VolumeId=current_drive )

	print( "Instance is awake, waiting for running" )
	_wait_instance_state( instance_id, 'running' )
	apps = roleDict['applications']
	launches = [a['launchCmd'] for a in apps]
	appids = [a['appID'] for a in apps]
	print( roleDict )

	choco_targets = [ a['install'] for a in roleDict['applications']]
	templated = partial( _file_templating, {
		"__USER":user,
		"__APPS":_gen_ps_appstring( launches ),
		"__CHOCO_TARGETS":_gen_ps_chocostring( choco_targets ) , 
		"__CONFIGLETS":_gen_configlets( appids ),
		} )
	commands = [
		templated( "powershell/filterIdle.ps1" ),
		templated( "powershell/chocolatey.ps1" ),
		templated( "powershell/remote_setup.ps1" ),
	]
	if( 'user' in roleDict['mounts'] ):
		efsn = saniefsuser( user )
		userk, userpass = efsshim.createshim( efsn )
		commands.append( "choco install sshfs -y --force" )
		commands.append( '''net use \\\\sshfs\\{}={}@10.1.1.101!2222 "{}"'''.format( user, efsn, userpass ) )
	commands.append( templated( "powershell/remote_user_add.ps1" ) )
	commands.append( templated( "powershell/ffmerge.ps1" ) )

	print( "Sending commands to {}".format( instance_id ) )
	cmd = None
	for i in range( 0, 10 ):
		try:
			cmd = ssm.send_command(
				InstanceIds=[instance_id],
				DocumentName="AWS-RunPowerShellScript",
				OutputS3BucketName="siege-apl-ssm",
				Parameters={
					# commands should be dynamic based on, for example
					# - whether ebs needs to be added as D
					# - whether ebs needs to be formatted
					"commands":commands
				}
			)
			break
		except Exception as e:
			# This is probably just because the node isn't up yet
			print( "Instance {} is not accepting commands yet. Attempt {}/10".format( instance_id, i ) )
			sleep( 60 )
	else:
		raise Exception( "Instance is not ready after 10 minutes. Something has gone wrong" )
	cmd_id = cmd['Command']['CommandId']
	print( "Commands accepted by {}, waiting for them to complete".format( instance_id ) )
	print( cmd )
	while( True ):
		cmd_status = _get_command_status( cmd_id )
		if( cmd_status in ['Pending', 'InProgress'] ):
			sleep( 15 )
		elif( cmd_status == 'Success' ):
			print( "Commands completed successfully for {}".format( instance_id ) )
			break
		else: # One of: Cancelled, Failed, TimedOut, or Cancelling
			raise Exception( "Commands on instance {} in failure status {}".format( instance_id, cmd_status ) )

	print( "Virtue {} is now available".format( wininfo['VirtueId'] ) )
	print( "Total time was {}".format( time() - t0 ) )
	# Done
	ec2.create_tags(
		Resources=[instance_id],
		Tags=_dict_to_kvlist( {"Status":"running","Available":"True"} )
	)


def _extract_virtueId( taglist ):
	f = list( filter( (lambda tag: tag['Key'] == 'VirtueId'), taglist ) )
	if( len( f ) == 1 ):
		return f[0]['Value']

	raise Exception('No VirtueId found in taglist')

def _extract_it( taglist, it ):
	f = list( filter( (lambda tag: tag['Key'] == it), taglist ) )
	if( len( f ) == 1 ):
		return f[0]['Value']

	raise Exception('No ' + it + ' found in taglist')



def _amzret_to_wininfo( d ):
	try:
		return {
			'InstanceId':d['InstanceId'],
			'PrivateIpAddress':d['PrivateIpAddress'],
			'VirtueType':'Windows',
			'VirtueId':_extract_virtueId( d['Tags'] ),
			'RoleId':_extract_it( d['Tags'], 'RoleId' ),
			'UserId':_extract_it( d['Tags'], 'UserId' ),
			'Status':_extract_it( d['Tags'], 'Status' ),
			'Available':_extract_it( d['Tags'], 'Available' ),
			}
	except:
		return {}

def _dict_to_kvlist( d ):
	return [ {"Key":k,"Value":v} for (k,v) in d.items() ]

@lru_cache(maxsize=1)
def _get_win_ami( repoId ):
	imgs = ec2.describe_images(
		Owners=['self'],
		Filters=[
			{
				'Name':'tag:virtueRepo',
				'Values':[repoId],
			}
		]
	)['Images']
	if( len( imgs ) == 0 ):
		print( "Role specific ami not found, using default. This may cause stuff to not work" )
		#return "ami-07a29e78aeb420471" #2016 base
		return "ami-0248e06237a315396"
#todo code to automatically find a mavenwindows ami id?
	elif( len( imgs ) > 1 ):
		print( "Multiple amis found with a specific role. We'll use the first, but this is bad")

	return imgs[0]['ImageId']


@lru_cache(maxsize=1)
def _get_rdp_security_group():
	groups = ec2.describe_security_groups(
		Filters=[ {
				"Name":"tag:Name",
				"Values":["Win_Virtue_sg"]}
			] )
	sec_grps = groups.get( 'SecurityGroups', [] )
	if( len( sec_grps ) < 1 ):
		raise Exception( 'No Security group with the name we are looking for: "Windows Virtue RDP"')
	return sec_grps[0]["GroupId"]
	# TODO check vpc
	# TODOTODO replace by creating a special security group that
	#   only accepts connections from that user's workspace


def _name_from_path( path ):
	return basename( path ).split('.')[0]

def _gen_ps_appstring( apps ):
	return ",".join( ('("{}","{}")'.format(_name_from_path(path),path) for path in apps) )

# TODO wrape thes in quotes
def _gen_ps_chocostring( apps ):
	installs = [ '"{}"'.format(i) for i in apps if i ]
	# if there are any choco apps, put them in a list. Otherwise return a powershell style empty array
	return ",".join( installs ) if installs else "@()"

def _gen_configlets( apps ):
	configlets =  " ".join( [ "C:\\maven\\config\\{}.txt".format( a ) for a in apps ] )
	return configlets

def _file_templating( subs, ftemplate ):
	with open( ftemplate ) as f:
		text = f.read()
		for k,v in subs.items():
			text = text.replace( k, v )
	print( ftemplate + ": -> " )
	print( text )
	print( "\n" )
	return text

@lru_cache(maxsize=1)
def _ssm_assoc_name():
	dirid, dirname = _find_siege_directory()
	# TODO verify this exists
	docname = "awsconfig_Domain_" + dirid + "_" + dirname
	try:
		ssm.describe_document( Name=docname )
	except:
		print( "Document not found!" )
		raise
	return docname

@lru_cache(maxsize=1)
def _find_siege_directory( ):
	# TODO
	dirdescs = ds.describe_directories()['DirectoryDescriptions']
	if( len( dirdescs ) == 0  ):
		raise Exception( "No directory exists, is making one a TODO?" )
	if( len( dirdescs ) == 1 ):
		return (dirdescs[0]['DirectoryId'],dirdescs[0]['Name'])

	matchdir = next( ( dom for dom in dirdescs if dom['Name'] == "siege.virtue.com" ), None )
	if( matchdir is None ):
		# Hope this is good enough
		return (dirdescs[0]['DirectoryId'],dirdescs[0]['Name'])

	return (matchdir['DirectoryId'],matchdir['Name'] )


@lru_cache(maxsize=1)
def _find_workspace_subnet():
	subnets = ec2.describe_subnets(
		# TODO make this better?
		Filters=[
			{"Name":"tag:Name","Values":["Win_Virtue Private"]}
		]
	)
	assert( len( subnets['Subnets'] ) == 1 )
	return subnets['Subnets'][0]

@lru_cache(maxsize=1)
def _get_winvirtue_az( ):
	return _find_workspace_subnet()['AvailabilityZone']


@lru_cache(maxsize=1)
def _get_winvirtue_subnet():
	return _find_workspace_subnet()['SubnetId']

def _drive_for_instance( instance_id ):
	drives = ec2.describe_volumes(
		Filters=[
			{
				"Name":"attachment.instance-id",
				"Values":[instance_id]
			}
		]
	)
	# not a given, but will be true for everything we call
	print( drives )
	assert( len( drives['Volumes'] ) == 1 )
	return drives['Volumes'][0]['VolumeId']

def _wait_instance_state( instance_id, state, poll_seconds=60, timeout_seconds=600 ):
	if( state not in ['running', 'stopped', 'stopping', 'pending'] ):
		raise Exception( "brah" )
	for i in range( 0, timeout_seconds // poll_seconds ):
		try:
			i = ec2.describe_instances(
				Filters=[
				{"Name":"instance-id","Values":[instance_id]}
				])['Reservations']
			if( len( i ) < 1 ):
				raise Exception( "Instance {} not found".format( instance_id ) )
			if( len( i ) > 1 ):
				raise Exception( "This should never happen. Two instances with same id???" )
			i1 = i[0]['Instances'][0]
			if( i1['State']['Name'] == state ):
				break
		except:
			print( "Got botocore exception. We'll just assume it isn't ready" )
			print_exc()
		sleep( poll_seconds )
	else:
		raise Exception( "Instance {} not {} after {} seconds".format( instance_id, state, timeout_seconds ) )

# returns one of:
# 'Pending'|'InProgress'|'Success'|'Cancelled'|'Failed'|'TimedOut'|'Cancelling'
# Or raises an exception if the command is not found
def _get_command_status( cmd_id ):
	cmds = ssm.list_commands( CommandId=cmd_id )['Commands']
	if( len( cmds ) == 0 ):
		raise Exception( "Unknown command id {}".format( cmd_id ) )
	assert( len( cmds ) == 1 )
	return cmds[0]['Status']


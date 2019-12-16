from pprint import pprint
from traceback import print_exc
import boto3
from functools import lru_cache,partial


@lru_cache(maxsize=1)
def grabAccountNum():
	return boto3.client('sts').get_caller_identity()['Account']



#deprecated
#todo make this invalidate after x minutes?
#@lru_cache(maxsize=5)
def grabWorkspaceIp(username):
#api ref offline at time of writing this
	#im gonna write some guesses
	cl = boto3.client('workspaces')
	res = (cl.describe_workspaces())
	pprint(res)
	try:
		for bb in res['Workspaces']:
			try:
				uname = bb['UserName']
				ip = bb['IpAddress'] #this ip is the 10.whatever one... maybe we dont want 10.
				if username == uname and bb['State'] == 'AVAILABLE' and ip:
					return ip
			except:
				pass
	except:
		pass
	return "workspacenotfound"




@lru_cache(maxsize=1)
def grabVirtueBucket():
	cl = boto3.client('s3')
	res = (cl.list_buckets())
	virtuebucket = ''
	for bb in res['Buckets']:
		bname = bb['Name']
#		print(bname)
		try:
			btags = (cl.get_bucket_tagging(Bucket=bname))
#			pprint.pprint(btags)
			##grab the type
			for j in btags['TagSet']:
				if j['Key'] == 'type' and j['Value'].lower() == 'virtuevms':
					virtuebucket = bname
					return bname
		except Exception as e:
#			pprint.pprint (e)
			pass
	if not virtuebucket:
		print("warning/error.... virtue bucket not found?")

	return virtuebucket



#todo be able to define a name
@lru_cache(maxsize=1)
def grabWSDirectoryId():
	cl = boto3.client( 'workspaces' )
	dirs = cl.describe_workspace_directories()['Directories']
	if( len(dirs) == 0 ):
		raise "No workspaces present. You need these"
	if( len(dirs) != 1 ):
		print("More than one workspace directory present. Using the first one, but there may be problems")
		dirs = dirs[0:1]
	return dirs[0]['DirectoryId']

def usersWorkspaceIP( username ):
	cl = boto3.client( 'workspaces' )
	dirs = cl.describe_workspaces( DirectoryId=grabWSDirectoryId(), UserName=username )['Workspaces']
	activedirs = [ d for k in dirs if k['State'] == "AVAILABLE" ]
	if( len( active ) == 0 ):
		raise("User not found")

	return activedirs[0]['IpAddress'] #10.whatever ip address

#just forces a cache, and verifies that they work
grabAccountNum()
grabVirtueBucket()
grabWSDirectoryId()

import hashlib
import re
import string
import urllib.parse
import uuid
def genvirtueid():
	return 'v'+ uuid.uuid4().hex + 'v'

def genbakeid():
	return 'b'+ uuid.uuid4().hex + 'b'


def saniuser(name):
	#cant go directly with sha512 because the max length a volume name can be is 63 chars
	return hashlib.sha1(hashlib.sha512(name.encode('utf-8')).digest()).hexdigest().lower()

def saniefsuser(name):		#can only be 32 chars long
	return hashlib.sha512(name.encode('utf-8')).hexdigest().lower()[:32]
def saniefsurd(name, role):
	#throw some nasties in htere so someone cant malisiously cross the user/role name boundry
	return saniefsuser(name + '#@!/%^&*$@$' + role)

def saniurd(name, role):
	#throw some nasties in htere so someone cant malisiously cross the user/role name boundry
	return saniuser(name + '#@!/%^&*$@$' + role)


alphanum = string.ascii_letters + string.digits

useridwhitelist = alphanum + '.' + '_' + '-'
def useridwhite(inps):
	if not inps:
		return False
	if inps[0] not in alphanum or inps[-1] not in alphanum:
		return False
	return all (c in useridwhitelist for c in inps )



#todo double check this one... uppercase?
roleidwhitelist = alphanum + '.' + '_' + '-'
def roleidwhite(inps):
	if not inps:
		return False
	#so someone cant duplicate the efsnfs
	if inps == 'efsnfsproxy':
		return False
	if inps[0] not in alphanum or inps[-1] not in alphanum:
		return False
	return all (c in roleidwhitelist for c in inps )

#todo double check this one... uppercase?
applicationidwhitelist = alphanum + '.' + '_' + '-'
def applicationidwhite(inps):
	if not inps:
		return False
	if inps[0] not in alphanum or inps[-1] not in alphanum:
		return False
	return all (c in applicationidwhitelist for c in inps )


assert(applicationidwhite('urxvt'))
assert(applicationidwhite('urxvt-advanced'))
installwhitelist = alphanum + '.' + '-' + ' '
def installwhite( inps ):
	if not inps:
#		return False
		return True	#empty installs
	if( inps[0] not in alphanum ):
		return False
	return all (c in installwhitelist for c in inps )
assert( installwhite( 'python3-pip' ) )
assert( installwhite( '0ad' ) )
assert( installwhite( 'linux-modules-extra-4.15.0-36-generic' ) )

def iconwhite( inps ):
	# TODO: check against urllib for valid s3 address
	# s3 bucket may be hardcoded
	# something like:
	# urllib.parse.urlparse( inps ) :: string -> (scheme,netloc,path,params,query,fragment)
	# in try-catch?
	# scheme is s3
	# netloc is our bucket
	# path is whatever
	return True

virtueidwhitelist = string.ascii_lowercase + string.digits
def virtueidwhite(inps):
	if not inps:
		return False
	if inps[0] not in string.ascii_lowercase or inps[-1] not in string.ascii_lowercase:
		return False
	return all (c in virtueidwhitelist for c in inps )



efsshimwhitelist = string.ascii_lowercase + string.digits
def efsshimwhite(inps):
	if not inps:
		return False
	if len(inps) > 32:	#arbitrary linux best practice?
		return False
	return all (c in virtueidwhitelist for c in inps )



whitelist = string.ascii_letters + string.digits + '.' + '_'
def iswhitelist(inps):
	return all( c in whitelist for c in inps )
#p = re.compile('[a-zA-Z0-9._\-]*')
#def issanitized(string):
#	return p.match(string)

#print(str(issanitized('ye-\et')))

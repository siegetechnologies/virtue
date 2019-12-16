import urllib.request, urllib.parse
import pprint
import json
import sys
import time
import os
import uuid
import traceback
import subprocess #for ssh-keygen
import base64
import hashlib

from pathlib import Path
#for ignoring invalid certs

#https://stackoverflow.com/questions/27835619/urllib-and-ssl-certificate-verify-failed-error
import ssl
gcontext = ssl.SSLContext(ssl.PROTOCOL_TLSv1)  # Only for gangstars

url = 'https://cc.prod.virtue.com:4443'



import pc


def api(path, args):
	try:
		data = json.dumps(args).encode()
		res = urllib.request.urlopen(url + path, data, context=gcontext)
		return res.read().decode('utf-8')
	except:
		pc.pc('Error when accessing api at ' + url + path + ' are you sure it is available?', pc.FAIL)
		raise

def getit(path):
	try:
		res = urllib.request.urlopen(url + path, context=gcontext)
		return res.read()
	except:
		pc.pc('Unable to download ' + path + ' are you sure it is available?', pc.FAIL)
		raise



virtueconf = {}
virtueconf['userToken'] = ''

def known_hosts(virts):
	pc.pc("Adjusting known hosts")
	#open the known_hosts file
	#look through each line
	#if the line matches the host:port in the list
		#if has a virtueid comment:
			#if the virtueid matches:
				#ignore
			#else
				#remove line, mark file to write back out
		#else
			#add the virtueid onto the end, mark file to write back out
	#if marked to write back out
		#write it ya doofus

	hostsfile = str(Path.home()) + "/ssh/known_hosts" #yes it is supposed to be /ssh/... blame x2go'
	j = None
	with open(hostsfile) as f:
		j = f.readlines()
	if not j:
		return


	birts = {}
	#create a badlist of hosts out of virts
	for k,v in virts.items():
		hoststring = '[' + v['host'] + ']:' +  str(v['port'])
		birts[hoststring] = k


	newlist = []
	needswrite = False
	for l in j:
		pl = l.split(None, 3)
		if len(pl) < 3:
			continue
		host = pl[0]
		if host in birts and birts[host]:	# we have a match
			if len(pl) > 3 and pl[3]:
				comment = pl[3].strip()	#strip required for newline
				if comment != birts[host]:
					needswrite = True
					pc.pc("Found a duplicate for " + host  + " old:new " + comment + ":" + birts[host])
					continue
					#we have a bad guy!, dont emit
				#otherwise, no mods
			else:
				pc.pc("Found a host without a virtue... " + host + " " + birts[host])
				needswrite = True
				l = l.strip() + ' ' + birts[host] + '\n'
		newlist.append(l)


	#todo use f.writelines?
	if needswrite:
		pc.pc("Writing known hosts")
		with open(hostsfile, 'w') as f:
			for l in newlist:
				f.write(l)

def changepw(userToken = "", newpassword = ""):
	global virtueconf
	if not userToken:
		userToken = virtueconf['userToken']
	if not newpassword:
		newpassword = 'pw' + str(uuid.uuid4()) + 'WP'
	pc.pc('Changing PW to ' + newpassword)
	pwch = json.loads(api('/user/password', {"userToken":userToken, "password":newpassword}))
	pprint.pprint(pwch)

	if 'error' not in pwch:
		virtueconf['password'] = newpassword
		#invalidate the usertoken (the rest server did, so why bother keeping it around over here)
		virtueconf['userToken'] = ''
		return True
	return False

#TODO some sort of locks?
def login(username = "", password = ""):
	global virtueconf
	if not username:
		username = virtueconf['username']
	if not password:
		password = virtueconf['password']
	pc.pc('Logging in as user ' + username)
	lres = json.loads(api('/user/login', {"username":username, "password":password}))
	pprint.pprint(lres)

	if 'userToken' in lres and lres['userToken']:
		virtueconf['userToken'] = lres['userToken']
		pc.pc("Logged in successfully")
		if password == 'virtuerestdefaultpw':
			#change password
			changepw()
			#re-login with new password
			login()
	else:
		pc.pc("Error logging in... are you sure the credentials are right?", pc.FAIL)
		if password != 'virtuerestdefaultpw':
			pc.pc("Trying again with defaults.... Maybe it was reset?")
			login(password='virtuerestdefaultpw')




#will upload the private key to the server
# if pubkey isnt known, tries to load it
# will also try to generate a pubkey if it doesnt exist in ~/.ssh/id_rsa.pub
# if the uploading fails, will try the whole process again
# will also call login before trying again if the error is that the user isnt logged in
def keyupload():
	pubkey,privkey = readkeys()
	if 'userToken' not in virtueconf or not virtueconf['userToken']:
		login()
	pc.pc('uploading pubkey')
	# Try this twice, since we might need to re-login and try again
	for i in range( 0, 2 ):
		try:
			keyboy = json.loads(api('/user/key', {"userToken":virtueconf['userToken'], "sshkey":pubkey}))
			pprint.pprint(keyboy)
			if keyboy and 'error' in keyboy:
				if 'userToken' in keyboy['error']: #usertoken is expired? lets log in and try again
					login()
					continue
			break
		except:
			traceback.print_exc()
			pc.pc('Issue uploading pubkey to server', pc.WARNING)
			continue
	else: # No break
		return False
	return True

'''
Returns (public,private) keys as a tuple
'''
def readkeys():
	sshpath = os.path.join( Path.home(), ".ssh" )
	privkeypath = os.path.join( sshpath, "id_rsa" )
	pubkeypath = os.path.join( sshpath, "id_rsa.pub" )
	tup = _readkeys_helper( pubkeypath, privkeypath )
	if( tup ):
		return tup

	# else, key does not exist
	os.makedirs(sshpath, exist_ok=True)
	pc.pc('Generating ssh keypair')
	#https://unix.stackexchange.com/questions/69314/automated-ssh-keygen-without-passphrase-how
	p = subprocess.Popen(['C:/Program Files (x86)/x2goclient/ssh-keygen.exe', '-q', '-N',"",'-f', privkeypath])
	p.wait(timeout=10)
	return _readkeys_helper( pubkeypath, privkeypath )

def _readkeys_helper( pubkeypath, privkeypath ):
	if( os.path.exists( pubkeypath ) and os.path.exists( privkeypath ) ):
		return (
			open( pubkeypath ).read(),
			open( privkeypath).read(),
		)
	else:
		return None


'''
returns sha512( sha512( ... sha512( id_rsa ) + vals[0] ) + vals[1] ) .. )
will be in binary format, you might want to base64 encode or something
if vals is empty, returns b'' (empty bytestring)
'''
def sshhashchain( vals ):
	pubkey,privkey = readkeys()
	return _hashchain( privkey, vals )

def _hashchain( val1, vals ):
	if (not vals):
		raise Exception( "sshhashchain got empty list" )
	currentdigest = _ezhash( val1.encode('utf-8') )
	for v in vals:
		currentdigest += v.encode('utf-8')
		currentdigest = _ezhash( currentdigest )
	return currentdigest

def _ezhash( bytestr ):
	s = hashlib.sha512()
	s.update( bytestr )
	return s.digest()



#will attempt to list virtues.. If it errors with a usertoken error, will login and try again (for a few times)
def listvirtues():
	pc.pc('Grabbing my virtues ')
	if 'userToken' not in virtueconf or not virtueconf['userToken']:
		login()

	for i in range(0, 1):
		try:
			virtues = json.loads(api('/virtue/list', {"userToken": virtueconf['userToken']}))
			if 'error' in virtues:
				if 'userToken' in virtues['error']: #usertoken is expired? lets log in and try again
					login()
				continue
		except:
			traceback.print_exc()
			pc.pc("Error grabbing virtues.", pc.WARNING)
			continue
		return virtues
	return None

def listroles():
	pc.pc('Grabbing my roles ')
	if 'userToken' not in virtueconf or not virtueconf['userToken']:
		login()
#set to only 1 for reducing spam
	for i in range(0, 1):
		try:
			roles = json.loads(api('/user/role/list', {"userToken": virtueconf['userToken']}))
			if 'error' in roles:
				if 'userToken' in roles['error']: #usertoken is expired? lets log in and try again
					login()
				continue
		except:
			traceback.print_exc()
			pc.pc("Error grabbing roles.", pc.WARNING)
			continue
		return roles
	return None

def virtuecreate(roleid):
	pc.pc('creating virtue for role ' + roleid)
	if 'userToken' not in virtueconf or not virtueconf['userToken']:
		login()
#set to 3 for login, key upload, and since this is manually done
	for i in range(0, 3):
		try:
			v = json.loads(api('/virtue/create', {
				"userToken": virtueconf['userToken'],
				"roleID":roleid,
				"windows_key_hack": base64.b64encode( sshhashchain( [roleid] ) ).decode('ascii') }))
			if 'error' in v:
				if 'userToken' in v['error']: #usertoken is expired? lets log in and try again
					login()
				if 'ssh key' in v['error']: #usertoken is expired? lets log in and try again
					keyupload()
				continue
		except:
			pc.pc( "Error virtue create" )
			pc.pc( traceback.format_exc() )
			pc.pc("Error creating virtue", pc.WARNING)
			continue
		return v
	return None

def virtuedestroy(virtueid):
	pc.pc('destroying virtue ' + virtueid)
	if 'userToken' not in virtueconf or not virtueconf['userToken']:
		login()

	for i in range(0, 2):
		try:
			v = json.loads(api('/virtue/destroy', {"userToken": virtueconf['userToken'], "virtueID":virtueid}))
			if 'error' in v:
				if 'userToken' in v['error']: #usertoken is expired? lets log in and try again
					login()
				continue
		except:
			traceback.print_exc()
			pc.pc("Error destroying virtue", pc.WARNING)
			continue
		return v
	return None

def virtuedestroyall():
	pc.pc('destroying all virtue')
	if 'userToken' not in virtueconf or not virtueconf['userToken']:
		login()

	for i in range(0, 2):
		try:
			v = json.loads(api('/virtue/destroyall', {"userToken": virtueconf['userToken']}))
			if 'error' in v:
				if 'userToken' in v['error']: #usertoken is expired? lets log in and try again
					login()
				continue
		except:
			traceback.print_exc()
			pc.pc("Error destroying virtues?", pc.WARNING)
			continue
		return v
	return None

def virtuecreateall():
	pc.pc('creating all virtue')
	if 'userToken' not in virtueconf or not virtueconf['userToken']:
		login()

	for i in range(0, 2):
		try:
			v = json.loads(api('/virtue/createall', {"userToken": virtueconf['userToken']}))
			if 'error' in v:
				if 'userToken' in v['error']: #usertoken is expired? lets log in and try again
					login()
				if 'ssh key' in v['error']: #usertoken is expired? lets log in and try again
					keyupload()
				continue
			for a in v:
				if 'error' in a and 'ssh key' in a['error']: #usertoken is expired? lets log in and try again
					keyupload()
					raise	#hacky bullstuff because im twoo looops in
		except:
			traceback.print_exc()
			pc.pc("Error creating virtues?", pc.WARNING)
			continue
		return v
	return None


def loadconf(path = ""):
	global virtueconf
	if not path:
		path = str(Path.home()) + '/.virtue.json'
	pc.pc('Loading virtue config from ' + path)
	try:
		with open(path) as f:
			virtueconf = json.load(f)
	except:
		pass

def saveconf(path = ""):
	if not path:
		path = str(Path.home()) + '/.virtue.json'
	pc.pc('Saving virtue config to ' + path)
	try:
		with open(path, 'w') as f:
			json.dump(virtueconf, f, indent=4, sort_keys=True)
	except:
		pass



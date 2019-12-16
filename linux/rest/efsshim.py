#!/usr/bin/python3
import json

import pprint
import strutil
import subprocess
import os
import traceback
import hashlib

import base64

import paramiko	#for ssh stuffs


shimserver = '10.1.1.101'
shimport = 2222
shimuser = 'efsshim'
def createshim(name):
	if not strutil.efsshimwhite(name):
		print('Invalid shim name ' + name)
		return None
	name = name.strip()

	if name == shimuser:
		print('Invalid shim name ' + name)
		print("DON'T TRY ME, FOOL!")
		return None

	try:
		#generate ssh key if not already there
		st = 'storage/'+name+'/'
		print('Mkdiring ' + st + ' if it doesnt already exist')
		os.makedirs(st, exist_ok=True)
		keypath = st + 'id_rsa'
		if not os.path.exists(keypath):
			print('Generating ssh keypair')
			p = subprocess.Popen(['ssh-keygen', '-q' , '-N', "", '-f', keypath])
			p.wait(timeout=10)
	except:
		traceback.print_exc()
		print('Shim key gen failed?')
		return None
	try:
		print('loading pub key')
		with open(keypath + '.pub') as f:
			pubkey = f.read()
		if not pubkey:
			raise
	except:
		traceback.print_exc()
		print('Shim pubkey load failed')
	try:
		print('loading priv key')
		with open(keypath, mode='rb') as f:
			privkey = base64.b64encode(f.read()).decode( 'utf-8' ).strip()
		if not privkey:
			raise
	except:
		traceback.print_exc()
		print('Shim privkey load failed')
		return None
	#should have a pubkey and a privkey now
	#lets call upon the shim server to setup a user
	password = base64.b64encode( hashlib.sha256( privkey.encode('utf-8') ).digest() ).decode( 'utf-8' ).strip()
	try:
		cmd = 'sudo /root/create-user.sh -u "{}" -k "{}" -p "{}"'.format( name, pubkey, password )
		ssh = paramiko.SSHClient()
		ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
		ssh.connect(shimserver, port=shimport, username=shimuser, timeout=5)
		print('Exucuting shim command ' + cmd)
		ssh.exec_command(cmd)
	except:
		traceback.print_exc()
		print('Error executing shim command? Im going to return gracefully anyway')

#at this point shim should be done
	return privkey,password

def main():
	pprint.pprint(createshim('tester'))

if (__name__ == "__main__"):
	main()

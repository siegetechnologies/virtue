import subprocess
import json
import multiprocessing
from multiprocessing import Process
from multiprocessing import Lock

import os.path #for basevm check

from threading import Thread

import strutil
import poolb

import sqlite_backend
import aws_utils
#todo actually queue this instead of just spamming them all at once?

from time import sleep

from pprint import pprint


from traceback import print_exc

import tempfile


nbdlock = Lock()

def queue_role_creation(role):
	aws_utils.grabAccountNum()
	#just starts up a thread
	#multiprocess has some issues with the sqlite DB
#	t = Process(target=runRoleCreate, args = (role,))
	t = Thread(target=runRoleCreate, args = (role,))
	t.start()
	multiprocessing.active_children()
'''
def queue_efs_creation():
	aws_utils.grabAccountNum()
	#just starts up a thread
	#multiprocess has some issues with the sqlite DB
#	t = Process(target=runRoleCreate, args = (role,))
	t = Thread(target=runEfsCreate)
	t.start()
	multiprocessing.active_children()
'''

dockerdir = './docker'
builddir = './dockerbuild'
#vmdir = './vmbuild'
vmdir = './vmcus'
vmorigdir = './vmorig'
vmorigfname = 'virtuevm.qcow2'
vmorig = vmorigdir + '/' + vmorigfname

virtueimagesdir = '/tmp/virtueimages'
virtuevmsdir = '/tmp/virtuevms'

dockfile = ''
def readDockFile():
	global dockfile
	if not dockfile:
#todo move to a .template
		with open(dockerdir + '/' + 'Dockerfile') as f:
			dockfile = f.read()
	return dockfile
'''
def readEfsFile():
	with open(dockerdir + '/' + 'efsfile') as f:
		efsfile = f.read()
	return efsfile
'''


#verifies that the orig image is there
def getBaseVm():
	with nbdlock:
		if os.path.isfile(vmorig): #needs to be inside this check to avoid re-generating this multiple times (example, a bunch of roles are created at the start)
			return vmorig

		vmtemp = vmorig + '.temp'
		vmtempfname = vmorigfname + '.temp'
		try:
			os.remove(vmtemp)
		except:
			pass
		print('Generating base virtuevm image in ' + vmtemp)
		#might be able to get this paralellizable up to 16x, but for now we are just going to lock on it
		deppy = subprocess.run('for f in /dev/nbd*; do sudo qemu-nbd -d ${f} ; done', shell=True);
		#todo auto figure out vm size based on size of virtueimage???????
		#todo detect errors with this?
		vmcreat = subprocess.run('cd ' + vmorigdir + ' && chmod +x ./alpine-make-vm-image && chmod +x ./alpineconfigure.sh && sudo ./alpine-make-vm-image --image-format qcow2 --image-size 10G -b v3.9 --packages "$(cat alpinepackages)" -c ' + vmtempfname + ' -- ./alpineconfigure.sh', shell=True, check=True);
		#shink image and also finalize it
		vmconvert = subprocess.run(['qemu-img', 'convert', '-O', 'qcow2', vmtemp, vmorig], check=True)
		tsize = os.path.getsize(vmtemp)
		fsize = os.path.getsize(vmorig)
		print("\ttemp size: " + str(tsize >> 20) + "MB\n\tfinal size: " + str(fsize >> 20) + "MB\n\tdelta size: " + str((tsize - fsize)>>10) + "KB\n")
		try:
			os.remove(vmtemp)
		except:
			pass

		return vmorig


def roleBake(role):
	roleID = role['roleID']
	virtuevmfile = roleID +'.qcow2'
	srcimg = 'https://s3.amazonaws.com/' + aws_utils.grabVirtueBucket() + '/' + virtuevmfile
	dstimg = srcimg + '_baked'

	sqlite_backend.global_sqlite_db.role_set_status(role['roleID'], 'baking')

	bakeid = strutil.genbakeid()

	poolb.poolbake(bakeid,
		srcimg, dstimg,
		{"bakeID": bakeid},
		'VIRTUEID=%s\n' % "BAKE")

	while True:
		sleep(10)
		try:
			bb = poolb.bakelist(bakeid = bakeid)
			pprint(bb)
			if len(bb) > 0 and bb[0].get('state') == 'completed':
				break
		except:
			print_exc()
	#ok its baked, lets remove it
	poolb.bakedestroy(bakeid)
		#todo errors


#this will deprecate runRoleCreate
#this whole thing needs to be wrapped in a try-except
def runRoleCreatePython(role):
	registry = aws_utils.grabAccountNum() + '.dkr.ecr.us-east-1.amazonaws.com'
	roleID = role['roleID']
	#todo this should be maketmp
##	repodir = repodir + '/' + roleID
	repodir = tempfile.mkdtemp()

	virtueimage = virtueimagesdir + '/' + roleID + '.tar'
	virtuevmfile = roleID +'.qcow2'
	virtuevm = virtuevmsdir + '/' + virtuevmfile


	try:
		lp = subprocess.run('aws ecr get-login --no-include-email | bash', shell=True, check=True)
		# create the repo. it will error if the repo already exists, we dont care
		cr = subprocess.run(['aws', 'ecr', 'create-repository', '--repository-name', roleID])
		#sync it up?
		cr = subprocess.run(['rsync', '--no-relative', '-r', dockerdir + '/', repodir], check=True)
		#import dockerfile
		mydockfile = readDockFile()
		#generate install strings
		appstring = ''
		pinststring = ''
		for z in role['applications']:
			if 'install' in z and z['install']:
				appstring = appstring + ' ' + z['install'].strip()
			if 'postInstall' in z and z['postInstall']:
				zimpy = z['postInstall'].replace('\n', ' \\\n')
				pinststring = pinststring + '\nRUN ' + zimpy + '\n\n'
		#apply them
		if appstring:
			zappstring = 'RUN apt-get update -y && apt-get install -y ' + appstring
			mydockfile = mydockfile.replace('#__INSTALL', zappstring)
		if pinststring:
			mydockfile = mydockfile.replace('#__POSTINSTALL', pinststring)
		#we dont use TARGZ yet

		#output dockkrfile
		with open(repodir + '/Dockerfile', 'w') as kl:
			kl.write(mydockfile)

		#build docker image
		dbo = subprocess.run(['docker', 'build', '-t', registry + '/' + roleID, repodir + '/'], check=True)



		#create VM if a virlet, else just upload it to ecs (else is further down)
		sqlite_backend.global_sqlite_db.role_set_status(role['roleID'], 'bundling')
		#export it
		print("exporting dockerimage")

		exd = subprocess.run(['mkdir', '-p', virtueimagesdir])
		exp = subprocess.run(['docker', 'tag', registry + '/' + roleID, roleID])
		exp = subprocess.run(['docker', 'save', '-o', virtueimage, roleID])
		##exu = subprocess.run(['aws', 's3', 'cp', '/tmp/virtuedocks/'+roleID+'.tar', 's3://siege-virtueimages/'+roleID+'.tar'])

		vimgsize = os.path.getsize(virtueimage)
		print("\tDockerImage size: " + str(vimgsize >> 20) + " MB\n")
		orig = getBaseVm()
		print('Using orig virtuevm image from ' + orig)

		#basically a recreation of bettermakeimage.sh
		print("customizing vm now")
		exd = subprocess.run(['mkdir', '-p', virtuevmsdir], check = True)
		rmer = subprocess.run(['rm', '-f', virtuevm]);
		#new, this can be outside the critical lock zone because it will only reach here after getBaseVm returns
		cper = subprocess.run(['rsync', '--no-relative', orig, virtuevm], check=True);


		#might be able to get this paralellizable up to 16x, but for now we are just going to lock on it
		with nbdlock:
			deppy = subprocess.run('for f in /dev/nbd*; do sudo qemu-nbd -d ${f} ; done', shell=True);
			#todo auto figure out vm size based on size of virtueimage????
#			vmcreat = subprocess.run('cd ' + vmdir + ' && chmod +x ./alpine-make-vm-image && chmod +x ./alpineconfigure.sh && sudo ./alpine-make-vm-image --image-format qcow2 --image-size 10G -b v3.9 --packages "$(cat alpinepackages)" --virtueimage "' + virtueimage + '" -c ' + virtuevm + ' -- ./alpineconfigure.sh', shell=True, check=True);
			vmcreat = subprocess.run('cd ' + vmdir + ' && chmod +x ./brunchable-image && chmod +x ./alpineconfigure.sh && sudo ./brunchable-image --virtueimage "' + virtueimage + '" -c ' + virtuevm + ' -- ./alpineconfigure.sh', shell=True, check=True);


		vmchown = subprocess.run('sudo chown $USER ' + virtueimage, shell=True)
		#upload to s3
		sqlite_backend.global_sqlite_db.role_set_status(role['roleID'], 'uploading')
		vmupload = subprocess.run(['aws', 's3', 'cp', virtuevm, 's3://' + aws_utils.grabVirtueBucket() + '/' + virtuevmfile, '--acl', 'public-read'])
		fimgsize = os.path.getsize(virtuevm)
		print("\tFinal VirtueImage size: " + str(fimgsize >> 20) + " MB\n")

#bake the bread!
		roleBake(role)
#clean up
		rmo = subprocess.run(['rm', '-rf', 'app.tar.gz', repodir, virtueimage, virtuevm])
	except:
		try:
			rmo = subprocess.run(['rm', '-rf', 'app.tar.gz', repodir , virtueimage, virtuevm])
		except:
			pass
		raise

	#queue a bake
#	rmo = subprocess.run(['rm', '-rf', 'app.tar.gz', repodir , '/tmp/virtueimages/'+roleID+'/'])

#force nodes to grab it from s3
	#todo make sure no bash escape from this
#	kube.runcommandonallnodes('sudo mkdir -p /tmp/virtueimages/'+roleID+'/ && aws s3 cp s3://siege-virtueimages/'+roleID+'.tar /tmp/virtueimages/'+roleID+'/image.tar', sudo=False)




def cleanCache():
	try:
		subprocess.run(r"""[ ! -z "$(df -P | awk '$6 == "/" && 0+$5 >= 75 {print}')" ] && docker system prune -a -f --volumes""", shell=True, timeout=30)
	except:
		print_exc()


def runRoleCreate(role):
	pprint(role)
	for i in range(0, 2):
		try:
			cleanCache()
			runRoleCreatePython(role)
			#success
			print('Success in creating role\n')
			#update the thing
			sqlite_backend.global_sqlite_db.role_set_status(role['roleID'], 'ready')
			return
		except Exception as e:
			pprint(e)
			print_exc()
			sqlite_backend.global_sqlite_db.role_set_status(role['roleID'], 'broken')

	#lets trigger a bake i guess

'''
def runEfsCreate():
	print("starting efs creation")
	for i in range(0, 2):
		try:
			cleanCache()
			runEfsCreatePython()
			#success
			print('Success in creating Efs\n')
			return
		except Exception as e:
			pprint(e)
			print_exc()
'''


#queue_efs_creation()

#!/usr/bin/python3
from kubernetes import client, config

import json
import pprint
import datetime
import boto3
import strutil
import paramiko
import io
import hashlib
import base64
from multiprocessing import Pool
from functools import partial,lru_cache
from scp import SCPClient

pvctemplate = {}
def readPVCTemplate():
	global pvctemplate
	with open("template_pvc.json") as f:
		pvctemplate = json.load(f)

pvcroletemplate = {}
def readPVCRoleTemplate():
	global pvcroletemplate
	with open("template_rolepvc.json") as f:
		pvcroletemplate = json.load(f)

deploytemplate = {}
def readDeployTemplate():
	global deploytemplate
	with open("template_deployment.json") as f:
		deploytemplate = json.load(f)
deployvirtlettemplate = {}
def readDeployVirtletTemplate():
	global deployvirtlettemplate
	with open("template_virtlet.json") as f:
		deployvirtlettemplate = json.load(f)

deploywintemplate = {}
def readDeployWinTemplate():
	global deploywintemplate
	with open("template_win.json") as f:
		deploywintemplate = json.load(f)

efsnfstemplate = {}
def readEfsNfsTemplate():
	global efsnfstemplate
	with open("template_efsnfs.json") as f:
		efsnfstemplate = json.load(f)

#https://stackoverflow.com/questions/10756427/loop-through-all-nested-dictionary-values was useful
#muh pure functions
def dictoreplace(d, find, replace):
	if isinstance(d, dict):
		d = {dictoreplace(k,find,replace):dictoreplace(v, find, replace) for k,v in d.items()}
	elif isinstance(d, list):
		d = [dictoreplace(v, find, replace) for v in d]
	elif isinstance(d, str): # or isinstance(d, unicode):
		if isinstance(replace, str):
			d = d.replace(find, replace)
		elif find in d:
			d = replace
#	print(type(d))
	return d
'''
def dictokey(d, find, replace, item):
	if isinstance(d, dict):
		d = {k.replace(find,replace):item if find in k else k:dictokey(v,find,replace,item) for k,v in d.items()}
	elif isinstance(d, list):
		d = [dictokey(v, find, replace, item) for v in d]
	return d
'''

def applyKubeJayson(jj):
	config.load_kube_config()
	kapi = client.CoreV1Api()
	aapi = client.AppsV1beta1Api()
	napi = client.NetworkingV1Api()

	resp = []
	for j in jj['items']:
		try:
			print("yeah " + j['kind'])
			if j['kind'] == "Pod":
				resp.append(kapi.create_namespaced_pod(body=j, namespace="default"))
			if j['kind'] == "Deployment":
				resp.append(aapi.create_namespaced_deployment(body=j, namespace="default"))
			elif j['kind'] == "Service":
				resp.append(kapi.create_namespaced_service(body=j, namespace="default"))
## dont need CM anymore? TODO delete
			elif j['kind'] == "ConfigMap":
				resp.append(kapi.create_namespaced_config_map(body=j, namespace="default"))
			elif j['kind'] == "PersistentVolumeClaim":
				resp.append(kapi.create_namespaced_persistent_volume_claim(body=j, namespace="default"))
			elif j['kind'] == "NetworkPolicy":
				resp.append( napi.create_namespaced_network_policy( body=j, namespace="default" ) )
		except client.rest.ApiException as e:
			resp.append(e.body)
			pprint.pprint(e.body)
	return resp


#used to removing the mounts and stuff
def dictoremove(d, find):
	if isinstance(d, dict):
		if 'name' in d and find in d['name']:
			d = None
		else :
			d = {k:dictoremove(v, find) for k,v in d.items() if dictoremove(v, find)}
	elif isinstance(d, list):
		d = [dictoremove(v, find) for v in d if dictoremove(v, find)]

	return d


# todo virtlet flag in roles
def createUserDeploy(usernm, rolenm, reponm, isolation, apps, key, mounts, debug=False, usan='', usak='', usapass='', psan='', psak='', apparmor_profile="runtime/default", virtlet=True, ramsize="", cpucount=1, mavenconf = "",ostype='linux', windows_key_hack=''):
	global deploytemplate
	global deployvirtuetemplate
	global accountnum
	global virtuebucket
	global deploywintemplate
	print("Creating deploy for user " + usernm)

	vname = strutil.genvirtueid()

	template = ""
	userdrive = "" # hack so we only evaluate powershell for windows stuff
	if( ostype == 'windows' ):
		print( "Doing exerimental windows thing, hold tight" )
		if not deploywintemplate:
			readDeployWinTemplate()
		template = deploywintemplate
		winkey_digest = base64.b64decode( windows_key_hack )
		s = hashlib.sha512( )
		appended = winkey_digest + vname.encode('utf-8')
		s.update( appended )
		digest = s.digest()

		pprint.pprint( apps )
		userdrive = _userdrive_format_script( {
			"__APPS":",".join( ['"{}"'.format( a['launchCmd'] ) for a in apps] ),
			"__HASUSERDRIVE": str("user" in mounts),
			"__USAN": usan,
			"__USAP": usapass,
			"__B85PASS": base64.b85encode( digest ).decode('ascii'),
			"__CHOCO_TARGETS": ",".join( [ '"{}"'.format( a['install']) for a in apps if a['install']] )
			},
			_ezread("powershell/bake.ps1") + _ezread( "powershell/boot.ps1" )
		)
	elif(virtlet):
		print("launching virtlet virtue")
		if not deployvirtlettemplate:
			readDeployVirtletTemplate()
		template = deployvirtlettemplate
	else:
		print("launching standard virtue")
		if not deploytemplate:
			readDeployTemplate()
		template = deploytemplate

	if not cpucount or not isinstance(cpucount, int):
		print("cpucount not set?. Must be 1>=cpucount>=32. Setting to 1 (min)")
		cpucount = 1
	if cpucount < 1:
		print("cpucount " + str(cpucount) + " is too low. Must be 1>=cpucount>=32. Setting to 1 (min)")
		cpucount = 1
	if cpucount > 32:
		print("cpucount " + str(cpucount) + " is too low. Must be 1>=cpucount>=32. Setting to 32 (max)")
		cpucount = 32
	if not ramsize:
		print("warning.... ramsize not set. Defaulting to 1024Mi")
		ramsize = "1024Mi"

	if not accountnum:
		grabAccountNum()

	if not usak:
		usak = ''
	if not psak:
		usak = ''
	if not usan:
		usak = ''
	if not psan:
		usak = ''






	#edit the stuffs
	#must start with a lowercase letter, end with a lowercase letter, and consist only of lowercase letters, numbers, '.' or '-'
	#maybe should just replace it with a hasho

	res = dictoreplace(template, "__USERDRIVE", userdrive )
	res = dictoreplace(res, "__USER", usernm)
	res = dictoreplace(res, "__SANIUSER", strutil.saniuser(usernm))
	res = dictoreplace(res, "__SANIURD", strutil.saniurd(usernm, rolenm))
	res = dictoreplace(res, "__ROLE", rolenm)
	res = dictoreplace(res, "__REGISTRY", accountnum+".dkr.ecr.us-east-1.amazonaws.com")
	res = dictoreplace(res, "__REPO", reponm)
	res = dictoreplace(res, "__ISOLATION", isolation)
	res = dictoreplace(res, "__KEY", key)
	#for newfangled storage stuffs
	res = dictoreplace(res, "__STORAGEUSERUSER", usan)
	res = dictoreplace(res, "__STORAGEPERSISTENTUSER", psan)
	res = dictoreplace(res, "__STORAGEUSERKEY", usak)
	res = dictoreplace(res, "__STORAGEPERSISTENTKEY", psak)
	#must start and end with a letter, has to be lowercase letters or numbers
	res = dictoreplace(res, "__VNAME", vname)
	res = dictoreplace(res, "__VIRTUEVMBUCKET", grabVirtueBucket())
	res = dictoreplace(res, "__APPS",  json.dumps(apps))
	res = dictoreplace(res, "__APPARMOR", apparmor_profile)
	res = dictoreplace(res, "__RAMSIZE", ramsize)
	res = dictoreplace(res, "__CPUCOUNT", str(cpucount))
	res = dictoreplace(res, "__MAVENCONFIG", mavenconf)


	res = dictoreplace(res, "__TIME", str(datetime.datetime.now()).replace(':', '-').replace(' ', '_'))

	if 'user' not in mounts:
		res = dictoremove(res, "-data-volume")
	if 'persistent' not in mounts:
		res = dictoremove(res, "-urd-volume")

	if debug:
		print("createUserDeploy debug")
		pprint.pprint(res)

	return applyKubeJayson(res)



#deprecated?
'''
def createUserEfsNfs(usernm, debug=False):
	global efsnfstemplate
	global accountnum
	print("Creating deploy for user " + usernm)

	if not deploytemplate:
		readEfsNfsTemplate()
	if not accountnum:
		grabAccountNum()


	#edit the stuffs
	#must start with a lowercase letter, end with a lowercase letter, and consist only of lowercase letters, numbers, '.' or '-'
	#maybe should just replace it with a hasho
	res = dictoreplace(efsnfstemplate, "__USER", usernm)
	res = dictoreplace(res, "__SANIUSER", strutil.saniuser(usernm))
	res = dictoreplace(res, "__TIME", str(datetime.datetime.now()).replace(':', '-').replace(' ', '_'))
	res = dictoreplace(res, "__REGISTRY", accountnum+".dkr.ecr.us-east-1.amazonaws.com")

	if debug:
		print("createUserEfsNfs debug")
		pprint.pprint(res)

	return applyKubeJayson(res)
'''


def createUserPVC(usernm, debug=False):
	global pvctemplate

	print("Creating pvc for user " + usernm)
	if not pvctemplate:
		readPVCTemplate()


#	dep = pvctemplate
#	dep['metadata']['name'] = username + "-data-volume"
	dep = dictoreplace(pvctemplate, "__USER", usernm)
	dep = dictoreplace(dep, "__SANIUSER", strutil.saniuser(usernm))

#todo make sure no special chars?
#[a-z0-9]([-a-z0-9]*[a-z0-9])?(\\.[a-z0-9]([-a-z0-9]*[a-z0-9])?)*

	if debug:
		print("createUserPVC debug")
		pprint.pprint(dep)

	return applyKubeJayson(dep)




#role
def createUserRolePVC(usernm, rolenm, debug=False):
	global pvctemplate

	print("Creating pvc for user/role " + usernm + '/' + rolenm)
	if not pvcroletemplate:
		readPVCRoleTemplate()


#	dep = pvctemplate
#	dep['metadata']['name'] = username + "-data-volume"
	dep = dictoreplace(pvcroletemplate, "__USER", usernm)
	dep = dictoreplace(dep, "__ROLE", rolenm)
	#throw in some nasty characters here so people cant manually adjust
	dep = dictoreplace(dep, "__SANIURD", strutil.saniurd(usernm, rolenm))

#todo make sure no special chars?
#[a-z0-9]([-a-z0-9]*[a-z0-9])?(\\.[a-z0-9]([-a-z0-9]*[a-z0-9])?)*

	if debug:
		print("createUserRolePVC debug")
		pprint.pprint(dep)

	return applyKubeJayson(dep)


def deleteDeployment(depid):
	print("Deleting deployment " + depid)
	config.load_kube_config()
	aapi = client.AppsV1beta1Api()
	body = client.V1DeleteOptions()
	body.propagation_policy='Background'
	resp = {}
	try:
		resp = aapi.delete_namespaced_deployment(depid, body=body, propagation_policy='Background', namespace="default")
	except client.rest.ApiException:
		pass
	return resp

def deletePod(podid):
	print("Deleting pod " + podid)
	config.load_kube_config()
	kapi = client.Corev1Api()
	body = client.V1DeleteOptions()
	body.propagation_policy='Background'
	resp = {}
	try:
		resp = kapi.delete_namespaced_pod(podid, body=body, propagation_policy='Background', namespace="default")
	except client.rest.ApiException:
		pass
	return resp

def deleteService(servid):
	print("Deleting service " + servid)
	config.load_kube_config()
	kapi = client.CoreV1Api()
	body = client.V1DeleteOptions()
	body.propagatio_policy='Background'
	resp = {}
	try:
		resp = kapi.delete_namespaced_service(servid, body=body, propagation_policy='Background', namespace="default")
	except client.rest.ApiException:
		pass
	return resp

def deleteConfigMap(cmid):
	print("Deleting configmap " + cmid)
	config.load_kube_config()
	kapi = client.CoreV1Api()
	body = client.V1DeleteOptions()
	body.propagatio_policy='Background'
	resp = {}
	try:
		resp = kapi.delete_namespaced_config_map(cmid, body=body, propagation_policy='Background', namespace="default")
	except client.rest.ApiException:
		pass
	return resp


def deletePVC(pvcid):
	print("Deleting pvc " + pvcid)
	config.load_kube_config()
	kapi = client.CoreV1Api()
	body = client.V1DeleteOptions()
	body.propagation_policy='Background'
	resp = {}
	try:
		resp = kapi.delete_namespaced_persistent_volume_claim(pvcid, body=body, propagation_policy='Background', namespace="default")
	except client.rest.ApiException:
		pass
	return resp

def deleteNetPol( netpolid ):
	print( "Deleting netpol " + netpolid )
	config.load_kube_config()
	napi = client.NetworkingV1Api()
	body = client.V1DeleteOptions()
	resp = {}
	try:
		resp = napi.delete_namespaced_network_policy( name=netpolid, body=body, namespace="default" )
	except client.rest.ApiException:
		pass
	return resp


def listServiceInfo(usernm='', rolenm='', virtueid=''):
	#todo make suer this cant be injected
	#todo or just delete it... i dont use it so far?
	select = 'virtue=ViRtUe'
	if usernm:
		if select: select += ','
		select += 'userID='+usernm
	if rolenm:
		if select: select += ','
		select += 'roleID='+rolenm
	if virtueid:
		if select: select += ','
		select += 'name='+virtueid
	config.load_kube_config()
	kapi = client.CoreV1Api()
	resp = {}
	try:
		resp = kapi.list_namespaced_service(label_selector=select, namespace="default")
	except client.rest.ApiException:
		pass
	return resp


def getServiceInfo(servid):
	config.load_kube_config()
	kapi = client.CoreV1Api()
	resp = {}
	try:
		resp = kapi.read_namespaced_service(servid, namespace="default")
	except client.rest.ApiException:
		pass
	return resp

def getDeploymentInfo(depid):
	config.load_kube_config()
	aapi = client.AppsV1beta1Api()
	resp = {}
	try:
		resp = aapi.read_namespaced_deployment(depid, namespace="default")
	except client.rest.ApiException:
		pass
	return resp

def getConfigMapInfo(cmid):
	config.load_kube_config()
	kapi = client.CoreV1Api()
	resp = {}
	try:
		resp = kapi.read_namespaced_config_map(depid, namespace="default")
	except client.rest.ApiException:
		pass
	return resp

def listDeploymentInfo(usernm='', rolenm='', virtueid=''):
	#todo make suer this cant be injected
	#todo or just delete it... i dont use it so far?
	select = 'virtue=ViRtUe'
	if usernm:
		if select: select += ','
		select += 'userID='+usernm
	if rolenm:
		if select: select += ','
		select += 'roleID='+rolenm
	if virtueid:
		if select: select += ','
		select += 'name='+virtueid
	config.load_kube_config()
	aapi = client.AppsV1beta1Api()
	resp = {}
	try:
		resp = aapi.list_namespaced_deployment(label_selector=select, namespace="default")
	except client.rest.ApiException:
		pass
#	pprint.pprint(resp)
	return resp


def listPodInfo(usernm='', rolenm='', virtueid=''):
	#todo make suer this cant be injected
	#todo or just delete it... i dont use it so far?
	select = 'virtue=ViRtUe'
	if usernm:
		if select: select += ','
		select += 'userID='+usernm
	if rolenm:
		if select: select += ','
		select += 'roleID='+rolenm
	if virtueid:
		if select: select += ','
		select += 'name='+virtueid
	config.load_kube_config()
	kapi = client.CoreV1Api()
	resp = {}
	try:
		resp = kapi.list_namespaced_pod(label_selector=select, namespace="default")
	except client.rest.ApiException:
		pass
	return resp

def listConfigMapInfo(usernm='', rolenm='', virtueid=''):
	#todo make suer this cant be injected
	#todo or just delete it... i dont use it so far?
	select = 'virtue=ViRtUe'
	if usernm:
		if select: select += ','
		select += 'userID='+usernm
	if rolenm:
		if select: select += ','
		select += 'roleID='+rolenm
	if virtueid:
		if select: select += ','
		select += 'name='+virtueid
	config.load_kube_config()
	kapi = client.CoreV1Api()
	resp = {}
	try:
		resp = kapi.list_namespaced_config_map(label_selector=select, namespace="default")
	except client.rest.ApiException:
		pass
	return resp

def getPodInfo(podid):
	config.load_kube_config()
	kapi = client.CoreV1Api()
	resp = {}
	try:
		resp = kapi.read_namespaced_pod(podid, namespace="default")
	except client.rest.ApiException:
		pass
#	pprint.pprint(resp)
	return resp

#might be unused....
def getUserPods(userid):
	config.load_kube_config()
	kapi = client.CoreV1Api()
	resp = {}
	try:
		resp = kapi.list_namespaced_pod(label_selector='userID='+userid, namespace="default")
	except client.rest.ApiException:
		pass
#	pprint.pprint(resp)
	return resp

def getVirtuePod(virtueid):
	config.load_kube_config()
	kapi = client.CoreV1Api()
	resp = {}
	try:
		resp = kapi.list_namespaced_pod(label_selector='name='+virtueid, namespace="default")
	except client.rest.ApiException:
		pass
#	pprint.pprint(resp)
	return resp

def loadApparmorOnNodes( profiletext ):
	pro_file = "/etc/apparmor.d/profile.txt" # see what i did there
	copyFileToAllNodes( profiletext, "/tmp/tmp.txt" )
	runCommandOnAllNodes( "mv /tmp/tmp.txt {}".format( pro_file ) )
	runCommandOnAllNodes( "apparmor_parser -r {}".format( pro_file ) )
	#runCommandOnAllNodes( "rm {}".format( pro_file ) )

def runCommandOnAllNodes( cmd , sudo=True, masters=False):
	ips = getNodeIps(masters=masters)
	cmd_runner = partial( _runCommandOnNode, cmd )
	with Pool(len(ips)) as p:
		outs = p.map( cmd_runner, ips )
	return outs

def _runCommandOnNode( cmd, node_ip , sudo=True):
	ssh = paramiko.SSHClient()
	ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
	ssh.connect(node_ip, port=22, username='ubuntu', timeout=5)
	(stdin,stdout,stderr) =  ssh.exec_command(('sudo ' + cmd) if sudo else (cmd))
	return stdout.read()


def copyFileToAllNodes( filetext, filename , masters=False):
	ips = getNodeIps(masters=masters)
	file_copy = partial( _copyToNode, filetext, filename )
	with Pool(len(ips)) as p:
		outs = p.map( file_copy, ips )
	return outs

def _copyToNode( filetext, filename, node_ip ):
	ssh = paramiko.SSHClient()
	ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
	ssh.connect(node_ip, port=22, username='ubuntu', timeout=5)
	b = io.BytesIO()
	b.write( filetext.encode( 'ascii' ) )
	b.seek( 0 )
	scp = SCPClient( ssh.get_transport() )
	scp.putfo( b, filename )


def getNodeIps(masters=False):
	config.load_kube_config()
	kapi = client.CoreV1Api()
#	pprint.pprint(kapi.list_node().items)
#	for i in kapi.list_node().items:
#		pprint.pprint(i.metadata.labels)
	return [i.status.addresses[0].address for i in kapi.list_node().items
		if masters or
		('kubernetes.io/role' in i.metadata.labels and i.metadata.labels['kubernetes.io/role'] == 'node')
	]


def getAllPods():
	config.load_kube_config()
	kapi = client.CoreV1Api()
	resp = {}
	try:
		resp = kapi.list_pod_for_all_namespaces()
	except client.rest.ApiException:
		pass
#	pprint.pprint(resp)
	return resp

def deletePod(podid):
	print("Deleting pod for " + podid)
	config.load_kube_config()
	kapi = client.CoreV1Api()
	resp = {}
	try:
		resp = kapi.delete_namespaced_pod(podid, body=client.V1DeleteOptions(), namespace="default")
	except client.rest.ApiException:
		pass
	#pprint.pprint(resp)
	return resp

'''
JSON doesn't deal with some strings well. Notably, it hates unescaped quotation marks,
backslashes, and newlines. We have to escape those before putting it in
'''
def _json_multiline_format( text ):
	return text.replace("\\", "\\\\").replace( "\"","\\\"").replace( "\n","\\n" )

'''
Reps is a dict of entries to find and replace
	be careful that no entry is a prefix of another entry, or things get weird
	reps={"__USER":"test","__USERCRED":"admin"}, for example, is very bad
script is a string that probably represents a powershell
'''
def _userdrive_format_script( reps={}, script="" ):
	for k,v in reps.items():
		script = script.replace( k, v )
	return "#ps1_sysnative\n" + script

@lru_cache()
def _ezread( fn ):
	with open( fn, "r" ) as f:
		text = f.read()
	return text

#getPodInfo("efs-provisioner-787d66567b-dmtrg")
#getAllPods()

#pprint.pprint(createUserPVC("yeet"))


#createUserEfsNfs('awgaines', debug=False)

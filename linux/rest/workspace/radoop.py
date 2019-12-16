
#from win32com.client import Dispatch

import winshell
from pathlib import Path

import sys, os

import subprocess

import pprint
import base64


from os import listdir
from os.path import isfile, join, isdir
from shutil import rmtree

import pc

import ezfuncs

rdpskel = ""



#load the rdp template into memory
#grabs it from the API
def loadrdp():
	global rdpskel
	pc.pc('Grabbing RDP template')
	sys.stdout.flush()
	try:
		rdpskel = ezfuncs.getit("/rdp.template").decode('utf-8')
	except:
		rdpskel = ''
		pc.pc('Unable to grab RDP template', pc.FAIL)
		raise
	pprint.pprint(rdpskel)


# removes the shortcuts files on the desktop for virtues that no longer exist
# called approx every 10 seconds with a list of currently active virtues (aka a list of DONT DELETE ME!s)
#also called on quit of the service. to clean up any leftovers

#creates a desktop shortcut for every Virtue application.
def outputshortcuts(v, desktop = '', path = ''):
#	pc.pc("Outputting shortcuts for rdps")
#	print( "writing desktop stuff for {}".format( v['roleID'] ) )
	if not desktop:
		desktop = str(Path.home()) + '/Desktop/'
	if not path:
		path = str(Path.home()) + '/.virtuerdps/'

	folder = join( desktop,  v['roleID'] + " virtue" )

	if( not( isdir( folder ) ) ):
		os.mkdir( folder )

#	print( "Folder is {}".format( folder ) )


	#todo a convig?
	for a in v['applications']:
		sessionid=a['appID'] + '.' + v['virtueID']
		fid=path + sessionid + '.rdp'


		if 'desktop' in a and a['desktop']:
			shapath = join(folder, a['desktop'])
		else:
			shapath = join(folder, sessionid)
		if 'roleID' in v:
			shpath = shapath + '-' + v['roleID'] + '.lnk'
		else:
			shpath = shapath + '.lnk'

		iname = ''
		if 'iconpath' in a:
			iname = a['iconpath']

	#todo dont overwrite if already exists?
		with winshell.shortcut(shpath) as sh:
			sh.path = fid
			sh.icon_location = (iname, 0)
#			sh.arguments = r"--session-conf=" +str(fid)+""
			sh.working_directory = str(Path.home())
			sh.description = v['virtueID']
			sh.write(shpath)

#cleans the rdp files for virtues that no longer exist
#called basically right after cleanshortcuts
def cleanrdps(vs, path = ''):
	if not path:
		path = str(Path.home()) + '/.virtuerdps/'
	try:
		files = {f:join(path, f) for f in listdir(path) if f.endswith('.rdp')}
	except:
		#in case the path doesnt exist
		return

	#early out i guess?
	if not files:
		return

	cleanups = []
	for k,v in files.items():
		try:
			##get virtueid from filename
			if all( vid not in vs for vid in k.split('.')):
				cleanups.append(v)
#				os.remove(v)
		except:
			continue

	for f in cleanups:
		try:
			pc.pc("removing rdp file " + f);
			os.remove(f)
		except:
			pass


def outputrdps(virtues, path = ''):
	pc.pc("Outpudding rdps for")
	if not path:
		path = str(Path.home()) + '/.virtuerdps/'
		os.makedirs(path, exist_ok=True)
	#lets process the skel
	#trigger the cache to grab it if it doesnt exist
	if not rdpskel:
		loadrdp()

	for k, v in virtues.items():
		vpass = ezfuncs.sshhashchain( [v['roleID'],v['virtueID'] ] )
		unbinned = base64.b85encode( vpass ).decode('utf-8')
		pscmd = 'ConvertTo-SecureString "{}" -AsPlainText -Force | ConvertFrom-SecureString'.format( unbinned )
		si = subprocess.STARTUPINFO()
		si.dwFlags = subprocess.STARTF_USESHOWWINDOW
		si.wShowWindow = subprocess.SW_HIDE
		psexe = subprocess.run( ["powershell.exe","-Command",pscmd], stdout=subprocess.PIPE, stdin=subprocess.DEVNULL, stderr=subprocess.DEVNULL, startupinfo=si )
		bpass = psexe.stdout.strip().decode('ascii')

		temp = rdpskel.replace('__VIRTUE', v['virtueID'])
		temp = temp.replace('__HOST', v['host'])
		temp = temp.replace('__BPASS', bpass )
		temp = temp.replace('__PORT', str(v['port']))
		temp = temp.replace('__KEY', (str(Path.home()) + '/.ssh/id_rsa').strip().replace("\\", "/")) #todo make this nicer?
		for a in v['applications']:
			sessionid=a['appID'] + '.' + v['virtueID']
			fid=path + sessionid + '.rdp'
			#TODO check if the file already exists!
			bemp = temp.replace('__SESSIONID', sessionid)
			bemp = temp.replace('__CMD', a['launchCmd'])
			with open(fid, 'w') as f:
				f.write(bemp)

	sys.stdout.flush()


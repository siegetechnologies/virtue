
#from win32com.client import Dispatch

import winshell

from pathlib import Path

import sys, os

import pprint


from os import listdir
from os.path import isfile, join, isdir
from shutil import rmtree

import pc

import ezfuncs

x2goskel = ""



#load the x2go template into memory
#grabs it from the API
def loadx2go():
	global x2goskel
	pc.pc('Grabbing X2Go template')
	sys.stdout.flush()
	try:
		x2goskel = ezfuncs.getit("/virtue.template").decode('utf-8')
	except:
		pc.pc('Unable to grab X2Go template', pc.FAIL)
		raise
	pprint.pprint(x2goskel)


# removes the shortcuts on the desktop for virtues that no longer exist
# called approx every 10 seconds with a list of currently active virtues (aka a list of DONT DELETE ME!s)
#also called on quit of the service. to clean up any leftovers
def cleanshortcuts(vs, desktop = ''):
	print('cleaning unneeded shortcuts')
#	sys.stdout.flush()
	if not desktop:
		desktop = str(Path.home()) + '/Desktop/'
	#grab all files from desktop

	#files = [join(desktop, f) for f in listdir(desktop) if f.endswith('.lnk')]
	folders = [f for f in listdir(desktop) if f.endswith(' virtue')]

	#early out i guess?
	if not folders:
		return

	try:
		rolids = [ v['roleID'] + ' virtue' for k,v in vs.items() ]
	except:
		rolids = []


#	pprint.pprint(vs)

#should only have links at this point
	for f in folders:
		virtname = f.strip()
		fullpath = join( desktop, f )
		if( virtname not in rolids ):
			pprint.pprint( "removing desktop folder {}".format( f ) )
			rmtree( fullpath )
	sys.stdout.flush()


#creates a desktop shortcut for every Virtue appliocation.
def outputshortcuts(v, desktop = ''):
#	pc.pc("Outputting shortcuts for togos")
#	print( "writing desktop stuff for {}".format( v['roleID'] ) )
	if not desktop:
		desktop = str(Path.home()) + '/Desktop/'

	folder = join( desktop, v['roleID'] + " virtue" )

	if( not( isdir( folder ) ) ):
		os.mkdir( folder )

#	print( "Folder is {}".format( folder ) )


	#todo a convig?
	for a in v['applications']:
		sessionid = a['appID'] + '.' + v['virtueID'] ##TODO


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
			sh.path = r"C:\Program Files (x86)\x2goclient\x2goclient.exe"
			sh.icon_location = (iname, 0)
			sh.arguments = r"--session-conf=" +str(Path.home())+"/.x2gosessions --add-to-known-hosts --hide --sessionid=" + sessionid
			sh.working_directory = str(Path.home())
			sh.description = v['virtueID']
			sh.write(shpath)



#used to cache the x2go files... (dont constantly re-write the x2go file)
prevx2gos = 'no this isnt an x2gofile'

#takes a list of virtues, outputs a x2go config
#uses prevx2gos to cache. If changed, outputs a file
def outputx2gos(virtues, path = ''):
	pc.pc("Outpudding togos for")
	global prevx2gos
#todo auto mkdir
	if not path:
		path = str(Path.home()) + '/.x2gosessions'

	#lets process the skel
	#trigger the cache to grab it if it doesnt exist
	if not x2goskel:
		loadx2go()


	demp = ''
	for k, v in virtues.items():
		temp = x2goskel.replace('__VIRTUE', v['virtueID'])
		temp = temp.replace('__HOST', v['host'])
		temp = temp.replace('__PORT', str(v['port']))
		temp = temp.replace('__KEY', (str(Path.home()) + '/.ssh/id_rsa').strip().replace("\\", "/")) #todo make this nicer?
		for a in v['applications']:
			sessionid=a['appID'] + '.' + v['virtueID']
			bemp = temp.replace('__SESSIONID', sessionid)
			bemp = bemp.replace('__CMD', a['launchCmd'])
			demp += '\n' + bemp + '\n'



	if demp != prevx2gos:
		#write the template out
		pc.pc('Writing new x2gos')
		pprint.pprint(demp)
		with open(path, 'w') as f:
			f.write(demp)
		prevx2gos = demp
	sys.stdout.flush()


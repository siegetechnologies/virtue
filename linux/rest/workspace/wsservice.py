#!/usr/bin/python3

import os
import sys
import pprint
import time
import traceback

import togo
import radoop
import pc
import icon
import gui
import ezfuncs


try:
	ezfuncs.url = sys.argv[1]
except:
	ezfuncs.url = 'https://cc.prod.virtue.com:4443'


pc.pc('Starting windows service')

ezfuncs.loadconf()

icon.crfolder()

#todo signals

#set defaults, if stuff isnt there

ezfuncs.virtueconf['username'] = os.getlogin()

#set default pw?
if "password" not in ezfuncs.virtueconf:
	ezfuncs.virtueconf['password'] = 'virtuerestdefaultpw'

try:
	gui.updateit({}, {})
except:
	traceback.print_exc()

#login as a user
try:
	ezfuncs.login()
	ezfuncs.saveconf()
except:
	traceback.print_exc()

#upload key
try:
	ezfuncs.keyupload() #really could call this without logging in, but for sake of clean logs... we call login first
except:
	traceback.print_exc()

while not gui.should_quit:
	virtues = {}
	roles = {}
	virts = {}
	wvirts = {}
	lvirts = {}
	gui.should_update = False
	try:
		ezfuncs.saveconf()
	except:
		traceback.print_exc()
	try:
		virtues = ezfuncs.listvirtues()
		pprint.pprint(virtues)
	except:
		traceback.print_exc()
	if not virtues:
		virtues = {}

	try:
		if not isinstance(virtues, dict):
			virtues = { v['virtueID']:v for v in virtues }
		virts = { k:v for k,v in virtues.items() if (v['available'] and v['state'].lower() == 'running' ) }
		pprint.pprint(virts)
	except:
		traceback.print_exc()
	if not virts:
		virts = {}

	try:
		lvirts = { k:v for k,v in virts.items() if v['type'].lower() == 'linux' }
#		pprint.pprint(lvirts)
	except:
		traceback.print_exc()

	try:
		wvirts = { k:v for k,v in virts.items() if v['type'].lower() == 'windows' }
#		pprint.pprint(wvirts)
	except:
		traceback.print_exc()

	if not wvirts:
		wvirts = {}
	if not lvirts:
		lvirts = {}

	try:
		rl =  ezfuncs.listroles()
		if rl:
			roles = { l['roleID']:l for l in rl['roles'] if 'roleID' in l}
			pprint.pprint(roles)
	except:
		traceback.print_exc()
	if not roles:
		roles = {}

	try:
		togo.cleanshortcuts(virts)
	except:
		traceback.print_exc()

#	try:
#		radoop.cleanshortcuts(virts)
#	except:
#		traceback.print_exc()

	try:
		radoop.cleanrdps(virts)
	except:
		traceback.print_exc()

	try:
		gui.updateit(virtues, roles)
	except:
		traceback.print_exc()

	if virts:
	#output template
		try:
			togo.outputx2gos(lvirts)
		except Exception as e:
			pc.pc(str(e), pc.FAIL)
	#output rdps
		try:
			radoop.outputrdps(wvirts)
		except Exception as e:
			pc.pc(str(e), pc.FAIL)
	#fixup hosts
		try:
			ezfuncs.known_hosts(virts)
		except:
			traceback.print_exc()
	#adjust icons
		try:
			icon.cacheicons(virts)
		except:
			traceback.print_exc()
	#adjust shortcuts linux
		try:
			for k,v in lvirts.items():
				togo.outputshortcuts(v)
		except:
			traceback.print_exc()
	#adjust shortcuts windows
		try:
			for k,v in wvirts.items():
				radoop.outputshortcuts(v)
		except:
			traceback.print_exc()
	#that should be all?
	sys.stdout.flush()
	try:
		for i in range(10):
			if gui.should_update:
				break
			time.sleep(1)
	except:
		gui.should_quit = True

#todo logoff event?


virts = []
try:
	radoop.cleanrdps(virts)
except:
	traceback.print_exc()
try:
	togo.cleanshortcuts(virts)
except:
	traceback.print_exc()
try:
	togo.outputx2gos(virts)
except:
	traceback.print_exc
gui.shutitdown()

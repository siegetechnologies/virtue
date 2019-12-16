import pkg_resources #hack to get pyinstaller to include pkg_resouraces (needed for tray icon stuff)
from infi.systray import SysTrayIcon	#for the tray icon
from pathlib import Path
import sys
import traceback
import ezfuncs
import pc
import pprint
import icon

should_quit = False
should_update = False
#callback to signal the main loop to quit
def setshouldquit(systray):
	global should_update
	global should_quit
	should_quit=True
	should_update = True


#upload a key, trigger a mail loop early update after
def callkeyupload(systray):
	global should_update
	try:
		ezfuncs.keyupload()
	except:
		traceback.print_exc()
	sys.stdout.flush()
	should_update = True

def calldestroyall(systray):
	global should_update
	try:
		ezfuncs.virtuedestroyall()
	except:
		traceback.print_exc()
	sys.stdout.flush()
	should_update = True

def callcreateall(systray):
	global should_update
	try:
		ezfuncs.virtuecreateall()
	except:
		traceback.print_exc()
	sys.stdout.flush()
	should_update = True


def doota(croleid):
	global should_update
	ezfuncs.virtuecreate(croleid)
	should_update = True

def dootb(cvid):
	global should_update
	ezfuncs.virtuedestroy(cvid)
	should_update = True

#returns a lambda function witht the virtueid or rollid set
#I need to use this weird lambda trickery since the icon only allows a callback function. I cant specify data to pass the callback function, and the "systray" argument is the global systray. If it were the object of the individual button that was clicked, then i could probably hide data in that. But it isnt.
#so instead, I use python's lambda functionality to essentially create a "custom" function with the virtueid hardcoded into it for every button
#this is why i dislike lambdas (and their reliance). Just make your callbacks take an arbitrary data object ffs.
#blame the person who made this library for the need to do this crazy stuff
def customcreate(croleid):
	return lambda a:doota(croleid)
def customdestroy(cvirtid):
	return lambda a:dootb(cvirtid)


#default no-op callback (since NULL isnt a valid callback)
def dupdate(systray):
	pass




sysTrayIcon = None
#cleanly removes the icon
def shutitdown():
	global sysTrayIcon
	if sysTrayIcon:
		#disables the quit callback so this doesnt accidentally trigger setshouldquit
		sysTrayIcon._on_quit = None
		sysTrayIcon.shutdown()
	sysTrayIcon = None




old_options = ()
old_roles = []
old_virtues = []

#called approx every 10 seconds. Updates the content of the taskbar icon
def updateit(virtues, roles):
	global old_options
	global old_roles
	global old_virtues
	roles = dict(roles)
	menu_options = (
		("Re-upload key", None, callkeyupload),
#		("Quit", None, setshouldquit),
		)
	global sysTrayIcon


	for k,v in virtues.items():
		if 'roleID' in v and v['roleID']:
			if v['roleID'] in roles:
				del roles[v['roleID']]

	new_roles = [ k + ' - ' + v['status'] for k,v in roles.items() ]
	new_virtues = [ k + ' - ' + v['roleID'] + ' - ' + v['state'] for k,v in virtues.items() ]


	#if any differences, update everything
	#basically, if no changes need to be made, dont re-make the taskbar icon. (it makes it annoying to use because it would reset every 10 seconds)
	if menu_options != old_options or new_roles != old_roles or old_virtues != new_virtues:
		old_options = menu_options
		old_roles = new_roles
		old_virtues = new_virtues


		virtue_options = ()
		role_options = ()

		for k,v in virtues.items():
			virtue_options = virtue_options + ((v['virtueID'] + ' - ' + v['roleID'] + ' - ' + v['state'], None, customdestroy(v['virtueID'])),)

		for k,v in roles.items():
			if 'status' in v and v['status'].lower() != 'ready':
				role_options = role_options + ((k + ' - ' + v['status'], None, dupdate) ,)
			else:
				role_options = role_options + ((k, None, customcreate(k)) ,)



		if len(virtue_options) > 1:
			virtue_options = virtue_options + ( ('Stop All Virtues', None, calldestroyall), )

		if virtue_options:
			menu_options = menu_options + ( ('Virtues', None, virtue_options), )


		if len(role_options) > 1:
			role_options = role_options + ( ('Start All Virtues', None, callcreateall), )

		if role_options:
			menu_options = menu_options + ( ('Roles', None, role_options), )

		try:
			#if the icon does not exist, grab it at this point
			iconpath = str(Path.home()) + "/.virtueicons/virtuelogo.png"
			resultpath = icon.geticon(ezfuncs.url + '/virtuelogo.png', iconpath)
		except:
			resultpath = ""
			traceback.print_exc()

		pc.pc("Updating tray menu")
	#kill old tray icon
		if sysTrayIcon:
			shutitdown()
	#and make a new one!
		sysTrayIcon = SysTrayIcon(resultpath, "Virtue", menu_options, on_quit = setshouldquit)
		sysTrayIcon.start()

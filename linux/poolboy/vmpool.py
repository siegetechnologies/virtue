from pprint import pprint
from multiprocessing import Lock
#import psutil

import subprocess
from time import sleep

import threading
import os

from traceback import print_exc

from cdconfig import genconf
import portpool
import imagecache
import nettest

virtueconfsdir = '/tmp/virtueconfs'


vmpoollock = Lock()
vmpool = {}

#https://stackoverflow.com/questions/5114292/break-interrupt-a-time-sleep-in-python

class davm:
	def __init__(self, virtueID, imageurl, info, vconf, toport, nettest, ramsize="", cpucount=""):
		self.virtueID = virtueID
		self.imageurl = imageurl

		self.info = info
		self.vconf = vconf

		self.port = portpool.getport()
		self.toport = toport
		self.nettest = nettest

		self.ramsize = ramsize
		self.cpucount = cpucount

		self.stop = threading.Event()
		self.p = None
		self.configfile = None

		if not self.ramsize:
			self.ramsize = "1G"
		if not self.cpucount:
			self.cpucount = "1"




	def start(self):
		if self.port is None or self.toport is None:
			self.state = 'portfail'
			return
		else:
			self.state = 'initializing'

		#lets try to download the repo
		self.state = 'imagedl'
		try:
			image = imagecache.getimage(self.imageurl)
			if not image:
				raise
		except:
			print_exc()
			self.state = 'imagefail'
			return
		#generate config
		self.state = 'configgen'
		try:
			self.configfile = genconf(virtueconfsdir + '/' + self.virtueID + '.iso', self.vconf)
		except:
			print_exc()
			self.state = 'configfail'
			return

		self.p = subprocess.Popen(['qemu-system-x86_64', image, '-enable-kvm',
#			'-cpu', 'host'
			'-smp', self.cpucount,
			'-m', self.ramsize,
			'-snapshot', '-nographic',
			"-cdrom", self.configfile,
			"-net", "nic,model=virtio",
			"-net",  "user,hostfwd=tcp::{}-:{}".format( self.port, self.toport )
			],
			stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
			)

		self.state = 'booting'

		sleep(0.5)
		while not self.stop.is_set():
			try:
				self.monitor()
			except:
				print_exc()
			if self.state in ['running', 'ready']:
				self.stop.wait(60)
			else:
				self.stop.wait(10)

		#we done
		vmremove(self)


	def __del__(self):
		print("stopping vm " + self.virtueID)
		pprint(vars(self))
		if self.p:
			self.p.terminate()
		portpool.putport(self.port)
		self.port = None
		try:
			if self.configfile:
				os.remove(self.configfile)
		except:
			print_exc()
	def setstop(self):
		self.state = 'stopping'
		self.stop.set()

	def monitor(self):
		if self.p.poll() is not None:
			self.state = 'procfail'
			#proc should be dead here
			try:
				outs,errs = self.p.communicate()
				print("stdout " + outs)
				print("stderr " + errs)
			except:
				pass
		if self.state in ['ready', 'running', 'booting', 'connfail']:
			isRunning = False
			isRunning = nettest.nettest('localhost', self.port, self.nettest)
			if isRunning:
				self.state = 'running'
			elif self.state in ['ready', 'running']:
				self.state = 'connfail'




def vmremove(d):
	global vmpool
	#todo remove this to a nicer place
	with vmpoollock:
		vmpool.pop(d.virtueID)
		del(d)

def createandadd(virtueID, imageurl, info, vconf, toport, nettest, ramsize="", cpucount=""):
	d = vmpool
	with vmpoollock:
		if virtueID in vmpool:	#todo better error reporting?
			raise
		d = davm(virtueID, imageurl, info, vconf, toport, nettest, ramsize=ramsize, cpucount=cpucount )
		vmpool[virtueID] = d
		return d

def stopid(virtueID):
	if not virtueID:
		return
	with vmpoollock:
		d = vmpool.get(virtueID)
	if d:
		d.setstop()

def list(selector):
#	pprint(selector)
	ret = []
	with vmpoollock:
		for v in vmpool.values():
			try:
				if selector:
					#todo make this into a nice all clause
					for k,va in selector.items():
	#					print("looking for " + k + " " + va)
						if v.info.get(k) != va:
							break
					else :
						rvars = vars(v).copy()
						rvars.pop("p")
						rvars.pop("configfile")
						rvars.pop("stop")
						ret.append(rvars)
				else :
					rvars = vars(v).copy()
					rvars.pop("p")
					rvars.pop("configfile")
					rvars.pop("stop")
					ret.append(rvars)

			except:
				print_exc()
	return ret

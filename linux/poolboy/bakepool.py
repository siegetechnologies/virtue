from pprint import pprint
from multiprocessing import Lock

import subprocess

from time import sleep

import random
import os

import threading

import imagecache

from traceback import print_exc

from cdconfig import genconf,rmconf



bakeconfsdir = '/tmp/bakeconfs'


bakepoollock = Lock()

bakepool = {}


def squashimage(name, image):
	try:
		fplace = imagecache.genfname(name)
		vmconvert = subprocess.run(['qemu-img', 'convert', '-O', 'qcow2', image, fplace], check=True)
	except:
		#todo better?
		print_exc()
		return image
	tsize = os.path.getsize(image)
	fsize = os.path.getsize(fplace)
	os.remove(image)
	print("\ttemp size: " + str(tsize >> 20) + "MB\n\tfinal size: " + str(fsize >> 20) + "MB\n\tdelta size: " + str((tsize - fsize)>>10) + "KB\n")
	return fplace

def gens3url(inurl):
	if 'https://s3.amazonaws.com/' not in inurl:
		raise Exception("Invalid s3 url")
	return inurl.replace( 'https://s3.amazonaws.com/', 's3://', 1)




class chef:
	def __init__(self, bakeID, srcurl, dsturl, info, vconf, ramsize="", cpucount=""):
		self.bakeID = bakeID
		self.srcurl = srcurl
		self.dsturl = dsturl
		#TODO how to get this
		self.vconf = vconf

		self.info = info

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
		self.state = 'imagedl'
		try:
			#image = imagecache.tempimage(self.srcurl)
			image = imagecache.snapshotimage( self.srcurl )
			if not image:
				raise
		except:
			print_exc()
			self.state = 'imagefail'
			return
		self.state = 'configgen'
		try:
			print("self.vconf is {}".format( self.vconf ) )
			self.configfile = genconf(bakeconfsdir + '/' + self.bakeID + '.iso', self.vconf)
		except:
			print_exc()
			self.state = 'configfail'
			return
		print("cpucount: {}, ramsize: {}".format( self.cpucount, self.ramsize ) )


		self.p = subprocess.Popen(['qemu-system-x86_64', image, '-enable-kvm',
			'-smp',  self.cpucount,
			'-m', self.ramsize,
			#'-nographic',
			"-cdrom", self.configfile,
			],
			stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
			)
		self.state = 'baking'
		sleep(0.5)

		while not self.stop.is_set():
			try:
				self.monitor()
			except:
				print_exc()
			self.stop.wait(10)
		self.stop.clear()
		if self.state == 'stopping':
			bakeremove(self)

		rmconf( self.configfile )
		print( "Baking finished. Squashing image" )
		self.state = 'squashing'
		#we are finished, lets squash
		try:
			image = squashimage(self.dsturl, image)
		except:
			print_exc()
			self.state = 'squashfail'
#			bakeremove(self) #????
			return
		print( "Squashing complete, uploading to s3?")

		if self.state == 'stopping':
			bakeremove(self)

		imagecache.manuallycache(self.dsturl, image, force=True)
		self.state = 'completed'


		#TODO this stuff should really be in its own endpoint
		# /imagecache/upload

		#self.state = 'uploading'
		#try:
		#	vmupload = subprocess.run(['aws', 's3', 'cp', image, gens3url(self.dsturl), '--acl', 'public-read'], check=True)
		#	self.state = 'completed'
		#except:
		#	print_exc()
		#	self.state = 'uploadfail'

		print( "Baking completed, waiting to notify REST" )
		while not self.stop.is_set():
			self.stop.wait(10)
		self.stop.clear()
		bakeremove(self)
		print( "Chef has completed, nice working with you, sire" )

	def __del__(self):
		print("Deleting bake " + self.bakeID)
		pprint(vars(self))
		if self.p:
			self.p.terminate()
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
			print("Qemu stopped!?")
#			pass
			self.stop.set()
#			self.state = 'ended'
		#todo more

def bakeremove(d):
	global bakepool
	with bakepoollock:
		bakepool.pop(d.bakeID)
		del(d)


def createandadd(bakeID, srcurl, dsturl, info, vconf, ramsize="", cpucount=""):
	d = bakepool
	with bakepoollock:
		if bakeID in bakepool:
			raise Exception("Duplicate bakeID {}".format( bakeID ) )
		d = chef(bakeID, srcurl, dsturl, info, vconf=vconf, ramsize=ramsize, cpucount=cpucount)
		bakepool[bakeID] = d
		return d

def stopid(bakeID):
	if not bakeID:
		return
	with bakepoollock:
		d = bakepool.get(bakeID)
	if d:
		d.setstop()



def list(selector):
	ret = []
	with bakepoollock:
		for v in bakepool.values():
			try:
				if selector:
#					pprint(selector)
#					pprint(v.info)
					for k,va in selector.items():
						if v.info.get(k) != va:
							break
					else :
						rvars = vars(v).copy()
						rvars.pop("p")
						rvars.pop("stop")
						ret.append(rvars)
				else :
					rvars = vars(v).copy()
					rvars.pop("p")
					rvars.pop("stop")
					ret.append(rvars)
			except:
				print_exc()
	return ret

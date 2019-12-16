from pprint import pprint
from multiprocessing import Lock
from urllib.request import urlretrieve
from traceback import print_exc


import subprocess
import boto3
import random

import uuid
import os
import shutil

#once we hit 3.8, dont bother with pyfastcopy because 3.8 adds the functionality
import pyfastcopy


from functools import lru_cache




@lru_cache(maxsize=1)
def accountnum():
	return boto3.client('sts').get_caller_identity()['Account']

@lru_cache(maxsize=1)
def virtuebucket():
	cl = boto3.client('s3')
	res = (cl.list_buckets())
	for bb in res['Buckets']:
		bname = bb['Name']
#		print(bname)
		try:
			btags = (cl.get_bucket_tagging(Bucket=bname))
#			pprint.pprint(btags)
			##grab the type
			for j in btags['TagSet']:
				if j['Key'] == 'type' and j['Value'].lower() == 'virtuevms':
					return bname
		except Exception as e:
#			pprint.pprint (e)
			pass
	raise Exception("warning/error.... virtue bucket not found?")



def imagedl(repoID, imageurl=""):
	os.makedirs(virtueimagesdir, exist_ok=True)
	virtuevmfile = repoID +'.qcow2'
	virtuevm = virtueimagesdir + '/' + virtuevmfile
	#todo dl-if-newer?
	if os.path.isfile(virtuevm):
		#ladies and gentlemen, we got him
		return virtuevm

	if not imageurl:
		imageurl = "https://s3.amazonaws.com/%s/%s.qcow2" % (grabVirtueBucket(), repoID)
	print("downloading vm image from %s\n" % imageurl)

	urlretrieve( imageurl, filename=virtuevm )
	print( "Download complete" )
	#todo errs?
	return virtuevm




imagecachelock = Lock()
imagecache = {}




virtueimagesdir = './virtueimages/'
tempimagesdir = './virtuetemps/'

#these should work OK if you give them a URL or just a path
#todo make sure no duplicates?
def genfname(imageurl):
	bname = imageurl.split('/')[-1].strip() + uuid.uuid4().hex
	imname = virtueimagesdir + bname
	return imname
def gentempname(imageurl):
	bname = imageurl.split('/')[-1].strip() + uuid.uuid4().hex
	imname = tempimagesdir + bname
	return imname


#can force an override
def manuallycache(imageurl, path, force=False):
	with imagecachelock:
		if not force and imagecache.get(imageurl):
			raise
		imagecache[imageurl] = path

def getimage(imageurl, force=False):
	#todo extra checks to make sure file actually exists
	with imagecachelock:
		#if already exists, we just return
		image = imagecache.get(imageurl)
		if not force and image:
			return image
		image=genfname(imageurl)
		os.makedirs(os.path.dirname(image), exist_ok=True)
		print( "Downloading imageurl {} into {}".format( imageurl, image ) )
		urlretrieve(imageurl, filename=image)
		print( "Download complete" )
		imagecache[imageurl] = image

	return image

'''
Download (or retrieve the existing copy of) the target image, and create
a qemu snapshot from it.
More info: https://wiki.qemu.org/Documentation/CreateSnapshot

Throws an exception if something fails
'''
def snapshotimage( imageurl ):
	im = getimage(imageurl)
	snapname = gentempname( imageurl )
	os.makedirs(os.path.dirname(snapname), exist_ok=True)
	#NOTE: The "."+ part here is actually important, since the base image path must
	# be given as a relative path from the snapshot. I think the way qemu
	# does this is bad, but I can only document my workaround
	# The command will look like:
	# "qemu-img create -f qcow2 -b ../virtueimages/foo ./virtuetemps/bar"
	try:
		p = subprocess.run( ["qemu-img","create","-f","qcow2","-b","."+im,snapname ], check=True )
	except CalledProcessError as e:
		print_exc()
		raise Exception("Failed to create qemu snapshot" )
	# TODO check result, raise exception
	return snapname


def tempimage(imageurl):
	im = getimage(imageurl)
	#copy it somewhere nice
	dest = gentempname(imageurl)
	os.makedirs(os.path.dirname(dest), exist_ok=True)
	shutil.copy(im, dest)
	return dest

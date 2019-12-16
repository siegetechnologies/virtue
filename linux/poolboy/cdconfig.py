import os
import random
import string

try:
	from cStringIO import StringIO as BytesIO
except ImportError:
	from io import BytesIO
import pycdlib


def genconf(path, vconf):
	#back compat
	if not isinstance(vconf, dict):
		assert( isinstance( vconf, str ) )
		vconf = { '/user-data':vconf }

	os.makedirs(os.path.dirname(path), exist_ok=True)

	iso = pycdlib.PyCdlib()
	iso.new(joliet=3, vol_ident="cidata", sys_ident="cidata")

	isonameb = set()

	for k,v in vconf.items():
		#very basic path checking... TODO more thourough
		print( "{}:{}".format(k,v) )
		if k[0] != '/':
			print("Skipping " + k)
			continue
		confstr = v.encode('utf-8')

		#https://stackoverflow.com/questions/2257441/random-string-generation-with-upper-case-letters-and-digits
		isoname= ''.join(random.choices(string.ascii_uppercase + string.digits, k=8))
		while isoname in isonameb: #keep generating them until there is a new, avoid dups
			print('dup isoname! ' + isoname)
			isoname= ''.join(random.choices(string.ascii_uppercase + string.digits, k=8))
		isonameb.add(isoname)

		iso.add_fp(BytesIO(confstr), len(confstr), '/' + isoname + '.;1', joliet_path=k)


	iso.write(path)
	iso.close()

	return path

#TODO error checking?
def rmconf( path ):
	os.remove( path )

# Find a drive
import psutil
import subprocess
import os
from pprint import pprint


def main():
	cds = [p for p in psutil.disk_partitions() if (p.fstype == 'CDFS' and 'cdrom' in p.opts) ]
	if( len( cds ) != 1 ):
		print( "No CDs found. Hopefully you wanted this" )
		return # This is bad, but I'm not sure how to handle it
	for cd in cds:
		cd_walk( cd.mountpoint )

def cd_walk( root ):
	foundnone = True
	for dirpath,dirnames,filenames in os.walk( root ):
		for fn in filenames:
			if( fn.endswith('.ps1') or fn == "user-data" ):
				print( "Found runnable file {}".format( fn ) )
				foundnone = False
				p = subprocess.Popen( [
					"C:\\Windows\\system32\\WindowsPowerShell\\v1.0\\powershell.exe",
					dirpath + fn ],stderr=subprocess.PIPE, stdout=subprocess.PIPE )
				pprint( p.stdout.read() )
				pprint( p.stderr.read() )
	if( foundnone ):
		print( "Found no runnable files" )



if (__name__ == "__main__"):
	main()

#!/usr/bin/python3

import sys
import funcs
import json
import auth

def main():
	while True:
		args = input( "-> " )
		splitters =  [x.strip() for x in args.split( ' ' ) ]
		print( splitters )
		fname = splitters[:-1]
		try:
			arg = json.loads(splitters[-1])
			print( str(funcs.parse(fname, arg)))
		except Exception as e:
			print( e )

if (__name__=="__main__"):
	main()

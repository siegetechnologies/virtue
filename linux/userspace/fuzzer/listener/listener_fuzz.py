#!/usr/bin/python3
import subprocess
import maven_us


print("this needs to run as root for the permissions for the socket\n")

prevcmd = b''

while True:
	#gather some fuzzy
	radout = subprocess.run(['radamsa','-n', '1000', '-r', './testcases/'], stdout=subprocess.PIPE).stdout.split(b'\n')


	try:
		#send it out
		for s in radout:
			#ignore duplicates
			if s == prevcmd: continue
			prevcmd = s
			if len(s) < 1: continue
			if len(s) > 50 :
				print("Trying string (truncated from " + str(len(s)) + "):"+ str(s[:50]))
			else:
				print("Trying string:" + str(s))
			maven_us.send_nl_msg(s)
	except (BlockingIOError, OSError):
		pass

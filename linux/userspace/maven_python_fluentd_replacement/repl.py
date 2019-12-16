#!/usr/bin/python3
#ref of https://pymotw.com/2/socket/uds.html

import socket
import sys
import os
import binascii

address = '/var/run/maven.sock'



def process_log_packet(data):
	try:
		#idk lol
		# real hacky test
		splitdata = data.split(b';sign_hash:b:')
		crypt = splitdata[0]
		hash = splitdata[1][8:-1]
		print(str(len(hash)) + ' ASH ' +str(binascii.hexlify(hash)))
	except:
		pass


def grab_log_packet(conn):
	data = conn.recv(1024)
	if len(data) < 1:
		return False
	print("Got " + str(data))
	process_log_packet(data)
	return True





try:
	os.unlink(address)
except OSError:
	if os.path.exists(address):
		raise

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.bind(address)
sock.listen(4)
while True:
	connection, client_addr = sock.accept()
	try:
		print("grabbin a conn")
		while grab_log_packet(connection):
			print("processed packlet")
	finally:
		connection.close()

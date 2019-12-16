#!/usr/bin/python3
import sys
import time
import os
import struct
import binascii # for hexilify
import socket
import socketserver

NL_GRP=0
NLM_F_REQUEST=1
nl_hdrlen=16

TCP_BUFFER_SIZE = 2048
TCP_TIMEOUT = 5.0

class MyTCPHandler(socketserver.BaseRequestHandler):
	"""
		The RequestHandler class for our server.

		It is instantiated once per connection to the server, and must
		override the handle() method to implement communication to the
		client.
		"""

	def handle(self):
		data = self.request.recv(1024).strip()
		print( "Sending message: {}".format( data ) )
		raw_resp = send_nl_msg( data )
		if( raw_resp is None ):
			print( "Error in sending netlink message" )
			return
		minus_nlmsg = resp[nl_hdrlen:]
		code,msg = struct.unpack( "I{}s".format( len(minus_nlmsg)-4), minus_nlmsg )
		print( "Code: {}\nmsg: {}".format( code, msg ) );
		self.request.sendall( raw_resp )


def send_nl_msg( command ):
	if isinstance(command, bytes):
		ascii_cmd = command
	else:
		ascii_cmd = command.encode('ascii')
	cmdlen = len( ascii_cmd )
	nl_sock = socket.socket( socket.AF_NETLINK, socket.SOCK_RAW, proto=socket.NETLINK_USERSOCK )
	nl_sock.bind((0,NL_GRP))
	pid = os.getpid()
	nlmsg_hdr = struct.pack( "IHHII{}s".format( cmdlen+1 ),
			nl_hdrlen+cmdlen+1,
			0,
			NLM_F_REQUEST,
			0,
			pid, # everything before here is boilerplate netlink header
			ascii_cmd)
	print( "sending header\n{}".format( binascii.hexlify(nlmsg_hdr ) ))
	assert( len( nlmsg_hdr ) == nl_hdrlen+1+cmdlen )
	try:
		nl_sock.send( nlmsg_hdr )
	except ConnectionRefusedError as e:
		print( "Connection refused, are you sure mod_listener is loaded?" )
		return None

	# receive response
	raw_resp = nl_sock.recv( 1024 )
	return raw_resp
	print( "raw response: {}".format( resp ) )
	return (code,msg)


def main():
	server = socketserver.TCPServer(('localhost', 80), MyTCPHandler)
	#server.serve_forever()
	for arg in sys.argv[1:]:        ##skip first
		inp = arg.strip()
		if(len(inp) > 0):
			send_nl_msg( inp )
	while True:
		inp = input().strip()
		if( inp ):
			send_nl_msg( inp )
		else:
			break


if (__name__=="__main__"):
	main()


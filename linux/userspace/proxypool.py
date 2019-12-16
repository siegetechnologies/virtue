from multiprocessing import Process,Lock,Queue
from queue import Empty, Full
import docker
import socket
import sys
import time
import os
import struct
import binascii # for hexilify
import json
from uuid import UUID

from collections import namedtuple

ServerData = namedtuple( 'ServerData', ['appname', 'listenport', 'handler_data'] )
HandlerData = namedtuple( 'HandlerData', ['image_name','uuid'] )
DockerPort = namedtuple( 'DockerPort', ['number', 'prevopenport'] )
QueueOptions = namedtuple( 'QueueOptions', ['new_q','recycle_q'] )

LISTEN_BACKLOG=5

NL_GRP=0
NLM_F_REQUEST=1
UUID_BYTES=16
nl_hdrlen=16

'''
Listens for traffic on recv_sock, sends anything it gets to send_sock
	without looking at it
These should be started in pairs, like so
p1 = DumbProxy( sock1, sock2 )
p2 = DumbProxy( sock2, sock1 )
This allows all traffic between 2 hosts to be connected seamlessly
If either host terminates the connection, both DumbProxys will return
'''
class DumbProxy:
	def __init__( self, recv_sock, send_sock ):
		self.recv_sock = recv_sock
		self.send_sock = send_sock
	
	def __call__( self ):
		try:
			while True:
				data = self.recv_sock.recv( 1024 )
				if( len(data)==0 ):
					break
				self.send_sock.sendall( data )
		except KeyboardInterrupt as e:
			pass
		except BrokenPipeError as e:
			print( "Proxy: broken pipe, closing" )
			pass
		except ConnectionResetError as e:
			print( "Proxy: connection reset, closing" )
			pass

		try:
			# These proxies come in pairs. Our send socket is our twin's recieve socket.
			# Shutting it down should kill him, then he'll kill our recv socket.
			# Fratricide!
			self.send_sock.shutdown( socket.SHUT_RDWR )
			self.send_sock.close()
		except OSError as e:
			# I guess it's already closed
			pass
			


'''
When we start up a docker container, we need to be able to find
an open port to bind it to on the host.
The mechanism for this is very simple, we use two queues for IPC
The first queue, the recycle_queue, is a list of ports that have
been used and returned.
Whenever a container is stopped, the port it was bound to should
be added to the recycle queue
The second queue, the new queue, is where all ports initially begin
You can think of it as a lazily evaluated infinite list that starts
	at a given number and counts up forever.
The way we achieve this is with a blocking finite sized queue with a
	single producer that is constantly pushing sequential numbers.
'''
class PortPusher:
	def __init__( self, first_port, q ):
		self.current_port=first_port
		self.q = q
	
	def __call__(self):
		try:
			while( True ):
				self.q.put( self.current_port )
				self.current_port = self.current_port + 1
		except KeyboardInterrupt:
			return


'''
An AppServer waits for connections on a server socket.
Any incoming connections are handled by a ConnHandler (go figure)
'''
class AppServer:
	def __init__( self, srv_data, srv_sock, qs, docker_client, reg_lock ):
		self.srv_data = srv_data
		self.srv_sock = srv_sock
		self.qs = qs
		self.docker_client = docker_client
		self.reg_lock = reg_lock
		self.handlers = []
		self.threads = []
	
	def __call__( self ):
		print( "Listening for connections with {}".format( self.srv_data ) )
		try:
			while( True ):
				# wait for connection, then spin up new thread
				(vnc_sock,addr) = self.srv_sock.accept()
				print( "New connection from {}".format( addr ) )
				tmp_handler = ConnHandler(
					self.docker_client,
					self.srv_data.handler_data,
					self.qs,
					vnc_sock,
					self.reg_lock )
				tmp_thr = Process( target=tmp_handler )
				tmp_thr.start()
				self.handlers.append( tmp_handler )
				self.threads.append( tmp_thr )
		except KeyboardInterrupt as e:
			self.cleanup()


	def cleanup(self):
		print( "Cleaning up AppServer" )
		self.srv_sock.shutdown( socket.SHUT_RDWR )
		self.srv_sock.close()
		for thr in self.threads:
			thr.join()


'''
A ConnHandler handles vnc connections on the given socket
When a connection is received, a docker container is started, and bound to
the first open port we can get from qs

All subsequent traffic is then proxied from the vnc socket to a server socket
in the docker container

When either side closes the connection, the proxy shuts down, and the ConnHandler
will clean up by stopping the docker container and recycling the port it was
bound to 
'''
class ConnHandler:
	def __init__( self, client, hnd_data, qs, vnc_sock, reg_lock ):
		self.client = client
		self.image_name = hnd_data.image_name
		self.uuid = hnd_data.uuid
		self.qs = qs
		self.vnc_sock = vnc_sock
		self.forward_port = None
		self.dock_sock = None
		self.container = None
		self.reg_lock = reg_lock

	def __call__( self ):
		self.forward_port = find_open_port( self.qs )
		self.reg_lock.acquire()
		ConnHandler._register_uuid( self.uuid )

		self.container = ConnHandler._start_container( self.client, self.image_name, self.forward_port )
		if( self.container is None ):
			print( "Could not start container on port {}, is already started?".format( self.forward_port) )
			ConnHandler._cancel_uuid( self.uuid )
			# all cleanup does is return the port, in this case, we don't really want to do that
			#self.cleanup()
			return
		self.reg_lock.release()
		# or we can wait for the module to send something back

		time.sleep(1) #TODO replace this with something that waits for the container to come up
		print( "Container {} started up on port {}".format( self.container.id, self.forward_port ) )

		self.dock_sock = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
		self.dock_sock.connect( ('localhost',self.forward_port) )
		t1 = Process( target=DumbProxy( self.dock_sock, self.vnc_sock ) )
		t2 = Process( target=DumbProxy( self.vnc_sock, self.dock_sock ) )
		t1.start()
		t2.start()

		try:
			t1.join()
		except KeyboardInterrupt as e:
			t1.join()
		try:
			t2.join()
		except KeyboardInterrupt as e:
			t2.join()

		self.cleanup()

	def cleanup( self ):
		print( "Removing container {}, freeing port {}".format( self.container.name, self.forward_port ) )
		if( self.container is not None ):
			self.container.remove(force=True)

		if( self.forward_port is not None ): 
			return_port( self.qs, self.forward_port )

	'''
	starts a virtue docker container
	client: the docker daemon client from docker.from_env()
	image_name: name of the image, string
	forward_port: we will forward traffic from container port 5900 to this host port
	return: a Container object. The container should be started. If we could not start
		the container, returns None
	'''
	@staticmethod
	def _start_container( client, image_name, forward_port ):
		try:
			container = client.containers.run( 
				image_name,
				detach=True,
				command="tigervncserver -fg -autokill -xstartup /usr/bin/xstartup :0",
				ports={'5900/tcp':forward_port} )
			# TODO --log-*
			# any other commands you want to run should go here
			# maybe wait for it to be up?
			return container
		except docker.errors.APIError as e:
# probably because the port is alerady bound
# there's no easy way to figure out which container is using it
			# if we need to in the future, use the APIClient directly
			print( e )
			return None

		except docker.errors.ImageNotFound as e:
			# Probably a config file error
			print( "Image not found: {}".format( image_name ) )
			return None

	# command is a 32 bit int, maybe one day it'll be an enum
	# uuid is a uuid object (from python's uuid library)
	@staticmethod
	def _send_nl_msg( command, uuid ):
		nl_sock = socket.socket( socket.AF_NETLINK, socket.SOCK_RAW, proto=socket.NETLINK_USERSOCK )
		nl_sock.bind((0,NL_GRP))
		pid = os.getpid()
		nlmsg_hdr = struct.pack( "IHHIII16s", 
			nl_hdrlen+4+16, 
			0, 
			NLM_F_REQUEST, 
			0, 
			pid, # everything before here is boilerplate netlink header
			command,
			uuid.bytes )
		print( "sending header\n{}".format( binascii.hexlify(nlmsg_hdr ) ))
		assert( len( nlmsg_hdr ) == nl_hdrlen+20 )
		try:
			nl_sock.send( nlmsg_hdr )
		except ConnectionRefusedError as e:
			print( "Connection refused, are you sure mod_registry is loaded?" )
		
	
	@staticmethod
	def _register_uuid( uuid ): # TODO rename as register_uuid
		ConnHandler._send_nl_msg( 1, uuid )

	@staticmethod
	def _cancel_uuid( uuid ):
		ConnHandler._send_nl_msg( 3, uuid )
		
'''	
static helper method to find the first open port
first, check the queue of recycled ports.
if there are no free ports there, grab a new port
qs: A QueueOptions tuple
return: An integer port or None
'''
def find_open_port( qs ):
	port = None
	try:
		port =qs.recycle_q.get_nowait( )
	except Empty:
		try:
			port = qs.new_q.get( timeout=0.5 )
		except Empty:
			print( "Warning, can't pull from any queues" )
	return port

'''
static helper method to return a port by adding it to the recycle queue
qs: A QueueOptions tuple
return: None
'''
def return_port( qs, port ):
	try:
		qs.recycle_q.put( port )
	except Full as e:
		print( "Recycle queue full???" )
		

def main():
	# ports can be generated at startup

	cfg_file = open( 'config.json' )
	cfg = json.load( cfg_file )
	cfg_file.close()

	all_mappings = []
	try:
		# unpack the configuration json into our immutable structs	
		first_docker_port = cfg['first_docker_port']
		for v in cfg['virtues']:
			all_mappings.append( 
				ServerData( v['name'], v['port'],
					HandlerData( v['image'], UUID(v['uuid']))))
		# end of config stuff
	except KeyError as e:
		print( "malformed config file" )
		print( e )

	servers = []
	threads = []
	qs = QueueOptions( 
		new_q = Queue( len(all_mappings)+1 ),
		recycle_q = Queue( )
	)

	docker_client = docker.from_env()
	reg_lock = Lock()

	try:
		for m in all_mappings:
			srv_sock = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
			srv_sock.bind( ('localhost',m.listenport) )
			srv_sock.listen( LISTEN_BACKLOG )
			servers.append( AppServer( m, srv_sock, qs, docker_client, reg_lock) )
	except OSError as e:
		print( e )
		print( "Looks like the socket is already in use, try again in a few seconds" )
		for s in servers:
			s.cleanup()
		exit( 0 )
	except:
		print( sys.exc_info()[0] )
		for s in servers:
			s.cleanup()
		exit( -1 )

	port_pusher_thread = Process( target=PortPusher( first_docker_port, qs.new_q ) )
	port_pusher_thread.start()
	
	for s in servers:
		tmp_thr = Process( target=s )
		threads.append( tmp_thr )
		tmp_thr.start()
	try:
		while( True): time.sleep( 100 )
	except KeyboardInterrupt as e:
		print( "received keyboard interrupt" )

	for thr in threads:
		thr.join()

if (__name__=="__main__"):
	main()


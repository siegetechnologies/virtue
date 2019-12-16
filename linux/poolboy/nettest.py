import socket
import ssl
from traceback import print_exc


def sshtest(host, port):
	s = None
	res = False
	try:
		s = socket.create_connection((host, port), timeout=1.0)
		s.settimeout(1.0)
		r = s.recv(4)
#		pprint(r)
		if r == b'SSH-':
			res = True
	except:
#		print_exc()
		pass

	if s:
		s.close()
	return res

def rdptest(host, port):
	try:
		s = socket.create_connection((host,port), timeout=1.0)
	except ConnectionRefusedError as e:
		# normal failure. Means the socket isn't open
		return False
	except Exception as e:
		# Might be a weird failure case
		print_exc()
		return False
	context = ssl.create_default_context()
	context.check_hostname = False
	context.verify_mode = ssl.CERT_NONE
	try:
		ss = ssl.wrap_socket( s )
		ss.close()
	except socket.timeout as e:
		# Normal failure
		return False
	except Exception as e:
		# Weird failure
		print_exc()
		return False

	return True


tests = {	"ssh":sshtest,
	 	"rdp":rdptest,
		"default":sshtest,
	}


def nettest(host, port, method):
	test = tests.get(method, tests['default'])
	return test(host, port)




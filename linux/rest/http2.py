#!/usr/bin/python3

import sys
import funcs
import json
import pprint
import ssl
from traceback import print_exc
from fluentd import log_json_input, log_json_output
from uuid import uuid4
#from mimetypes import guess_type as mrmime
if sys.version_info >= (3, 0):
	#python3
	from http.server import HTTPServer, BaseHTTPRequestHandler
	import socketserver
else:
	#python2
	from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler
	import SocketServer as socketserver





class ThreadingSimpleServer(socketserver.ThreadingMixIn, HTTPServer):
	pass


getfiles = {
	'/wsservice.exe': "./workspace/wsservice.exe",
	'/wsservice.py': "./workspace/wsservice.py",
	'/togo.py': "./workspace/togo.py",
	'/radoop.py': "./workspace/radoop.py",
	'/pc.py': "./workspace/pc.py",
	'/icon.py': "./workspace/icon.py",
	'/gui.py': "./workspace/gui.py",
	'/ezfuncs.py': "./workspace/ezfuncs.py",
	'/tester.py': "./usefules/tester.py",
	'/virtue.template': "./workspace/virtue.template",
	'/rdp.template': "./workspace/rdp.template",
	'/x2gosetup.exe': "./workspace/x2gosetup.exe",
	'/pyinst.sh': "./workspace/pyinst.sh",
	'/pybuild.sh': "./workspace/pybuild.sh",
	'/virtuelogo.png': "./workspace/virtuelogo.png",
	'/virtuelogo.png.ico': "./workspace/virtuelogo.png.ico"}

mimetypes = {
	'.png':'image/png',
	'.py':'text/x-python',
	'.template':'text/plain',
	'.exe':'application/vnd.microsoft.portable-executable'
}
#https://blog.anvileight.com/posts/simple-python-http-server/#python-3-x-1
class VCCHTTPRequestHandler(BaseHTTPRequestHandler):
	def end_headers(self):
##		self.send_header('Access-Control-Allow-Origin', '*') #TODO remove in deploym
##removed
		BaseHTTPRequestHandler.end_headers(self)

	def do_GET(self):
		self.send_response(200)
		fname = self.path
		getname = './workspace/wsservice.exe'
		if fname in getfiles:
			getname = getfiles[fname]

		self.send_header('Content-Disposition', 'attachment; filename=\"' + getname.split('/')[-1] + '\"')


#		try:
#			mtype = mrmime(getname)
#			if not mtype:
#				mtype = 'application/octet-stream'
#			self.send_header('Content-Type', mtype)
#		except:
#			pass
		try:
			endname = '.' + getname.split('.')[-1]
			self.send_header('Content-Type', mimetypes[endname])
		except:
			self.send_header('Content-Type', 'application/octet-stream')

		self.end_headers()
		with open(getname, 'rb') as f:
			self.wfile.write(f.read())

	def do_POST(self):
#		self.send_header('Content-Type', 'text/json')
		content_length = int(self.headers['Content-Length'])
		fname = self.path.split('/')[1:]
		body = self.rfile.read(content_length).decode('utf-8')
		#grab the jaysonn
		jays = json.loads(body)
		reqid = str(uuid4())
		bjays = {}
		bjays['Data'] = jays
		bjays['Log_Request_ID'] = reqid
		bjays['Log_Request_fname'] = fname
		log_json_input( json.dumps( bjays ) )


		#hack to only work on ip
		try:
			ip = self.address_string()
		except:
			print_exc()
			ip = 'bad'
		res = funcs.parse(fname, jays, ip)

		bres = {}
		bres['Data'] = res
		bres['Log_Request_ID'] = reqid

		try:
			log_json_output( json.dumps(bres) )
		except Exception as e:
			pprint.pprint( bjays )
			print( "vvvvvvvvvvvvvvvvvvvvvvvvvvvv" )
			pprint.pprint( bres )
			print_exc()
			res = {"error":"500 system did a whoopsie"}


		resp = json.dumps(res)
		self.send_response(200)
		self.end_headers()
		self.wfile.write((resp + '\n').encode('utf-8'))



def main():
	#https://kdecherf.com/blog/2012/07/29/multithreaded-python-simple-http-server/

	httpd = ThreadingSimpleServer(('', 4443), VCCHTTPRequestHandler)

	httpd.socket = ssl.wrap_socket (httpd.socket, keyfile="key.pem", certfile='cert.pem', server_side=True)
	print('We are live!\n')
	while 1:
		sys.stdout.flush()
		httpd.handle_request()

if (__name__=="__main__"):
	main()


#example
# curl -d '["virtue", "get", "yeet", "virtuecfc19a9ea3f84180a066b82e6f177f74u"]' -H "Content-Type: application/json" -X POST https://localhost:4443/ -k


#generate a key with
#openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes



#example


#login: curl -d '{"username":"admin", "password":"changeMe!"}' -H "Content-Type: application/json" -X POST https://localhost:4443/user/login -k
#create role: curl -d '{"usertoken":"+J1eDA3i7ajnbDB7EQ11UsX6oH0JItRIAn3XzopiS7DT16P91sLzn1r55qR+e1hU", "role":{"id":"nex2", "apps":["nexuiz"]}}' -H "Content-Type: application/json" -X POST https://localhost:4443/role/create -k
#create virtue: curl -d '{"usertoken":"+J1eDA3i7ajnbDB7EQ11UsX6oH0JItRIAn3XzopiS7DT16P91sLzn1r55qR+e1hU", "roleid":"nex2"}' -H "Content-Type: application/json" -X POST https://localhost:4443/virtue/create -k
#list virtues for my user: curl -d '{"usertoken":"+J1eDA3i7ajnbDB7EQ11UsX6oH0JItRIAn3XzopiS7DT16P91sLzn1r55qR+e1hU"}' -H "Content-Type: application/json" -X POST https://localhost:4443/virtue/list -k



#or just use the usefules

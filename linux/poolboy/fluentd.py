import json
from fluent import sender,event
import traceback

fluent_host = '127.0.0.1'
fluent_port = 5170 #needs to be int

fluent_port = int(fluent_port)	#just in case someone was a dummy and set it as a string

sender = sender.FluentSender( tag="rest", host=fluent_host, port=fluent_port )

def log_json_output( j ):
	try:
		log_json( 'output', j )
	except:
		traceback.print_exc()
def log_json_input( j ):
	try:
		log_json( 'input', j )
	except:
		traceback.print_exc()

def log_json( tag, j ):
	try:
#		try:
			d = json.loads( j )
			sender.emit( tag, d )
#		except JSONDecodeError:
#			sender.emit( tag, {"malformed json": j } )
#			raise
	except:
		traceback.print_exc()

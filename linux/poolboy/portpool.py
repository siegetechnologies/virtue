from multiprocessing import Lock


ppoollock = Lock()
ppool = set(range(30000, 60000))


import random

#todo exceptions
def getport():
	global ppool
	with ppoollock:
		p = random.sample(ppool,1)[0]
		ppool.remove(p)
		return p

def putport(port):
	global ppool
	with ppoollock:
		try:
			ppool.add(port)
		except:
			pass

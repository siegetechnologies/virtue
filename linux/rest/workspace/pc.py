import sys
#https://stackoverflow.com/questions/287871/print-in-terminal-with-colors
#class bc:
HEADER = '\033[95m'
OKBLUE = '\033[94m'
OKGREEN = '\033[92m'
WARNING = '\033[93m'
FAIL = '\033[91m'
ENDC = '\033[0m'
BOLD = '\033[1m'
UNDERLINE = '\033[4m'

#	@staticmethod
#just for nice printing (as well as flushing the terminal)
def pc(stran, color = HEADER):
	print(color + stran + ENDC)
	sys.stdout.flush()

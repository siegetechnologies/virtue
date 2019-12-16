#!/usr/bin/python3
import argparse
import re
import maven_us
# from https://www.adampalmer.me/iodigitalsec/2014/11/21/performing-dns-queries-python/
import dns.resolver
myResolver = dns.resolver.Resolver()


def get_ips_from_domain( domain ):
	try:
		return [str(a) for a in myResolver.query(domain, "A")]
	except:
		return []

def parserulefile(filename):
	with open(filename) as f:
		for line in f:
			cols = line.strip().split()
			if(len(cols) < 1 or len(cols[0]) < 1): continue
			command = ''
			if(cols[0][0] == 'B'): command = 'add_netrule black'
			elif(cols[0][0] == 'A'): type = 'add_netrule white'

			if(len(command)<1):
				continue
			#re-combine the rest
			string = ' '.join(cols[1:])
			strings = [string]

			#check  if we are a domain name, if so, replace stringlist with list of ips
			if(re.search('[a-zA-Z]', string)):
				domain_name = string.split('/')[0].strip()
				print("grabbing all IPs associated with " + domain_name)
				strings = get_ips_from_domain(domain_name)
			for s in strings:
				print(command + " " + s)
				maven_us.send_nl_msg(command + " " + s)

def main():
##todo argparse
	parser = argparse.ArgumentParser(description='netrule ease_of_use script')
	parser.add_argument('input_files', metavar='input_files', type=str, nargs='+')
	args = parser.parse_args()

	for f in args.input_files:
		parserulefile(f)

	return

if(__name__=="__main__"):
	main()

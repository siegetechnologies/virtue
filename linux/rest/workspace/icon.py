from PIL import Image
import pc
import os
from pathlib import Path
import urllib.request, urllib.parse
import ssl
gcontext = ssl.SSLContext(ssl.PROTOCOL_TLSv1)  # Only for gangstars

def crfolder():
	iconpath = str(Path.home()) + "/.virtueicons/"
	os.makedirs(iconpath, exist_ok=True)

def convimg(inm, out):
	pc.pc('Converting ' + inm + ' to ' + out)
	img = Image.open(inm)
	img.save(out)
#todo config an icon dir to download to

def geticon(where, to):
	if not os.path.exists(to):
		res = urllib.request.urlopen(where, context=gcontext)
		with open(to, 'wb') as f:
			f.write(res.read())
	if not to.lower().endswith('.ico'):
		if not os.path.exists(to + '.ico'):
			convimg(to, to + '.ico')
		return to + '.ico'
	return to


def cacheicons(virtues):
	iconpath = str(Path.home()) + "/.virtueicons/"
	os.makedirs(iconpath, exist_ok=True)
	for k,v in virtues.items():
		for a in v['applications']:
			try:
				icon = a['icon']
				if not icon:
					continue
				iname = icon.split('/')[-1].split("\\")[-1]
				iconfull = iconpath + iname
				a['iconpath'] = geticon(icon, iconfull)
				'''
				a['iconpath'] = iconfull
				if not os.path.exists(iconfull):
					res = urllib.request.urlopen(icon, context=gcontext)
					with open(iconfull, 'wb') as f:
						f.write(res.read())
				if not iconfull.lower().endswith('.ico'):
					if not os.path.exists(iconfull + '.ico'):
						convimg(iconfull, iconfull + '.ico')
					a['iconpath'] = iconfull + '.ico'

				'''
			except Exception as e:
				pc.pc(str(e), pc.FAIL)
				continue
		#update it
		virtues[k] = v


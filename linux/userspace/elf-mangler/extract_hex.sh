#!/bin/sh
	echo "" > ${2}
	EXTRACT_SECTION_NAMES=$(rabin2 -S ./${1} | grep -v 'Sections' | tr -s ' ' |  cut -d ' ' -f 7 )
	for s in ${EXTRACT_SECTION_NAMES}; do
		STRNAME=$(echo "hex${s}" | sed "s/\./_/g")
		HEX=$(rabin2 -O  d/S/${s} ./${1} 2>/dev/null| sed "s/.\{2\}/\\\x&/g")
		echo "char ${STRNAME}[] = \"${HEX}\";" >> ${2}
	done;

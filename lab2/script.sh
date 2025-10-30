#!/bin/sh
S=/shared L=$S/.lock I=$(hostname|cut -c1-8) C=0
mkdir -p $S; touch $L

trap 'rm -f "$S/$f" 2>/dev/null; exit' EXIT TERM INT

while :; do
	{
		flock -x 200
		i=1
		while [ $i -le 999 ]; do
			f=$(printf %03d $i) 
			p=$S/$f
			if [ ! -e "$p" ]; then
				C=$((C+1))
				echo "$I $C" > "$p"
				break
			fi
			i=$((i+1))
		done
	} 200>"$L"
	sleep 1
	rm -f "$S/$f"
	sleep 1
done

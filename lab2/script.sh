#!/bin/sh
S=/shared L=$S/.lock I=$(hostname|cut -c1-8) C=0
mkdir -p $S; touch $L

while :; do
	exec 200>"$L"
	flock -x 200
	
	i=1
	f=
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
	
	flock -u 200
	exec 200>&-
	
	if [ -n "$f" ]; then
		sleep 1
		rm -f "$S/$f"
		sleep 1
	else
		sleep 2
	fi
done

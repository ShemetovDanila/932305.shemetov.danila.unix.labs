#!/bin/sh
S=/shared L=$S/.lock I=$(hostname|cut -c1-8) C=0
mkdir -p $S; touch $L

exec 200>"$L"
while :; do
	flock -x 200
	i=1
	f=
	while [ $i -le 999 ]; do
		f=$(printf %03d $i); p=$S/$f
		if [ ! -e "$p" ]; then
			C=$((C+1))
			echo "$I $C" > "$p"
			echo $f
		fi
		i=$((i+1))
	done
	flock -u 200

	sleep 1
	[ -n "$f" ] && rm -f "$S/$f"
	sleep 1
done

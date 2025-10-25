#!/bin/sh
S=/shared L=$S/.lock I=$(hostname|cut -c1-8) C=0 N=/tmp/n
mkdir -p $S; touch $L
trap 'rm -f $N; exit' INT TERM EXIT
while :; do
	(
		flock -x 200
		i=1
		while [ $i -le 999 ]; do
			f=$(printf %03d $i); p=$S/$f
			[ ! -e "$p" ] && { C=$((C+1)); echo "$I $C" >"$p"; echo $f >$N; break;}
			i=$((i+1))
		done
	) 200>"$L"
	sleep 1
	[ -f $N ] && { f=$(cat $N); rm -f $S/$f $N; }
	sleep 1
done

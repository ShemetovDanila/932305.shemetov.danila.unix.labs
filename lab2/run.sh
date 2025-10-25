#!/bin/sh
n=$(1:-1); v=vol; i=lab2
while [ $((n--)) -ge 0 ]; do
	docker run -d --rm --label l2 -v $v:/share $i
done

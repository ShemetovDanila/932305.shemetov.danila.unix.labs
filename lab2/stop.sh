#!/bin/sh
docker stop $(docker ps -q --filter label=l2)

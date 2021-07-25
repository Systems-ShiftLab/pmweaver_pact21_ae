#!/usr/bin/env sh

(cd /pmweaver_ae/scripts/ && python3 -m http.server 8888 || echo "ERROR: Couldn't start the server" )

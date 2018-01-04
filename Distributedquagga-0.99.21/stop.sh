#!/bin/bash
kill `cat /var/run/zebra.pid`
#kill `cat /var/run/ospf6d.pid`
kill `cat /var/run/ospfd.pid`

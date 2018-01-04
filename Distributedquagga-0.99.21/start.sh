#!/bin/bash
echo "Nowe uruchomienie modułu zebra" > /var/log/quagga/zebra.log
echo "Nowe uruchomienie modułu ospf6d" > /var/log/quagga/ospf6d.log

touch /var/run/zebra.pid
chmod 666 /var/run/zebra.pid
./zebra/zebra -f /etc/quagga/zebra.conf -d -g root -u root

#touch /var/run/ospf6d.pid
#chmod 666 /var/run/ospf6d.pid
#./ospf6d/ospf6d -f /etc/quagga/ospf6d.conf -d

touch /var/run/ospfd.pid
chmod 666 /var/run/ospfd.pid
./ospfd/ospfd -f /etc/quagga/ospfd.conf -d

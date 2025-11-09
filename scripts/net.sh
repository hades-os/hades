#!/bin/bash

ip tuntap add tap0 mode tap
ip addr add 192.168.100.1/24 tap0
sysctl net.ipv4.ip_forward=1

iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
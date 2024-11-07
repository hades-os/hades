#!/bin/bash

ip tuntap add mode tap tap0
ip addr add 192.168.100.1/24 dev tap0
sysctl net.ipv4.ip_forward=1

iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
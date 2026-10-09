#!/bin/bash
# Truncate active log files
sudo truncate -s 0 /var/log/syslog
sudo truncate -s 0 /var/log/kern.log
sudo truncate -s 0 /var/log/auth.log
sudo truncate -s 0 /var/log/dpkg.log
sudo truncate -s 0 /var/log/alternatives.log
sudo truncate -s 0 /var/log/dmesg
sudo truncate -s 0 /var/log/fontconfig.log
sudo truncate -s 0 /var/log/ubuntu-advantage.log
sudo truncate -s 0 /var/log/Xorg.0.log
sudo truncate -s 0 /var/log/btmp
sudo truncate -s 0 /var/log/wtmp
sudo truncate -s 0 /var/log/lastlog

# Remove rotated and old log files
sudo rm -f /var/log/*.gz
sudo rm -f /var/log/*.[0-9]
sudo rm -f /var/log/*.old

sudo apt clean

sudo journalctl --vacuum-size=100M
rm -rf /home/firefly/.vscode-server
rm -rf /home/firefly/.cache/*
rm -rf /home/firefly/.ros/log
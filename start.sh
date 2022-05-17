#!/bin/bash

sudo systemctl start elasticsearch
sudo systemctl is-active -q elasticsearch && echo "elasticsearch is running" || echo "elasticsearch is not running"

sudo systemctl start logstash
sudo systemctl is-active -q logstash && echo "logstash is running" || echo "logstash is not running"

sudo systemctl start kibana
sudo systemctl is-active -q kibana && echo "kibana is running" || echo "kibana is not running"

sudo systemctl start filebeat
sudo systemctl is-active -q filebeat && echo "filebeat is running" || echo "filebeat is not running"

export PATH=/usr/local/zeek/bin:$PATH
zeekctl deploy
echo "Zeek Running"
#sudo systemctl is-active -q zeek && echo "zeek is running" || echo "zeek is not running"

echo "Menjalankan Machine Learning setiap berapa menit ?"
echo "1. 10 Menit Sekali"
echo "2. 20 Menit Sekali"
echo "3. 30 Menit Sekali (Default)"
read options

#temp
if [ $options -eq 10 ]
then
        #echo "/bin/bash -c 'echo "*/10 * * * * root /home/ubuntu/Downloads/StratosphereLinuxIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log" >> /etc/crontab'"
	crontab -l | grep -Fq '*/10 * * * * root /home/ubuntu/Downloads/StratosphereLinuxIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log' && echo 'Sudah ada' || echo 'Crontab telah dibuat' && (crontab -l | echo "*/2 * * * * root Downloads/StratosphereIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log") | crontab -
	echo "Akan menjalankan Machine Learning setiap 10 menit"
elif [ $options -eq 20 ]
then
        #echo "/bin/bash -c 'echo "*/20 * * * * root Downloads/StratosphereLinuxIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log" >> /etc/crontab'"
	crontab -l | grep -Fq '*/20 * * * * root /home/ubuntu/Downloads/StratosphereLinuxIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log' && echo 'Sudah ada' || echo 'Crontab telah dibuat' && (crontab -l | echo "*/20 * * * * root Downloads/StratosphereIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log") | crontab -
	echo "Akan menjalankan Machine Learning setiap 20 menit"
else
        #echo "/bin/bash -c 'echo "*/30 * * * * root Downloads/StratosphereIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log" >> /etc/crontab'"
	crontab -l | grep -Fq '*/30 * * * * root /home/ubuntu/Downloads/StratosphereLinuxIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log' && echo 'Sudah ada' || echo 'Crontab telah dibuat' && (crontab -l | echo "*/30 * * * * root Downloads/StratosphereIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log") | crontab -
	echo "Akan menjalankan Machine Learning setiap 30 menit"
fi

echo "Buka link berikut untuk membuka sistem monitoring"
echo "http://localhost:5601"

exec bash

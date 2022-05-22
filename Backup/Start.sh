#!/bin/bash

echo "$(tput setaf 1) ---- Starting Elasticsearch Service ----"
sudo systemctl start elasticsearch
sudo systemctl is-active -q elasticsearch && echo "elasticsearch is running" || echo "elasticsearch is not running"

echo ""
echo ""
echo "$(tput setaf 1) ---- Starting Logstash Service ----"
sudo systemctl start logstash
sudo systemctl is-active -q logstash && echo "logstash is running" || echo "logstash is not running"

echo ""
echo ""
echo "$(tput setaf 1) ---- Starting Kibana Service ----"
sudo systemctl start kibana
sudo systemctl is-active -q kibana && echo "kibana is running" || echo "kibana is not running"

echo ""
echo ""
echo "$(tput setaf 1) ---- Starting Filebeat Service ----"
sudo systemctl start filebeat
sudo systemctl is-active -q filebeat && echo "filebeat is running" || echo "filebeat is not running"

echo ""
echo ""
export PATH=/opt/zeek/bin:$PATH
zeekctl deploy
echo "Zeek Running"
#sudo systemctl is-active -q zeek && echo "zeek is running" || echo "zeek is not running"

echo ""
echo ""

echo "Masukan PATH directory Slips.py pada folder StratosphereLinuxIPS secara lengkap (Ex: /home/ubuntu/Downloads/StratosphereLinux/Slips.py)"
read pathslips

echo "Menjalankan Logstash & Machine Learning setiap berapa menit ?(Tuliskan saja angka nya (Ex: 1) "
echo "1. 30 Menit Sekali"
echo "2. 20 Menit Sekali"
echo "3. 10 Menit Sekali (Default)"
read options

#temp
if [ $options -eq 1 ]
then
	redis-server --daemonize yes
    #echo "/bin/bash -c "echo "*/30 * * * * root "$pathslips" -c slips.conf -f /usr/local/zeek/logs/current/conn.log" >> /etc/crontab""
	crontab -l | grep -Fq "*/30 * * * * root "$pathslips" -c slips.conf -f /usr/local/zeek/logs/current/conn.log" && echo "Sudah ada" || echo "Crontab telah dibuat" && (crontab -l | echo -e "*/30 * * * * root "$pathslips" -c slips.conf -f /usr/local/zeek/logs/current/conn.log\n*/30 * * * * root /usr/share/logstash/bin/logstash -f /etc/logstash/conf.d/zeek.conf") | crontab -
	echo "Logstash & Machine Learning akan running setiap 30 menit"
elif [ $options -eq 2 ]
then
	redis-server --daemonize yes
    #echo "/bin/bash -c 'echo "*/20 * * * * root Downloads/StratosphereLinuxIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log" >> /etc/crontab'"
	#crontab -l | grep -Fq '*/20 * * * * root /home/ubuntu/Downloads/StratosphereLinuxIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log' && echo 'Sudah ada' || echo 'Crontab telah dibuat' && (crontab -l | echo "*/20 * * * * root Downloads/StratosphereIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log") | crontab -
	crontab -l | grep -Fq "*/10 * * * * root "$pathslips" -c slips.conf -f /usr/local/zeek/logs/current/conn.log" && echo "Sudah ada" || echo "Crontab telah dibuat" && (crontab -l | echo -e "*/20 * * * * root "$pathslips" -c slips.conf -f /usr/local/zeek/logs/current/conn.log\n*/20 * * * * root /usr/share/logstash/bin/logstash -f /etc/logstash/conf.d/zeek.conf") | crontab -	
	echo "Logstash & Machine Learning akan running setiap 20 menit"
else
	redis-server --daemonize yes
    #echo "/bin/bash -c 'echo "*/30 * * * * root Downloads/StratosphereIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log" >> /etc/crontab'"
	#crontab -l | grep -Fq '*/10 * * * * root /home/ubuntu/Downloads/StratosphereLinuxIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log' && echo 'Sudah ada' || echo 'Crontab telah dibuat' && (crontab -l | echo "*/30 * * * * root Downloads/StratosphereIPS/slips.py -c slips.conf -f /usr/local/zeek/logs/current/conn.log") | crontab -
	crontab -l | grep -Fq "*/10 * * * * root "$pathslips" -c slips.conf -f /usr/local/zeek/logs/current/conn.log" && echo "Sudah ada" || echo "Crontab telah dibuat" && (crontab -l | echo -e "*/10 * * * * root "$pathslips" -c slips.conf -f /usr/local/zeek/logs/current/conn.log\n*/10 * * * * root /usr/share/logstash/bin/logstash -f /etc/logstash/conf.d/zeek.conf") | crontab -
	echo "Logstash & Machine Learning akan running setiap 10 menit"
fi

echo ""

echo "Buka link berikut untuk membuka sistem monitoring"
echo "http://localhost:5601"

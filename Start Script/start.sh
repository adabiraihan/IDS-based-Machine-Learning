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

echo "Masukan PATH directory Slips.py pada folder StratosphereLinuxIPS secara lengkap (Ex: /home/ubuntu/Downloads/StratosphereLinuxIPS)"
read pathslips

echo "Menjalankan Logstash & Machine Learning setiap berapa menit ?(Tuliskan saja angka nya (Ex: 1) "
echo "1. Machine Learning 30 Menit Sekali & Parsing Data 35 Menit Sekali"
echo "2. Machine Learning 20 Menit Sekali & Parsing Data 25 Menit Sekali"
echo "3. Machine Learning 10 Menit Sekali & Parsing Data 15 Menit Sekali (Default)"
read options

#temp
if [ $options -eq 1 ]
then
	redis-server --daemonize yes
	crontab -l | grep -Fq "*/30 * * * * cd "$pathslips" && ./slips.py -c slips.conf -f /opt/zeek/logs/current/conn.log" && echo "Sudah ada" || echo "Crontab telah dibuat" && (crontab -l | echo -e "*/30 * * * * cd "$pathslips" && ./slips.py -c slips.conf -f /opt/zeek/logs/current/conn.log\n*/35 * * * * /usr/share/logstash/bin/logstash -f /etc/logstash/conf.d/zeek.conf") | crontab -
	echo "Logstash & Machine Learning akan running setiap 30 menit"
elif [ $options -eq 2 ]
then
	redis-server --daemonize yes
	crontab -l | grep -Fq "*/20 * * * * cd "$pathslips" && ./slips.py -c slips.conf -f /opt/zeek/logs/current/conn.log" && echo "Sudah ada" || echo "Crontab telah dibuat" && (crontab -l | echo -e "*/20 * * * * cd "$pathslips" && ./slips.py -c slips.conf -f /opt/zeek/logs/current/conn.log\n*/25 * * * * /usr/share/logstash/bin/logstash -f /etc/logstash/conf.d/zeek.conf") | crontab -
	echo "Logstash & Machine Learning akan running setiap 20 menit"
else
	redis-server --daemonize yes
	crontab -l | grep -Fq "*/10 * * * * cd "$pathslips" && ./slips.py -c slips.conf -f /opt/zeek/logs/current/conn.log" && echo "Sudah ada" || echo "Crontab telah dibuat" && (crontab -l | echo -e "*/10 * * * * cd "$pathslips" && ./slips.py -c slips.conf -f /opt/zeek/logs/current/conn.log\n*/15 * * * * /usr/share/logstash/bin/logstash -f /etc/logstash/conf.d/zeek.conf") | crontab -
	echo "Logstash & Machine Learning akan running setiap 10 menit"
fi

echo ""

echo "Buka link berikut untuk membuka sistem monitoring"
echo "http://localhost:5601"

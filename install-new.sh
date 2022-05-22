#!/bin/bash

echo "$(tput setaf 1) ---- Creating Folder for Zeek & Slips ----"

cd /home/

echo ""
echo ""

echo "$(tput setaf 3) ---- Ketikan di direktori mana folder akan di buat (ex: /home/ubuntu/Downloads/  )     ----"

echo "Tujuan Direktori : "
read directory

cd $directory

echo ""
echo ""

echo "$(tput setaf 3) ---- Ketikan Nama Folder yang ingin dibuat (ex: ProjecTA/ )     ----"
echo "Nama Folder : "

read folder
mkdir $folder

cd $directory$folder

echo ""

echo "Current Directory : "

pwd

echo ""
echo ""

echo "$(tput setaf 1) ---- Performing upgrade ----"

apt update -y
apt upgrade -y


echo ""
echo ""

echo "$(tput setaf 1) ---- Installing Zeek (Intrusion Detection System) ----"

apt-get install cmake make gcc g++ flex bison libpcap-dev libssl-dev python3 python3-dev swig zlib1g-dev git curl net-tools -y

echo ""
echo ""
echo ""

echo "Install di ubuntu versi berapa ? (Tuliskan Angka saja (ex: 1)"

echo "1. Ubuntu 20.04 (Default)"

echo "2. Ubuntu 22.04"

echo "3. Ubuntu 18.04"

read versi


if [ $versi -eq 1 ] 

then
	echo 'deb http://download.opensuse.org/repositories/security:/zeek/xUbuntu_20.04/ /' | sudo tee /etc/apt/sources.list.d/security:zeek.list
	curl -fsSL https://download.opensuse.org/repositories/security:zeek/xUbuntu_20.04/Release.key | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/security_zeek.gpg > /dev/null
	sudo apt update -y
	sudo apt install zeek -y
elif [ $versi -eq 2 ] 

then
	echo 'deb http://download.opensuse.org/repositories/security:/zeek/xUbuntu_22.04/ /' | sudo tee /etc/apt/sources.list.d/security:zeek.list
	curl -fsSL https://download.opensuse.org/repositories/security:zeek/xUbuntu_22.04/Release.key | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/security_zeek.gpg > /dev/null
	sudo apt update -y
	sudo apt install zeek -y

else 
	echo 'deb http://download.opensuse.org/repositories/security:/zeek/xUbuntu_18.04/ /' | sudo tee /etc/apt/sources.list.d/security:zeek.list
	curl -fsSL https://download.opensuse.org/repositories/security:zeek/xUbuntu_18.04/Release.key | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/security_zeek.gpg > /dev/null
	sudo apt update -y
	sudo apt install zeek -y
fi

export PATH=/opt/zeek/bin:$PATH

#git clone --recursive https://github.com/zeek/zeek
#zeekstring="zeek"
#cd $directory$folder$zeekstring
#./configure && make && make install

echo ""
echo ""

echo "$(tput setaf 1) ---- Installing Slips (Machine Learning) ----"

apt install pip -y
apt install redis-server -y
pip install maxminddb 
pip install numpy 
pip install watchdog 
pip install redis 
pip install urllib3 
pip install pandas 
pip install tzlocal 
pip install cabby 
pip install stix2 
pip install certifi 
pip install tensorflow 
pip install colorama 
pip install keras 
pip install validators 
pip install ipwhois 
pip install matplotlib 
pip install recommonmark 
pip install scikit_learn 
pip install slackclient 
pip install psutil 
pip install yara_python 
pip install six 
pip install pytest 
pip install pytest-xdist 
pip install scipy 
pip install sklearn 
pip install gitpython 
pip install setuptools 
pip install whois 
pip install wheel

git clone https://github.com/stratosphereips/StratosphereLinuxIPS

echo ""
echo ""

echo "$(tput setaf 3) ---- Jangan Lupa untuk Configure file slips.conf ----"

echo ""

echo "$(tput setaf 1) ---- Installing Java 8 ----"

apt install openjdk-8-jdk -y

# Install Elasticsearch Debian Package 

# ref: https://www.elastic.co/guide/en/elasticsearch/reference/current/deb.html

echo ""

echo "$(tput setaf 1) ---- Setting up public signing key ----"

#wget -qO - https://artifacts.elastic.co/GPG-KEY-elasticsearch | sudo apt-key add -
wget -qO - https://artifacts.elastic.co/GPG-KEY-elasticsearch | sudo apt-key add -


echo ""


echo "$(tput setaf 1) ---- Installing the apt-transport-https package ----"

apt-get install apt-transport-https -y

apt update -y

echo "$(tput setaf 1) ---- Saving Repository Definition to /etc/apt/sources/list.d/elastic-7.x.list ----"

echo "deb https://artifacts.elastic.co/packages/7.x/apt stable main" | sudo tee -a /etc/apt/sources.list.d/elastic-7.x.list


echo ""
echo ""


echo "$(tput setaf 1) ---- Installing the Elasticsearch Debian Package ----"

apt-get update -y 
apt-get install elasticsearch -y



echo ""
echo ""


echo "$(tput setaf 1) ---- Installing the Kibana Debian Package ----"

apt-get install kibana -y


echo ""
echo ""


echo "$(tput setaf 1) ---- Installing Logstash ----"

apt-get install logstash -y


echo ""
echo ""


echo "$(tput setaf 1) ---- Installing Filebeat ----"

#curl -L -O https://artifacts.elastic.co/downloads/beats/filebeat/filebeat-8.2.0-amd64.deb

apt-get install filebeat -y

#dpkg -i filebeat-8.2.0-amd64.deb

rm filebeat*

#filebeat modules enable system
#filebeat modules enable netflow
#filebeat modules enable osquery
#filebeat modules enable elasticsearch
#filebeat modules enable kibana
#filebeat modules enable logstash
filebeat modules enable zeek

echo ""
echo ""

echo "$(tput setaf 1) ---- Starting Elasticsearch ----"

#sudo systemctl daemon-reload
#sudo systemctl enable elasticsearch.service
systemctl start elasticsearch.service

echo ""
echo ""

echo "$(tput setaf 1) ---- Starting Kibana ----"

#systemctl enable kibana.service
systemctl start kibana.service

echo ""
echo ""

echo "$(tput setaf 1) ---- Starting Logstash ----"

#systemctl enable logstash.service
systemctl start logstash.service

echo ""
echo ""

echo "$(tput setaf 1) ---- Starting Filebeat ----"

#systemctl enable filebeat
systemctl start filebeat

filebeat setup -e
filebeat setup --dashboards
filebeat setup --index-management
filebeat setup --pipelines

echo ""
echo ""

echo "$(tput setaf 1) ---- Install Finish ----"

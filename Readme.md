**Product Description**

This system can detect an attack like port scanning, c&c channel, and many more using anomaly detection techniques.

The traffic log are generated by IDS and will be process by machine learning to classify the attack, and later on the results will be visualized in elk dashboard. 

In this system there will be 3 of open-source tools that plays an important role :

<br>
<br>

<img width="138" alt="Zeek" src="https://user-images.githubusercontent.com/43380235/169695699-0a33f0a1-7b38-44c9-9c54-e093ae7ff411.png">

**Zeek Intrusion Detection System (IDS)**
--
Zeek is a passive, open-source network traffic analyzer. Many operators use Zeek as a network security monitor (NSM) to support investigations of suspicious or malicious activity. Zeek also supports a wide range of traffic analysis tasks beyond the security domain, including performance measurement and troubleshooting, in this case i use it because the log generated by zeek is possible to process by machine learning process. <br>
Source : https://github.com/zeek/zeek

<br>
<br>

<center> <img width="138" alt="Slips" src="https://user-images.githubusercontent.com/43380235/169695789-a290d114-2f6c-471e-9c94-87d2322085aa.png"> </center>

**StratospherelinuxIPS (Machine Learning Process)**
--
StratospeherelinuxIPS or Slips is a behavioral-based Python intrusion prevention system that uses machine learning to detect malicious behaviors in the network traffic. Slips was designed to focus on targeted attacks, detection of command and control channels to provide good visualisation for the analyst. Slips is a modular software. <br>
Source : https://github.com/stratosphereips/StratosphereLinuxIPS

<br>
<br>

<img width="138" alt="ELK" src="https://user-images.githubusercontent.com/43380235/169695696-7b2cd0f0-0a2e-4ea7-b5d6-b32772c68203.png">

**ELK Stack / Elasticsearch, Logstash, and Kibana (Log Management, SIEM, and Data Visualization)**
--
ELK is the acronym for three open source projects: Elasticsearch, Logstash, and Kibana. Elasticsearch is a search and analytics engine. Logstash is a server???side data processing pipeline that ingests data from multiple sources simultaneously, transforms it, and then sends it to a "stash" like Elasticsearch. Kibana lets users visualize data with charts and graphs in Elasticsearch. <br>
Source : https://github.com/elastic

<br>

**How to Install**
--
Note: Run all this script with superuser (root), to prevent from failing when installing

1. Run install.sh in Install Requirement folder and follow the instructions, it will automatically install 3 of open-source tools.
``` ./install.sh ```

2. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Pellentesque aliquam id lectus eget pharetra.
 
3. Run start.sh in Start folder and follow the instructions
``` ./start.sh ```

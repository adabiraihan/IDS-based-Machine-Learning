# Stratosphere Linux IPS. A machine-learning Intrusion Detection System
# Copyright (C) 2021 Sebastian Garcia

# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
# Contact: eldraco@gmail.com, sebastian.garcia@agents.fel.cvut.cz, stratosphere@aic.fel.cvut.cz

import multiprocessing
import sys
from datetime import datetime
from datetime import timedelta
import os
import threading
import time
from slips_files.core.database import __database__
import configparser
import json


def timing(f):
    """ Function to measure the time another function takes. It should be used as decorator: @timing"""
    def wrap(*args):
        time1 = time.time()
        ret = f(*args)
        time2 = time.time()
        print('function took {:.3f} ms'.format((time2-time1)*1000.0))
        return ret
    return wrap


# Logs output Process
class LogsProcess(multiprocessing.Process):
    """ A class to output data in logs files """
    def __init__(self, inputqueue, outputqueue, verbose, debug, config, mainfoldername):
        self.name = 'Logs'
        multiprocessing.Process.__init__(self)
        self.verbose = verbose
        self.debug = debug
        self.config = config
        # Start the DB
        __database__.start(self.config)
        self.separator = '_'
        # From the config, read the timeout to read logs. Now defaults to 5 seconds
        self.inputqueue = inputqueue
        self.outputqueue = outputqueue
        # Read the configuration
        self.read_configuration()
        self.fieldseparator = __database__.getFieldSeparator()
        # For some weird reason the database loses its outputqueue and we have to re set it here.......
        __database__.setOutputQueue(self.outputqueue)
        self.mainfoldername = mainfoldername
        self.timeline_first_index = {}
        #self.stop_counter = 0
        self.is_timline_file = False

    def read_configuration(self):
        """ Read the configuration file for what we need """
        # Get the time of log report
        try:
            self.report_time = int(self.config.get('parameters', 'log_report_time'))
        except (configparser.NoOptionError, configparser.NoSectionError, NameError):
            # There is a conf, but there is no option, or no section or no configuration file specified
            self.report_time = 5
        self.outputqueue.put('01|logs|Logs Process configured to report every: {} seconds'.format(self.report_time))

    def print(self, text, verbose=1, debug=0):
        """
        Function to use to print text using the outputqueue of slips.
        Slips then decides how, when and where to print this text by taking all the processes into account
        :param verbose:
            0 - don't print
            1 - basic operation/proof of work
            2 - log I/O operations and filenames
            3 - log database/profile/timewindow changes
        :param debug:
            0 - don't print
            1 - print exceptions
            2 - unsupported and unhandled types (cases that may cause errors)
            3 - red warnings that needs examination - developer warnings
        :param text: text to print. Can include format like 'Test {}'.format('here')
        """

        levels = f'{verbose}{debug}'
        self.outputqueue.put(f"{levels}|{self.name}|{text}")

    def run(self):
        try:
            # Create our main output folder. The current datetime with microseconds
            # TODO. Do not create the folder if there is no data? (not sure how to)
            self.print('Using the folder {} for storing results.'.format(self.mainfoldername))
            # go into the main  folder
            os.chdir(self.mainfoldername)

            # Process the data with different strategies
            # Strategy 1: Every X amount of time
            # Create a timer to process the data every X seconds
            timer = TimerThread(self.report_time, self.process_global_data)
            timer.start()

            while True:
                line = self.inputqueue.get()
                if 'stop_process' in line:
                    timer.shutdown()
                    return True
                elif 'stop' != line:
                    # CHECK if we ever go here...
                    # we are not processing input from the queue yet
                    # without this line the complete output thread does not work!!
                    # WTF???????
                    print(line)
                    pass
                else:
                    # CHECK if we ever go here...
                    # Here we should still print the lines coming in the input for a while after receiving a 'stop'. We don't know how to do it.
                    self.outputqueue.put('stop')
                    timer.shutdown()
                    return True
            # Stop the timer
            timer.shutdown()

        except KeyboardInterrupt:
            # Stop the timer
            timer.shutdown()
            return True
        except Exception as inst:
            # Stop the timer
            try:
                timer.shutdown()
            except UnboundLocalError:
                # The timer variable didn't exist, so just end
                pass


            self.outputqueue.put('01|logs|\t[Logs] Error with LogsProcess')
            self.outputqueue.put('01|logs|\t[Logs] {}'.format(type(inst)))
            self.outputqueue.put('01|logs|\t[Logs] {}'.format(inst))
            sys.exit(1)

    def createProfileFolder(self, profileid):
        """
        Receive a profile id, create a folder if its not there. Create the log files.
        """
        # Ask the field separator to the db
        profilefolder = profileid.split(self.fieldseparator)[1].replace(':','-')
        if not os.path.exists(profilefolder):
            os.makedirs(profilefolder)
            ip = profileid.split(self.fieldseparator)[1]
            # If we create the folder, add once there the profileid. We have to do this here if we want to do it once.
            self.addDataToFile(profilefolder + '/' + 'ProfileData.txt', 'Profiled IP: ' + ip)

            # Add more data into the file that is only for the global profile of this IP, without any time window

            # Add the info we have about this IP
            ip_info = __database__.getIPData(ip)
            printable_ip_info = ''
            if ip_info:
                printable_ip_info = ', '.join('{} {}'.format(k, v) for k, v in ip_info.items())
                self.addDataToFile(profilefolder + '/' + 'ProfileData.txt', 'Info: ', file_mode='a+')
                for data in printable_ip_info.split(','):
                    self.addDataToFile(profilefolder + '/' + 'ProfileData.txt', '\t' + data.strip(), file_mode='a+')
        return profilefolder

    def addDataToFile(self, file, data, file_mode='w+', data_type='txt', data_mode='text'):
        """
        Receive data and append it in the general log of this profile
        If the filename was not opened yet, then open it, write the data and return the file object.
        Do not close the file

        :param file: can be file name or file object
        :param data_mode: in which format do we want the data to be added to file
        :param data_type: the data coming to this function
        In data_mode = 'text', we add a \n at the end
        In data_mode = 'raw', we do not add a \n at the end

        In data_type = 'text' we do not do anything now
        In data_type = 'json' format before printing
        In data_type = 'lines'  write all the lines together
        """

        try:

            if data_mode == 'text':
                # The other mode is 'raw' where we do not add the \n
                data = data + '\n'

            # get the type of file param
            if type(file) == str:
                # The file was not opened yet
                fileobj =  open(file, file_mode)
            else:
                fileobj = file


            if data_type == 'lines':
                # We received a bunch of lines all together. Just write them
                fileobj.writelines(data)
            elif data_type == 'line' or type(data)==str:
                # we just write one line
                fileobj.write(data)

            elif data_type == 'json':
                # format the json data before writing
                # data is a list of dicts
                to_print = ''
                for flow in data:
                    try:
                        flow = json.loads(flow)
                        # format the flow
                        ts = flow.get('timestamp','').split()
                        # print(f"flow: {flow}\n\n")
                        # discard the seconds and milliseconds in ts
                        ts = f'{ts[0]}  {ts[1][0:4]}'
                        dport_name = flow.get('dport_name','')
                        preposition = flow.get('preposition','')
                        daddr = flow.get('daddr','')
                        dport = flow.get('dport/proto','')
                        query = flow.get('Query',False)
                        ans = flow.get('Answers',False)
                        state = flow.get('state','')
                        critical_warning = flow.get('critical warning','')
                        trusted = flow.get('Trusted','')
                        if state == 'notestablished':
                            state ='not established'
                            if 'UDP' in dport:
                                state = 'not answered'
                        to_print += f'{ts} : {dport_name} {preposition} {daddr} {dport} {critical_warning} {state}'
                        if query: to_print+= f' query: {query}'
                        if ans: to_print += f' answers: {ans}'
                        if 'No' in trusted:  to_print += f', not trusted.'
                        to_print +='\n\n'
                    except json.decoder.JSONDecodeError:
                        # data is a str, leave it as it is
                        to_print+= data
                data = to_print
                fileobj.write(data)
            fileobj.flush()
            return file

        except KeyboardInterrupt:
            return True
        except Exception as inst:
            self.print('Error in addDataToFile()')
            self.print(type(inst))
            self.print(inst)
            sys.exit(1)

    def create_all_flow_possibilities(self) -> dict:
        # for client_or_server, sentence in zip(['Client', 'Server'], ['As a client, Dst', 'As a server, Src']):
        flow_types = {}
        for protocol in ['TCP', 'UDP', 'ICMP', 'ICMP6']:
            for client_or_server, as_cl_ser in zip(['Client', 'Server'], ['As a client,', 'As a server,']):
                for port_or_ip in ['Port', 'IP']:
                    for src_or_dst in ['Dst', 'Src']:
                        for est_notest in ['Established', 'NotEstablished']:
                            # 'As a client, Dst Ports we connected with TCP Established flows:'
                            key_name = src_or_dst + port_or_ip + client_or_server + protocol + est_notest
                            sentence = as_cl_ser + ' ' + src_or_dst + ' ' + port_or_ip + ' we connected with ' + \
                                       protocol + ' ' + est_notest + ' flow:'
                            flow_types[key_name] = sentence
        return flow_types

    def process_global_data(self):
        """ 
        This is the main function called by the timer process
        Read the global data and output it on logs 
        """
        try:
            # Get the list of all the modifed TW for all the profiles
            TWModifiedforProfile = __database__.getModifiedTW()
            last_profile_id = None
            description_of_malicious_ip_profile = None
            for profileTW in TWModifiedforProfile:

                # Get the profileid and twid
                profileid = profileTW[0].split(self.fieldseparator)[0] + self.fieldseparator + profileTW[0].split(self.fieldseparator)[1]
                twid = profileTW[0].split(self.fieldseparator)[2]
                # Get the time of this TW. For the file name
                twtime = __database__.getTimeTW(profileid, twid)
                twtime = time.strftime('%Y-%m-%dT%H:%M:%S', time.localtime(twtime))
                self.print('\tStoring Profile: {}. TW {}. Time: {}'.format(profileid, twid, twtime), 3, 0)
                #self.print('\tProfile: {} has {} timewindows'.format(profileid, twLen), 0, 3)

                # Create the folder for this profile if it doesn't exist
                profilefolder = self.createProfileFolder(profileid)
            
                # Create the TW log file
                twlog = twtime + '.' + twid
                # First Erase its file and save the data again
                self.addDataToFile(profilefolder + '/' + twlog, '', file_mode='w+', data_mode='raw')

                # Save in the log file all parts of the profile

                # 0. Write the profileID for people getting know what they see in the file.
                self.addDataToFile(profilefolder + '/' + twlog, 'ProfileID: {}\n'.format(profileid), file_mode='a+',
                                   data_type='text')

                # 0. Is a ip of this profile stored as malicious?
                # If it still one profile do not ask again the database for each new time_window.
                if last_profile_id != profileid:
                    description_of_malicious_ip_profile = __database__.is_profile_malicious(profileid)
                if description_of_malicious_ip_profile:
                    ip_of_profile = profileid.split(self.separator)[1]
                    text_data = '[THREAT INTELIGENCE] IP of this profile: {} was detected as malicious. Description: "{}"\n'.format(ip_of_profile, description_of_malicious_ip_profile)
                    self.addDataToFile(profilefolder + '/' + twlog, text_data, file_mode='a+', data_type='text')

                # 1. Detections to block. The getBlockingRequest function return {True, False}
                blocking = __database__.checkBlockedProfTW(profileid, twid)
                if blocking:
                    text_data = 'Was requested to block in this time window: ' + str(blocking)
                    self.addDataToFile(profilefolder + '/' + twlog, text_data, file_mode='a+', data_type='json')
                    self.outputqueue.put('03|logs|\t\t[Logs] Blocking Request: ' + str(blocking))

                # 2. Info about the evidence so far for this TW.
                evidence = __database__.getEvidenceForTW(profileid, twid)
                if evidence:
                    evidence = json.loads(evidence)
                    self.addDataToFile(profilefolder + '/' + twlog, 'Evidence of detections in this TW:',
                                       file_mode='a+', data_type='text')
                    self.outputqueue.put('03|logs|\t\t[Logs] Evidence of detections in this TW:')
                    for data in evidence:
                        self.addDataToFile(profilefolder + '/' + twlog, '\tEvidence: {}'.format(data), file_mode='a+',
                                           data_type='text')
                        self.outputqueue.put('03|logs|\t\t\t Evidence: {}'.format(data[0]))

                # 3. DstIPs
                dstips = __database__.getDstIPsfromProfileTW(profileid, twid)
                if dstips:
                    # Add dstips to log file
                    self.addDataToFile(profilefolder + '/' + twlog, 'DstIP:', file_mode='a+', data_type='text')
                    self.outputqueue.put('03|logs|\t\t[Logs] DstIP:')
                    data = json.loads(dstips)
                    # Better printing of data
                    for key in data:
                        ip_info = __database__.getIPData(key)
                        if ip_info:
                            printable_ip_info = ', '.join('{} {}'.format(k, v) for k, v in ip_info.items())
                        else:
                            printable_ip_info = '-'
                        self.addDataToFile(profilefolder + '/' + twlog,
                                           '\t{} ({} times). Info: {}'.format(key, data[key], printable_ip_info),
                                           file_mode='a+', data_type='text')
                    self.outputqueue.put('03|logs|\t\t[Logs] DstIP: ' + dstips)

                # 4. SrcIPs
                srcips = __database__.getSrcIPsfromProfileTW(profileid, twid)
                if srcips:
                    # Add srcips
                    self.addDataToFile(profilefolder + '/' + twlog, 'SrcIP:', file_mode='a+', data_type='text')
                    self.outputqueue.put('03|logs|\t\t[Logs] SrcIP:')
                    data = json.loads(srcips)
                    for key in data:
                        ip_info = __database__.getIPData(key)
                        if ip_info:
                            printable_ip_info = ', '.join('{} {}'.format(k, v) for k, v in ip_info.items())
                        else:
                            printable_ip_info = '-'
                        self.addDataToFile(profilefolder + '/' + twlog,
                                           '\t{} ({} times). Info: {}'.format(key, data[key], printable_ip_info),
                                           file_mode='a+', data_type='text')
                        self.outputqueue.put('03|logs|\t\t\t[Logs] {} ({} times)'.format(key, data[key]))

                # 5. OutTuples
                out_tuples = __database__.getOutTuplesfromProfileTW(profileid, twid)
                if out_tuples:
                    # Add tuples
                    self.addDataToFile(profilefolder + '/' + twlog, 'OutTuples:', file_mode='a+', data_type='text')
                    self.outputqueue.put('03|logs|\t\t[Logs] OutTuples:')
                    data = json.loads(out_tuples)
                    for key in data:
                        self.addDataToFile(profilefolder + '/' + twlog, '\t{} ({})'.format(key, data[key]),
                                           file_mode='a+', data_type='text')
                        self.outputqueue.put('03|logs|\t\t\t[Logs] {} ({})'.format(key, data[key]))
                    self.outputqueue.put('03|logs|\t\t[Logs] Tuples: ' + out_tuples)

                # 6. InTuples
                in_tuples = __database__.getInTuplesfromProfileTW(profileid, twid)
                if in_tuples:
                    # Add in tuples
                    self.addDataToFile(profilefolder + '/' + twlog, 'InTuples:', file_mode='a+', data_type='text')
                    self.outputqueue.put('03|logs|\t\t[Logs] InTuples:')
                    data = json.loads(in_tuples)
                    for key in data:
                        self.addDataToFile(profilefolder + '/' + twlog, '\t{} ({})'.format(key, data[key]),
                                           file_mode='a+', data_type='text')
                        self.outputqueue.put('03|logs|\t\t\t[Logs] {} ({})'.format(key, data[key]))


                # 7. Print the port data
                all_roles = ['Client', 'Server']
                all_protocols = ['TCP', 'UDP', 'ICMP', 'IPV6ICMP']
                all_states = ['Established', 'NotEstablished']
                all_directions = ['Dst', 'Src']
                type_data = 'Ports'
                for role in all_roles:
                    for protocol in all_protocols:
                        for state in all_states:
                            for direction in all_directions:
                                text_data = 'As {}, {} {} {} ports:'.format(role, protocol, state, direction)
                                self.outputqueue.put('03|logs|\t\t\t[Logs]: ' + text_data)
                                data = __database__.getDataFromProfileTW(profileid, twid, direction, state, protocol, role, type_data)
                                #self.print('LOGING data: {}'.format(data))
                                if data:
                                    self.addDataToFile(profilefolder + '/' + twlog, text_data, file_mode='a+',
                                                       data_type='text')
                                    for port in data:
                                        text_data = '\tPort {}. Total Flows: {}. Total Pkts: {}. TotalBytes: {}.'.format(port, data[port]['totalflows'], data[port]['totalpkt'], data[port]['totalbytes'])
                                        self.addDataToFile(profilefolder + '/' + twlog, text_data, file_mode='a+',
                                                           data_type='text')
                                        self.outputqueue.put('03|logs|\t\t\t[Logs]: ' + text_data)

                # 8. Info about the evidence so far for this TW.
                evidence = __database__.getEvidenceForTW(profileid, twid)
                if evidence:
                    evidence = json.loads(evidence)
                    self.addDataToFile(profilefolder + '/' + twlog, 'Evidence of detections in this TW:',
                                       file_mode='a+', data_type='text')
                    for key, evidence_details in evidence.items():
                        evidence_details = json.loads(evidence_details)
                        # example of a key  'dport:32432:PortScanType1'
                        key = f'{evidence_details["type_detection"]}:{evidence_details["detection_info"]}:{evidence_details["type_evidence"]}'
                        self.addDataToFile(profilefolder + '/' + twlog,
                                           '\tEvidence Description: {}. Confidence: {}. Threat Level: {} (key:{})'.format(
                                               evidence_details.get('description'), evidence_details.get('confidence'),
                                               evidence_details.get('threat_level'), key,
                                               file_mode='a+', data_type='text'))

                # Add free line between tuple info and information about ports and IP.
                self.addDataToFile(profilefolder + '/' + twlog, '', file_mode='a+', data_type='text')
                """
                Dst ports and Src ports
                """
                """
                flow_type_key = [Src,Dst] + [Port,IP] + [Client,Server] + [TCP,UDP, ICMP, ICMP6] + [Established, NotEstablished] 
                Example: flow_type_key = 'SrcPortClientTCPEstablished'
                """
                flow_types_dict = self.create_all_flow_possibilities()
                hash_key = profileid + self.separator + twid
                for flow_type_key, sentence in flow_types_dict.items():
                    data = __database__.get_data_from_profile_tw(hash_key, flow_type_key)
                    if data:
                        self.addDataToFile(profilefolder + '/' + twlog, sentence, file_mode='a+', data_type='text')
                        for port, sample in data.items():
                            type = 'IP' if 'ip' in flow_type_key.lower() else 'Port'
                            text_data = '\t{} {}. Total Flows: {}. Total Pkts: {}. TotalBytes: {}.'.format(type, port, sample['totalflows'], sample['totalpkt'], sample['totalbytes'])
                            self.addDataToFile(profilefolder + '/' + twlog, text_data, file_mode='a+', data_type='text')
                            self.outputqueue.put('03|logs|\t\t\t[Logs]: ' + text_data)


                # 9. This should be last. Detections to block
                blocking = __database__.checkBlockedProfTW(profileid, twid)
                if blocking:
                    self.addDataToFile(profilefolder + '/' + twlog,
                                       'Was requested to block in this time window: ' + str(blocking), file_mode='a+',
                                       data_type='json')
                    self.outputqueue.put('03|logs|\t\t[Logs] Blocking Request: ' + str(blocking))

                ###########
                # Create Timeline for each profile
                # Store the timeline from the DB in a file
                # The timeline file is unique for all timewindows. Much easier to read this way.

                # Get all the TW for this profile
                tws = __database__.getTWsfromProfile(profileid)
                ip = profileid.split('_')[1]

                timeline_path = profilefolder + '/' + 'Complete-timeline-outgoing-actions.txt'
                # If the file does not exists yet, create it
                if not os.path.isfile(timeline_path):
                    self.addDataToFile(timeline_path, 'Complete Timeline of IP {}\n'.format(ip), file_mode='w+')

                for twid_tuple in tws:
                    (twid, starttime) = twid_tuple
                    hash_key = profileid + self.separator + twid
                    first_index = self.timeline_first_index.get(hash_key, 0)
                    data, first_index = __database__.get_timeline_last_lines(profileid, twid, first_index)
                    self.timeline_first_index[hash_key] = first_index
                    if data:
                        self.print('Adding to the profile line {} {}, data {}'.format(profileid, twid, data), 3, 0)
                        self.addDataToFile(profilefolder + '/' + 'Complete-timeline-outgoing-actions.txt', data,
                                           file_mode='a+', data_type='json', data_mode='raw')

                last_profile_id = profileid

            # Create the file of the blocked profiles and TW
            TWforProfileBlocked = __database__.getBlockedProfTW()
            # Create the file of blocked data
            if TWforProfileBlocked:
                self.addDataToFile('Blocked.txt', 'Detections:\n', file_mode='w+', data_type='text')
                for blocked in TWforProfileBlocked:
                    self.addDataToFile('Blocked.txt',
                                       '\t' + str(blocked).split('_')[1] + ': ' + str(blocked).split('_')[2],
                                       file_mode='a+', data_type='json')
                    #self.outputqueue.put('03|logs|\t\t[Logs]: Blocked file updated: {}'.format(TWforProfileBlocked))

            # Create a file with information about the capture in general
            #self.addDataToFile('Information.txt', 'Information about this slips run', file_mode='w+', data_type='text')
            #self.addDataToFile('Information.txt', '================================\n', file_mode='a+', data_type='text')
            #self.addDataToFile('Information.txt', 'Type of input: ' + , file_mode='a+', data_type='text')

        except KeyboardInterrupt:
            return True
        except Exception as inst:
            self.outputqueue.put('01|[Logs] Error in process_global_data in LogsProcess')
            self.outputqueue.put(f'01|[Logs] {type(inst)}')
            self.outputqueue.put('01|[Logs] {}'.format(inst))
            sys.exit(1)


class TimerThread(threading.Thread):
    """Thread that executes a task every N seconds. Only to run the process_global_data."""
    
    def __init__(self, interval, function):
        threading.Thread.__init__(self)
        self._finished = threading.Event()
        self._interval = interval
        self.function = function 

    def shutdown(self):
        """Stop this thread"""
        self._finished.set()
    
    def run(self):
        try:
            while 1:
                if self._finished.isSet(): return True
                self.task()
                
                # sleep for interval or until shutdown
                self._finished.wait(self._interval)
        except KeyboardInterrupt:
            return True
    
    def task(self):
        self.function()

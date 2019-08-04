# -*- coding: utf-8 -*-
from __future__ import print_function
import pdb  #for the purpose of debug
import sys
import math
import time

# -------------------------------------------------------------------------
VEHICLE_RUN = 1
VEHICLE_STOP = 0
PI = 3.1415926535897932384626

class global_config(object):
    MAX_TIME_DIFFERENCE = 360
    OUTPUT_DEBUG2_FILE = True   # debug interface, turned off now, output longitude/latitude/time
    OUTPUT_DEBUG1_FILE = True   # debug interface, turned off now, output long/latitude/time/time_difference between this point and previous point

#--- column name in **original** data ---
id_ = 0
trigger = 1
status_ = 2
time_ = 3
longtitude_ = 4
latitude_ = 5
speed_ = 6
orientation = 7
g_status = 8

#----- field name of databse ------------
FIELDS = ["id_",\
          "trigger",\
          "status",\
          "time_",\
          "longtitude",\
          "latitude",\
          "speed",\
          "orientation",\
          "g_status"
          ]

def debugout(str1):
    print("[Debug] "+str(str1))
    
#----------------------------------------------------------------------------------------------
# transfer time in excel to second
#  eg:
#>>> time_2_second("20170101015214")
#     1483253534
#>>> time_2_second("20170101015215")
#     1483253535
#>>> time_2_second("20170101015216")
#     1483253536
#>>> time_2_second("20170101025216")
#     1483257136
def time_2_second(time_str_in_txt):
    TIME_FORMAT = "%Y%m%d%H%M%S"
    strptime = time.strptime(time_str_in_txt,TIME_FORMAT)
    mktime = int(time.mktime(strptime))
    return mktime
    
def second_2_time(time_in_second):
    TIME_FORMAT = "%Y-%m-%d--%H:%M:%S"
    timeTuple = time.localtime(time_in_second)
    otherTime = time.strftime(TIME_FORMAT, timeTuple)
    return otherTime    
    
def status_2_string(vehicle_state_int_value):
    if(VEHICLE_RUN == vehicle_state_int_value):
        return "running"
    elif(VEHICLE_STOP == vehicle_state_int_value):
        return "stop"
    else:
        return "undefined"    
    
# -------------------------------------------------------------------------  
# API for find the best L0 of longitude for coordinate transferring
# -------------------------------------------------------------------------
""" def find_best_coordinate_transfer_L0(minlongitude, maxlongitude):
    midlong = (minlongitude+maxlongitude)/2
    sign = 1
    if(midlong <0):
        sign = -1
    midlong = abs(midlong)
    #---------------------------------------    
    L0_min = int(midlong)
    L0_max = L0_min + 1
    if((L0_max - midlong) < (midlong - L0_min)):
        return L0_max*sign
    else:
        return L0_min*sign """

# -------------------------------------------------------------------------
# coordinate range
# -------------------------------------------------------------------------
class MBR(object):
    def __init__(self):
        min_value = 1000.0    
        max_value = -min_value 
        self.__min_long_id = ''
        self.__min_long = min_value
        self.__min_lat_id = ''
        self.__min_lat = min_value
        self.__max_long_id = ''
        self.__max_long = max_value
        self.__max_lat_id = ''
        self.__max_lat = max_value  
    #............................................................................            
    def update(self, long, lat, mbr_id):
        if(long < self.__min_long):
            self.__min_long = long
            self.__min_long_id = mbr_id
        if(long > self.__max_long):
            self.__max_long = long
            self.__max_long_id = mbr_id
        if(lat < self.__min_lat):
            self.__min_lat = lat
            self.__min_lat_id = mbr_id
        if(lat > self.__max_lat):
            self.__max_lat = lat
            self.__max_lat_id = mbr_id
    #............................................................................        
    def get_boundary(self):
        return [self.__min_long_id, self.__min_long,\
                self.__min_lat_id,  self.__min_lat, \
                self.__max_long_id, self.__max_long,\
                self.__max_lat_id,  self.__max_lat]

# -------------------------------------------------------------------------
# each record is mapped to one line of excel file\
# we let every member to be public, so that to accelerate the speed
# -------------------------------------------------------------------------
class vehicle_pos_record(object):
    @staticmethod
    def createInstance(line_in_txt):
        data = line_in_txt.split(",")
        #gps_status is invalid
        if(data[g_status] == 0):
            return None
        else:
            running_status = vehicle_pos_record.__get_running_status__(data[status_], data[speed_])
            return vehicle_pos_record(data[id_],\
                                      time_2_second(data[time_]),\
                                      float(data[longtitude_]),\
                                      float(data[latitude_]),\
                                      running_status)
    #..........................................................................
    # Initialize record 
    #..........................................................................
    def __init__(self,id1,time1,longtitude1,latitude1,status1,others = None):
        self.vehicle_id = id1
        self.gps_time = time1
        self.gps_longt = longtitude1
        self.gps_lat = latitude1
        self.vehicle_status = status1
        self.time_diff = 0      # we will use this to interpolate position!
    
    #...........................................................................    
    def __str__(self):    # output longtitude & latitude
        return str(self.gps_longt) + "," + str(self.gps_lat)
    #...........................................................................
    def to_string(self):  # output into file 
        return str(self.vehicle_id) + "," + second_2_time(self.gps_time)\
                                    + "," + str(self.gps_longt)\
                                    + "," + str(self.gps_lat)\
                                    + "," + status_2_string(int(self.vehicle_status))\
                                    + "," + str(self.time_diff)\
                                    + "\n"
    #...........................................................................    
    @staticmethod
    def __get_running_status__(_vehicle_status, _vehicle_speed):
        if((_vehicle_status == "2") and (int(_vehicle_speed) < 1)):
            return VEHICLE_STOP
        elif(int(_vehicle_speed) < 1):
            return VEHICLE_STOP
        else:
            return VEHICLE_RUN
    #..........................................................................  
    @staticmethod
    def comparekey(record):
        #sort by time
        return record.gps_time
#end class vessel_pos_record

# -------------------------------------------------------------------------
#  *hash map data structure to organize data*
#
#   (vehicle_id, vector)
#   all position with the same vehicle_id are stored in the same vector
#   and we will sort all those vectors using time information
#
#   stop -> stop ,       ignore it
#   stop -> runing,      insert it, interpolate = time difference / 2
#   running -> running,  insert it, interpolate num = time difference
#   running -> stop,     insert it, interpolate num = time difference / 2
# -------------------------------------------------------------------------
# finite state machine
FSM_STATE_VEHICLE_STOP = 0
FSM_STATE_VEHICLE_RUNNING = 1

class record_tree(object):
    def __init__(self):
        self.data_map = {}
        self.fsm = FSM_STATE_VEHICLE_STOP
        self.mbr = MBR()
        self.couted_line = 0
        self.interpolated_point_num = 0
    
    # --- directly push a line of record to let it parse and create a record and push to the dictionary!
    def push(self,txt_vehicle_record_line):
        pos_record = vehicle_pos_record.createInstance(txt_vehicle_record_line)
        # how about gps_time is invalid
        if(pos_record ==  None):  
            return False        
        if(pos_record.vehicle_id not in self.data_map):
            self.data_map[pos_record.vehicle_id]=[]
        self.data_map[pos_record.vehicle_id].append(pos_record)
        return True
    
    # run -> run, we consider time difference = (pos_array[i].gps_time - pos_array[i-1].gps_time)
    # run -> stop, stop-> run, we consider time difference = (pos_array[i].gps_time - pos_array[i-1].gps_time)/2
    #                          because we do not know when 
    def filter_and_replace_positions(self):
        print('[Debug] starting filter useless positioning data ...')
        for key in self.data_map:
            new_list = []
            pos_array = self.data_map[key]
            iall = len(pos_array)
            if(iall > 0):
                new_list.append(pos_array[0])
                # Use the first data as the starting point for comparison
                self.fsm = pos_array[0].vehicle_status  
                self.mbr.update(pos_array[0].gps_longt, pos_array[0].gps_lat, pos_array[0].vehicle_id)
            for i in range(1, iall):
                # update ranges
                self.mbr.update(pos_array[i].gps_longt, pos_array[i].gps_lat, pos_array[0].vehicle_id)
                # stop previous frame
                if(self.fsm == FSM_STATE_VEHICLE_STOP):
                    if(pos_array[i].vehicle_status == FSM_STATE_VEHICLE_STOP):
                        # discard this point, two connected stop-points
                        continue
                    else:
                        pos_array[i].time_diff = int((pos_array[i].gps_time - pos_array[i-1].gps_time) / 2) 
                        new_list.append(pos_array[i])
                        self.fsm = FSM_STATE_VEHICLE_RUNNING
                        continue
                # running previous frame
                elif(self.fsm == FSM_STATE_VEHICLE_RUNNING):
                    if(pos_array[i].vehicle_status == FSM_STATE_VEHICLE_STOP):                        
                        self.fsm = FSM_STATE_VEHICLE_STOP  
                        pos_array[i].time_diff = int((pos_array[i].gps_time - pos_array[i-1].gps_time) / 2) 
                    else: 
                        pos_array[i].time_diff = pos_array[i].gps_time - pos_array[i-1].gps_time
                    new_list.append(pos_array[i])                                      
                else:
                    print('[Error] never should be here! Bad logic!') 
            #replace the old pos_array with new list
            pos_array = []
            self.data_map[key] = new_list
            self.couted_line = self.couted_line + len(new_list)
        print('[Debug] finish filering data ... ')
    #endif filter_and_interpolate_destroy
    
    def interpolate_and_output(self,outputfile):
        print('[Debug] starting to interpolate positioning data and save to file...')
        self.interpolated_point_num = 0
        for key in self.data_map:  # use finite state machine to handle positioning data
            pos_array = self.data_map[key]
            iall = len(pos_array)
            for i in range(iall):
                if(i > 0):
                    dx = pos_array[i].gps_longt - pos_array[i-1].gps_longt
                    dy = pos_array[i].gps_lat - pos_array[i-1].gps_lat
                    time_diff = pos_array[i].time_diff
                    if(time_diff < global_config.MAX_TIME_DIFFERENCE and time_diff >= 1):
                        for j in range(1,time_diff):
                            x = float('%.7f' %(pos_array[i-1].gps_longt + dx * j / time_diff))
                            y = float('%.7f' %(pos_array[i-1].gps_lat + dy * j / time_diff))
                            outputfile.write(str(x) + ',' + str(y) + '\n')    
                        self.interpolated_point_num = self.interpolated_point_num  + (time_diff - 1)
                if(pos_array[i].time_diff < 0):
                    print('[ERROR] time_diff =' + str(pos_array[i].time_diff)+", i="+str(i))
                # whatever which i, always write to file !
                outputfile.write(str(pos_array[i]) + '\n')     # I have implemented __str__(...) method in class vessel_pos_record                
    #end interpolate_and_output(self):
    
    # -- sort every vector, do NOT call it outside the class ------------
    # -- sort according time --
    def __sort_data__(self):   #sort all positioning data
        debugout('now sorting')
        for key1 in self.data_map:            
            self.data_map[key1].sort(key = vehicle_pos_record.comparekey)   
    
    # -- really sorted? --
    def dump(self, fout):
        for key1 in self.data_map: 
            pos_array =  self.data_map[key1]
            iall = len(pos_array)
            for i in range(iall):
                fout.write(pos_array[i].to_string())

                       
# -------------------------------------------------------------------------
# transformed from original C++ code, 
# -------------------------------------------------------------------------   
class process_notify(object):
    def __init__(self, step):
        self.__step = step
        self.__old_percent = -1
        return
    #.............................................................................
    
    def reset(self):
        self.__old_percent = -1
    #.............................................................................
    
    def push(self, percent, sNotify = "Processed "):
        if( (percent % self.__step == 0) and (percent != self.__old_percent)):
            print(str(sNotify) +":" + str(percent)+"%...")
            self.__old_percent = percent        
#end class  
    
def help(argv):
    print('========================================================================')
    print(' Tools to extract trajectory of vehicle to a new txt file')
    print('Usage:')
    print('      '+argv[0]+' source_txt_file  [destine_file]')
    print('      destine_file name is an optional parameter!')
    print('========================================================================')
    return

def main(argv):
    print(argv)
    #invalid input
    if (len(argv)<2 or len(argv)>3):
        help(argv)
        return
    
    filename = argv[1]
    fileout = ""
    fileout_desc = ""
    #set name of output files
    if(len(argv) == 2):
        #remove .txt and add something
        fileout = filename[:len(filename)-4] + "_long_lat.txt"
        fileout_desc = filename[:len(filename)-4] + "_long_lat_MBR.txt"
        if(global_config.OUTPUT_DEBUG1_FILE):
            fileout_debug1 = filename[:len(filename)-4] +"_long_lat_debug1.txt" 
        if(global_config.OUTPUT_DEBUG2_FILE):
            fileout_debug2 = filename[:len(filename)-4] +"_long_lat_debug2.txt"
    else:
        fileout = argv[2]
        fileout_desc = fileout + "_MBR.txt"
        if(global_config.OUTPUT_DEBUG1_FILE):
            fileout_debug1 = fileout + "_debug1.txt"
        if(global_config.OUTPUT_DEBUG2_FILE):    
            fileout_debug2 = fileout + "_debug2.txt"        
        
    print('Now open ' + fileout + ' for writing final results')
    try:
        fout = open(fileout,'w')
        fout_desc = open(fileout_desc,'w')
        if(global_config.OUTPUT_DEBUG1_FILE):
            fout_debug1 = open(fileout_debug1,"w")
        if(global_config.OUTPUT_DEBUG2_FILE):
            fout_debug2 = open(fileout_debug2,"w")
    except:
        print('Error: file to open file:' + fileout)
        return
    
    #now read and write files
    iall_valid = 0
    vehicle_hash_table = record_tree()
    with open(filename) as file_object:
        lines = file_object.readlines()
        iall = len(lines)       
        print('[OK] total '+ str(iall)+ ' lines')
        notify = process_notify(5)   # each 5% give a notification
                
        # 1. generate record using each lines' information
        i = 0
        for line in lines[1:]:              #ignore the first line!!!
            notify.push(int(i*100 / iall))   #push to notify user!
            i = i + 1
            if(vehicle_hash_table.push(line) == True):
                iall_valid = iall_valid + 1
        
        # 2. sort and output data to debug file to check it
        vehicle_hash_table.__sort_data__()
        if(global_config.OUTPUT_DEBUG1_FILE):
            vehicle_hash_table.dump(fout_debug1)
        
        # 3.--- filte out repeated stop points ----
        vehicle_hash_table.filter_and_replace_positions() 
        if(global_config.OUTPUT_DEBUG2_FILE):
            vehicle_hash_table.dump(fout_debug2)
        
        # 4. interpolate points
        vehicle_hash_table.interpolate_and_output(fout)
    #end with
    print("")
    print('[OK] Successfully transferred to file:'+fileout)
    print('[0K] Total ' + str(iall_valid) + ' records are valid')
    str_record = '[OK] Total '+ str(vehicle_hash_table.couted_line + vehicle_hash_table.interpolated_point_num) + ' records are written!'
    str_boundary = '[OK] Boundary:'+ str(vehicle_hash_table.mbr.get_boundary())
    print(str_record)
    print(str_boundary)

    #output additional information to description file
    fout_desc.write("[source file]: "+filename+"\n")
    fout_desc.write("[destination file]: "+fileout+"\n")
    fout_desc.write("[Extracted fields]:"  +"LANGTITUDE  " + "LATITUDE" + "\n") 
    fout_desc.write(str_record+"\n")
    fout_desc.write(str_boundary+"\n")
    
    # write a recommanded Lo for transferring Long/Lat to x/y to file!
"""     [minlong,minlat,maxlong,maxlat]=vehicle_hash_table.mbr.get_boundary()
    L0 = find_best_coordinate_transfer_L0(minlong,maxlong)
    fout_desc.write('L0 = '+str(L0)+"\n")
    fout_desc.write('The recommended L0 = '+str(int(L0))+"\n") """

#--------------------------------------------------------------------------------------------------
if __name__=="__main__":
    main(sys.argv)

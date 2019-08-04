# -*- coding: utf-8 -*-
"""
   Extract trajectory from publiced AIS-data file
   Created on Tue Mar 26 19:10:57 2019

   currently, we only extract the following vessels
   
   const VESSEL_tug_tow="tug tow";
   const VESSEL_cargo="Cargo";
   const VESSEL_tanker="Tanker";

1. to select more vessel types, modify data structure 
                   select_vessel_type
2. to output more data, please modify              
                   selected_output_column

3. about None in python
    - None means empty data, nothing there
    - if (None) always return false    
    - type(None) ---> NoneType               
                   
4. if you want to debug python script, you can do like
       #import pdb
    and add the following code in script 
       pdb.set_trace()
    when python run there, will pause there ....       
                   
@author: xiaoguo zhang

===========
tips
===========
  1.time related API, refer to:
    https://blog.csdn.net/weixin_33781606/article/details/86874681
  2.about how to overload operators of python class, please refer to
    https://blog.csdn.net/goodlixueyong/article/details/52589979
        
======================
Revision History
======================
2019-06-02 Xiaoguo Zhang Initial creation
2019-06-22 Xiaoguo Zhang Add point interpolate functionality
                         Add removing vessel position when a vehicle is always stop somewhere
                         considering in AIS data file, there are the following issues
                         --- the data could be disordered in time
                         --- sparse sampled, the time duration between two points could reach 3 minuts sometimes!
                         Please be aware that, the original data is in unorder sequence!
2019-06-23 Xiaoguo Zhang Filter out big time difference, stop interpolate in such case, for example, the vessel stop sending message for about 2 days or more
                         Solution: only if the time diference is less than 300, will we interpolate positioning data


"""
from __future__ import print_function 
import pdb
import sys
import math
import time
 
""" 
OLD STANDARD:

   cargo:  “VesselType” = 70 OR “VesselType” = 71 OR “VesselType” = 72 OR “VesselType” = 73 OR “VesselType” = 74 OR “VesselType” = 75 OR “VesselType” = 76 OR “VesselType” = 77 OR “VesselType” = 78 OR “VesselType” = 79 
   tanker: “VesselType” = 80 OR “VesselType” = 81 OR “VesselType” = 82 OR “VesselType” = 83 OR “VesselType” = 84 OR “VesselType” = 85 OR “VesselType” = 86 OR “VesselType” = 87 OR “VesselType” = 88 OR “VesselType” = 89 
   tug and tow: “VesselType” = 31 OR “VesselType” = 32 OR “VesselType” = 52 
                and 21, 22
                
NEW-STANDARD SINCE 2018:
   cargo: 1003,1004,1016
   tanker: 1017,1024
   tug tow: 1023,1025

"""
# vessel types to be selected, which are all big vessel types
SELECTED_VESSEL_TYPES = (21,22,\
                         31,32,\
                         52,\
                         70,71,72,73,74,75,77,78,79,80,81,82,83,84,85,86,87,88,89,\
                         1003,1004,1016,\
                         1017,\
                         1024,\
                         1023,1025)

#------  vessel running status  ------------
VESSEL_RUN = 1
VESSEL_STOP = 0
PI=3.1415926535897932384626


class global_config(object):
    MAX_TIME_DIFFERENCE = 360
    OUTPUT_DEBUG2_FILE=False            # debug interface, turned off now,     output longitude/latitude/time
    OUTPUT_DEBUG1_FILE=False            # debug interface, turned off now,     output long/latitude/time/time_difference between this point and previous point
    
    
#----------------------------------------------------------------------------------------------
# transfer time in excel to second
#  eg:
#>>> time_2_second("2017-01-01T01:52:14")
#     1483253534
#>>> time_2_second("2017-01-01T01:52:15")
#     1483253535
#>>> time_2_second("2017-01-01T01:52:16")
#     1483253536
#>>> time_2_second("2017-01-01T02:52:16")
#     1483257136

def time_2_second(time_str_in_execel):
    TIME_FORMAT = "%Y-%m-%dT%H:%M:%S"
    strptime = time.strptime(time_str_in_execel,TIME_FORMAT)
    mktime = int(time.mktime(strptime))
    return mktime
    
def second_2_time(time_in_second):
    TIME_FORMAT = "%Y-%m-%dT%H:%M:%S"
    timeTuple = time.localtime(time_in_second)
    otherTime = time.strftime(TIME_FORMAT, timeTuple)
    return otherTime    
    
def status_2_string(vessel_state_int_value):
    if(VESSEL_RUN == vessel_state_int_value):
        return "running"
    elif(VESSEL_STOP == vessel_state_int_value):
        return "stop"
    else:
        return "undefined"    

# API for find the best L0 of longitude for coordinate transferring
def find_best_coordinate_transfer_L0(minlongitude, maxlongitude):
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
        return L0_min*sign
      
def debugout(str1):
    print("[debug] "+str(str1))
    
# def find_best_interpolation_num(long1,lat1,long2,lat2,time_diff):
    # dx = long2 - long1
    # dy = lat2 - lat1
    # ddlat = dy*3600*30  # 1 second latitide difference is about 30 meters
    # ddlong = dx * cos((lat1+lat2)*PI/180.0)*3600*30
    # s = math.sqrt(ddlat*ddlat + ddlong*ddlong)
        
############################################################################################
# column name in original csv-style AIS data
MMSI=0
BaseDateTime=1
LAT=2
LON=3
SOG=4
COG=5
Heading=6
VesselName=7
IMO=8
CallSign=9
VesselType=10
Status=11
Length=12
Width=13
Draft=14
Cargo=15
#----- field name of databse ------------
FIELDS = ["MMSI",\
          "BaseDateTime",\
          "LAT",\
          "LON",\
          "SOG",\
          "COG",\
          "Heading",\
          "VesselName",\
          "IMO",\
          "CallSign",\
          "VesselType",\
          "Status",\
          "Length",\
          "Width",\
          "Draft",\
          "Cargo"]


#--- hash table of vessel running status ---
#vessel status meaning 
# if we failed to find the status string in the following table, we also it is in running state!
vessel_running_status_dictionary={\
                    "":VESSEL_RUN,\
                    "undefined":VESSEL_RUN,\
                    "under way using engine":VESSEL_RUN,\
                    "under way sailing":VESSEL_RUN,\
                    "AIS-SART (active); MOB-AIS; EPIRB-AIS": VESSEL_RUN,\
                    \
                    "moored":VESSEL_STOP,\
                    "at anchor":VESSEL_STOP,\
                    "not under command":VESSEL_STOP,\
                    "constrained by her draught":VESSEL_STOP,\
                    "restricted maneuverability":VESSEL_STOP,\
                    "reserved for future use (13)":VESSEL_STOP,\
                    "power-driven vessel towing astern":VESSEL_STOP,\
                    "power-driven vessel pushing ahead or towing alongside":VESSEL_STOP,\
                    }

##########################################################################################
# column to be output to destination file
# if you want to output other fields, please modify this data structure
#SELECTED_OUTPUT_COLUMNS = [LON,LAT,Heading,VesselType]
SELECTED_OUTPUT_COLUMNS =  [LON,LAT]

#def DBG(var):
#    print(str(list(dict(var=var).keys()))+"="+str(var))

class MBR(object):
    def __init__(self,boundary=[]):
        if(len(boundary) != 4):
            min_value = 999999999999.0    # specify a big number as min, so that easily to be updated
            max_value = - min_value       # specify a minus big number 
            self.__min_long = min_value
            self.__min_lat = min_value
            self.__max_long = max_value
            self.__max_lat = max_value  
        else:
            self.__min_long = boundary[0]
            self.__min_lat = boundary[1]
            self.__max_long = boundary[2]
            self.__max_lat = boundary[3]
    #............................................................................            
    def update(self,long,lat):
        if(long < self.__min_long):
            self.__min_long = long
        if(long > self.__max_long):
            self.__max_long = long
        if(lat < self.__min_lat):
            self.__min_lat = lat
        if(lat>self.__max_lat):
            self.__max_lat = lat
    #............................................................................        
    def get_boundary(self):
        return [self.__min_long,self.__min_lat,self.__max_long,self.__max_lat]

##########################################################################
# each record is mapped to one line of excel file
# we let every member to be public, so that to accelerate the speed
class vessel_pos_record(object):
    @staticmethod
    def createInstance(line_in_excel):
        line = line_in_excel.rstrip()
        data = line.split(",")
        if(data[VesselType] == ""):
            return None
        vessel_type = int(data[VesselType])
        if vessel_type in SELECTED_VESSEL_TYPES:   
            run = vessel_pos_record.__get_running_status__(data[Status])
            return vessel_pos_record(data[MMSI],time_2_second(data[BaseDateTime]),float(data[LON]),float(data[LAT]),run)
    #..........................................................................
    def __init__(self,MMSI1,time_second1,long1,lat1,status1,others=None):
        self.MMSI = MMSI1
        self.time_second = time_second1
        self.long = long1
        self.lat = lat1
        self.status = status1
        self.time_diff = 0                # we will use this to interpolate position!!!!!!!!!!!!
        #self.others = others1
    #...........................................................................    
    def __str__(self):    # we just output longitude,latitude
        return str(self.long)+","+str(self.lat)
    #...........................................................................
    def to_string(self):  # output everything into file    
        return str(self.MMSI)+ "," + str(self.long)\
                            + "," + str(self.lat)\
                            + "," + str(self.status)\
                            + "," + second_2_time(self.time_second)\
                            + "," + status_2_string(self.status)\
                            + "," + str(self.time_diff)\
                            + "\n"
    #...........................................................................    
    @staticmethod
    def __get_running_status__(str):
        if (str in vessel_running_status_dictionary):
            return vessel_running_status_dictionary[str]
        else:
            return VESSEL_RUN
    #..........................................................................        
    @staticmethod
    def comparekey(record):
        return record.time_second;
#end class vessel_pos_record


#-------------------------------------------------------------------------
#  we use hash map data structure to organize data
#
# (MMSI, vector)
#   all position with the same MMSI are stored in the same vector
#   and we will sort all those vectors using time information
#   
#   stop -> stop ,       ignore it
#   stop -> runing,      insert it, interpolate = time difference / 2
#   running -> running,  insert it, interpolate num = time difference
#   running -> stop,     insert it, interpolate num = time difference / 2
#-------------------------------------------------------------------------
FSM_STATE_VESSEL_STOP = 0
FSM_STATE_VESSEL_RUNNING = 1
class record_tree(object):
    def __init__(self):
        self.data_map ={}
        self.fsm = FSM_STATE_VESSEL_STOP   # finite state machine
        self.mbr = MBR()
        self.interpolated_point_num = 0
        
    # --- directly push a line of excel record to let it parse and create a record and push to the dictionary!
    def push(self,excel_vessel_pos_record_line):
        pos_record = vessel_pos_record.createInstance(excel_vessel_pos_record_line)
        if(pos_record ==  None):    # sometimes if not big vessel, we will not create instance for it
            return False
        if(not self.data_map.has_key(pos_record.MMSI)):
            self.data_map[pos_record.MMSI]=[]
        self.data_map[pos_record.MMSI].append(pos_record)
        return True
  
    # run -> run, we consider time difference = (pos_array[i].time_second - pos_array[i-1].time_second)
    # run -> stop, stop-> run, we consider time difference = (pos_array[i].time_second - pos_array[i-1].time_second)/2
    #                          because we do not know when 
    def filter_and_replace_positions(self):
        print('[Debug] starting filter useless positioning data ...')
        for key in self.data_map:  # use finite state machine to handle positioning data
            self.fsm = FSM_STATE_VESSEL_RUNNING   # finite state machine
            new_list = []
            pos_array = self.data_map[key]
            iall = len(pos_array)
            if(iall > 0):
                new_list.append(pos_array[0])
            for i in xrange(1,iall):
                self.mbr.update(pos_array[i].long,pos_array[i].lat)
                if(self.fsm == FSM_STATE_VESSEL_STOP):
                    if(pos_array[i].status == FSM_STATE_VESSEL_STOP):
                        #discard this point, two connected stop-points                       
                        continue
                    else:
                        # keep it, need do nothing
                        pos_array[i].time_diff = int((pos_array[i].time_second - pos_array[i-1].time_second) / 2) # if 0, no need interpolate
                        new_list.append(pos_array[i])
                        self.fsm = FSM_STATE_VESSEL_RUNNING
                        #pdb.set_trace()
                        #print("time diff:"+str(pos_array[i].time_diff))
                        continue                        
                elif(self.fsm == FSM_STATE_VESSEL_RUNNING):
                    if(pos_array[i].status == FSM_STATE_VESSEL_STOP):                        
                        self.fsm = FSM_STATE_VESSEL_STOP  
                        pos_array[i].time_diff = int((pos_array[i].time_second - pos_array[i-1].time_second) / 2) # if 0, no need interpolate
                    else:
                        # keep it, need do nothing
                        #self.fsm = FSM_STATE_VESSEL_RUNNING
                        #continue  
                        pos_array[i].time_diff = pos_array[i].time_second - pos_array[i-1].time_second
                        #pdb.set_trace()
                        #print("time diff:"+str(pos_array[i].time_diff))
                    new_list.append(pos_array[i])
                else:               
                    print('[Error] never should be here! Bad logic!')
            # replace the old pos_array with new list
            pos_array=[]
            self.data_map[key] = new_list
        print('[Debug] finish filering data ...')
    #endif filter_and_interpolate_destroy
    
    def interpolate_and_output(self,outputfile):
        print('[Debug] starting to interpolate positioning data and save to file...')
        self.interpolated_point_num = 0;
        for key in self.data_map:  # use finite state machine to handle positioning data
            pos_array = self.data_map[key]
            iall = len(pos_array)
            for i in xrange(iall):
                if(i > 0):
                    dx = pos_array[i].long - pos_array[i-1].long
                    dy = pos_array[i].lat - pos_array[i-1].lat 
                    time_diff = pos_array[i].time_diff
                    #if(time_diff > 0):
                    #    print("dx="+str(dx)+",dy="+str(dy)+"\n");
                    #    print("time_diff="+str(time_diff)+"\n")
                    if(time_diff < global_config.MAX_TIME_DIFFERENCE and time_diff >= 1):
                        for j in xrange(1,time_diff):
                            x = pos_array[i-1].long + dx * j /time_diff;
                            y = pos_array[i-1].lat  + dy * j /time_diff;
                            outputfile.write(str(x)+','+str(y)+'\n')    
                        self.interpolated_point_num = self.interpolated_point_num  + (time_diff - 1)
                if(pos_array[i].time_diff < 0):
                    print('[ERROR] time_diff =' + str(pos_array[i].time_diff)+", i="+str(i))
                # whatever which i, always write to file !
                outputfile.write(str(pos_array[i]) + '\n')     # I have implemented __str__(...) method in class vessel_pos_record                
    #end def interpolate_and_output(self):
    
    # -- sort every vector, do NOT call it outside the class ------------
    def __sort_data__(self):   #sort all positioning data
        debugout('now sorting')
        for key1 in self.data_map:            
            self.data_map[key1].sort(key=vessel_pos_record.comparekey)             
            
    def dump(self,fout):            
        #debug code to check whether is really sorted!!!
        for key1 in self.data_map: 
            pos_array =  self.data_map[key1]
            iall = len(pos_array)
            for i in xrange(iall):
                fout.write(pos_array[i].to_string())                       
    
    # to interpolate points, do NOT call it outside the class!!!
    def __interpolate__(self,pfrom,pto,num):
        points = []
        [dx,dy] = [pto[0]-pfrom[0],pto[1]-pfrom[1]]
        for i in range(1,num+1):            
            points.append([pfrom[0]+dx*i,pfrom[1]+dy*i])
        return points
        
    
#end class  record_tree

"""
  process notification tool
"""
# transferred from original C++ code ...
class process_notify(object):
    def __init__(self, step):
        self.__step = step
        self.__old_percent = -1
        return
    #.............................................................................
    
    def reset(self):
        self.__old_percent = -1
    #.............................................................................
    
    def push(self,percent, sNotify = "Processed "):
        #print('percent % self.__step ='+str(percent % self.__step) )
        #print(percent)
        #print(self.__old_percent);
        if( (percent % self.__step == 0) and (percent != self.__old_percent)):
            #print(str(sNotify) +":" + str(percent)+"%...", end='\r')
            print(str(sNotify) +":" + str(percent)+"%...")
            self.__old_percent = percent        
    #.............................................................................
#end class        

def help(argv):
    print('========================================================================')
    print(' Tools to extract tracks of tank/tow-tug/cargo to a new csv file')
    print('Usage:')
    print('      '+argv[0]+' source_AIS_file  [destine_file]')
    print('      destine_file name is an optional parameter!')
    print('========================================================================')
    return

def main(argv):
    print(argv)
    if (len(argv)<2):
        help(argv)
        return
    
    filename = argv[1]
    fileout = ""
    fileout_desc = ""
    if(len(argv) == 2):
        fileout = filename[:len(filename)-4] +"_long_lat.csv"
        fileout_desc = filename[:len(filename)-4] +"_long_lat_MBR.txt"
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
    
    print('Now open '+fileout+' for writing final results')
    try:
        fout = open(fileout,'w')
        fout_desc = open(fileout_desc,'w')
        if(global_config.OUTPUT_DEBUG1_FILE):
            fout_debug1 = open(fileout_debug1,"w")
        if(global_config.OUTPUT_DEBUG2_FILE):
            fout_debug2 = open(fileout_debug2,"w")
    except:
        print('Error: file to open file:'+fileout)
        return
        
    # now read and write files 
    iall_valid = 0
    vessel_hash_table = record_tree()
    with open(filename) as file_object:
        lines = file_object.readlines()
        iall = len(lines)       
        print('[OK] total '+ str(iall)+ ' lines of data')       
        i = 0
        notify = process_notify(5)   # each 5% give a notification!
                
        #1. generate record using each lines' information
        i = 0
        for line in lines[1:]:             #ignore the first line!!!
            notify.push(int(i*100/iall))   #push to notify user!
            i = i + 1
            if(vessel_hash_table.push(line) == True):
                iall_valid = iall_valid + 1
        
        # 2. sort and output data to debug file to check it
        vessel_hash_table.__sort_data__()       
        if(global_config.OUTPUT_DEBUG1_FILE):
            vessel_hash_table.dump(fout_debug1)
            
        # 3.---------- sort-------------------
        vessel_hash_table.filter_and_replace_positions() 
        if(global_config.OUTPUT_DEBUG2_FILE):
            vessel_hash_table.dump(fout_debug2)
        
        
        # 4. filter out repeat stop point, and interpolate points!!!
        vessel_hash_table.interpolate_and_output(fout)
        
    #end with
    print("")     # remove the effection of "\r"
    print('[OK] Successfully transferred to file:'+fileout)
    str_record = '[OK] Total '+str(iall_valid + vessel_hash_table.interpolated_point_num) + ' records are written!'
    str_boundary = '[OK] Boundary:'+str(vessel_hash_table.mbr.get_boundary())
    print(str_record)
    print(str_boundary)
    
    #output additional information to description file
    fout_desc.write("[source file]:"+filename+"\n")
    fout_desc.write("[destination file]:"+fileout+"\n")
    fout_desc.write("[Extracted fields]:"+"\n") #SELECTED_OUTPUT_COLUMNS =  [LON,LAT],FIELDS
    fout_desc.write(str_record+"\n")
    fout_desc.write(str_boundary+"\n")
    
    # write a recommanded Lo for transferring Long/Lat to x/y to file!
    [minlong,minlat,maxlong,maxlat]=vessel_hash_table.mbr.get_boundary()
    L0 = find_best_coordinate_transfer_L0(minlong,maxlong)
    fout_desc.write('L0 = '+str(L0)+"\n")
    fout_desc.write('The recommended L0 = '+str(int(L0))+"\n")
    
    
#--------------------------------------------------------------------------------------------------        
if __name__=="__main__":
    main(sys.argv)
    
#include <vector>
#include <fstream>
/*stringstream*/
#include <sstream>
#include <cstring>
#include <iomanip>	
#include <cmath>
#include <thread>
#include <mutex>
#include <map>
#include <algorithm>

/*计时*/
#include "Timer.h"
/*定时汇报消息*/
#include "notify.h"

#if defined(linux) || defined(__linux) || defined(__linux__)
    //CPU core
	#include <sys/sysinfo.h>
#else
    #include <windows.h>
    #include <Winbase.h>
#endif

inline int get_CPU_core_num()
{
#if defined(WIN32)
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
#elif defined(LINUX) || defined(SOLARIS) || defined(AIX) || defined(linux) || defined(__linux__) || defined(__linux)
	// GNU fuction
	return get_nprocs();   
#else
    #error  "not supported!"
#endif
}

using namespace std;

/*
OLD STANDARD:

   cargo:  “VesselType” = 70 OR “VesselType” = 71 OR “VesselType” = 72 OR “VesselType” = 73 OR “VesselType” = 74 OR “VesselType” = 75 OR “VesselType” = 76 OR “VesselType” = 77 OR “VesselType” = 78 OR “VesselType” = 79 
   tanker: “VesselType” = 80 OR “VesselType” = 81 OR “VesselType” = 82 OR “VesselType” = 83 OR “VesselType” = 84 OR “VesselType” = 85 OR “VesselType” = 86 OR “VesselType” = 87 OR “VesselType” = 88 OR “VesselType” = 89 
   tug and tow: “VesselType” = 31 OR “VesselType” = 32 OR “VesselType” = 52 
                and 21, 22
                
NEW-STANDARD SINCE 2018:
   cargo: 1003,1004,1016
   tanker: 1017,1024
   tug tow: 1023,1025
*/
const unsigned int SELECT_VESSEL_TYPE[] = {
	80,81,82,83,84,85,86,87,88,89,1017,1024	
}; // only select tankers

// if file with one line larger than this number, it is a illegal file
#define BUFFER_SIZE  10024
// vessel running state ...
#define VESSEL_RUN   1
#define VESSEL_STOP  0

/*----------------------------------------------------------------------------------------------
 transfer time in excel to second
  eg:
>>> time_2_second("2017-01-01T01:52:14")
     1483253534
>>> time_2_second("2017-01-01T01:52:15")
     1483253535
>>> time_2_second("2017-01-01T01:52:16")
     1483253536
>>> time_2_second("2017-01-01T02:52:16")
     1483257136
*/
//FIXME: C++时间格式转换研究一下


string status2string(int vessel_state_int_value)
{
	if (VESSEL_RUN == vessel_state_int_value)
		return "running";
	else if(VESSEL_STOP == vessel_state_int_value)
		return "stop";
	else
		return "undefined";
}

// API for find the best L0 of longitude for coordinate transfering
int find_best_coordinate_transfer_L0(float minlongitude, float maxlongtitude)
{
	float midlongti = (minlongitude + maxlongtitude) / 2;
	int sign = 1;
	if (midlongti < 0) sign = -1; 
	midlongti = abs(midlongti);
	//-------------------------
	int L0_min = int(midlongti);
	int L0_max = L0_min;
	if ((L0_max - midlongti) < (midlongti - L0_min))
		return L0_max * sign;
	else
		return L0_min * sign;
}

// column name in original csv-style AIS data
enum FIELDS {
	MMSI,
	BaseDateTime,
	LAT,
	LON,
	SOG,
	COG,
	Heading,
	VesselName,
	IMO,
	CallSign,
	VesselType,
	Status,
	Length,
	Width,
	Draft,
	Cargo
};

//----- field name of databse ------------
string FIELD_NAME[] = {
	"MMSI",
    "BaseDateTime",
    "LAT",
    "LON",
    "SOG",
    "COG",
    "Heading",
    "VesselName",
    "IMO",
    "CallSign",
    "VesselType",
    "Status",
    "Length",
    "Width",
    "Draft",
    "Cargo"
};

//--- hash table of vessel running status ---
// vessel status meaning 
// if we failed to find the status string in the following table, we also it is in running state!
map<string, int> vessel_running_dictionary = {
	{"", VESSEL_RUN},
	{"undefined", VESSEL_RUN},
	{"under way using engine", VESSEL_RUN},
	{"under way sailing", VESSEL_RUN},
	{"AIS-SART (active); MOB-AIS; EPIRB-AIS", VESSEL_RUN},
	{"moored", VESSEL_STOP},
	{"at anchor", VESSEL_STOP},
	{"not under command", VESSEL_STOP},
	{"constrained by her draught", VESSEL_STOP},
	{"restricted maneuverability", VESSEL_STOP},
	{"reserved for future use (13)", VESSEL_STOP},
	{"power-driven vessel towing astern", VESSEL_STOP},
	{"power-driven vessel pushing ahead or towing alongside", VESSEL_STOP}
};

class MBR
{
public:
	// 删除默认构造函数
	MBR() = delete;
	MBR(const float* b): boundary{*(b+min_longti), *(b+min_lanti),
								   *(b+max_longti), *(b+max_lanti)} {}
	~MBR();

protected:
	void set(float*);
	void update(float, float);
	float* get_boundary();
	bool is_inside(float, float);

protected:
	enum {min_longti, min_lanti, max_longti, max_lanti};
	float boundary[4];
};

MBR::~MBR() {}

void MBR::set(float *b)
{
	for(unsigned int i = 0; i < 4; ++i)
		boundary[i] = *(b+i);
}

void MBR::update(float longti, float lanti)
{
	if(longti < boundary[min_longti])
		boundary[min_longti] = longti;
	if(longti > boundary[max_longti])
		boundary[max_longti] = longti;
	if(lanti  < boundary[min_lanti])
		boundary[min_lanti] = lanti;
	if(lanti > boundary[max_lanti])
		boundary[max_lanti] = lanti;
}

float* MBR::get_boundary()
{
	float* b = boundary;
	return b;
}

bool MBR::is_inside(float longti, float lanti)
{
	
	if(longti < boundary[max_longti] && 
	   longti > boundary[min_lanti] && 
	   lanti  < boundary[max_longti] && 
	   lanti  > boundary[min_lanti])
		return true;
	else
		return false;
}

inline long estimate_record_num(istream &is)
{
	char buffer[BUFFER_SIZE];
	// estimate record numbers using 50 records
	is.seekg(0, ios::end);
	long long file_size = is.tellg();
	is.seekg(0, ios::beg);
	long long i = 0;
	for (i = 0; i < 50; ++i)
	{
		if (is.fail())
			break;
		is.getline(buffer, BUFFER_SIZE - 2);
	}
	if (i == 0)
	{
		cout << "[Error] bad file format" << endl;
		return 0;
	}
	int size1 = is.tellg();
	int iall = file_size / size1*i;
	cout << "total about " << iall << " records ..." << endl;
	// --- now estimate the mbr -------------------------    
	is.clear();
	is.seekg(0, ios::beg);
	return iall;
}

/*
 each record is mapped to one line of excel file
 we let every member to be public, so that to accelerate the speed
*/

class VesselPos
{
/*......................................................................................................................
     create a vessel_pos_record instance according to a line of AIS
     @parameters:
        - line_in_excel, line text extracted from an csv-formated AIS file if they are certain type of vessels specified in
                         SELECTED_VESSEL_TYPES
        - mbr_boudary,   only position in this boundary will be generated an instance
                         if mbr_boudary == None, will not limit positioning area!
     @return
        - if in the given area and is the specified vessel type, generate an instance
        - otherwise None is returned!
......................................................................................................................*/	
public:
	VesselPos() = delete;
	//VesselPos();
	~VesselPos();


public:
	int MMSI;
	
};

/*-------------------------------------------------------------------------
  we use hash map data structure to organize data

   (MMSI, vector)
   all position with the same MMSI are stored in the same vector
   and we will sort all those vectors using time information
   
   stop -> stop ,       ignore it
   stop -> runing,      insert it, interpolate = time difference / 2
   running -> running,  insert it, interpolate num = time difference
   running -> stop,     insert it, interpolate num = time difference / 2
-------------------------------------------------------------------------*/
class RecordTree
{
public:
	const int FSM_STATE_VESSEL_STOP = 0;
	const int FSM_STATE_VESSEL_RUNNING = 1;

	
};

bool extract_data(const char* input_file,
				  const char* output_file
				  //TODO: add some parameters to...
)
{
	const char* separater = ", \t";

	ifstream is(input_file);
	ofstream os(output_file);

	if (!is.is_open())
	{
		cerr << "[Error]: fail to open source file:" << input_file << endl;
		return false;
	}
	if (!os.is_open())
	{
		cerr << "[Error]: fail to write file:" << output_file << endl;
		return false;
	}

	int iall_record = estimate_record_num(is);

	std::ostringstream out;
	out << std::fixed << setprecision(2);

	// vector<string> values;
	char buffer[BUFFER_SIZE];
	char temp[BUFFER_SIZE];
	double longitude, latitude;

	vector<std::string>	ps;
	ps.reserve(iall_record * 1.2);

	// 1. try to estimate positioning data number
	cout << "[1] Now read data to memory ..." << endl;
	Process_notify notify0(5);	// each 5% gives a notification
	int _i(0);
	while(is.getline(buffer, BUFFER_SIZE - 2))
	{
		if(is.fail())
			break;
		
		ps.push_back(buffer);
		++_i;
		notify0.push(_i * 100 / iall_record, "      Progress of Reading");
	}

	// 2. generate record using each lines' information
	

	// n. write to file
	_i = 0;
	// TODO: ensure which step？
	cout << "[n] Now write to file ..." << endl;
	os << std::fixed << setprecision(2);
	for (auto &line : ps)
	{
		os << line << endl;
		++_i;
		notify0.push(_i * 100 / iall_record, "      Progress of Writing");
	}

	is.close();
	os.close();

	return true;
}

void help(const char** argv)
{
	cout << "========================================================================" << endl;
    cout << "Tools to extract tracks of tanker(AND ONLY tanker) to a new csv file"     << endl;
    cout << "Usage:" 																   << endl;
    cout << "  " << argv[0] << " source_AIS_file  [minx miny maxx maxy]" 			   << endl;
    cout << "parameters:" 															   << endl;
    cout << "       source_AIS_file - Source AIS file " 							   << endl;
    cout << "       minx - minx of clip area " 										   << endl;
    cout << "       miny - miny of clip area " 										   << endl;
    cout << "       maxx - maxx of clip area " 										   << endl;
    cout << "       maxy - maxy of clip area " 										   << endl;
    cout << "If no minx,miny,maxx,maxy are specified, whole map will be processed" 	   << endl;
    cout << "========================================================================" << endl;

}

int main(int argc, char const *argv[])
{
	cout << "========== Extract data interpolate ONLY TANK in certain area ==========" << endl;

	bool FILEOUT_DEBUG1 = true;
	bool FILEOUT_DEBUG2 = true;

	if(argc != 2 || argc != 6)
	{
		help(argv);
		return -1;
	}

	const char* input_longtilanti_file = argv[1];
	char* temp = new char[strlen(input_longtilanti_file) + 1];
	memcpy(temp, input_longtilanti_file, strlen(input_longtilanti_file) - 4);
	temp[strlen(input_longtilanti_file) - 4] = 0;
	string output_xy_file(temp);
	string output_xy_desc_file(temp);
	delete[]temp;

	// 设置一些输出文件的名称
	output_xy_file += "_long_lat.csv";
	output_xy_desc_file += output_xy_file + "_long_lat_MBR.txt";
	if (FILEOUT_DEBUG1)
		string output_xy_file_debug1 = output_xy_file + "long_lat_debug1.txt";
	if (FILEOUT_DEBUG2)
		string output_xy_file_debug2 = output_xy_file + "long_lat_debug2.txt";

	if(argc == 6)
		// 使用用户设置的提取区域范围
		MBR user_cut(float(argv[2]));

	cout << "Now open" << output_xy_file << "for writing final results" << endl;

 	if (!extract_data(input_longtilanti_file, static_cast<const char*>(output_xy_file.c_str())))
	{
		cout << "[ERROR] Failed to extract data ..." << endl;
	} 
	else
	{
		cout << "[OK] Successfully tranferred to file " << output_xy_file << endl;
		// cout << "[OK] Total " << endl;

		string str_boundary;
		string str_record;

		ofstream os(output_xy_desc_file);
		// output additional information to description file
		os << "[Source file]: " << input_longtilanti_file << endl;
		os << "[Destination file]: " << output_xy_file << endl;
		os << "[Extracted fields]: " << endl;
		os << str_record << endl;
		os << str_boundary << endl;

		// write a recommanded Lo for transferring Long/Lat to x/y to file!
		float recommand_L0[4]{0};
		
		int L0 = find_best_coordinate_transfer_L0(recommand_L0[0], recommand_L0[2]);
		// os << "L0 = " << to_string(L0) << endl;
		os << "The recommended L0  = " << to_string(L0) << endl;
		os.close();
	}

	return 0;
}

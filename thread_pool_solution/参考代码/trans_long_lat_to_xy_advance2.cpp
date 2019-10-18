// tanser_long_lat_to_xy.cpp : Defines the entry point for the console application.
//
//  author: xiaoguo zhang, SEU/UMD
//  some code is copied from SEU existing codes.
//
/*
-----------------------
Revision Histry
-----------------------
2019-06-06 Xiaoguo zhang, Modified command line interface
            Plan to update it to a multi-thread application to accelerate the speed....
            说明：这个工具好像可以在统计频率后使用，这样可以节省一点坐标转换的时间！
2019-08-27 Xiaoguo Zhang, add thread-pool to support multi-thread functionality
            currently a simple solution is used:
            I/O --> vector ---> taskcoordinator  ---> thread 1 , transfer and directly write (x,y) to replace the old (long/lat)
                                                    ---> thread 2 , transfer and directly write (x,y) to replace the old (long/lat)
                                                    ---> thread 3 , transfer and directly write (x,y) to replace the old (long/lat)
                                                    ---> thread 4 , transfer and directly write (x,y) to replace the old (long/lat)
            |                            |
            squence 1 -------------------> sequence 3
            actually, later we can optimize it, since I/O is always consuming a lot of time ....


*/
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <string.h>   // actually, cstring is a replacement of string.h
#include <iomanip>
#include <cmath>
#include <thread>
#include <mutex>
#include <map>
#include <algorithm>

#if defined(linux) || defined(__linux) || defined(__linux__)
    #include <sys/sysinfo.h>         // CPU core
#else
    #include <windows.h>
    #include <Winbase.h>
#endif

//#include "CoorTransfer.h"

using namespace std;

#define BUFFER_SIZE  10024           // if file with one line larger than this number, it is a illegal file
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(AFX_BLBETWEENXY1_H__7E8A8BAB_9C50_4778_85BF_4AF48E9BE433__INCLUDED_)
#define AFX_BLBETWEENXY1_H__7E8A8BAB_9C50_4778_85BF_4AF48E9BE433__INCLUDED_

//--------------------------------------------------------------------------------------
//  大地直角坐标系和经纬坐标系相互转换的类         (北纬坐标系) 
//
//
//                       ^ x (Latitude)
//                       | 
//                       |
//                      -+------->  y (Longitude)     
//                       |
//
//  本类不能实例化！！！，真正可用的是其派生类
//--------------------------------------------------------------------------------------
struct CXY
{
public:
	double x;
	double y;
public:
	CXY& operator =(const CXY& xy)
	{
		if (this == &xy)return *this;
		x = xy.x;
		y = xy.y;
		return *this;
	}
public:
	CXY()
	{
		x = 0;
		y = 0;
	}
public:
	CXY(CXY & xy)
	{
		x = xy.x;
		y = xy.y;
	}

};

//-----------------------------------------------------------------------------------
//  本类是内部类，不可实例化！！！
//
//-----------------------------------------------------------------------------------
class CCoorTransfer
{
protected:
	CCoorTransfer();
	virtual ~CCoorTransfer();
protected:
	//-----------------------------------------------------------------------------------
	// 设置中心子午线
	//----------------------------------------------------------------------------------
	void SetL0(const double fL0)
	{
		L0 = fL0;
	}
protected:
	//----------------------------------------------------------------------------------
	//  经纬度到大地平面直角坐标系   (北纬坐标系)                   
	//
	//返回值：                                          
	//       CXY::x---->对应Lat  
	//       CXY::y---->对应Long
	//
	//                       ^ x (Latitude)
	//                       | 
	//                       |
	//                      -+------->  y (Longitude)
	//                       |
	//----------------------------------------------------------------------------------
	CXY LongLatToXY(double Long, double Lat) const;
	void LongLatToXY(double& x, double &y, double Long, double Lat) const;
	//----------------------------------------------------------------------------------
	// 大地平面直角坐标系到经纬度   (北纬坐标系) 
	//
	//返回值:
	//             CXY::x---->存储纬度， CXY::y对应经度
	//----------------------------------------------------------------------------------
	CXY XYToLongLat(double x, double y) const;
	void XYToLongLat(double& Long, double& Lat, double x, double y) const;
protected:

	double p;
	double p2;
	double p4;

	double ee2;
	double ee4;

	double N;
	double e2;
	double c;
	double c0;
	double c2;
	double c4;
	double c6;
	double c8;

	double a;

	double L0;					//中心子午线角度 
};


//----------------------------------------------------------------------------------------------------
//             由经纬度到屏幕地图坐标系的相互转换                                 North  
//                                                                              | 
//             ^  Lat     (N)                           ^ x                   --+--------------->
//             |                                        |                       | O          x  (East)
//             |                                        |                       |              
//             |                                        |                       | 
//          ---+--------------> Longitude   ===> -------+--------> y   ===>     |
//           O |               (East)                 O |                       v  y
//
//
//         经纬度坐标系                          地理直角坐标                  地图屏幕坐标
//-----------------------------------------------------------------------------------------------------
class CCoorTransLocal : public CCoorTransfer
{
public:
	CCoorTransLocal()
	{
		dx = 0.0;
		dy = 0.0;
		CCoorTransLocal::SetL0(117.0);			//缺省中心子午线117度
	}
public:
	void SetL0(const double fL0) 	{ CCoorTransfer::SetL0(fL0); }


public:
	//---------------------------------------------------------------------------
	// 经纬度 直接向 地图屏幕坐标 的相互转换
	//---------------------------------------------------------------------------
	CXY MapXYFromLongLat(const double fLong, const float fLat) const
	{
		CXY p;
		CCoorTransfer::LongLatToXY(p.x, p.y, fLong, fLat);
		double temp = p.x;
		p.x = p.y;
		p.y = temp;	//保证纬度对应y,经度对应x
		p.x = p.x - dx;
		p.y = dy - p.y;				//地图翻转，适应屏幕坐标

		CXY p1;
		p1.x = p.x;
		p1.y = p.y;
		return p1;
	}
	//---------------------------------------------------------------------------

	CXY LongLatFromMapXY(const double x, const double y) const
	{
		//1. From Map XY to Geographical Coordinate
		double x1 = x + dx;
		double y1 = dy - y;
		//2. x,y倒置，Transfer to Longitude and Latiude
		double Long, Lat;
		CCoorTransfer::XYToLongLat(Long, Lat, y1, x1);

		CXY p1;
		p1.x = Long;
		p1.y = Lat;
		return p1;
	}

protected:
	//---------------------------------------------------------------------------
	// 经纬度 直接向   |_y__________________\ x
	//
	//  (经纬度向 上面坐标系的转换)
	//---------------------------------------------------------------------------
	CXY EarthXYFromLongLat(const double fLong, const float fLat) const
	{
		double x, y;
		CCoorTransfer::LongLatToXY(x, y, fLong, fLat);
		double temp = x;
		x = y;
		y = temp;	//保证纬度对应y,经度对应x		//保证纬度对应y,经度对应x
		CXY p;
		p.x = x;
		p.y = y;
		return p;
	}

	void MapXYFromLongLat(double &x, double &y, const double fLong, const float fLat) const
	{
		CCoorTransfer::LongLatToXY(x, y, fLong, fLat);
		double temp = x;
		x = y;
		y = temp;		//保证纬度对应y,经度对应x
		x = x - dx;
		y = dy - y;				//地图翻转，适应屏幕坐标
	}
	void LongLatFromMapXY(double &fLong, double &fLat, const double x, const double y) const
	{
		//1. From Map XY to Geographical Coordinate
		double x1 = x + dx;
		double y1 = dy - y;
		//2. x,y倒置，Transfer to Longitude and Latiude
		CCoorTransfer::XYToLongLat(fLong, fLat, y1, x1);
	}
protected:
	//-----------------------------------------------------------------------------
	// 设置偏移和翻转系数
	//-----------------------------------------------------------------------------
	void Set_DX(const double myDX)
	{
		dx = myDX;
	}
	void Set_DY(const double myDY)
	{
		dy = myDY;
	}

protected:
	double dx, dy;  // 坐标变换偏移！！！
};
#endif // !defined(AFX_BLBETWEENXY1_H__7E8A8BAB_9C50_4778_85BF_4AF48E9BE433__INCLUDED_)

// original CoorTransfer.cpp

#define constNum 0.000000001

inline int get_CPU_core_num()
{
#if defined(WIN32)
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
#elif defined(LINUX) || defined(SOLARIS) || defined(AIX) || defined(linux) || defined(__linux__) || defined(__linux)
	return get_nprocs();   //GNU fuction
#else
    #error  "not supported!"
#endif
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCoorTransfer::CCoorTransfer()
{
	///////////////////////////////////////////////////////////////////
	/*******************************************************************
	f=1/298.257223563;     //(0.0033528106647475)
	b=a-a*f;               //(6356752.3142452)
	//********************************************************************/
	//////////////////////////////////////////////////////////////////////
	a = 6378137.0;
	c = 6399593.6257585;         //c=a*a/b;              

	ee2 = 0.0067394967422765;	   //ee2=a*a/(b*b)-1; 
	ee4 = 4.5420816339155e-005;  //ee4=ee2*ee2;      

	e2 = 0.0066943799901413;     //e2=2*f-f*f; 

	p = 206265.0;
	p2 = p*p;                 //(42545250225.000)
	p4 = p2*p2;              //(1.8100983167079e+021)

	///////////////////////////////////////////////////////////////////
	/*******************************************************************
	B0=1-3.0/4.0*ee2+45.0/64.0*ee4-175.0/256.0*ee2*ee4+11025.0/16384.0*ee4*ee4;//0.
	99497710608580
	B2=B0-1; //-0.0050228939142032
	B4=15.0/32.0*ee4-175.0/384.0*ee2*ee4+3675.0/8192.0*ee4*ee4;  //2.1152428336160e
	-005
	B6=-35.0/96.0*ee2*ee4+735.0/2048.0*ee4*ee4;   //-1.1086345825527e-007
	B8=315.0/1024.0*ee4*ee4;    //6.3462980998858e-010
	//********************************************************************/
	//////////////////////////////////////////////////////////////////////

	c0 = 6367449.1458823;      //c0=B0*c;
	c2 = -32144.479876196;     //c2=B2*c;
	c4 = 135.36694554940;      //c4=B4*c;
	c6 = -0.70948108077997;    //c6=B6*c;
	c8 = 0.0040613728867193;   //c8=B8*c;
	//----------------------------------------------------------------------
	L0 = 117;

}

CCoorTransfer::~CCoorTransfer()
{

}

void CCoorTransfer::LongLatToXY(double& x, double&y, double Long, double Lat) const
{
	double X11;
	double cos1, cos2, cos4;
	double sin1;
	double l, l2, l4, N;
	double t, t2, n, n2;
	double Bf;

	Bf = Lat*3.1415926535 / 180;

	cos1 = cos(Bf);
	cos2 = cos1*cos1;
	cos4 = cos2*cos2;

	t = tan(Bf);
	t2 = t*t;

	n = sqrt(ee2)*cos1;
	n2 = n*n;

	sin1 = sin(Bf);

	l = (Long - L0) / p * 3600;
	l2 = l*l;
	l4 = l2*l2;

	N = a / sqrt(1 - ee2*sin1*sin1);

	X11 = c0*Bf + (c2*cos1 + c4*cos2*cos1 + c6*cos4*cos1 + c8*cos2*cos4*cos1)*sin1;

	x = X11 + l2 / 2 * N*sin1*cos1 + l4 / 24 * N*sin1*cos2*cos1*(5 - t2 + 9 * n2 + 4 * n2*n2)
		+ l2*l4 / 720.0*N*sin1*cos4*cos1*(61.0 - 58 * t2 + t2*t2);

	double zhu = l*N*cos1;

	y = l*N*cos1 + l2*l / 6.0*N*cos2*cos1*(1 - t2 + n2) + l4*l / 120.0*N*cos4*cos1*(5.0 - 18.0*t2 + t2*t2 + 14 * n2 - 58 * n2*t2) + 500000;
}

CXY CCoorTransfer::LongLatToXY(double Long, double Lat) const
{
	CXY xy;
	LongLatToXY(xy.x, xy.y, Long, Lat);
	return xy;
}

//---------------------------------------------------------------------------------
void CCoorTransfer::XYToLongLat(double &Long, double &Lat, double x, double y) const
{
	double Bf1, Bf;
	double cos1, cos2, cos4;
	double sin1;

	double Mf, Nf, Nf2;
	double t, t2, n, n2, l;
	double y2, y4;

	Bf = Bf1 = x / c0;  //设置迭代初值

	//迭代求出Bf
	do
	{
		Bf = Bf1;

		sin1 = sin(Bf);
		cos1 = cos(Bf);
		cos2 = cos1*cos1;
		cos4 = cos2*cos2;

		Bf1 = x / c0 - 1.0 / c0*(c2*cos1 + c4*cos2*cos1 + c6*cos4*cos1 + c8*cos2*cos4*cos1)*sin1;

	} while (!(fabs(Bf1 - Bf) <= 0.000000001));


	sin1 = sin(Bf1);
	cos1 = cos(Bf1);
	cos2 = cos1*cos1;
	cos4 = cos2*cos2;

	Mf = c*(1 - 1.5*ee2*cos2 + 15.0 / 8 * ee4*cos4 - 35.0 / 16 * ee2*ee4*cos2*cos4 + 315.0 / 128.0*ee4*ee4*cos4*cos4);
	Nf = c*(1 - 0.5*ee2*cos2 + 3.0 / 8 * ee4*cos4 - 5.0 / 16 * ee2*ee4*cos2*cos4 + 35.0 / 128 * ee4*ee4*cos4*cos4);

	t = tan(Bf1);
	t2 = t*t;

	n = sqrt(ee2)*cos1;
	n2 = n*n;

	y = y - 500000;
	y2 = y*y;
	y4 = y2*y2;
	Nf2 = Nf*Nf;

	Lat = (Bf1 - y2 / (2 * Mf*Nf)*t + y4 / (24 * Mf*Nf2*Nf)*t*(5.0 + 3 * t2 + n2 - 9.0*t2*n2)
		- y2*y4 / (720.0*Mf*Nf2*Nf2*Nf)*t*(61.0 + 90.0*t2 + 45 * t2*t2)) * 180 / 3.1415926535;


	l = (y / (Nf*cos1 + y2 / (6.0*Nf)*(1.0 + 2 * t2 + n2)*cos1) + y4*y / (360.0*Nf2*Nf2*Nf*cos1)
		*(5.0 + 44.0*t2 + 32 * t2*t2 - 2 * n2 - 16 * n2*t2)) * 180 / 3.1415926535;

	Long = l + L0;			//longitude
}

CXY CCoorTransfer::XYToLongLat(double x, double y) const
{
	CXY xy;
	XYToLongLat(xy.x, xy.y, x, y);
	return xy;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class process_notify
{
public:
	void push(int percent, const string& sNotify = "      Processed ")
	{
		if ((percent % __step == 0) && percent != __old_percent)
		{
			cout << sNotify << ":" << percent << "% ..." << endl;
			__old_percent = percent;
		}
	}
	void init(const int step1)
	{
		__step = step1;
		__old_percent = -1;
	}
public:
	process_notify(const int step1 = 5) :__old_percent(-1)
	{
		__step = step1;
	}
protected:
	int __old_percent;
	int __step;
};

//----------------------------------------------------------------------------------------------------------------------------------------
struct TaskInfo
{
	friend ostream & operator <<(ostream& os, const TaskInfo& info)
	{
		return (os << "threadid:" << info.id << ", [" << info.cursor_from << "," << info.cursor_to << "] ");
	}
public:
	std::thread::id    id;
	vector<std::string>*  posinfos;          // pos vector

	CCoorTransLocal*   ct;                   // used to transfer coordinate
	int                cursor_from;          // pos --- subscriber from  
	int                cursor_to;            // pos --- subscribe  to    

    int                long_col;             // which column is longitute/x
    int                lat_col;              // which column is latitude/y
};
//-----------------------------------------------------------------------------------------------------------------------------------------

class mutex_locker
{
public:
	mutex_locker() = delete;
	mutex_locker(std::mutex & locker1) :locker(locker1)
	{
		locker.lock();
	}
	~mutex_locker()
	{
		locker.unlock();
	}
protected:
	std::mutex&  locker;
};
//----------------------------------------------------------------------------------------------------------------------------------------
//   class TaskCoordinator is dedicatedly designed for coordinate resource control during multi-thread KDE computing
//        
//   this class won't activate any thread, and it is just be called by threads stored in threadpool
//---------------------------------------------------------------------------------------------------------------------------------------
class TaskCoordinator
{
public:
	//---------------------------------------------------------------------------------------------------------------------------------------
	// please be aware that, we will do some optimization on block size, so the parameter set could be changed during optimization
	//                   
	//---------------------------------------------------------------------------------------------------------------------------------------
	TaskCoordinator(const int report_percent = 5) : __notify(report_percent)
	{
		__total_threads = get_CPU_core_num() * 2;
		__cursor = 0;
	}

	TaskCoordinator() = delete;
public:
	void init(vector<string>* posinfos,
		CCoorTransLocal* ct,
        const int long_col,
        const int lat_col,
		const int batch_num = 200
		)
	{
		__posinfos = posinfos;
		__ct = ct;
		__batch_num = batch_num;
		__cursor = 0;

        __long_col = long_col;
        __lat_col  = lat_col;
	}

public:

	bool QueryTask(TaskInfo& info, const std::thread::id & thread_id)   //interface for thread, if no task, return false!
	{
		mutex_locker m_locker(__locker);
		// if we have processed all data, quit!
		int iall = __posinfos->size();
		if (this->__cursor == iall)
		{
			__live_tasks.erase(thread_id);
			//cout << "No task for thread:" << thread_id << ", now " << __live_tasks.size() << " threads running, soon to finish..." << endl;
			return false;
		}

		info.posinfos = __posinfos;
		info.ct = __ct;
		info.cursor_from = __cursor;
		info.cursor_to = (std::min<int>)(__cursor + __batch_num, iall-1);
        info.long_col = __long_col;
        info.lat_col  = __lat_col;

		__cursor = info.cursor_to + 1;
		__live_tasks[thread_id] = info;

		// set something for the next call of QueryTask(...)
		__notify.push(100.0*__cursor / (iall));

		return true;
	}

public:
	void dump()
	{

	}
public:

protected:
	std::mutex         __locker;
	process_notify     __notify;         // use this to tell user the current running status!

	vector<std::string>*  __posinfos;             // pos vector
	CCoorTransLocal*   __ct;
	long               __cursor;               // start from this position, the previous have been assigned to other thread!
	int                __batch_num;            // how many position will be distributed to each thread!
    
    int                __long_col;
    int                __lat_col;
public:
	int get_total_threads() const
	{
		return __total_threads;
	}
protected:
	int __total_threads;
	std::map<std::thread::id, TaskInfo>  __live_tasks;
};


//----------------------------------------------------------------------------------------------------------------------------------------
//   class thread_pool is designed for run muli-thread KDE computing based on C++ stad::thread
//   if you want to debug, you can enforce the thread number = 1
//
//---------------------------------------------------------------------------------------------------------------------------------------

class thread_pool
{
public:
	thread_pool(TaskCoordinator& coor) :__coordinator(coor)
	{
#if (DEBUG_TREAD==1)
		int thread_num = 1;
#else
		int thread_num = coor.get_total_threads();
#endif    			
		__threads.resize(thread_num);
	}
	~thread_pool()
	{
		for (auto it = __threads.begin(); it != __threads.end(); ++it)
		{
			delete (*it);
			*it = NULL;
		}
	}
	thread_pool() = delete;
public:
	void start_all()
	{
		for (auto it = __threads.begin(); it != __threads.end(); ++it)
		{
			(*it) = new std::thread(thread_pool::thread_function, &__coordinator);
		}
		cout << "[debug] created " << __threads.size() << " threads ..." << endl;
	}
	void join_all_threads()
	{
		//cout << "ENTER void join_all_threads()" << endl;
		for (auto it = __threads.begin(); it != __threads.end(); ++it)
		{
			// wait, let the thread which calling this function wait untill all those threads quit ...
			(*it)->join();
		}
	}
	//void enable_add_regular_grids
protected:
	static void thread_function(/*const*/ TaskCoordinator * pcoordinator)
	{
		//cout << ">>thread [" << std::this_thread::get_id() << "] actiavted..." << endl;

		TaskCoordinator&coordinator = *pcoordinator;
		TaskInfo  info;
        vector<string> values;
        //char buffer[BUFFER_SIZE];
        char temp[BUFFER_SIZE];
        double longitude, latitude;
        CXY xy;  //double x, y;
        ostringstream os;
        os << std::fixed<<setprecision(2);

		for (; coordinator.QueryTask(info, std::this_thread::get_id());)
		{
			//cout << "queryTask() already done ..." << endl;
			string * ps = info.posinfos->data();   // only c++11 or after supports it, you also can use:     &((*info.posinfos)[0]) to get the original pointer of the real memory of the array!
			CCoorTransLocal*  ct = info.ct;
            const int long_col = info.long_col;
            const int lat_col  = info.lat_col;

			for (int i = info.cursor_from; i <= info.cursor_to; ++i)
			{
				// 1. retrieve longitude and latitude
				values.resize(0);
                string &  line = ps[i];

                stringstream input(line);
                while (input.getline(temp, BUFFER_SIZE - 2, ','))
                {
                    values.push_back(temp);
                }

				longitude = atof(values[long_col].c_str());
				latitude =  atof(values[lat_col].c_str());
				// 2. transfer
				xy = ct->MapXYFromLongLat(longitude, latitude);    // std::to_string() doesn't support setting precision
               
			    // 3. then directly update line in the array
			    os.str("");
                os<<xy.x<<","<<xy.y;
                line = os.str();
			}
		}
	}

protected:
	vector<std::thread*>  __threads;
	TaskCoordinator& __coordinator;

};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline long estimate_record_num(istream & is)
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

bool transfer(const char* input_longlat_file,
	const char* output_xy_file,
	int long_col = 0,           // which col is stored longitude
	int lat_col = 1,            // which col is stored latitude 
	double L0 = 116          // longitude of Center Meridian, which will affect to transferring 
	)
{
	const char* separator = ", \t";

	ifstream is(input_longlat_file);
	ofstream os(output_xy_file);
	if (!is.is_open())
	{
		cerr << "[Error]: fail to open source file:" << input_longlat_file << endl;
		return false;
	}
	if (!os.is_open())
	{
		cerr << "[Error]: fail to write file:" << output_xy_file << endl;
		return false;
	}

	int iall_record = estimate_record_num(is);

	std::ostringstream out;
	out << std::fixed << setprecision(2);   // to make sure we have to digits after the dot, for instance, 1189.23, 23.01,
	// after transfer to xy, 0.01 means 1 centimeter! 
	vector<string> values;
	char buffer[BUFFER_SIZE];
	char temp[BUFFER_SIZE];
	double longitude, latitude;
	//CXY xy;  //double x, y;

	CCoorTransLocal ct;
	ct.SetL0(L0);
	vector<std::string>  ps;
	ps.reserve(iall_record*1.2);
	// 1. try to estimate positioning data number

	cout << "1. Now read data to memory ..." << endl;
	process_notify  notify1(10);
	int __i = 0;
	while (is.getline(buffer, BUFFER_SIZE - 2))
	{
		if (is.fail())
			break;

		ps.push_back(buffer);
		++__i;
		notify1.push(__i * 100 / iall_record, "      Read");
	}
	
	// 2. coordinate transferring
	cout << "2. Now using multi-threads to run coordinate transfering ..." << endl;
	TaskCoordinator coor(10);
	coor.init(&ps, 
              &ct, 
              long_col,
              lat_col,
              200);
	thread_pool     pool(coor);
	pool.start_all();
	cout << "[debug]: join all threads" << endl;
	pool.join_all_threads();

	// 3. write to file
	process_notify  notify2(10);
	__i = 0;
	cout << "3. Now write to file ..." << endl;
	os << std::fixed << setprecision(2);
	for (auto &line : ps)
	{
		os <<line<< endl;
		++__i;
		notify2.push(__i*100.0 / iall_record, "      Write");
	}

	is.close();
	os.close();

	return true;
}

void help(int argc, const char** argv)
{
	struct help_str
	{
		const char* item;
		const char* notice;
	};
	help_str notice[] =
	{
		{ "from_longitude_csv_file", "source file storing longitude,latitude file, in txt or csv file format." },
		{ "L0", "the Center Meridian specified for transferring coorindate, the default data is 116 degree" },
		//{"to_xy_csv_file","the destination csv file to store x-y values."},
		{ "[long_col_set=0]", "in which column, longitude is stored in your source file" },
		{ "[lat_col_set=1]", "in which column, lat is stored in your source file" },
		//{"shift_x","additional value will be added to x direction after transferring, remember east direction is the positive!"},
		//{"shift_y", "additional value will be added to y direction after transferring, remember south direction is the positive!"}	, 
	};

	cout << "====================================================================================================================" << endl;
	cout << argv[0] << " from_longitude_csv_file L0 [long_col_set=0] [lat_col_set=1] " << endl;
	for (int i = 0; i < sizeof(notice) / sizeof(notice[0]); ++i)
	{
		cout << "-" << notice[i].item << ":  " << notice[i].notice << endl;
	}
	cout << "====================================================================================================================" << endl;

}

int main(int argc, const char* argv[])
{
	cout << "============================ coordinate transferring app ==========================" << endl;
	/*     cout<<"argc = "<<argc<<endl;
	for(int __i=0; __i<argc; ++__i)
	{
	cout<<argv[__i]<<" ";
	}
	cout<<endl;
	*/
	if (argc < 3)
	{
		help(argc, argv);
		return -1;
	}
	const char* input_longlat_file = argv[1];
	char* temp = new char[strlen(input_longlat_file) + 1];
	memcpy(temp, input_longlat_file, strlen(input_longlat_file) - 4);
	temp[strlen(input_longlat_file) - 4] = 0;
	string output_xy_file(temp);
	delete[]temp;
	output_xy_file += "_L0_";
	output_xy_file += argv[2];
	output_xy_file += "_xy.csv";
	cout << "[OK] output file name:" << output_xy_file << endl;


	int long_col = 0;           // which col is stored longitude
	int lat_col = 1;            // which col is stored latitude 
	double L0 = atof(argv[2]);            // longitude of Center Meridian, which will affect to transferring 

	if (argc == 5)
	{
		long_col = atof(argv[3]);
		lat_col = atof(argv[4]);
	}
	else if (argc == 4)
	{
		long_col = atof(argv[3]);
		lat_col = long_col + 1;
	}

	if (!transfer(input_longlat_file,
		static_cast<const char*>(output_xy_file.c_str()),
		long_col,           // which col is stored longitude
		lat_col,            // which col is stored latitude 
		L0                  // longitude of Center Meridian, which will affect to transferring 
		))
	{
		cout << "[ERROR]:::failed to transferring data ..." << endl;
	}
	else
	{
		cout << "[OK]:::successfully transferred..." << endl;
	}

	return 0;
}


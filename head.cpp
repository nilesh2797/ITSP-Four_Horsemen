#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctime>
#include <string>
#include <iostream>
#include "opencv2/core.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/aruco.hpp> 
#include <time.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <sstream>

#include <libxml++/libxml++.h>
#include <libxml++/parsers/textreader.h>
#include <curl/curl.h>
#include <cstdlib>
#include <fstream>
#include <stdlib.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT        unsigned short
#define SOCKET    int
#define HOSTENT  struct hostent
#define SOCKADDR    struct sockaddr
#define SOCKADDR_IN  struct sockaddr_in
#define ADDRPOINTER  unsigned int*
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1

using namespace std;
using namespace cv;

std::vector<Mat> newsImages;
Mat weatherIcon, finalCal, tempImage;

class Stream
{
    std::map<int, int> packetsSend;
    std::vector<SOCKET> clients;
    SOCKET sock;
    fd_set master;
    int timeout; 
    int quality; 

    int _write( int sock, char *s, int len ) 
    { 
        if ( len < 1 ) { len = strlen(s); }
        #if defined(__APPLE_CC__) || defined(BSD)
            return send(sock, s, len, 0);
        #elif defined(__linux__)
            return send(sock, s, len, MSG_NOSIGNAL);
        #endif
    }

    public:

        Stream(int port = 0) : sock(INVALID_SOCKET), timeout(10), quality(50)
        {
            FD_ZERO( &master );
            if (port) open(port);
        }

        ~Stream() 
        {
            release();
        }

        bool release();
        bool open(int port);
        bool isOpened();
        bool connect();
        void write(Mat &image);
};

bool Stream::release()
    {
        for(int i = 0; i < clients.size(); i++)
        {
            shutdown(clients[i], 2);
            FD_CLR(clients[i],&master); 
        }
        
        clients.clear();
        
        if (sock != INVALID_SOCKET)
        {
            shutdown(sock, 2);
        }
        sock = (INVALID_SOCKET);
        
        return false;
    }

bool Stream::open(int port)
    {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        SOCKADDR_IN address;       
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        
        while(bind(sock, (SOCKADDR*) &address, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
        {
            cout << "Stream: couldn't bind sock";
            release();
            usleep(1000*10000);
            sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        }
        
        while(listen(sock, 2) == SOCKET_ERROR)
        {
            cout << "Stream: couldn't listen on sock";
            usleep(1000*10000);
        }
        
        FD_SET(sock, &master);    
        
        return true;
    }

bool Stream::isOpened() 
    {
        return sock != INVALID_SOCKET; 
    }

bool Stream::connect()
    {
        fd_set rread = master;
        struct timeval to = {0,timeout};
        SOCKET maxfd = sock+1;
        
        if(select( maxfd, &rread, NULL, NULL, &to ) <= 0)
            return true;

        int addrlen = sizeof(SOCKADDR);
        SOCKADDR_IN address = {0};     
        SOCKET client = accept(sock, (SOCKADDR*)&address, (socklen_t*) &addrlen);

        if (client == SOCKET_ERROR)
        {
            cout << "Stream: couldn't accept connection on sock";
            cout << "Stream: reopening master sock";
            release();
            open(8888);
            return false;
        }
  
        maxfd=(maxfd>client?maxfd:client);
        FD_SET( client, &master );
        _write( client,"HTTP/1.0 200 OK\r\n"
            "Server: Mozarella/2.2\r\n"
            "Accept-Range: bytes\r\n"
            "Max-Age: 0\r\n"
            "Expires: 0\r\n"
            "Cache-Control: no-cache, private\r\n"
            "Pragma: no-cache\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=mjpegstream\r\n"
            "\r\n",0);
        
        clients.push_back(client);
        packetsSend[client] = 0;

        return true;
    }

void Stream::write(Mat &image)
{
    try
    {
        if(clients.size()==0) return;
            
            // Encode the image
        cv::Mat frame = image;
        if(frame.cols > 0 && frame.rows > 0)
        {
            std::vector<uchar>outbuf;
            std::vector<int> params;
            params.push_back(cv::IMWRITE_JPEG_QUALITY);
            params.push_back(quality);
            cv::imencode(".jpg", frame, outbuf, params);
            int outlen = outbuf.size();

            for(int i = 0; i < clients.size(); i++)
            {
                packetsSend[clients[i]]++;

                int error = 0;
                socklen_t len = sizeof (error);
                int retval = getsockopt(clients[i], SOL_SOCKET, SO_ERROR, &error, &len);

                if (retval == 0 && error == 0)
                {
                    char head[400];
                    sprintf(head,"--mjpegstream\r\nContent-Type: image/jpeg\r\nContent-Length: %lu\r\n\r\n",outlen);

                    _write(clients[i],head,0);

                    retval = getsockopt(clients[i], SOL_SOCKET, SO_ERROR, &error, &len);

                    if (retval == 0 && error == 0)
                    {
                        _write(clients[i],(char*)(&outbuf[0]),outlen);
                    }
                }

                if (retval != 0 || error != 0)
                {
                    shutdown(clients[i], 2);
                    FD_CLR(clients[i],&master);
                    std::vector<int>::iterator position = std::find(clients.begin(), clients.end(), clients[i]);
                    if (position != clients.end())
                    {
                        clients.erase(position);
                    }
                }
            }
        }
    }
    catch(cv::Exception & ex){}
}

namespace patch
{
    template < typename T > std::string to_string( const T& n )
    {
        std::ostringstream stm ;
        stm << n ;
        return stm.str() ;
    }
}

Mat clk(640,640,CV_8UC3, Scalar(1, 1, 1)); 
Mat back_up(640,640,CV_8UC3);

Point cent(320,320);
Point perim(320,0);
int rad =320;
float sec_angle=270;
float min_angle=330;
float hour_angle=210;

time_t rawtime;
struct tm * timeinfo;
float second;
float minute;
float hour;
float millisec;
struct timeb tmb;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

struct indent {
  int depth_;
  indent(int depth): depth_(depth) {};
};

std::ostream & operator<<(std::ostream & o, indent const & in)
{
  for(int i = 0; i != in.depth_; ++i)
  {
    o << "  ";
  }
  return o;
}

bool createNewsImages()
{
	CURL *curl;
  CURLcode res;
  std::string readBuffer;

    
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "http://feeds.reuters.com/reuters/INtopNews");
        // example.com is redirected, so we tell libcurl to follow redirection
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        // Perform the request, res will get the return code
    res = curl_easy_perform(curl);
        //Check for errors
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        
        // always cleanup
    curl_easy_cleanup(curl);
         //std::cout << readBuffer << std::endl;
    ofstream myfile ("example.xml");
    myfile<<readBuffer;
  }
  int count = 1, number = 2;
  string toput[3]; 
  toput[0] = "topNews";
  Mat test;
  std::locale::global(std::locale(""));
  try
  {
    xmlpp::TextReader reader("example.xml");

    while(reader.read())
    {
      int depth = reader.get_depth();
      if(reader.has_value())
      {
        if(reader.get_value() == "topNews")
        {
          count = 1;
          toput[count-1] = "topNews";
        }
        else
        {
          if(reader.get_depth() == 4)
          {
            string s = reader.get_value();
            if(s.substr(0, 4) != "http" and !(s[0] >= '0' and s[0] <='9' ) and s != "Reuters News")
            {
              string sub = s.substr(0, 3);
              if(!(sub == "Mon" or sub == "Tue" or sub == "Wed" or sub == "Thu" or sub == "Fri" or sub == "Sat" or sub == "Sun"))
              {
                string toprint;
                count++;
                int i = s.find("<");
                if(i != string::npos)
                 toprint = s.substr(0, i);
                else
                  toprint = s;
                //cout << toprint << endl;
                toput[count-1] = toprint;
                if(count == 3)
                {
                  int cnt = 0, previ = 0;
                  for(int i = 0; i < toput[1].size(); ++i)
                  {
                    previ = i;
                    while(toput[1][i] != ' ' and i < toput[1].size()-1)
                    {
                    	if(toput[1][i] == '\'')
                    	{
                    		toput[1] = toput[1].substr(0, i)+toput[1].substr((i+1)%toput[1].size(), toput[1].size()-i-1);
                    		--i;
                    	}
                    	if(toput[1][i] == '\"')
                    	{
                    		toput[1] = toput[1].substr(0, i)+toput[1].substr((i+1)%toput[1].size(), toput[1].size()-i-1);
                    		--i;
                    	}

                        ++i;
                        ++cnt;
                    }
                    ++cnt;
                    if(cnt > 24)
                    {
                      toput[1] = toput[1].substr(0, previ)+'\n'+toput[1].substr(previ, toput[1].size()-previ);
                      cnt = 0;
                    }
                  }
                  string front = "convert -pointsize 20 -fill gray -draw 'text 30, 100 \"", rear = "\" ' newspaper.jpg ", newImage = "news", command;
                  if(number == 4)
                  	newImage = newImage + patch::to_string(200) + ".jpg";
              	  else
              	  	newImage = newImage + patch::to_string(number*20) + ".jpg";
                  cout<<"front = "<<front<<endl;
                  cout<<"toput[1] = "<<toput[1]<<endl;
                  cout<<"rear = "<<rear;
                  cout<<"newImage = "<<newImage<<endl;
                  command = front;
                  command += toput[1];
                  command += rear;
                  command += newImage;
                  //char *toBePassed = new char[command.size()];
                  std::cout<<"command = "<<command<<endl;/*
                  for(int i = 0; i < command.size(); ++i)
                  {
                    toBePassed[i] = command[i];
                    //cout<<"toBePassed["<<i<<"] = "<<toBePassed[i]<<" and command["<<i<<"] = "<<command[i]<<endl;
                  }
                  std::cout<<"command = "<<toBePassed;*/
                  system(command.c_str());

                  cout<<endl;
                  count = 0;
                  test = imread(newImage, 1);
                  newsImages.push_back(test.clone());
                  //imshow(newImage, test);
                  //waitKey(0);

                  number++;
                  //delete toBePassed;
                }
              }
            }
          }
        }
      }
    }
  }
  catch(const std::exception& e)
  {
    std::cerr << "Exception caught: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

bool getWeather()
{
	  CURL *curl;
    CURLcode res;
    std::string readBuffer;

    
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://api.openweathermap.org/data/2.5/weather?q=Mumbai%2CIndia&mode=xml&appid=4208d08d55c67489ec3914dac5d811b2");
        // example.com is redirected, so we tell libcurl to follow redirection
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        // Perform the request, res will get the return code
        res = curl_easy_perform(curl);
        //Check for errors
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        
        // always cleanup
        curl_easy_cleanup(curl);
         //std::cout << readBuffer << std::endl;
        ofstream myfile ("weather.xml");
        myfile<<readBuffer;

    }

  std::locale::global(std::locale(""));
  string temp, weather, icon;
  int tempInt;

  try
  {
    xmlpp::TextReader reader("weather.xml");

    while(reader.read())
    {
      int depth = reader.get_depth();

      if(reader.has_attributes())
      {
        if(!reader.get_name().compare("weather"))
        {
          reader.move_to_first_attribute();
          do
          {
            if(!reader.get_name().compare("value"))
              weather = reader.get_value();
            if(!reader.get_name().compare("icon"))
              icon = reader.get_value();
          } while(reader.move_to_next_attribute());
        }
        if(!reader.get_name().compare("temperature"))
        {
          reader.move_to_first_attribute();
          temp = reader.get_value();
        }
      }
    }
  }
  catch(const std::exception& e)
  {
    std::cerr << "Exception caught: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  tempInt = 0;
  int i = 0;
  while(isdigit(temp[i]))
  {
    tempInt *= 10;
    tempInt += (temp[i]-'0');
    i++;
  }
  tempInt -= 273;
  cout<<"temperature = "<<tempInt<<" weather = "<<weather<<" icon = "<<icon<<endl;
  string weatherIconPath = "/home/nilesh/ITSP/weather_icons/";
  weatherIconPath += (icon+".png");
  cout<<weatherIconPath<<endl;
  weatherIcon = imread(weatherIconPath, 1);
  string temper = patch::to_string(tempInt);
  string front = "convert -pointsize 120 -fill black -draw 'text 65, 245 ", rear = " ' temp_background.jpg temp_backgroundi.jpg", command;

    command = front+"\""+temper+"\""+rear;
    std::cout<<"command = "<<command;

    system(command.c_str());
    front = "convert -pointsize 40 -fill black -draw 'text 80, 95 ", rear = " ' temp_backgroundi.jpg temperature.jpg", command;

    command = front+"\""+"Mumbai"+"\""+rear;
    std::cout<<"command = "<<command;

    system(command.c_str());

    tempImage = imread("temperature.jpg", 1);

  return EXIT_SUCCESS;
}

void createDateImage()
{
	string day, date, mon, year;
	time_t t = time(0);  
    struct tm * now = localtime( & t );
	int monint = now->tm_mon;
    day = patch::to_string(now->tm_mday);
    year = patch::to_string(now->tm_year+1900);
    if(monint == 0)
    	mon = "JAN";
    if(monint == 1)
    	mon = "FEB";
    if(monint == 2)
    	mon = "MAR";
    if(monint == 3)
    	mon = "APR";
    if(monint == 4)
    	mon = "MAY";
    if(monint == 5)
    	mon = "JUN";
    if(monint == 6)
    	mon = "JUL";
    if(monint == 7)
    	mon = "AUG";
    if(monint == 8)
    	mon = "SEP";
    if(monint == 9)
    	mon = "OCT";
    if(monint == 10)
    	mon = "NOV";
    if(monint == 11)
    	mon = "DEC";

    string front = "convert -pointsize 250 -fill blue -draw 'text 130, 410 ", rear = " ' calendar.jpg cali.jpg", command;

    command = front+"\""+day+"\""+rear;
    std::cout<<"command = "<<command;
	system(command.c_str());
	front = "convert -pointsize 120 -fill black -draw 'text 80, 150 "; rear = " ' cali.jpg cal.jpg"; command = "";

    command = front+"\""+mon+"\""+rear;
    std::cout<<"command = "<<command;
	system(command.c_str());

	finalCal = imread("cal.jpg", 1);
}

int main(int argc, char const *argv[])
{
	Stream wri(7777);
	Mat markerImage, inputImage, imageLogo, finalImage;
	Ptr<aruco::Dictionary> dictionary=aruco::getPredefinedDictionary(aruco::DICT_6X6_250);
	VideoCapture capture(1);
	//VideoCapture capture(0), toBePasted;
	createNewsImages();
	getWeather();
	createDateImage();
	vector< int > markerIds;
	vector< vector<Point2f> > markerCorners, rejectedCandidates;
	Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create(); 

	clk = imread("clocks/clock3.png");
	resize(clk, clk, Size(640, 640));
	string day, date, mon, year;
    time_t t = time(0);  
    struct tm * now = localtime( & t );
    day = patch::to_string(now->tm_mday);
    mon = patch::to_string(now->tm_mon+1);
    year = patch::to_string(now->tm_year+1900);
    date = day+"-"+mon+"-"+year;
    putText(clk, date, Point(120, 280), FONT_HERSHEY_TRIPLEX, 2, Scalar(1, 1, 1), 4, false);

	back_up=clk.clone();

	while(capture.read(inputImage))
	{
		//inputImage = imread("full.jpg", 1);
		//imshow("inputImage", inputImage);
		ftime(&tmb);
		rawtime=tmb.time;
		timeinfo = localtime ( &rawtime );

		second     = timeinfo->tm_sec;
		minute     = timeinfo->tm_min;
		hour       = timeinfo->tm_hour;
		millisec   = tmb.millitm;


		second=second+millisec/1000;
		sec_angle=(second*6)+270;

		minute=minute+second/60;
		min_angle=minute*6+270; 

		if(hour>12)hour = hour-12;
		   hour_angle=(hour*30)+(minute*.5)+270; 


		if(sec_angle>360)sec_angle=sec_angle-360;
		if(min_angle>360)min_angle=min_angle-360;
		if(hour_angle>360)hour_angle=hour_angle-360;

		perim.x =  (int)round(cent.x + (rad-25) * cos(sec_angle * CV_PI / 180.0));
		perim.y =  (int)round(cent.y + (rad-25) * sin(sec_angle * CV_PI / 180.0));
		line(clk,cent,perim, Scalar(255,0,0,0), 4,CV_AA,0);


		perim.x =  (int)round(cent.x + (rad-50) * cos(min_angle * CV_PI / 180.0));
		perim.y =  (int)round(cent.y + (rad-50) * sin(min_angle * CV_PI / 180.0));
		line(clk,cent,perim, Scalar(255,0,0,0), 8,CV_AA,0);


		perim.x =  (int)round(cent.x + (rad-75) * cos(hour_angle * CV_PI / 180.0));
		perim.y =  (int)round(cent.y + (rad-75) * sin(hour_angle * CV_PI / 180.0));
		line(clk,cent,perim, Scalar(255,0,0,0), 12,CV_AA,0);

		imageLogo = clk.clone();
		//imshow("Clock",clk); 
		clk.setTo(0); 
		clk=back_up.clone();

		aruco::detectMarkers(inputImage, dictionary, markerCorners, markerIds, parameters, rejectedCandidates);
		finalImage = inputImage.clone();
			for(int i = 0; i < markerCorners.size(); ++i)
			{
				if(markerIds[i] == 1)
				{
				vector<Point2f> left_image;      
				vector<Point2f> right_image;

				left_image.push_back(Point2f(float(0),float(0)));
				left_image.push_back(Point2f(float(imageLogo.cols),float(0)));
				left_image.push_back(Point2f(float(imageLogo.cols),float(imageLogo.rows)));
			    left_image.push_back(Point2f(float(0),float(imageLogo.rows)));		    

			    for(int j = 0; j < markerCorners[i].size(); ++j)
			    {	
			    	right_image.push_back(markerCorners[i][j]);
			    }
				Mat H = findHomography(  left_image,right_image,0 );
		        Mat logoWarped;
		        warpPerspective(imageLogo,logoWarped,H,finalImage.size() );


		        Mat gray,gray_inv,src1final,src2final;
		        Mat src1 = finalImage, src2 = logoWarped.clone();
			    cvtColor(src2,gray,CV_BGR2GRAY);
			    threshold(gray,gray,0,255,CV_THRESH_BINARY);
			    //adaptiveThreshold(gray,gray,255,ADAPTIVE_THRESH_MEAN_C,THRESH_BINARY,5,4);
			    bitwise_not ( gray, gray_inv );
			    src1.copyTo(src1final,gray_inv);
			    src2.copyTo(src2final,gray);
			    finalImage = src1final+src2final;
				}
				if(markerIds[i] == 80)
				{
					vector<Point2f> left_image;      
					vector<Point2f> right_image;

					left_image.push_back(Point2f(float(0),float(0)));
					left_image.push_back(Point2f(float(weatherIcon.cols),float(0)));
					left_image.push_back(Point2f(float(weatherIcon.cols),float(weatherIcon.rows)));
				    left_image.push_back(Point2f(float(0),float(weatherIcon.rows)));		    

				    for(int j = 0; j < markerCorners[i].size(); ++j)
				    {	
				    	right_image.push_back(markerCorners[i][j]);
				    }
					Mat H = findHomography(  left_image,right_image,0 );
			        Mat logoWarped;
			        warpPerspective(weatherIcon,logoWarped,H,finalImage.size() );


			        Mat gray,gray_inv,src1final,src2final;
			        Mat src1 = finalImage, src2 = logoWarped.clone();
				    cvtColor(src2,gray,CV_BGR2GRAY);
				    threshold(gray,gray,0,255,CV_THRESH_BINARY);
				    //adaptiveThreshold(gray,gray,255,ADAPTIVE_THRESH_MEAN_C,THRESH_BINARY,5,4);
				    bitwise_not ( gray, gray_inv );
				    src1.copyTo(src1final,gray_inv);
				    src2.copyTo(src2final,gray);
				    finalImage = src1final+src2final;
				}
        if(markerIds[i] == 20)
        {
          vector<Point2f> left_image;      
          vector<Point2f> right_image;

          left_image.push_back(Point2f(float(0),float(0)));
          left_image.push_back(Point2f(float(tempImage.cols),float(0)));
          left_image.push_back(Point2f(float(tempImage.cols),float(tempImage.rows)));
            left_image.push_back(Point2f(float(0),float(tempImage.rows)));        

            for(int j = 0; j < markerCorners[i].size(); ++j)
            { 
              right_image.push_back(markerCorners[i][j]);
            }
          Mat H = findHomography(  left_image,right_image,0 );
              Mat logoWarped;
              warpPerspective(tempImage,logoWarped,H,finalImage.size() );


              Mat gray,gray_inv,src1final,src2final;
              Mat src1 = finalImage, src2 = logoWarped.clone();
            cvtColor(src2,gray,CV_BGR2GRAY);
            threshold(gray,gray,0,255,CV_THRESH_BINARY);
            //adaptiveThreshold(gray,gray,255,ADAPTIVE_THRESH_MEAN_C,THRESH_BINARY,5,4);
            bitwise_not ( gray, gray_inv );
            src1.copyTo(src1final,gray_inv);
            src2.copyTo(src2final,gray);
            finalImage = src1final+src2final;
        }
				if(markerIds[i] == 220)
				{
					vector<Point2f> left_image;      
					vector<Point2f> right_image;

					left_image.push_back(Point2f(float(0),float(0)));
					left_image.push_back(Point2f(float(finalCal.cols),float(0)));
					left_image.push_back(Point2f(float(finalCal.cols),float(finalCal.rows)));
				    left_image.push_back(Point2f(float(0),float(finalCal.rows)));		    

				    for(int j = 0; j < markerCorners[i].size(); ++j)
				    {	
				    	right_image.push_back(markerCorners[i][j]);
				    }
					Mat H = findHomography(  left_image,right_image,0 );
			        Mat logoWarped;
			        warpPerspective(finalCal,logoWarped,H,finalImage.size() );


			        Mat gray,gray_inv,src1final,src2final;
			        Mat src1 = finalImage, src2 = logoWarped.clone();
				    cvtColor(src2,gray,CV_BGR2GRAY);
				    threshold(gray,gray,0,255,CV_THRESH_BINARY);
				    //adaptiveThreshold(gray,gray,255,ADAPTIVE_THRESH_MEAN_C,THRESH_BINARY,5,4);
				    bitwise_not ( gray, gray_inv );
				    src1.copyTo(src1final,gray_inv);
				    src2.copyTo(src2final,gray);
				    finalImage = src1final+src2final;
				}
				if(markerIds[i] % 20 == 0 and (markerIds[i]/20) <= newsImages.size() and (markerIds[i]/20) <= 10 and (markerIds[i]/20) >= 2 and (markerIds[i]/20) != 4)
				{
					vector<Point2f> left_image;      
					vector<Point2f> right_image;

					left_image.push_back(Point2f(float(0),float(0)));
					left_image.push_back(Point2f(float(newsImages[(markerIds[i]/20)-1].cols),float(0)));
					left_image.push_back(Point2f(float(newsImages[(markerIds[i]/20)-1].cols),float(newsImages[(markerIds[i]/20)-1].rows)));
				    left_image.push_back(Point2f(float(0),float(newsImages[(markerIds[i]/20)-1].rows)));		    

				    for(int j = 0; j < markerCorners[i].size(); ++j)
				    {	
				    	right_image.push_back(markerCorners[i][j]);
				    }
					Mat H = findHomography(  left_image,right_image,0 );
			        Mat logoWarped;
			        warpPerspective(newsImages[(markerIds[i]/20)-1],logoWarped,H,finalImage.size() );


			        Mat gray,gray_inv,src1final,src2final;
			        Mat src1 = finalImage, src2 = logoWarped.clone();
				    cvtColor(src2,gray,CV_BGR2GRAY);
				    threshold(gray,gray,0,255,CV_THRESH_BINARY);
				    //adaptiveThreshold(gray,gray,255,ADAPTIVE_THRESH_MEAN_C,THRESH_BINARY,5,4);
				    bitwise_not ( gray, gray_inv );
				    src1.copyTo(src1final,gray_inv);
				    src2.copyTo(src2final,gray);
				    finalImage = src1final+src2final;
				}
		    }
		    
		//aruco::drawDetectedMarkers(finalImage, markerCorners, markerIds);
		imshow("outputImage", finalImage);
		wri.connect();
        wri.write(finalImage);
		waitKey(10);
	}
	waitKey(0);
}

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxml++/libxml++.h>
#include <libxml++/parsers/textreader.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <stdio.h>
#include <curl/curl.h>
#include "opencv2/core.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace std;
using namespace cv;

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

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int main(int /* argc */, char** /* argv */)
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
  int count = 1;
  string toput[3]; 
  std::locale::global(std::locale(""));
  try
  {
    xmlpp::TextReader reader("example.xml");

    while(reader.read())
    {
      int depth = reader.get_depth();
      if(reader.has_value())
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
              cout << toprint << endl;
              toput[count-1] = toprint;
              if(count == 3)
              {
                Mat clk(70, 1300, CV_8UC3, Scalar(0, 255, 255));
                cout<<endl;
                count = 0;
                for(int i = 0; i < 3; ++i)
                {
                  putText(clk, toput[i], Point(1, 20+i*15), FONT_HERSHEY_COMPLEX_SMALL, 0.75, Scalar(1, 1, 1), 1, false);
                  imshow("clk", clk);
                }
                waitKey(0);
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
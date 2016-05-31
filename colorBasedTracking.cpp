#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
 #include "opencv2/core.hpp"
#include "opencv2/face.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/objdetect.hpp"
 #include "opencv2/objdetect.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

using namespace std;
using namespace cv;

int main(int argc, char const *argv[])
{
	Mat src, hsv, red, sample, threshold_output;
	VideoCapture captue(0);
	while(captue.read(src))
	{
		Mat drawing = Mat::zeros(src.size(), src.type());

		imshow("src", src);
		cvtColor(src, hsv, CV_BGR2HSV);
		blur( hsv, hsv, Size(3,3) );
		imshow("hsv", hsv);
		inRange(hsv, Scalar(-1, 100, 100), Scalar(6, 255, 255), red);
		Canny( red, threshold_output, 50, 150, 3 );
		imshow("threshold_output", threshold_output);
		imshow("red", red);
		vector<vector<Point> > contours;
  		vector<Vec4i> hierarchy;
		findContours( threshold_output, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_NONE, Point(0, 0) );
		for( size_t i = 0; i< contours.size(); i++ )
     	{
      		Scalar color = Scalar(0, 0, 255);
        	drawContours( drawing, contours, (int)i, color, 1, 8, vector<Vec4i>(), 0, Point() );
        }
        imshow("drawing", drawing);
		waitKey(10);
	}
	return 0;
}
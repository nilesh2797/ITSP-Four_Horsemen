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

using namespace cv;
using namespace std;

int main(int argc, char const *argv[])
{
	Mat input;
	VideoCapture cap("http://192.168.0.102:8080/video");
	while(cap.read(input))
	{
		//cout<<input.rows<<", "<<input.cols<<endl;
		Mat threshold = Mat::zeros(input.rows, input.cols, CV_8UC1);
		//cout<<threshold.rows<<", "<<threshold.cols<<endl;
		for(int row = 0; row < input.rows; ++row)
		{
			for (int col = 0; col < input.cols; ++col)
			{
				double gaddb = input.at<Vec3b>(row, col)[1]+input.at<Vec3b>(row, col)[0], r = input.at<Vec3b>(row, col)[2];
				if(gaddb < 5)
					gaddb = 5;
				double value = (r/gaddb)*((2*r)-gaddb);
				if(value > 1500)
					threshold.at<uchar>(row, col) = 255;
			}
		}
		imshow("input", input);
		imshow("threshold", threshold);
		waitKey(10);
	}
	return 0;
}
#include <stdio.h>
#include <iostream>
#include "opencv2/core.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/aruco.hpp> 

using namespace std;
using namespace cv;

int main(int argc, char const *argv[])
{
	Mat markerImage, inputImage, imageLogo, finalImage;
	imageLogo = imread(argv[1], 1);
	Ptr<aruco::Dictionary> dictionary=aruco::getPredefinedDictionary(aruco::DICT_6X6_250);
	aruco::drawMarker(dictionary, 23, 200, markerImage, 1); 
	//imshow("markerImage", markerImage);
	VideoCapture capture(0);
	while(capture.read(inputImage))
	{
		//imshow("inputImage", inputImage);
		vector< int > markerIds;
		vector< vector<Point2f> > markerCorners, rejectedCandidates;
		Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create(); 
		aruco::detectMarkers(inputImage, dictionary, markerCorners, markerIds, parameters, rejectedCandidates);
		finalImage = inputImage.clone();
			for(int i = 0; i < markerCorners.size(); ++i)
			{
				vector<Point2f> left_image;      // Stores 4 points(x,y) of the logo image. Here the four points are 4 corners of image.
				vector<Point2f> right_image;

				left_image.push_back(Point2f(float(0),float(0)));
			    left_image.push_back(Point2f(float(0),float(imageLogo.rows)));
			    left_image.push_back(Point2f(float(imageLogo.cols),float(imageLogo.rows)));
			    left_image.push_back(Point2f(float(imageLogo.cols),float(0)));

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

		Mat outputImage;
		aruco::drawDetectedMarkers(finalImage, markerCorners, markerIds);
		imshow("outputImage", finalImage);
		waitKey(10);
	}
	waitKey(0);
}
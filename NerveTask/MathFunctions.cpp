#include "stdafx.h"
#include "MathFunctions.h"


// Note: angles should be in degrees!!!
void rotateCoordinates (float x, float y, float alpha, float * xprime, float * yprime)
{
	*xprime = x*cos(alpha*PI/180) + y*sin(alpha*PI/180);
	*yprime = -x*sin(alpha*PI/180) + y*cos(alpha*PI/180);	
}

//// Convert from cartesian to polar coordinates (degrees)
//void CartToPolar (x, y, &rho, &theta_deg)
//{
//	// always use positive valued angles between 0 and 360
//		while (maxAngle > 360)
//		{
//			maxAngle = maxAngle - 360;
//		}
//		while (maxAngle < 0)
//		{
//			maxAngle = maxAngle + 360;
//		}
//}
//
//// Convert from polar (degrees) to cartesian coordinates
//void PolarToCart (rho, theta_deg, &x, &y);
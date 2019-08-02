////////////////////////////////////////////////////////////////////////////////
// File: exponential_variate_inversion.c                                      //
// Routine(s):                                                                //
//    Exponential_Variate_Inversion                                           //
////////////////////////////////////////////////////////////////////////////////

#include <math.h>                             // required for log()
#include <float.h>                            // required for DBL_MAX

//                    Required Externally Defined Routines                    //

extern double Uniform_0_1_Random_Variate( void );

////////////////////////////////////////////////////////////////////////////////
// double Exponential_Variate_Inversion( void )                               //
//                                                                            //
//  Description:                                                              //
//     This function returns an exponentially distributed variate on the      //
//     interval [0, DBL_MAX] using the inversion method i.e. if a random      //
//     variable x has an exponential distribution F(x) = 1 - exp(-x), then    //
//     F(x) has a uniform (0,1) distribution.  So if u is a uniform(0,1)      //
//     distributed random variable, then x = -ln(1-u) is an exponentially     //
//     distributed random variable.  Note that 1 - u is also a uniform(0,1)   //
//     random variable, so that x = -ln(u) is also exponentially distributed. //
//                                                                            //
//  Arguments:                                                                //
//     None                                                                   //
//                                                                            //
//  Return Values:                                                            //
//     A random number with an exponential distribution between 0 and DBL_MAX.//
//                                                                            //
//  Example:                                                                  //
//     double x;                                                              //
//                                                                            //
//     x = Exponential_Variate_Inversion();                                   //
////////////////////////////////////////////////////////////////////////////////
        
double Exponential_Variate_Inversion( void )
{
   double u = Uniform_0_1_Random_Variate();
   if (u == 0.0) return DBL_MAX;
   if (u == 1.0) return 0.0;
   return -log(u);
}

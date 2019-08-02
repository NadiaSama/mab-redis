////////////////////////////////////////////////////////////////////////////////
// File: beta_random_variate.c                                                //
// Routine(s):                                                                //
//    Beta_Random_Variate                                                     //
////////////////////////////////////////////////////////////////////////////////

#include <math.h>                    // required for sqrt(), log(), and pow()

//                    Required Externally Defined Routines                    //

extern double Gamma_Random_Variate( double a );

////////////////////////////////////////////////////////////////////////////////
// double Beta_Random_Variate( double a, double b)                            //
//                                                                            //
//  Description:                                                              //
//     This function returns a beta(a,b) distributed random variate using     //
//     a gamma distributed random variate generator.  If Ga,Gb are independent//
//     random variables, Ga having a gamma(a) distribution and Gb having a    //
//     gamma(b) distribution, the B = Ga / (Ga + Gb) is a random variable     //
//     having a beta distribution with probability density                    //
//             f(x) = (1/B(a,b)) x^(a-1) (1-x)^(b-1),   0 < x < 1             //
//                  = 0                             , elsewhere.              //
//     where B(a,b) is the beta function.                                     //
//                                                                            //
//  Arguments:                                                                //
//     double a                                                               //
//            A shape parameter of the beta distribution corresponding to the //
//            exponent of x in the density given above.                       //
//            This shape parameter must be positive.                          //
//     double b                                                               //
//            A shape parameter of the beta distribution corresponding to the //
//            exponent of 1-x in the density given above.                     //
//            This shape parameter must be positive.                          //
//                                                                            //
//  Return Values:                                                            //
//     A random number distributed as a Beta distribution with shape          //
//     parameters a and b.                                                    //
//                                                                            //
//  Example:                                                                  //
//     double x;                                                              //
//     double a;                                                              //
//     double b;                                                              //
//                                                                            //
//              (* Set the shape parameters a > 0 and b > 0 *)                //
//                                                                            //
//     x = Beta_Random_Variate( a, b );                                       //
////////////////////////////////////////////////////////////////////////////////
        
double Beta_Random_Variate( double a, double b)
{
   double ga = Gamma_Random_Variate(a);
   double gb = Gamma_Random_Variate(b);

   return ga / (ga + gb);
}

////////////////////////////////////////////////////////////////////////////////
// File: gamma_random_variate.c                                               //
// Routine(s):                                                                //
//    Gamma_Random_Variate                                                    //
////////////////////////////////////////////////////////////////////////////////

#include <math.h>                    // required for sqrt(), log(), and pow()

//                    Required Externally Defined Routines                    //

extern double Uniform_0_1_Random_Variate( void );
extern double Exponential_Random_Variate( void );

//                    Required Internally Defined Routines                    //

static double Gamma_Variate_Small_Shape_Parameter( double shape );

////////////////////////////////////////////////////////////////////////////////
// double Gamma_Random_Variate( double shape )                                //
//                                                                            //
//  Description:                                                              //
//     This function returns a Gamma distributed random variate with shape    //
//     parameter "shape" using Best's rejection method from a t-distribution  //
//     with 2 degrees of freedom.                                             //
//                                                                            //
//     Best's algorithm requires that the shape parameter be strictly         //
//     greater than 1.  If the shape parameter equals one, the Gamma          //
//     distribution becomes the exponential distribution.  If the shape       //
//     parameter is positive and strictly less than 1, then Stuart's theorem  //
//     can be used (see Gamma_Variate_Small_Shape_Parameter ).                //
//                                                                            //
//  Arguments:                                                                //
//     double shape                                                           //
//            The shape parameter of the gamma distribution i.e. the          //
//            parameter a where the gamma distribution is given as"           //
//                       (1/gamma(a)) x^(a-1) e^(-x)                          //
//            The shape parameter must be positive.                           //
//                                                                            //
//  Return Values:                                                            //
//     A random number distributed as a Gamma distribution with shape         //
//     parameter "shape".                                                     //
//                                                                            //
//  Example:                                                                  //
//     double x;                                                              //
//     double a;                                                              //
//                                                                            //
//                    (* Set the shape parameter a > 0 *)                     //
//                                                                            //
//     x = Gamma_Random_Variate( a );                                         //
////////////////////////////////////////////////////////////////////////////////
        
double Gamma_Random_Variate( double shape )
{
   double u, v;                 // uniform(0,1) variates
   double w;                    // auxiliary variable
   double x;                    // Gamma(shape) variate
   double y;                    // x - (shape - 1)
   double z;                    
   double a = shape - 1;
   
   if (shape < 1.0) return Gamma_Variate_Small_Shape_Parameter(shape);
   if (shape == 1.0) return Exponential_Random_Variate(); 
   for ( ; ;) {
      u = Uniform_0_1_Random_Variate();
      v = Uniform_0_1_Random_Variate();
      w = u * (1.0 - u);
      y = sqrt( 3.0 * (shape - 0.25) / w) * (u - 0.5);
      x = y + a;
      if ( v == 0.0 ) return x;
      w *= 4.0;
      v *= w;
      z = w * v * v;
      if (log(z) <= 2.0 * (a * log(x/a) - y) ) return x;
   }
}


////////////////////////////////////////////////////////////////////////////////
// static double Gamma_Variate_Small_Shape_Parameter( double shape )          //
//                                                                            //
//  Description:                                                              //
//     This function returns a Gamma distributed random variate with shape    //
//     parameter "shape" using Stuart's theorem:  If X has a Gamma(a+1)       //
//     distribution and U has a uniform(0,1) distribution, then X(U^(1/a))    //
//     has a Gamma(a) distribution.                                           //
//                                                                            //
//  Arguments:                                                                //
//     double shape                                                           //
//            The shape parameter of the gamma distribution i.e. the          //
//            parameter "a" where the gamma distribution is given as          //
//                       (1/gamma(a)) x^(a-1) e^(-x)                          //
//            The shape parameter must be positive.                           //
//                                                                            //
//  Return Values:                                                            //
//     A random number distributed as a Gamma distribution with shape         //
//     parameter "shape".                                                     //
//                                                                            //
//  Example:                                                                  //
//     double x;                                                              //
//     double a;                                                              //
//                                                                            //
//                    (* Set the shape parameter a > 0 *)                     //
//                                                                            //
//     x = Gamma_Variate_Small_Shape_Parameter( a );                          //
////////////////////////////////////////////////////////////////////////////////
        
static double Gamma_Variate_Small_Shape_Parameter( double shape )
{
   double u;
   
   if (shape >= 1.0) return Gamma_Random_Variate(shape);
   u = Uniform_0_1_Random_Variate();
   return (u == 0.0) ? 0.0 : Gamma_Random_Variate(shape+1.0)*pow(u, 1.0/shape);
}

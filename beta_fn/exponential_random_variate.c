////////////////////////////////////////////////////////////////////////////////
// File: exponential_random_variate.c                                         //
// Routine(s):                                                                //
//    Init_Exponential_Random_Variate                                         //
//    Exponential_Random_Variate                                              //
////////////////////////////////////////////////////////////////////////////////

static double (*rn)(void);

////////////////////////////////////////////////////////////////////////////////
// void Init_Exponential_Random_Variate( double (*r_generator)(void) )        //
//                                                                            //
//  Description:                                                              //
//     This function saves the pointer to an exponential random number        //
//     generator.  Subsequent calls to Exponential_Random_Variate (below)     //
//     will call this routine.                                                //
//                                                                            //
//  Arguments:                                                                //
//     double (*r_generator)(void)                                            //
//        The user supplied random number generator generating an             //
//        exponentially distributed random number on [0,inf].                 //
//                                                                            //
//  Return Values:                                                            //
//     None                                                                   //
//                                                                            //
//  Example:                                                                  //
//     extern double (*exprand)(void);                                        //
//     Init_Exponential_Random_Variate( exprand );                            //
////////////////////////////////////////////////////////////////////////////////
        
void Init_Exponential_Random_Variate( double (*r_generator)(void) )
{
   rn = r_generator;
}


////////////////////////////////////////////////////////////////////////////////
// double Exponential_Random_Variate( void )                                  //
//                                                                            //
//  Description:                                                              //
//     This function returns an exponentially distributed random variate on   //
//     [0, DBL_MAX]                                                           //
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
//     x = Exponential_Random_Variate();                                      //
////////////////////////////////////////////////////////////////////////////////
        
double Exponential_Random_Variate( void ) { return (*rn)(); }

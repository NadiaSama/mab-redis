////////////////////////////////////////////////////////////////////////////////
// File: uniform_0_1_variate.c                                                //
// Routine(s):                                                                //
//    Init_32_Uniform_0_1_Random_Variate                                      //
//    Init_64_Uniform_0_1_Random_Variate                                      //
//    Uniform_0_1_Random_Variate                                              //
//    Uniform_32_Bits_Random_Variate                                          //
////////////////////////////////////////////////////////////////////////////////

typedef unsigned long long UINT64;

static double (*rn)(void);
static unsigned long (*in)(void);

////////////////////////////////////////////////////////////////////////////////
// void Init_32_Uniform_0_1_Random_Variate(void (*init_rv)(unsigned int seed),//
//                            unsigned int seed, double (*r_generator)(void) )//
//                                                                            //
//  Description:                                                              //
//     This function calls the user supplied random number seed function      //
//     'init_rv' with the seed 'seed', sets the internal function 'rn'        //
//     to the user-supplied random number generator 'r_generator' which is    //
//     then called for subsequent random numbers on [0,1] by calling          //
//     Uniform_0_1_Random_Variate and sets the internal function 'in' to the  //
//     user-supplied random number generator 'i_generator' which is then      //
//     called for subsequent random integers on [0,2^32] by calling           //
//     Uniform_32_Bits_Random_Variate.                                        //
//                                                                            //
//  Arguments:                                                                //
//     void (*init_rv)(unsigned int seed)                                     //
//        The user supplied random number seed function.                      //
//     unsigned int seed                                                      //
//        The seed, argument to init_rv, used to generate a sequence of       //
//        random numbers by calling r_generator().                            //
//     double (*r_generator)(void)                                            //
//        The user supplied random number generator generating a uniformly    //
//        distributed random number on [0,1].                                 //
//     unsigned long (*i_generator)(void)                                     //
//        The user supplied random number generator generating a uniformly    //
//        distributed random integer on [0,2^32].                             //
//                                                                            //
//  Return Values:                                                            //
//     None                                                                   //
//                                                                            //
//  Example:                                                                  //
//     Init_32_Uniform_0_1_Random_Variate( srand, 4109, normrand, rand);      //
////////////////////////////////////////////////////////////////////////////////
        
void Init_32_Uniform_0_1_Random_Variate( void (*init_rv)(unsigned long seed),  
                             unsigned long seed, double (*r_generator)(void),
                                         unsigned long (*i_generator)(void) )
{
   init_rv(seed);
   rn = r_generator;
   in = i_generator;
}


////////////////////////////////////////////////////////////////////////////////
// void Init_64_Uniform_0_1_Random_Variate( void (*init_rv)(UINT64 seed),     //
//                    unsigned long long seed, double (*r_generator)(void) )  //
//                                                                            //
//  Description:                                                              //
//     This function calls the user specified random number seed function     //
//     'init_rv' with the seed 'seed' and sets the internal function 'rn'     //
//     to the user specified random number generator 'r_generator' which is   //
//     then called for subsequent random numbers on [0,1].                    //
//                                                                            //
//  Arguments:                                                                //
//     void (*init_rv)(UINT64 seed)                                           //
//        The user supplied random number seed function.                      //
//     UINT64 seed                                                            //
//        The seed, argument to init_rv, used to generate a sequence of       //
//        random numbers by calling r_generator().                            //
//     double (*r_generator)(void)                                            //
//        The user supplied random number generator generating a uniformly    //
//        distributed random number on [0,1].                                 //
//                                                                            //
//  Return Values:                                                            //
//     None                                                                   //
//                                                                            //
//  Example:                                                                  //
//     extern double genrand64_real1(void);                                   //
//     extern void init_genrand64(UINT64 seed);                               //
//     uint64 seed = 49967LL;                                                 //
//     Init_64_Uniform_0_1_Random_Variate( init_genrand64, seed,              //
//                                                        genrand64_real1);   //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
        
void Init_64_Uniform_0_1_Random_Variate(
                         void (*init_rv)(unsigned long long seed),  
                        unsigned long long seed, double (*r_generator)(void) )
{
   init_rv(seed);
   rn = r_generator;
}

////////////////////////////////////////////////////////////////////////////////
// double Uniform_0_1_Random_Variate( void )                                  //
//                                                                            //
//  Description:                                                              //
//     This function returns a uniform variate on [0,1].                      //
//                                                                            //
//  Arguments:                                                                //
//     None                                                                   //
//                                                                            //
//  Return Values:                                                            //
//     A random number with a uniform distribution between 0 and 1.           //
//                                                                            //
//  Example:                                                                  //
//     double x;                                                              //
//                                                                            //
//     x = Uniform_0_1_Random_Variate();                                      //
////////////////////////////////////////////////////////////////////////////////
        
double Uniform_0_1_Random_Variate( void )
{
   return rn();
}


////////////////////////////////////////////////////////////////////////////////
// unsigned long Uniform_32_Bits_Random_Variate( void )                       //
//                                                                            //
//  Description:                                                              //
//     This function returns a random integer on [0, 2^32].                   //
//                                                                            //
//  Arguments:                                                                //
//     None                                                                   //
//                                                                            //
//  Return Values:                                                            //
//     A uniform random number r, 0 <= r <= 2^32.                             //
//                                                                            //
//  Example:                                                                  //
//     unsigned long x;                                                       //
//                                                                            //
//     x = Uniform_32_Bits_Random_Variate( void )                             //
////////////////////////////////////////////////////////////////////////////////
        
unsigned long Uniform_32_Bits_Random_Variate( void )
{
   return in();
}

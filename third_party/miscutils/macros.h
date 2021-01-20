#ifndef MISCMACROS_H
#define MISCMACROS_H

#ifndef fora
  #define fora(a,s,e) for (int a=s; a<e; a++)
#endif
#ifndef forlist
  #define forlist(a,s) for (size_t a=0; a<(s).size(); a++)
#endif

#ifndef MEASURE_TIME
#define MEASURE_TIME(x,result) \
{ \
  const auto tStart = std::chrono::high_resolution_clock::now(); \
  x; \
  const auto tFinish = std::chrono::high_resolution_clock::now(); \
  const auto tElapsed = tFinish - tStart; \
  result = std::chrono::duration_cast<std::chrono::milliseconds>(tElapsed).count(); \
}
#endif

#ifndef MEASURE_TIME_START
#define MEASURE_TIME_START(tStart) \
  const auto tStart = std::chrono::high_resolution_clock::now();
#endif

#ifndef MEASURE_TIME_END
#define MEASURE_TIME_END(tStart,result) \
{ \
  const auto tFinish = std::chrono::high_resolution_clock::now(); \
  const auto tElapsed = tFinish - tStart; \
  result = std::chrono::duration_cast<std::chrono::milliseconds>(tElapsed).count(); \
}
#endif

#ifndef DEBUG_CMD
  #ifdef ENABLE_DEBUG_CMD
    #define DEBUG_CMD(x) {x}
  #else
    #define DEBUG_CMD(x) ;
  #endif
#endif

#endif

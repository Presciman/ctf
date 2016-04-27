
#include "btwn_central.h"
#include <ctf_cuda.hpp>
using namespace CTF;

#ifdef __CUDACC_
#define DEVICE __device__
#define HOST __host__
#else
#define DEVICE
#define HOST
#endif

void DEVICE HOST cnfunc(cpath a, cpath & b){
  if (a.w>b.w){ b=a; }
  else if (b.w>a.w){ }
  else { b=cpath(a.w, a.m+b.m, a.c+b.c); }
}

cpath DEVICE HOST cfunc(cpath a, cpath b){
  if (a.w>b.w){ return a; }
  else if (b.w>a.w){ return b; }
  else { return cpath(a.w, a.m+b.m, a.c+b.c); }
}


//(min, +) tropical semiring for mpath structure
Semiring<mpath> get_mpath_semiring(){
  //struct for mpath with w=mpath weight, h=#hops
  MPI_Op ompath;

  MPI_Op_create(
      [](void * a, void * b, int * n, MPI_Datatype*){ 
        for (int i=0; i<*n; i++){ 
          if (((mpath*)a)[i].w < ((mpath*)b)[i].w){
            ((mpath*)b)[i] = ((mpath*)a)[i];
          } else if (((mpath*)a)[i].w == ((mpath*)b)[i].w){
            ((mpath*)b)[i].m += ((mpath*)a)[i].m;
          }
        }
      },
      1, &ompath);

  //tropical semiring with hops carried by winner of min
  Semiring<mpath> p(mpath(INT_MAX/2,1), 
                   [](mpath a, mpath b){ 
                     if (a.w<b.w){ return a; }
                     else if (b.w<a.w){ return b; }
                     else { return mpath(a.w, a.m+b.m); }
                   },
                   ompath,
                   mpath(0,1),
                   [](mpath a, mpath b){ return mpath(a.w+b.w, a.m*b.m); });

  return p;
}



// min Monoid for cpath structure
Monoid<cpath> get_cpath_monoid(){
  //struct for cpath with w=cpath weight, h=#hops
  MPI_Op ocpath;

  MPI_Op_create(
      [](void * a, void * b, int * n, MPI_Datatype*){ 
        for (int i=0; i<*n; i++){ 
          if (((cpath*)a)[i].w > ((cpath*)b)[i].w){
            ((cpath*)b)[i] = ((cpath*)a)[i];
          } else if (((cpath*)a)[i].w == ((cpath*)b)[i].w){
            ((cpath*)b)[i].m += ((cpath*)a)[i].m;
            ((cpath*)b)[i].c += ((cpath*)a)[i].c;
          }
        }
      },
      1, &ocpath);

  Monoid<cpath> cp(cpath(-INT_MAX/2,1,0.), 
                  [](cpath a, cpath b){ 
                    if (a.w>b.w){ return a; }
                    else if (b.w>a.w){ return b; }
                    else { return cpath(a.w, a.m+b.m, a.c+b.c); }
                  }, ocpath);

  Bivar_Kernel<cpath,cpath,cpath,cfunc, cnfunc> tmp;
//  Kernel k = tmp.generate_kernel<cfunc>();

//  Monoid<cpath> cp(cpath(-INT_MAX/2,1,0.), ker);
//  Kernel<cpath, cpath, cpath, cfunc> ker;

  return cp;
}

/*
 * print.h
 *
 *  Created on: 24 mars 2020
 *      Author: sriou
 */

#ifndef PRINT_PREFIX
  #error "PRINT_PREFIX undefined"
#endif

//shall be included after the definition of print_impl function
//void <PRINT_PREFIX>print_impl(const char*msg);

#ifndef PRINT_FUNC_DECL
  #define PRINT_FUNC_DECL static inline
#endif
#ifndef PRINT_DATA_DECL
  #define PRINT_DATA_DECL static
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef PP_STRINGIFY
#define PP_STRINGIFY_IMPL(X) #X
#define PP_STRINGIFY(X) PP_STRINGIFY_IMPL(X)
#endif

#ifndef PP_CONCAT
#define PP_CONCAT_IMPL(x, y) x##y
#define PP_CONCAT(x, y) PP_CONCAT_IMPL( x, y )
#endif

#define ADD_PRINT_PREFIX(a) PP_CONCAT(PRINT_PREFIX,a)

#define _print_enabled       ADD_PRINT_PREFIX(print_enabled      )
#define _print_cfg           ADD_PRINT_PREFIX(print_cfg          )
#define _print               ADD_PRINT_PREFIX(print              )
#define _print_uint_as_hex   ADD_PRINT_PREFIX(print_uint_as_hex  )
#define _print_uint_as_dec   ADD_PRINT_PREFIX(print_uint_as_dec  )
#define _println             ADD_PRINT_PREFIX(println            )
#define _print8x             ADD_PRINT_PREFIX(print8x            )
#define _println8x           ADD_PRINT_PREFIX(println8x          )
#define _print8d             ADD_PRINT_PREFIX(print8d            )
#define _println8d           ADD_PRINT_PREFIX(println8d          )
#define _print16x            ADD_PRINT_PREFIX(print16x           )
#define _println16x          ADD_PRINT_PREFIX(println16x         )
#define _print16d            ADD_PRINT_PREFIX(print16d           )
#define _println16d          ADD_PRINT_PREFIX(println16d         )
#define _print32x            ADD_PRINT_PREFIX(print32x           )
#define _println32x          ADD_PRINT_PREFIX(println32x         )
#define _print32d            ADD_PRINT_PREFIX(print32d           )
#define _println32d          ADD_PRINT_PREFIX(println32d         )
#define _print64x            ADD_PRINT_PREFIX(print64x           )
#define _println64x          ADD_PRINT_PREFIX(println64x         )
#define _print64d            ADD_PRINT_PREFIX(print64d           )
#define _println64d          ADD_PRINT_PREFIX(println64d         )
#define _print_bytes_sep     ADD_PRINT_PREFIX(print_bytes_sep    )
#define _print_bytes         ADD_PRINT_PREFIX(print_bytes        )
#define _println_bytes       ADD_PRINT_PREFIX(println_bytes      )
#define _print_bytes_0x      ADD_PRINT_PREFIX(print_bytes_0x     )
#define _println_bytes_0x    ADD_PRINT_PREFIX(println_bytes_0x   )
#define _print_array32x_0xln ADD_PRINT_PREFIX(print_array32x_0xln)
#define _printuix            ADD_PRINT_PREFIX(printuix           )
#define _printlnuix          ADD_PRINT_PREFIX(printlnuix         )
#define _printulx            ADD_PRINT_PREFIX(printulx           )
#define _printlnulx          ADD_PRINT_PREFIX(printlnulx         )


PRINT_DATA_DECL bool _print_enabled=1;
PRINT_FUNC_DECL void _print_cfg(bool enable){_print_enabled=enable;}

extern void ADD_PRINT_PREFIX(print_impl)(const char*msg);

PRINT_FUNC_DECL void _print(const char*msg){if(!_print_enabled) return;ADD_PRINT_PREFIX(print_impl)(msg);}


PRINT_FUNC_DECL void _print_uint_as_hex(uint64_t num, unsigned int nhexdigits){
  if(!_print_enabled) return;
  const char hex[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

  uint8_t car;
  nhexdigits = nhexdigits < 16 ? nhexdigits : 16;
  uint8_t i = nhexdigits;
  char str_num[17];
  str_num[i] = 0;
  while (i > 0){
    --i;
    car = num & 0xF;
    str_num[i] = hex[car];
    num >>= 4;
  }
  _print(str_num);
}

PRINT_FUNC_DECL void _print_uint_as_dec(uint64_t num, unsigned int ndecdigits){
  if(!_print_enabled) return;
  char res[21];
  char* resptr = &res[20];
  *resptr=0;
  uint64_t t=num;
  unsigned int l=0;
  char zero='0';
  do{
    uint64_t d=t/10;
    uint64_t m=t-10*d;
    *--resptr = zero+m;
    l++;
    zero=d ? zero : ' ';//replace leading zeroes by blank
    t=d;
  } while (t!=0);
  while(l<ndecdigits){
      *--resptr = ' ';
      l++;
  }
  _print(resptr);
}

PRINT_FUNC_DECL void _println(const char*msg){
  if(!_print_enabled) return;
  _print(msg);_print("\n");
}

PRINT_FUNC_DECL void _print8x(const char*msg1,uint8_t val,const char*msg2){
  if(!_print_enabled) return;
  if(msg1) _print(msg1);
  _print_uint_as_hex(val,2);
  if(msg2) _print(msg2);
}
PRINT_FUNC_DECL void _println8x(const char*msg1,uint8_t val){
  if(!_print_enabled) return;
  _print8x(msg1,val,"\n");
}

PRINT_FUNC_DECL void _print8d(const char*msg1,uint32_t val,const char*msg2){
  if(!_print_enabled) return;
  if(msg1) _print(msg1);
  _print_uint_as_dec(val,3);
  if(msg2) _print(msg2);
}
PRINT_FUNC_DECL void _println8d(const char*msg1,uint32_t val){
  if(!_print_enabled) return;
  _print8d(msg1,val,"\n");
}


PRINT_FUNC_DECL void _print16x(const char*msg1,uint16_t val,const char*msg2){
  if(!_print_enabled) return;
  if(msg1) _print(msg1);
  _print_uint_as_hex(val,4);
  if(msg2) _print(msg2);
}
PRINT_FUNC_DECL void _println16x(const char*msg1,uint16_t val){
  if(!_print_enabled) return;
  _print16x(msg1,val,"\n");
}

PRINT_FUNC_DECL void _print16d(const char*msg1,uint32_t val,const char*msg2){
  if(!_print_enabled) return;
  if(msg1) _print(msg1);
  _print_uint_as_dec(val,5);
  if(msg2) _print(msg2);
}
PRINT_FUNC_DECL void _println16d(const char*msg1,uint32_t val){
  if(!_print_enabled) return;
  _print16d(msg1,val,"\n");
}


PRINT_FUNC_DECL void _print32x(const char*msg1,uint32_t val,const char*msg2){
  if(!_print_enabled) return;
  if(msg1) _print(msg1);
  _print_uint_as_hex(val,8);
  if(msg2) _print(msg2);
}
PRINT_FUNC_DECL void _println32x(const char*msg1,uint32_t val){
  if(!_print_enabled) return;
  _print32x(msg1,val,"\n");
}

PRINT_FUNC_DECL void _print32d(const char*msg1,uint32_t val,const char*msg2){
  if(!_print_enabled) return;
  if(msg1) _print(msg1);
  _print_uint_as_dec(val,10);
  if(msg2) _print(msg2);
}
PRINT_FUNC_DECL void _println32d(const char*msg1,uint32_t val){
  if(!_print_enabled) return;
  _print32d(msg1,val,"\n");
}

PRINT_FUNC_DECL void _print64x(const char*msg1,uint64_t val,const char*msg2){
  if(!_print_enabled) return;
  if(msg1) _print(msg1);
  _print_uint_as_hex(val,16);
  if(msg2) _print(msg2);
}
PRINT_FUNC_DECL void _println64x(const char*msg1,uint64_t val){
  if(!_print_enabled) return;
  _print64x(msg1,val,"\n");
}

PRINT_FUNC_DECL void _print64d(const char*msg1,uint32_t val,const char*msg2){
  if(!_print_enabled) return;
  if(msg1) _print(msg1);
  _print_uint_as_dec(val,10);
  if(msg2) _print(msg2);
}
PRINT_FUNC_DECL void _println64d(const char*msg1,uint32_t val){
  if(!_print_enabled) return;
  _print64d(msg1,val,"\n");
}

PRINT_FUNC_DECL void _print_bytes_sep(const char*msg1,const void*const buf,unsigned int size, const char*msg2, const char*const separator){
  if(!_print_enabled) return;
  const uint8_t*const buf8=(const uint8_t*const)buf;
  if(msg1) _print(msg1);
  const char*sep=0;
  for(unsigned int i=0;i<size;i++){
      _print8x(sep,buf8[i],0);
      sep=separator;
  }
  if(msg2) _print(msg2);
}

PRINT_FUNC_DECL void _print_bytes(const char*msg1,const void*const buf,unsigned int size, const char*msg2){
  if(!_print_enabled) return;
  _print_bytes_sep(msg1,buf,size,msg2," ");
}
PRINT_FUNC_DECL void _println_bytes(const char*msg1,const void*const buf,unsigned int size){
  if(!_print_enabled) return;
  _print_bytes(msg1,buf,size,"\n");
}

PRINT_FUNC_DECL void _print_bytes_0x(const char*msg1,const void*const buf,unsigned int size, const char*msg2){
  if(!_print_enabled) return;
  _print(msg1);
  _print_bytes_sep("0x",buf,size,msg2,",0x");
}
PRINT_FUNC_DECL void _println_bytes_0x(const char*msg1,const void*const buf,unsigned int size){
  if(!_print_enabled) return;
  _print_bytes_0x(msg1,buf,size,"\n");
}

PRINT_FUNC_DECL void _print_array32x_0xln(const void*const buf, unsigned int n_elements){
  if(!_print_enabled) return;
  const uint32_t *elements=(const uint32_t*)buf;
  for(unsigned int i=0;i<n_elements;i++){
      _print16d("",i,"");
      _println32x(": 0x",elements[i]);
  }
}

PRINT_FUNC_DECL void _printuix(const char*msg1,unsigned int val,const char*msg2){
  if(!_print_enabled) return;
  if(msg1) _print(msg1);
  _print_uint_as_hex(val,sizeof(unsigned int)*2);
  if(msg2) _print(msg2);
}
PRINT_FUNC_DECL void _printlnuix(const char*msg1,unsigned int val){
  if(!_print_enabled) return;
  _printuix(msg1,val,"\n");
}

PRINT_FUNC_DECL void _printulx(const char*msg1,unsigned long val,const char*msg2){
  if(!_print_enabled) return;
  if(msg1) _print(msg1);
  _print_uint_as_hex(val,sizeof(unsigned long)*2);
  if(msg2) _print(msg2);
}
PRINT_FUNC_DECL void _printlnulx(const char*msg1,unsigned long val){
  if(!_print_enabled) return;
  _printulx(msg1,val,"\n");
}

#if 0
#define PRINTLN16X_VAR(name) do{\
    _print(#name);\
    println16x(" = 0x",name);\
  }while(0)
#define PRINTLN16D_VAR(name) do{\
    _print(#name);\
    println16d(" = ",name);\
  }while(0)
#endif

#undef _print_enabled
#undef _print_cfg
#undef _print
#undef _print_uint_as_hex
#undef _print_uint_as_dec
#undef _println
#undef _print8x
#undef _println8x
#undef _print8d
#undef _println8d
#undef _print16x
#undef _println16x
#undef _print16d
#undef _println16d
#undef _print32x
#undef _println32x
#undef _print32d
#undef _println32d
#undef _print64x
#undef _println64x
#undef _print64d
#undef _println64d
#undef _print_bytes_sep
#undef _print_bytes
#undef _println_bytes
#undef _print_bytes_0x
#undef _println_bytes_0x
#undef _print_array32x_0xln
#undef _printuix
#undef _printlnuix
#undef _printulx
#undef _printlnulx

#undef ADD_PRINT_PREFIX
#undef PRINT_PREFIX
#undef PRINT_FUNC_DECL
#undef PRINT_DATA_DECL
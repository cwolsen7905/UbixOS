/**
 * Compiler configuration
 */

#ifdef __GNUC__
#define COMPILER "GNU C Compiler " __VERSION__
typedef enum {false = 0, true = !0} bool;
typedef unsigned						size_t;
typedef signed char					__int8;
typedef signed short int			__int16;
typedef signed int					__int32;
typedef signed long long int		__int64;

#define	int8		__int8
#define	int16		__int16
#define	int32		__int32
#define	int64		__int64

typedef unsigned char               uint8;
typedef unsigned short int          uint16;
typedef unsigned int                uint32;
typedef unsigned long long int      uint64;

#define	uint8		__uint8
#define	uint16	__uint16
#define	uint32	__uint32
#define	uint64	__uint64

/*typedef void*                       pointer;*/
/*typedef unsigned char               string;*/

#define discardable
#else
#error "Compiler not supported"
#endif /* __GNUC__ */


/*
 * For your sanity, I recommend not reading the following articles
 * https://github.com/pfultz2/Cleak/wiki/C-Preprocessor-tricks,-tips,-and-idioms
 * http://jhnet.co.uk/articles/cpp_magic
 */
#define EMPTY()
#define DEFER(id) id EMPTY()
#define OBSTRUCT(...) __VA_ARGS__ DEFER(EMPTY)()

#define EVAL(...) EVAL1(EVAL1(EVAL1(EVAL1(__VA_ARGS__))))
#define EVAL1(...) EVAL2(EVAL2(EVAL2(EVAL2(__VA_ARGS__))))
#define EVAL2(...) EVAL3(EVAL3(EVAL3(EVAL3(__VA_ARGS__))))
#define EVAL3(...) __VA_ARGS__

#define CAT(a,...) CAT1(a,__VA_ARGS__)
#define CAT1(a, ... ) a ## __VA_ARGS__

#define PROBE(x) x,1
#define CHECK_( x, n, ... ) n
#define CHECK(...) CHECK_(__VA_ARGS__,0)
#define TESTNULL( a ) CHECK( PROBE##a(()) )

#define NOT(p) CAT( _NOT_, p )
#define _NOT_0 1
#define _NOT_1 0

#define IF( p ) CAT( _IF_, p )
#define _IF_0(...)
#define _IF_1(...) __VA_ARGS__

#define IF_ELSE( p ) CAT( _IF_, p )
#define _IF_ELSE_0(...) _ELSE_0
#define _IF_ELSE_1(...) __VA_ARGS__ _ELSE_1
#define _ELSE_0(...) __VA_ARGS__
#define _ELSE_1(...)


#define CHAIN( varname, ... ) \
	EVAL(\
	do { \
		varname = 0;\
		CHAIN1( varname ,##__VA_ARGS__, )\
	} while(0))

#define CHAIN1( varname, a, ... )\
	IF( NOT( TESTNULL( a ) ) )\
		(\
			varname = a; \
			if( varname ) { \
				printf("Command \"%s\" failed with error %d\n", #a, err);\
				break;\
			}\
			OBSTRUCT(_CHAIN1)()( varname, __VA_ARGS__ )\
		)
#define _CHAIN1() CHAIN1

#define ANDCHAIN( varname, ... ) \
	EVAL(\
	do { \
		varname = 0;\
		NCHAIN1( varname ,##__VA_ARGS__, )\
	} while(0))

#define NCHAIN1( varname, a, ... )\
	IF( NOT( TESTNULL( a ) ) )\
		(\
			varname = a; \
			if( !varname ) { \
				printf("Command \"%s\" not true\n", #a);\
				break;\
			}\
			OBSTRUCT(_NCHAIN1)()( varname, __VA_ARGS__ )\
		)
#define _NCHAIN1() NCHAIN1

int printf_chainable( const char* format, ... );

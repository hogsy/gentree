/* Copyright (C) Mark E Sowden <hogsy@oldtimes-software.com> */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#	include <direct.h>
#	include <Windows.h>
#	define l_mkdir( X ) mkdir( X )
#else
#	include <sys/stat.h>
#	include <errno.h>
#	include <linux/limits.h>
#	define _stricmp     strcasecmp
#	define l_mkdir( X ) mkdir( X, 0777 )
#	define MAX_PATH     PATH_MAX
#endif

void *GTAllocOrDie( size_t pool ) {
	void *p = malloc( pool );
	if ( p == NULL ) {
		printf( "Failed on allocating %zu bytes!\n", pool );
		abort();
	}

	memset( p, 0, pool );
	return p;
}

/* https://stackoverflow.com/a/42794218
 * CC BY-SA 3.0 */
static char *convert_from_wstring( const wchar_t *wstr, int wstr_len ) {
#if defined( _WIN32 )
	size_t num_chars = WideCharToMultiByte( CP_UTF8, 0, wstr, wstr_len, NULL, 0, NULL, NULL );
#else
	size_t num_chars = wcstombs( NULL, wstr, 0 );
#endif
	char *strTo = ( char * ) GTAllocOrDie( ( num_chars + 1 ) * sizeof( char ) );
#if defined( _WIN32 )
	WideCharToMultiByte( CP_UTF8, 0, wstr, wstr_len, strTo, num_chars, NULL, NULL );
#else
	wcstombs( strTo, wstr, num_chars );
#endif
	strTo[ num_chars ] = '\0';
	return strTo;
}

enum {
	GT_FILETYPE_C,
	GT_FILETYPE_ASM,
	GT_FILETYPE_UNKNOWN,
	GT_FILETYPE_ERROR,
};

unsigned int GTGetFileType( const char *path ) {
	const char *c = strrchr( path, '.' );
	if ( c == NULL ) {
		return GT_FILETYPE_ERROR;
	}

	/* c-like languages */
	if ( _stricmp( c, ".c" ) == 0 ||
	     _stricmp( c, ".cpp" ) == 0 ||
	     _stricmp( c, ".cs" ) == 0 ||
	     _stricmp( c, ".h" ) == 0 ||
	     _stricmp( c, ".hpp" ) == 0 ) {
		return GT_FILETYPE_C;
	} else if ( _stricmp( c, ".asm" ) == 0 ) {
		return GT_FILETYPE_ASM;
	}

	return GT_FILETYPE_UNKNOWN;
}

static void GTCreatePath( const char *path ) {
	size_t length = strlen( path );
	char *tmp = ( char * ) GTAllocOrDie( ( length + 1 ) * sizeof( char ) );
	for ( size_t i = 0; i < length; ++i ) {
		tmp[ i ] = path[ i ];
		if ( i != 0 &&
		     ( path[ i ] == '\\' || path[ i ] == '/' ) &&
		     ( path[ i - 1 ] != '\\' && path[ i - 1 ] != '/' ) ) {
			if ( l_mkdir( tmp ) != 0 ) {
				if ( errno == EEXIST ) {
					continue;
				}

				printf( "Failed to create directory, \"%s\"\n", tmp );
				free( tmp );
				return;
			}
		}
	}

	l_mkdir( tmp );
	free( tmp );
}

/**
 * Really fucking dumb way to check, but I'm lazy.
 */
static bool IsUTF16( FILE *file ) {
	char buf[ 2 ];
	fread( buf, sizeof( char ), 2, file );
	rewind( file );
	return ( buf[ 0 ] == '\xff' && buf[ 1 ] == '\xfe' );
}

int main( int argc, char **argv ) {
	printf( "gentree\n"
	        "    Created by Mark Sowden (https://oldtimes-software.com)\n"
	        "----------------------------------------------------------\n\n" );

	if ( argc != 2 ) {
		printf( "Usage: gentree <file>\n" );
		return EXIT_SUCCESS;
	}

	const char *path = argv[ 1 ];

	FILE *file = fopen( path, "rb" );
	if ( file == NULL ) {
		printf( "Failed to open \"%s\"!\n", path );
		return EXIT_FAILURE;
	}

	printf( "Opened \"%s\" - proceeding to generate tree\n", path );

	/* get the file length so we can allocate an appropriate buffer */
	fseek( file, 0, SEEK_END );
	size_t length = ftell( file );
	rewind( file );

	/* check if it's UTF-16 ... */
	char *sout;
	if ( IsUTF16( file ) ) {
		printf( "File is encoded as UTF-16 - why do you bring me such pain?\n" );

		/* read it all in */
		int numWChars = ( int ) ( length / sizeof( wchar_t ) );
		wchar_t *wout = GTAllocOrDie( sizeof( wchar_t ) * numWChars );
		fread( wout, sizeof( wchar_t ), numWChars, file );
		fclose( file );

		/* now convert it */
		sout = convert_from_wstring( wout, numWChars );
		free( wout );
	} else {
		/* life is goooood */
		sout = GTAllocOrDie( ( length + 1 ) * sizeof( char ) );
		fread( sout, sizeof( char ), length, file );
	}

	/* and now, finally, parse it... */
	char tmp[ MAX_PATH ];
	char *p = sout, *d = tmp;
	while ( *p != '\0' ) {
		if ( *p < 0 ) {
			p++;
			continue;
		}

		*d = *p++;
		if ( *d == '\r' || *d == '\n' ) {
			*d = '\0';
			if ( tmp[ 1 ] == ':' && ( tmp[ 2 ] == '\\' || tmp[ 2 ] == '/' ) ) {
				d = tmp + 3;
			} else if ( tmp[ 0 ] == '\\' || tmp[ 0 ] == '/' ) {
				d = tmp + 1;
			} else {
				d = tmp;
			}

			/* create the path */
			if ( !( d[ 0 ] == '.' && d[ 1 ] == '.' ) ) {
				const char *lslash;
				if ( ( lslash = strrchr( d, '/' ) ) == NULL )
					lslash = strrchr( d, '\\' );
				if ( lslash != NULL ) {
					size_t pl = ( lslash - d ) + 1;
					char *dir = GTAllocOrDie( sizeof( char ) * pl );
					strncpy( dir, d, pl - 1 );
					dir[ pl - 1 ] = '\0';// Manually null-terminate the string
					GTCreatePath( dir );
					free( dir );
				}

				printf( "\"%s\": ", d );
				if ( ( file = fopen( d, "w" ) ) != NULL ) {
					unsigned int type = GTGetFileType( d );
					switch ( type ) {
						default:
							break;
						case GT_FILETYPE_C:
							fprintf( file, "// " );
							break;
						case GT_FILETYPE_ASM:
							fprintf( file, "; " );
							break;
					}

					fprintf( file, "Generated by gentree (https://github.com/hogsy/gentree)\n" );
					fclose( file );

					printf( "DONE\n" );
				} else {
					printf( "FAILED\n" );
				}
			}

			if ( *p == '\n' ) p++;
			d = tmp;
			continue;
		}
		d++;
	}

	return 0;
}

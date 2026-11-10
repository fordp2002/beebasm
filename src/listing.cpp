/*************************************************************************************************/
/**
	listing.cpp

	Writes a source listing (see the -list command line option).

	This file is part of BeebAsm.

	BeebAsm is free software: you can redistribute it and/or modify it under the terms of the GNU
	General Public License as published by the Free Software Foundation, either version 3 of the
	License, or (at your option) any later version.

	BeebAsm is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
	even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along with BeebAsm, as
	COPYING.txt.  If not, see <http://www.gnu.org/licenses/>.
*/
/*************************************************************************************************/

#include <cstdio>

#include "listing.h"
#include "globaldata.h"
#include "sourcecode.h"
#include "sourcefile.h"

using namespace std;

Listing* Listing::m_gInstance = NULL;

// Layout of a listing row (0-based columns)

static const int	BYTES_COLUMN = 5;			// first code byte; 4 bytes of "XX " take columns 5 to 15
static const int	FLAG_COLUMN = 18;
static const int	SOURCE_COLUMN = 20;
static const int	BYTES_PER_ROW = 4;
static const int	MAX_ROWS_PER_LINE = 16;		// longer runs of data are summarised
static const int	TAB_WIDTH = 4;


/*************************************************************************************************/
/**
	Expand tabs to spaces (so the text lines up in the listing), and drop trailing white space and
	line end characters.
*/
/*************************************************************************************************/
static string Detab( const string& s )
{
	string out;

	for ( size_t i = 0; i < s.length(); i++ )
	{
		char c = s[ i ];

		if ( c == '\t' )
		{
			out += string( TAB_WIDTH - ( out.length() % TAB_WIDTH ), ' ' );
		}
		else if ( c != '\r' && c != '\n' )
		{
			out += c;
		}
	}

	size_t last = out.find_last_not_of( ' ' );
	return ( last == string::npos ) ? string() : out.substr( 0, last + 1 );
}


void Listing::Create()
{
	assert( m_gInstance == NULL );
	m_gInstance = new Listing;
}

void Listing::Destroy()
{
	delete m_gInstance;
	m_gInstance = NULL;
}

Listing::Listing()
	:	m_bEnabled( false ),
		m_bSkipBlank( false )
{
}

Listing::~Listing()
{
}


/*************************************************************************************************/
/**
	Remove leading and trailing spaces.
*/
/*************************************************************************************************/
static string Trim( const string& s )
{
	size_t first = s.find_first_not_of( ' ' );

	if ( first == string::npos )
	{
		return string();
	}

	return s.substr( first, s.find_last_not_of( ' ' ) - first + 1 );
}


/*************************************************************************************************/
/**
	Listing::Open()
*/
/*************************************************************************************************/
bool Listing::Open( const char* filename )
{
	m_filename = filename;
	m_file.open( filename, ios_base::out | ios_base::trunc );

	if ( !m_file )
	{
		return false;
	}

	m_bEnabled = true;

	m_file << "# BeebAsm listing, version 1" << "\n";
	m_file << "# Fixed columns (0-based): 0-3 address (hex), 5-15 machine code (up to 4 bytes)," << "\n";
	m_file << "# 18 flag, 20 onwards the source text exactly as written (tabs expanded to 4)." << "\n";
	m_file << "# Flag: blank = main source file, I = line from an INCLUDEd file," << "\n";
	m_file << "# M = line from a macro expansion, . = more code from the line above." << "\n";
	m_file << "# A label or ORG line shows its address with no code. Other lines have no address." << "\n";
	m_file << "# A row starting with # in column 0 is a comment, never an address." << "\n";

	return true;
}


/*************************************************************************************************/
/**
	Listing::Close()
*/
/*************************************************************************************************/
void Listing::Close( bool keep )
{
	if ( !m_bEnabled )
	{
		return;
	}

	m_file.close();
	m_bEnabled = false;

	if ( !keep )
	{
		remove( m_filename.c_str() );
	}
}


/*************************************************************************************************/
/**
	Listing::FormatRow()

	Lay out one row of the listing.
*/
/*************************************************************************************************/
string Listing::FormatRow( bool showAddr, int addr, const unsigned char* bytes, int count,
						   char flag, const string& text )
{
	string row( SOURCE_COLUMN, ' ' );

	if ( showAddr )
	{
		char buf[ 16 ];
		snprintf( buf, sizeof( buf ), "%04X", addr );
		row.replace( 0, 4, buf );
	}

	for ( int i = 0; i < count; i++ )
	{
		char buf[ 16 ];
		snprintf( buf, sizeof( buf ), "%02X", bytes[ i ] );
		row.replace( BYTES_COLUMN + i * 3, 2, buf );
	}

	row[ FLAG_COLUMN ] = flag;
	row += text;

	size_t last = row.find_last_not_of( ' ' );
	return ( last == string::npos ) ? string() : row.substr( 0, last + 1 );
}


/*************************************************************************************************/
/**
	Listing::BeginLine() / EndLine()
*/
/*************************************************************************************************/
void Listing::BeginLine( const SourceCode* sourceCode, const string& text )
{
	Row row;
	row.m_file = sourceCode->GetFilename();

	if ( sourceCode->GetParent() == NULL )
	{
		row.m_flag = ' ';
	}
	else if ( dynamic_cast<const SourceFile*>( sourceCode ) != NULL )
	{
		row.m_flag = 'I';
	}
	else
	{
		row.m_flag = 'M';
	}

	// Show the text as written (with its tabs) when we have it, and it is the whole line. It is
	// only part of the line when a FOR loop has jumped back to the middle of its first line, and
	// then we show just that part.

	string shown = text;
	const string* raw = sourceCode->GetRawLine();

	if ( raw != NULL )
	{
		string untabbed = *raw;

		for ( size_t i = 0; i < untabbed.length(); i++ )
		{
			if ( untabbed[ i ] == '\t' )
			{
				untabbed[ i ] = ' ';
			}
		}

		if ( Detab( untabbed ).find_first_not_of( ' ' ) == string::npos
				? Detab( text ).find_first_not_of( ' ' ) == string::npos
				: Trim( untabbed ) == Trim( text ) )
		{
			shown = *raw;
		}
	}

	row.m_text = Detab( shown );
	row.m_skip = false;
	row.m_hasAddr = false;

	// The first line of a macro, or of a FOR loop after it jumps back, is just the rest of the line
	// that started it. Leave it out when there is nothing on it.

	if ( row.m_text.empty() && ( row.m_flag == 'M' || m_bSkipBlank ) )
	{
		row.m_skip = true;
	}

	m_bSkipBlank = false;
	row.m_addr = 0;

	m_rows.push_back( row );
	m_open.push_back( m_rows.size() - 1 );
}

void Listing::EndLine()
{
	m_open.pop_back();

	if ( m_open.empty() )
	{
		WriteRows();
	}
}


/*************************************************************************************************/
/**
	Listing::WriteRows()

	Write out the rows of a completed top level line, which may include the rows of macro
	expansions and included files that it caused.
*/
/*************************************************************************************************/
void Listing::WriteRows()
{
	for ( size_t r = 0; r < m_rows.size(); r++ )
	{
		const Row& row = m_rows[ r ];

		if ( row.m_file != m_lastFile )
		{
			m_file << "# file: " << row.m_file << "\n";
			m_lastFile = row.m_file;
		}

		// Cut the code into chunks that fit in a row

		struct Chunk { int m_addr; const unsigned char* m_bytes; int m_count; };
		vector<Chunk> chunks;

		for ( size_t s = 0; s < row.m_segments.size(); s++ )
		{
			const Segment& seg = row.m_segments[ s ];

			for ( size_t i = 0; i < seg.m_bytes.size(); i += BYTES_PER_ROW )
			{
				Chunk c;
				c.m_addr = seg.m_addr + static_cast<int>( i );
				c.m_bytes = &seg.m_bytes[ i ];
				c.m_count = static_cast<int>( seg.m_bytes.size() - i );

				if ( c.m_count > BYTES_PER_ROW )
				{
					c.m_count = BYTES_PER_ROW;
				}

				chunks.push_back( c );
			}
		}

		if ( chunks.empty() && row.m_skip && !row.m_hasAddr )
		{
			continue;
		}

		if ( chunks.empty() )
		{
			m_file << FormatRow( row.m_hasAddr, row.m_addr, NULL, 0, row.m_flag, row.m_text ) << "\n";
		}
		else
		{
			size_t shown = chunks.size();

			if ( shown > static_cast<size_t>( MAX_ROWS_PER_LINE ) )
			{
				shown = MAX_ROWS_PER_LINE - 1;
			}

			for ( size_t k = 0; k < shown; k++ )
			{
				m_file << FormatRow( true, chunks[ k ].m_addr, chunks[ k ].m_bytes, chunks[ k ].m_count,
									 k == 0 ? row.m_flag : '.', k == 0 ? row.m_text : string() ) << "\n";
			}

			if ( shown < chunks.size() )
			{
				int remaining = 0;

				for ( size_t k = shown; k < chunks.size(); k++ )
				{
					remaining += chunks[ k ].m_count;
				}

				char note[ 64 ];
				snprintf( note, sizeof( note ), "... %d more bytes", remaining );
				m_file << FormatRow( true, chunks[ shown ].m_addr, NULL, 0, '.', note ) << "\n";
			}
		}

		for ( size_t n = 0; n < row.m_notes.size(); n++ )
		{
			m_file << row.m_notes[ n ] << "\n";
		}
	}

	m_rows.clear();
}


/*************************************************************************************************/
/**
	Listing::NoteByte() / NoteAddress() / NoteSave()

	These attribute things to the line currently being processed.
*/
/*************************************************************************************************/
void Listing::NoteByte( int addr, int value )
{
	if ( m_open.empty() )
	{
		return;
	}

	Row& row = m_rows[ m_open.back() ];

	if ( row.m_segments.empty() ||
		 row.m_segments.back().m_addr + static_cast<int>( row.m_segments.back().m_bytes.size() ) != addr )
	{
		Segment seg;
		seg.m_addr = addr;
		row.m_segments.push_back( seg );
	}

	row.m_segments.back().m_bytes.push_back( static_cast<unsigned char>( value ) );
}

void Listing::NoteLoopBack()
{
	if ( !m_open.empty() )
	{
		m_bSkipBlank = true;
	}
}

void Listing::NoteAddress( int addr )
{
	if ( m_open.empty() )
	{
		return;
	}

	Row& row = m_rows[ m_open.back() ];

	if ( !row.m_hasAddr )
	{
		row.m_hasAddr = true;
		row.m_addr = addr;
	}
}

void Listing::NoteSave( const string& name, int start, int end, int exec, int reload )
{
	if ( m_open.empty() )
	{
		return;
	}

	char numbers[ 96 ];
	snprintf( numbers, sizeof( numbers ), " %04X %04X %X %X", start, end, exec, reload );

	string clean;

	for ( size_t i = 0; i < name.length(); i++ )
	{
		clean += ( name[ i ] == '\r' || name[ i ] == '\n' || name[ i ] == '\t' ) ? ' ' : name[ i ];
	}

	m_rows[ m_open.back() ].m_notes.push_back( "# SAVE \"" + clean + "\"" + numbers + "   (start end exec reload)" );
}


/*************************************************************************************************/
/**
	Listing::Line
*/
/*************************************************************************************************/
Listing::Line::Line( const SourceCode* sourceCode, const string& text )
	:	m_bActive( false )
{
	Listing& listing = Listing::Instance();

	if ( listing.m_bEnabled && GlobalData::Instance().IsSecondPass() )
	{
		m_bActive = true;
		listing.BeginLine( sourceCode, text );
	}
}

Listing::Line::~Line()
{
	if ( m_bActive )
	{
		Listing::Instance().EndLine();
	}
}

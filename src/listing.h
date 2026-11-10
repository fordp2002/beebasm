/*************************************************************************************************/
/**
	listing.h

	Writes a source listing (see the -list command line option): every line of source text, with
	the address and machine code it produced in fixed columns on the left. It is meant to be read
	by people and to be easy for a debugger or emulator to parse.

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

#ifndef LISTING_H_
#define LISTING_H_

#include <cassert>
#include <fstream>
#include <string>
#include <vector>

class SourceCode;

class Listing
{
public:

	static void Create();
	static void Destroy();
	static inline Listing& Instance() { assert( m_gInstance != NULL ); return *m_gInstance; }

	// Open the listing file and write its header. Returns false if it cannot be created.

	bool Open( const char* filename );

	// Close the listing file. If keep is false the (partial) file is deleted.

	void Close( bool keep );

	inline bool IsEnabled() const { return m_bEnabled; }

	// A byte of code or data has been assembled at 'addr' (second pass only).

	void NoteByte( int addr, int value );

	// The current line defines something at 'addr' (a label, or ORG) so show the address on it.

	void NoteAddress( int addr );

	// A FOR loop has jumped back to its start; the next line read is only the rest of the FOR line.

	void NoteLoopBack();

	// A SAVE on the second pass.

	void NoteSave( const std::string& name, int start, int end, int exec, int reload );

	// One line of source being processed. Every line read (including each pass of a FOR loop, and
	// the lines of macros and included files) creates one of these. They nest.

	class Line
	{
	public:

		Line( const SourceCode* sourceCode, const std::string& text );
		~Line();

	private:

		Line( const Line& );
		Line& operator=( const Line& );

		bool	m_bActive;
	};

private:

	struct Segment
	{
		int						m_addr;
		std::vector<unsigned char>	m_bytes;
	};

	struct Row
	{
		std::string				m_file;
		char					m_flag;
		std::string				m_text;
		bool					m_skip;
		bool					m_hasAddr;
		int						m_addr;
		std::vector<Segment>	m_segments;
		std::vector<std::string>	m_notes;
	};

	Listing();
	~Listing();

	void BeginLine( const SourceCode* sourceCode, const std::string& text );
	void EndLine();
	void WriteRows();

	static std::string FormatRow( bool showAddr, int addr, const unsigned char* bytes, int count,
								  char flag, const std::string& text );

	static Listing*		m_gInstance;

	std::string			m_filename;
	std::ofstream		m_file;
	bool				m_bEnabled;
	std::string			m_lastFile;
	bool				m_bSkipBlank;
	std::vector<Row>	m_rows;
	std::vector<size_t>	m_open;
};

#endif // LISTING_H_

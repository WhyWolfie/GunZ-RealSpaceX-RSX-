#include "stdafx.h"
#include "MMatchDBFilter.h"
#include <sstream>
#include <iomanip>
#include <utility>


MMatchDBFilter::MMatchDBFilter()
{
}


MMatchDBFilter::~MMatchDBFilter()
{
}


string MMatchDBFilter::Filtering( const string& str )
{
	/*
	static string strRemoveTok = "'";

	string strTmp = str;

	string::size_type pos;

	while( (pos = strTmp.find_first_of(strRemoveTok)) != string::npos )
		strTmp.erase( pos, 1 );

	return strTmp;
	*/
	ostringstream ret;

	for (string::size_type i = 0; i < str.length(); ++i)
		ret << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << (int)str[i];

	return "0x" + ret.str();
}
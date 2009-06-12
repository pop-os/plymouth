#! /bin/sh
exec perl -x $0 ${1+"$@"}
	if 0;
#!perl

print "static const char* STRINGIFY_VAR = ";
while (<>)
{
	chomp;
    s/\\/\\\\/g;
    s/"/\\"/g;
	print "\"$_\\n\"\n";
}
print ";";
